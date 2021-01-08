#include <AudioUnit/AudioUnit.h>
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>
#include <unistd.h>
#include <errno.h>

#include <obs-module.h>
#include <util/threading.h>
#include <util/c99defs.h>

#include "mac-helpers.h"
#include "audio-device-enum.h"



#define PROPERTY_DEFAULT_DEVICE kAudioHardwarePropertyDefaultInputDevice
#define PROPERTY_FORMATS kAudioStreamPropertyAvailablePhysicalFormats

#define SCOPE_OUTPUT kAudioUnitScope_Output
#define SCOPE_INPUT  kAudioUnitScope_Input
#define SCOPE_GLOBAL kAudioUnitScope_Global

#define BUS_OUTPUT 0
#define BUS_INPUT  1

#define MAX_DEVICES 20

#define set_property AudioUnitSetProperty
#define get_property AudioUnitGetProperty

#define TEXT_AUDIO_INPUT    obs_module_text("CoreAudio.InputCapture");
#define TEXT_AUDIO_OUTPUT   obs_module_text("CoreAudio.OutputCapture");
#define TEXT_DEVICE         obs_module_text("CoreAudio.Device")
#define TEXT_DEVICE_DEFAULT obs_module_text("CoreAudio.Device.Default")

struct coreaudio_data {
	char               *device_name;
	char               *device_uid;
	AudioUnit           unit;
	AudioDeviceID       device_id;
	AudioBufferList    *buf_list;
	bool                au_initialized;
	bool                active;
	bool                default_device;
	bool                input;
	bool                no_devices;

	uint32_t            sample_rate;
	enum audio_format   format;
	enum speaker_layout speakers;

	pthread_t           reconnect_thread;
	os_event_t          *exit_event;
	volatile bool       reconnecting;
	unsigned long       retry_time;

	obs_source_t        *source;
    int64_t             prev_update_ts;
};

static void coreaudio_update(void *data, obs_data_t *settings);

static bool get_default_output_device(struct coreaudio_data *ca)
{
	struct device_list list;

	memset(&list, 0, sizeof(struct device_list));
	coreaudio_enum_devices(&list, false);

	if (!list.items.num)
		return false;

	bfree(ca->device_uid);
	ca->device_uid = bstrdup(list.items.array[0].value.array);

	device_list_free(&list);
	return true;
}

static bool find_device_id_by_uid(struct coreaudio_data *ca)
{
	UInt32      size      = sizeof(AudioDeviceID);
	CFStringRef cf_uid    = NULL;
	CFStringRef qual      = NULL;
	UInt32      qual_size = 0;
	OSStatus    stat;
	bool        success;

	AudioObjectPropertyAddress addr = {
		.mScope   = kAudioObjectPropertyScopeGlobal,
		.mElement = kAudioObjectPropertyElementMaster
	};

	if (!ca->device_uid)
		ca->device_uid = bstrdup("default");

	/* have to do this because mac output devices don't actually exist */
	if (astrcmpi(ca->device_uid, "default") == 0) {
		if (ca->input) {
			ca->default_device = true;
		} else {
			if (!get_default_output_device(ca)) {
				ca->no_devices = true;
				return false;
			}
		}
	}

	cf_uid = CFStringCreateWithCString(NULL, ca->device_uid,
			kCFStringEncodingUTF8);

	if (ca->default_device) {
		addr.mSelector = PROPERTY_DEFAULT_DEVICE;
		stat = AudioObjectGetPropertyData(kAudioObjectSystemObject,
				&addr, qual_size, &qual, &size, &ca->device_id);
		success = (stat == noErr);
	} else {
		success = coreaudio_get_device_id(cf_uid, &ca->device_id);
	}

	if (cf_uid)
		CFRelease(cf_uid);

	return success;
}


static inline enum audio_format convert_ca_format(UInt32 format_flags,
		UInt32 bits)
{
	bool planar = (format_flags & kAudioFormatFlagIsNonInterleaved) != 0;

	if (format_flags & kAudioFormatFlagIsFloat)
		return planar ? AUDIO_FORMAT_FLOAT_PLANAR : AUDIO_FORMAT_FLOAT;

	if (!(format_flags & kAudioFormatFlagIsSignedInteger) && bits == 8)
		return planar ? AUDIO_FORMAT_U8BIT_PLANAR : AUDIO_FORMAT_U8BIT;

	/* not float?  not signed int?  no clue, fail */
	if ((format_flags & kAudioFormatFlagIsSignedInteger) == 0)
		return AUDIO_FORMAT_UNKNOWN;

	if (bits == 16)
		return planar ? AUDIO_FORMAT_16BIT_PLANAR : AUDIO_FORMAT_16BIT;
	else if (bits == 32)
		return planar ? AUDIO_FORMAT_32BIT_PLANAR : AUDIO_FORMAT_32BIT;

	return AUDIO_FORMAT_UNKNOWN;
}

static inline enum speaker_layout convert_ca_speaker_layout(UInt32 channels)
{
	/* directly map channel count to enum values */
	if (channels >= 1 && channels <= 8 && channels != 7)
		return (enum speaker_layout)channels;

	return SPEAKERS_UNKNOWN;
}
//! already do this.
static bool coreaudio_init_format(struct coreaudio_data *ca)
{
	AudioStreamBasicDescription desc;
	OSStatus stat;
	UInt32 size = sizeof(desc);

	stat = get_property(ca->unit, kAudioUnitProperty_StreamFormat,
			SCOPE_INPUT, BUS_INPUT, &desc, &size);
	if (!ca_success(stat, ca, "coreaudio_init_format", "get input format"))
		return false;

	/* Certain types of devices have no limit on channel count, and
	 * there's no way to know the actual number of channels it's using,
	 * so if we encounter this situation just force to stereo */
        if (desc.mChannelsPerFrame > 8) {
                desc.mChannelsPerFrame = 2;
                desc.mBytesPerFrame = 2 * desc.mBitsPerChannel / 8;
                desc.mBytesPerPacket =
                        desc.mFramesPerPacket * desc.mBytesPerFrame;
        }

	stat = set_property(ca->unit, kAudioUnitProperty_StreamFormat,
			SCOPE_OUTPUT, BUS_INPUT, &desc, size);
	if (!ca_success(stat, ca, "coreaudio_init_format", "set output format"))
		return false;

	if (desc.mFormatID != kAudioFormatLinearPCM) {
		ca_warn(ca, "coreaudio_init_format", "format is not PCM");
		return false;
	}

	ca->format = convert_ca_format(desc.mFormatFlags, desc.mBitsPerChannel);
	if (ca->format == AUDIO_FORMAT_UNKNOWN) {
		ca_warn(ca, "coreaudio_init_format", "unknown format flags: "
				"%u, bits: %u",
				(unsigned int)desc.mFormatFlags,
				(unsigned int)desc.mBitsPerChannel);
		return false;
	}

	ca->sample_rate = (uint32_t)desc.mSampleRate;
	ca->speakers = convert_ca_speaker_layout(desc.mChannelsPerFrame);

	if (ca->speakers == SPEAKERS_UNKNOWN) {
		ca_warn(ca, "coreaudio_init_format", "unknown speaker layout: "
				"%u channels",
				(unsigned int)desc.mChannelsPerFrame);
		return false;
	}

	return true;
}

static bool coreaudio_init_buffer(struct coreaudio_data *ca)
{
	UInt32 buf_size = 0;
	UInt32 size     = 0;
	UInt32 frames   = 0;
	OSStatus stat;

	AudioObjectPropertyAddress addr = {
		kAudioDevicePropertyStreamConfiguration,
		kAudioDevicePropertyScopeInput,
		kAudioObjectPropertyElementMaster
	};

	stat = AudioObjectGetPropertyDataSize(ca->device_id, &addr, 0, NULL,
			&buf_size);
	if (!ca_success(stat, ca, "coreaudio_init_buffer", "get list size"))
		return false;

	size = sizeof(frames);
	stat = get_property(ca->unit, kAudioDevicePropertyBufferFrameSize,
			SCOPE_GLOBAL, 0, &frames, &size);
	if (!ca_success(stat, ca, "coreaudio_init_buffer", "get frame size"))
		return false;

	/* ---------------------- */

	ca->buf_list = bmalloc(buf_size);

	stat = AudioObjectGetPropertyData(ca->device_id, &addr, 0, NULL,
			&buf_size, ca->buf_list);
	if (!ca_success(stat, ca, "coreaudio_init_buffer", "allocate")) {
		bfree(ca->buf_list);
		ca->buf_list = NULL;
		return false;
	}

	for (UInt32 i = 0; i < ca->buf_list->mNumberBuffers; i++) {
		size = ca->buf_list->mBuffers[i].mDataByteSize;
		ca->buf_list->mBuffers[i].mData = bmalloc(size);
	}
	
	return true;
}

static void buf_list_free(AudioBufferList *buf_list)
{
	if (buf_list) {
		for (UInt32 i = 0; i < buf_list->mNumberBuffers; i++)
			bfree(buf_list->mBuffers[i].mData);

		bfree(buf_list);
	}
}

//////
typedef struct obs_source_audio obs_source_audio;
typedef struct buf{
    uint8_t *data;
    int32_t  size;
} buf_t;
#ifndef __cplusplus
#define nullptr NULL
#endif
#if 1
NSContext         *NS_inst;
buf_t              NS_data;
SwrContext         *swr_ctx;
struct obs_source_audio audio_ns;
bool             enable_ns;
u_char             *audio_frame_cache;
int32_t             audio_frame_cache_capacity;
int32_t             audio_frame_cache_size;
#endif



#ifndef DBG_OUT_API
#define DBG_OUT_API
#define LEVEL            3
#define dbg(fmt, ...)  do{ if( LEVEL>3 )                             \
printf( "DBG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}while(0)
#define msg(fmt, ...)  do{ if( LEVEL>2 )                             \
printf( "MSG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}while(0)
#define war(fmt, ...)  do{ if( LEVEL>1 )                             \
printf( "WAR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}while(0)
#define err(fmt, ...)  do{ if( LEVEL>0 )                             \
printf( "ERR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}while(0)
#define check_and_return(_c,_f1,_f2,_f3,_e)                            \
if(_c){ err("\"%s\" is triggered!\n",#_c);_f1;_f2;_f3;return _e; }
#endif

/**
 *@ brief  audio_frame_alloc
 */
static AVFrame*
audio_frame_alloc(enum AVSampleFormat sample_fmt,
                  int32_t sample_rate, int32_t nb_samples, uint64_t channel_layout)
{
    AVFrame *frame = nullptr;
    
    check_and_return(nullptr == (frame = av_frame_alloc()), av_frame_free(&frame), nullptr, nullptr, NULL);
    frame->format = sample_fmt;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    frame->channel_layout = channel_layout;
    frame->channels = av_get_channel_layout_nb_channels(channel_layout);
    /*frame->pts = 0;         NOTE: maybe the pts need to be set! */
    if (nb_samples)
        check_and_return(av_frame_get_buffer(frame, 0) < 0, av_frame_free(&frame), nullptr, nullptr, NULL);
    
    return frame;
}

/**
 *@ brief  audio_frame_xfree
 */
static void
audio_frame_xfree(AVFrame** frame)
{
    av_frame_free(frame);
}



static inline void
volume_adjust(short  *data, int32_t size, int32_t multiple)
{
    check_and_return( NULL== data && size <=0,
                     nullptr, nullptr, nullptr, );
    
    if (multiple < 0) multiple = 1;
    if (multiple >10) multiple = 10;
    
    for (int32_t i=0; i< size/2; ++i )
    {
        data[i]  *= multiple;
        if (data[i] > 32767 )
            data[i] = 32767;
        if (data[i] < -32768)
            data[i] = -32768;
    }
}

/////////
static OSStatus input_callback(
		void *data,
		AudioUnitRenderActionFlags *action_flags,
		const AudioTimeStamp *ts_data,
		UInt32 bus_num,
		UInt32 frames,
		AudioBufferList *ignored_buffers)
{
	struct coreaudio_data *ca = data;
	OSStatus stat;
	struct obs_source_audio audio;

	stat = AudioUnitRender(ca->unit, action_flags, ts_data, bus_num, frames,
			ca->buf_list);
    if (!ca_success(stat, ca, "input_callback", "audio retrieval")){
        unsigned long cur_ms = clock();
        if (!ca->prev_update_ts || cur_ms - ca->prev_update_ts > 5000000) {
            blog(LOG_WARNING, "Prepare reset the coreaudio module.");
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                blog(LOG_WARNING, "Reset the coreaudio module.");
                obs_data_t * settings = obs_source_get_settings(ca->source);
                coreaudio_update(ca, settings);
                obs_data_release(settings);
            });
            ca->prev_update_ts = cur_ms;
        }
        return noErr;
    }

	for (UInt32 i = 0; i < ca->buf_list->mNumberBuffers; i++)
		audio.data[i] = ca->buf_list->mBuffers[i].mData;

	audio.frames          = frames;
	audio.speakers        = ca->speakers;
	audio.format          = ca->format;
	audio.samples_per_sec = ca->sample_rate;
	audio.timestamp       = ts_data->mHostTime;
    //enable_ns=false;
    //blog(LOG_INFO, "seting.....enable_ns=%\n",enable_ns);
    if ( enable_ns )
    {
#if USE_NOISE_SUPPRESSION
        int32_t block_size= get_audio_bytes_per_channel(audio.format)*get_audio_channels(audio.speakers);
        int32_t size = block_size * audio.frames;//total size
        
        const  int32_t cache_10ms_size = block_size*audio.samples_per_sec / 100;
        // the capture's capture interval is not 10ms.
        if ( audio.samples_per_sec / audio.frames != 100 )
        {
            if ( audio_frame_cache != nullptr )
            {
                if (size < (audio_frame_cache_capacity - audio_frame_cache_size))
                {
                    memcpy(audio_frame_cache + audio_frame_cache_size, audio.data[0], size);
                    audio_frame_cache_size += size;
                }
                else {
                    blog(LOG_ERROR, "%s: size=%d is too big!\n", obs_source_get_name(ca->source), size);
                    return noErr;
                }
                
                while (audio_frame_cache_size >= cache_10ms_size)
                {
                    audio.data[0] = (u_char*)audio_frame_cache;
                    audio.frames = audio.samples_per_sec / 100;
                    
                    // enlarge  multiple=2.
                    // volume_adjust((short*)audio.data[0], cache_10ms_size, 2);
                    
                    // noise suppression only support 16k && 8k sampling rate.
                    if (!NS_Process_Ext(enable_ns, 0, ca->source, &audio, &audio_ns))
                        blog(LOG_ERROR, "%s:  NS_Process_Ext failed!\n", obs_source_get_name(ca->source));
                    if (audio.format != AUDIO_FORMAT_UNKNOWN)
                        obs_source_output_audio(ca->source, &audio_ns);
                    
                    audio_frame_cache_size -= cache_10ms_size;
                    memmove(audio_frame_cache, audio_frame_cache + cache_10ms_size, audio_frame_cache_size);
                }
            }
        }else {
            //enlarge  multiple=2.
            //volume_adjust((short*)audio.data[0], size, 2);
            if (!NS_Process_Ext(enable_ns, ca->source, 2, &audio, &audio_ns))
                blog(LOG_ERROR, "%s:  NS_Process_Ext failed!\n", obs_source_get_name(ca->source));
            if (audio.format != AUDIO_FORMAT_UNKNOWN)
                obs_source_output_audio(ca->source, &audio_ns);
        }
#endif
        
    }else{
        obs_source_output_audio(ca->source, &audio);
    }
	UNUSED_PARAMETER(ignored_buffers);
	return noErr;
}

static void coreaudio_stop(struct coreaudio_data *ca);
static bool coreaudio_init(struct coreaudio_data *ca);
static void coreaudio_uninit(struct coreaudio_data *ca);

static void *reconnect_thread(void *param)
{
	struct coreaudio_data *ca = param;

	ca->reconnecting = true;

    int n = 0;
	while (os_event_timedwait(ca->exit_event, ca->retry_time) == ETIMEDOUT) {
		if (coreaudio_init(ca))
			break;
        else if(!ca->input && ++n > 3)
            break;
	}

	blog(LOG_DEBUG, "coreaudio: exit the reconnect thread");
	ca->reconnecting = false;
	return NULL;
}

static void coreaudio_begin_reconnect(struct coreaudio_data *ca)
{
	int ret;

	if (ca->reconnecting)
		return;

	ret = pthread_create(&ca->reconnect_thread, NULL, reconnect_thread, ca);
	if (ret != 0)
		blog(LOG_WARNING, "[coreaudio_begin_reconnect] failed to "
		                  "create thread, error code: %d", ret);
}
//! already do this.
static OSStatus notification_callback(
		AudioObjectID id,
		UInt32 num_addresses,
		const AudioObjectPropertyAddress addresses[],
		void *data)
{
	struct coreaudio_data *ca = data;

	coreaudio_stop(ca);
	coreaudio_uninit(ca);

	if (addresses[0].mSelector == PROPERTY_DEFAULT_DEVICE)
		ca->retry_time = 300;
	else
		ca->retry_time = 2000;

	blog(LOG_INFO, "coreaudio: device '%s' disconnected or changed.  "
	               "attempting to reconnect", ca->device_name);

	coreaudio_begin_reconnect(ca);

	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(num_addresses);

	return noErr;
}

static OSStatus add_listener(struct coreaudio_data *ca, UInt32 property)
{
	AudioObjectPropertyAddress addr = {
		property,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	return AudioObjectAddPropertyListener(ca->device_id, &addr,
			notification_callback, ca);
}

static bool coreaudio_init_hooks(struct coreaudio_data *ca)
{
	OSStatus stat;
	AURenderCallbackStruct callback_info = {
		.inputProc       = input_callback,
		.inputProcRefCon = ca
	};

	stat = add_listener(ca, kAudioDevicePropertyDeviceIsAlive);
	if (!ca_success(stat, ca, "coreaudio_init_hooks",
				"set disconnect callback"))
		return false;

	stat = add_listener(ca, PROPERTY_FORMATS);
	if (!ca_success(stat, ca, "coreaudio_init_hooks",
				"set format change callback"))
		return false;

	if (ca->default_device) {
		AudioObjectPropertyAddress addr = {
			PROPERTY_DEFAULT_DEVICE,
			kAudioObjectPropertyScopeGlobal,
			kAudioObjectPropertyElementMaster
		};

		stat = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
				&addr, notification_callback, ca);
		if (!ca_success(stat, ca, "coreaudio_init_hooks",
					"set device change callback"))
			return false;
	}

	stat = set_property(ca->unit, kAudioOutputUnitProperty_SetInputCallback,
			SCOPE_GLOBAL, 0, &callback_info, sizeof(callback_info));
	if (!ca_success(stat, ca, "coreaudio_init_hooks", "set input callback"))
		return false;

	return true;
}

static void coreaudio_remove_hooks(struct coreaudio_data *ca)
{
	AURenderCallbackStruct callback_info = {
		.inputProc       = NULL,
		.inputProcRefCon = NULL
	};

	AudioObjectPropertyAddress addr = {
		kAudioDevicePropertyDeviceIsAlive,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioObjectRemovePropertyListener(ca->device_id, &addr,
			notification_callback, ca);

	addr.mSelector = PROPERTY_FORMATS;
	AudioObjectRemovePropertyListener(ca->device_id, &addr,
			notification_callback, ca);

	if (ca->default_device) {
		addr.mSelector = PROPERTY_DEFAULT_DEVICE;
		AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
				&addr, notification_callback, ca);
	}

	set_property(ca->unit, kAudioOutputUnitProperty_SetInputCallback,
			SCOPE_GLOBAL, 0, &callback_info, sizeof(callback_info));
}

static bool coreaudio_get_device_name(struct coreaudio_data *ca)
{
	CFStringRef cf_name = NULL;
	UInt32 size = sizeof(CFStringRef);
	char name[1024];

	const AudioObjectPropertyAddress addr = {
		kAudioDevicePropertyDeviceNameCFString,
		kAudioObjectPropertyScopeInput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus stat = AudioObjectGetPropertyData(ca->device_id, &addr,
			0, NULL, &size, &cf_name);
	if (stat != noErr) {
		blog(LOG_WARNING, "[coreaudio_get_device_name] failed to "
		                  "get name: %d", (int)stat);
		return false;
	}

	if (!cf_to_cstr(cf_name, name, 1024)) {
		blog(LOG_WARNING, "[coreaudio_get_device_name] failed to "
		                  "convert name to cstr for some reason");
		return false;
	}

	bfree(ca->device_name);
	ca->device_name = bstrdup(name);

	if (cf_name)
		CFRelease(cf_name);

	return true;
}

//! already do this.
static bool coreaudio_start(struct coreaudio_data *ca)
{
	OSStatus stat;

	if (ca->active)
		return true;

	stat = AudioOutputUnitStart(ca->unit);
	ca->active = ca_success(stat, ca, "coreaudio_start", "start audio");
    return ca->active;
}

//! already do this.
static void coreaudio_stop(struct coreaudio_data *ca)
{
	OSStatus stat;

	if (!ca->active)
		return;

	ca->active = false;

	stat = AudioOutputUnitStop(ca->unit);
	ca_success(stat, ca, "coreaudio_stop", "stop audio");
}

//! already do this.
static bool coreaudio_init_unit(struct coreaudio_data *ca)
{
	AudioComponentDescription desc = {
		.componentType    = kAudioUnitType_Output,
		.componentSubType = kAudioUnitSubType_HALOutput
	};

	AudioComponent component = AudioComponentFindNext(NULL, &desc);
	if (!component) {
		ca_warn(ca, "coreaudio_init_unit", "find component failed");
		return false;
	}

	OSStatus stat = AudioComponentInstanceNew(component, &ca->unit);
	if (!ca_success(stat, ca, "coreaudio_init_unit", "instance unit"))
		return false;

	ca->au_initialized = true;
	return true;
}

#if 0 //test code.
 void AudioUnitChangeListener(    void *                inRefCon,
AudioUnit            inUnit,
AudioUnitPropertyID    inID,
AudioUnitScope        inScope,
AudioUnitElement    inElement)
{
    auto ca = (struct coreaudio_data*)inRefCon;
    
}

OSStatus AudioUnitRenderNotify(    void *                            inRefCon,
                    AudioUnitRenderActionFlags *    ioActionFlags,
                    const AudioTimeStamp *            inTimeStamp,
                    UInt32                            inBusNumber,
                    UInt32                            inNumberFrames,
                    AudioBufferList * __nullable    ioData)
{
    auto ca = (struct coreaudio_data*)inRefCon;
    return noErr;
}
#endif //0

static bool coreaudio_init(struct coreaudio_data *ca)
{
	OSStatus stat;

	if (ca->au_initialized)
		return true;

	if (!find_device_id_by_uid(ca))
		return false;
	if (!coreaudio_get_device_name(ca))
		return false;
	if (!coreaudio_init_unit(ca))
		return false;

#if 0   //test code.
    AudioUnitAddPropertyListener(ca->unit,
                                 kAudioOutputUnitProperty_EnableIO,
                                 AudioUnitChangeListener,ca);
    
    AudioUnitAddRenderNotify(ca->unit,AudioUnitRenderNotify,ca);
#endif //0
    
	stat = enable_io(ca, IO_TYPE_INPUT, true);
	if (!ca_success(stat, ca, "coreaudio_init", "enable input io"))
		goto fail;

	stat = enable_io(ca, IO_TYPE_OUTPUT, false);
	if (!ca_success(stat, ca, "coreaudio_init", "disable output io"))
		goto fail;

	stat = set_property(ca->unit, kAudioOutputUnitProperty_CurrentDevice,
			SCOPE_GLOBAL, 0, &ca->device_id, sizeof(ca->device_id));
	if (!ca_success(stat, ca, "coreaudio_init", "set current device"))
		goto fail;

	if (!coreaudio_init_format(ca))
		goto fail;
	if (!coreaudio_init_buffer(ca))
		goto fail;
	if (!coreaudio_init_hooks(ca))
		goto fail;

	stat = AudioUnitInitialize(ca->unit);
	if (!ca_success(stat, ca, "coreaudio_initialize", "initialize"))
		goto fail;

	if (!coreaudio_start(ca))
		goto fail;

	blog(LOG_INFO, "coreaudio: device '%s' initialized", ca->device_name);
	return ca->au_initialized;

fail:
	coreaudio_uninit(ca);
	return false;
}

static void coreaudio_try_init(struct coreaudio_data *ca)
{
	if (!coreaudio_init(ca)) {
		blog(LOG_INFO, "coreaudio: failed to find device "
		               "uid: %s, waiting for connection",
		               ca->device_uid);

		ca->retry_time = 2000;

		if (ca->no_devices)
			blog(LOG_INFO, "coreaudio: no device found");
		else
			coreaudio_begin_reconnect(ca);
	}
}


/* ------------------------------------------------------------------------- */




static void coreaudio_update(void *data, obs_data_t *settings)
{
	struct coreaudio_data *ca = data;

	coreaudio_shutdown(ca);

	bfree(ca->device_uid);
	ca->device_uid = bstrdup(obs_data_get_string(settings, "device_id"));
    enable_ns=obs_data_get_bool(settings, "enable_ns");
	coreaudio_try_init(ca);
}

static void coreaudio_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "device_id", "default");
}

static void *coreaudio_create(obs_data_t *settings, obs_source_t *source,
		bool input)
{
	struct coreaudio_data *ca = bzalloc(sizeof(struct coreaudio_data));

	if (os_event_init(&ca->exit_event, OS_EVENT_TYPE_MANUAL) != 0) {
		blog(LOG_ERROR, "[coreaudio_create] failed to create "
		                "semephore: %d", errno);
		bfree(ca);
		return NULL;
	}

	ca->device_uid = bstrdup(obs_data_get_string(settings, "device_id"));
	ca->source     = source;
	ca->input      = input;

	if (!ca->device_uid)
		ca->device_uid = bstrdup("default");

	coreaudio_try_init(ca);
    
#if USE_NOISE_SUPPRESSION
    NS_inst=nullptr;
    swr_ctx=nullptr;
    NS_data.data=nullptr;
    NS_data.size=0;
    enable_ns=obs_data_get_bool(settings, "enable_ns");
    audio_frame_cache_capacity=10*1024*1024;
    audio_frame_cache=(u_char*)calloc(1, audio_frame_cache_capacity);
    audio_frame_cache_size=0;
#endif
	return ca;
}

static void *coreaudio_create_input_capture(obs_data_t *settings,
		obs_source_t *source)
{
	return coreaudio_create(settings, source, true);
}

static void *coreaudio_create_output_capture(obs_data_t *settings,obs_source_t *source)
{
	return coreaudio_create(settings, source, false);
}

static obs_properties_t *coreaudio_properties(bool input)
{
	obs_properties_t   *props = obs_properties_create();
	obs_property_t     *property;
	struct device_list devices;

	memset(&devices, 0, sizeof(struct device_list));

	property = obs_properties_add_list(props, "device_id", TEXT_DEVICE,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	coreaudio_enum_devices(&devices, input);

	if (devices.items.num)
		obs_property_list_add_string(property, TEXT_DEVICE_DEFAULT,
				"default");

	for (size_t i = 0; i < devices.items.num; i++) {
		struct device_item *item = devices.items.array+i;
		obs_property_list_add_string(property,
				item->name.array, item->value.array);
	}

	device_list_free(&devices);
	return props;
}

static obs_properties_t *coreaudio_input_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	return coreaudio_properties(true);
}

static obs_properties_t *coreaudio_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	return coreaudio_properties(false);
}

struct obs_source_info coreaudio_input_capture_info = {
	.id             = "coreaudio_input_capture",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_AUDIO |
	                  OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name       = coreaudio_input_getname,
	.create         = coreaudio_create_input_capture,
	.destroy        = coreaudio_destroy,
	.update         = coreaudio_update,
	.get_defaults   = coreaudio_defaults,
	.get_properties = coreaudio_input_properties
};

struct obs_source_info coreaudio_output_capture_info = {
	.id             = "coreaudio_output_capture",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_AUDIO |
	                  OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name       = coreaudio_output_getname,
	.create         = coreaudio_create_output_capture,
	.destroy        = coreaudio_destroy,
	.update         = coreaudio_update,
	.get_defaults   = coreaudio_defaults,
	.get_properties = coreaudio_output_properties
};
