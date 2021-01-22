#include "MacExtenalAudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardware.h>
#include <QString>

extern "C"
{

#include <libavutil/opt.h>
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}


namespace CC{
#define OBSERVER_LOG(str)                                       \
    do{                                                         \
        if(m_obsever){                                         \
            m_obsever->onCaptureAudioLog(str);                 \
        }                                                       \
    }while(0)


bool checkStatus(int status){
    if (status) {
        printf("Status not 0! %d\n", status);
        return false;
    }
    return true;
}


#define PROPERTY_DEFAULT_DEVICE kAudioHardwarePropertyDefaultInputDevice
#define PROPERTY_FORMATS kAudioStreamPropertyAvailablePhysicalFormats

#define SCOPE_OUTPUT kAudioUnitScope_Output
#define SCOPE_INPUT  kAudioUnitScope_Input
#define SCOPE_GLOBAL kAudioUnitScope_Global

#define BUS_OUTPUT 0
#define BUS_INPUT  1

#define set_property AudioUnitSetProperty
#define get_property AudioUnitGetProperty


#define OUTPUT_PCM_FILE
#ifdef OUTPUT_PCM_FILE
#include <stdio.h>
#ifdef WIN32
static FILE *g_pFile = nullptr;
#else
static FILE *g_pFile = fopen("source.pcm", "wb");
static FILE *g_pFile1 = fopen("beforesource.pcm", "wb");
#endif
#endif


OSStatus MacExtenalAudio::notification_callback(AudioObjectID id, UInt32 num_addresses, const AudioObjectPropertyAddress addresses[], void *data)
{

}

OSStatus MacExtenalAudio::inputCallback(void *data, AudioUnitRenderActionFlags *action_flags, const AudioTimeStamp *ts_data, UInt32 bus_num, UInt32 frames, AudioBufferList *ignored_buffers)
{
    MacExtenalAudio *pThis = (MacExtenalAudio *)data;
    OSStatus stat;
//    struct obs_source_audio audio;

    stat = AudioUnitRender(pThis->m_audio_unit, action_flags, ts_data, bus_num, frames,
            pThis->m_tempBufferList);

    if(!checkStatus(stat))
    {
        return noErr;
    }

    for (UInt32 i = 0; i < pThis->m_tempBufferList->mNumberBuffers; i++)
    {
        AudioBuffer ab= pThis->m_tempBufferList->mBuffers[i];
        uint8_t **dst_data = NULL;
        int dst_linesize;

        /* buffer is going to be directly written to a rawaudio file, no alignment */
        int dst_nb_samples;
         dst_nb_samples =
                av_rescale_rnd(frames, pThis->m_want_sample_rate, pThis->m_sample_rate, AV_ROUND_UP);
        static int max_dst_nb_samples = dst_nb_samples;
        int ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, pThis->m_want_channels,
            dst_nb_samples, AV_SAMPLE_FMT_S16, 0);
        if (ret<0)
        {
            pThis->m_obsever->onCaptureAudioLog("sample alloc array and sample error.");
            return noErr;
        }

        dst_nb_samples = av_rescale_rnd(swr_get_delay(pThis->m_swr_ctx, pThis->m_sample_rate) +
                                          frames, pThis->m_want_sample_rate, pThis->m_sample_rate, AV_ROUND_UP);
        pThis->m_obsever->onCaptureAudioLog("get des samples : "+std::to_string(dst_nb_samples)+" src frames : "+std::to_string(frames));
        if (dst_nb_samples > max_dst_nb_samples) {
            av_freep(&dst_data[0]);
            int ret = av_samples_alloc(dst_data, &dst_linesize, pThis->m_want_channels,
                dst_nb_samples, AV_SAMPLE_FMT_S16, 1);
            if (ret < 0)
                break;
            max_dst_nb_samples = dst_nb_samples;
        }
//#ifdef OUTPUT_PCM_FILE
//        if (g_pFile1)
//        {
//            fwrite(ab.mData,ab.mDataByteSize, 1, g_pFile1);
//            fflush(g_pFile1);
//        }
//#endif
        int convertRet = swr_convert(pThis->m_swr_ctx,dst_data,dst_nb_samples,
                                    (const uint8_t**)&ab.mData,frames);
        if(convertRet<0)
        {
            pThis->m_obsever->onCaptureAudioLog("convert audio data error.");
            return noErr;
        }
        int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, pThis->m_want_channels,
                                                         convertRet, AV_SAMPLE_FMT_S16, 1);
        pThis->m_obsever->onCaptureAudioLog("buf size :" +std::to_string(dst_bufsize));

        if(pThis->m_data_callback)
        {
            // sample_num,int32_t channels,int32_t byte_per_sample
            pThis->m_data_callback(dst_data[0],dst_nb_samples,pThis->m_want_channels,2);
        }
#ifdef OUTPUT_PCM_FILE
        if (g_pFile)
        {
            fwrite(dst_data[0],dst_bufsize, 1, g_pFile);
            fflush(g_pFile);
        }
#endif
        av_freep(&dst_data[0]);
    }

    return noErr;
}

bool MacExtenalAudio::coreAudioInit()
{
    AudioComponentDescription desc = {
        .componentType    = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_HALOutput
    };

    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (!component) {
       m_obsever->onCaptureAudioLog("Audio Component Find Next error.");
       return false;
    }

    OSStatus stat = AudioComponentInstanceNew(component, &m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("AudioComponentInstanceNew error :"+std::to_string(stat));
        return false;
    }
    return true;
}

void MacExtenalAudio::coreAudioShutdown()
{
    coreAudioUninit();
    if(m_audio_unit)
    {
        AudioComponentInstanceDispose(m_audio_unit);
    }
}

bool MacExtenalAudio::coreAudioStart()
{
    OSStatus stat;
    if (m_active)
        return true;
    stat = AudioOutputUnitStart(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit start error :"+std::to_string(stat));
    }
    m_active = true;
    return m_active;
}

bool MacExtenalAudio::coreAudioStop()
{
    OSStatus stat;
    stat = AudioOutputUnitStop(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit stop error :"+std::to_string(stat));
        return false;
    }
    return true;
}

bool MacExtenalAudio::initAudioUnitFormat()
{
    AudioStreamBasicDescription desc;
    OSStatus stat;
    UInt32 size = sizeof(desc);
    stat = get_property(m_audio_unit, kAudioUnitProperty_StreamFormat,
            SCOPE_INPUT, BUS_INPUT, &desc, &size);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit get stream format error :"+std::to_string(stat));
        return false;
    }

    if (desc.mChannelsPerFrame > 8)
    {
        desc.mSampleRate = 48000;
        desc.mChannelsPerFrame = 2;
        desc.mBytesPerFrame = 2 * desc.mBitsPerChannel / 8;
        desc.mBytesPerPacket =
                desc.mFramesPerPacket * desc.mBytesPerFrame;
    }

    stat = set_property(m_audio_unit, kAudioUnitProperty_StreamFormat,
            SCOPE_OUTPUT, BUS_INPUT, &desc, size);
    if(!checkStatus(stat))//！maybe kAudioUnitErr_FormatNotSupported
    {
        m_obsever->onCaptureAudioLog("auduio out put unit set stream format error :"+std::to_string(stat));
        return false;
    }
    if (desc.mFormatID != kAudioFormatLinearPCM) {
        m_obsever->onCaptureAudioLog("auduio out put format is not linear PCM.");
        return false;
    }

    if(!formatIsValid(desc.mFormatFlags,desc.mBitsPerChannel))
    {
        m_obsever->onCaptureAudioLog("format is not valid.");
        return false;
    }

    int srcformat = convert_ca_format(desc.mFormatFlags, desc.mBitsPerChannel);

    m_byte_per_frame = desc.mBytesPerFrame;
    m_sample_rate=desc.mSampleRate;
    m_channels=desc.mChannelsPerFrame;

    if(!initResampleContext())
    {
        OBSERVER_LOG("resample audio context is not create.");
        return false;
    }




    return true;
}

bool MacExtenalAudio::initAudioUnitBuffer()
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

    stat = AudioObjectGetPropertyDataSize(m_device_id, &addr, 0, NULL,
            &buf_size);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit get data size error :"+std::to_string(stat));
        return false;
    }

    size = sizeof(frames);
    stat = get_property(m_audio_unit, kAudioDevicePropertyBufferFrameSize,
            SCOPE_GLOBAL, 0, &frames, &size);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit get PropertyBufferFrameSize error :"+std::to_string(stat));
        return false;
    }


    /* ---------------------- */

    m_tempBufferList = (AudioBufferList*)malloc(buf_size);

    stat = AudioObjectGetPropertyData(m_device_id, &addr, 0, NULL,
            &buf_size, m_tempBufferList);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit get PropertyBufferFrameSize2 error :"+std::to_string(stat));
        free(m_tempBufferList);
        m_tempBufferList=nullptr;
        return false;
    }

    for (UInt32 i = 0; i < m_tempBufferList->mNumberBuffers; i++) {
        size = m_tempBufferList->mBuffers[i].mDataByteSize;
        m_tempBufferList->mBuffers[i].mData = malloc(size);
    }

    return true;
}

bool MacExtenalAudio::initCoreAudioUnitCallback()
{
    OSStatus stat;
    AURenderCallbackStruct callback_info = {
        .inputProc       = inputCallback,
        .inputProcRefCon = this
    };
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    stat = AudioObjectAddPropertyListener(m_device_id, &addr,
            notification_callback, this);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("audio object add property listener:"+std::to_string(stat));
        return false;
    }

    addr.mScope=PROPERTY_FORMATS;

    stat = AudioObjectAddPropertyListener(m_device_id, &addr,
            notification_callback, this);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("audio object add PROPERTY_FORMATS property listener:"+std::to_string(stat));
        return false;
    }

    if (m_default_device) {
        AudioObjectPropertyAddress addr = {
            PROPERTY_DEFAULT_DEVICE,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        stat = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                &addr, notification_callback, this);
        if(!checkStatus(stat))
        {
            m_obsever->onCaptureAudioLog("audio object add kAudioObjectSystemObject property listener:"+std::to_string(stat));
            return false;
        }
    }

    stat = set_property(m_audio_unit, kAudioOutputUnitProperty_SetInputCallback,
            SCOPE_GLOBAL, 0, &callback_info, sizeof(callback_info));
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("audio object add SetInputCallback property listener:"+std::to_string(stat));
        return false;
    }

    return true;
}

void MacExtenalAudio::resetCoreAudioUnitCallback()
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
    OSStatus stat;
    stat = AudioObjectRemovePropertyListener(m_device_id, &addr, notification_callback, this);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit stop error :"+std::to_string(stat));
    }
    addr.mSelector = PROPERTY_FORMATS;
    AudioObjectRemovePropertyListener(m_device_id, &addr,
            notification_callback, this);

    if (m_default_device) {
        addr.mSelector = PROPERTY_DEFAULT_DEVICE;
        AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
                &addr, notification_callback, this);

    }
    set_property(m_audio_unit, kAudioOutputUnitProperty_SetInputCallback,
                SCOPE_GLOBAL, 0, &callback_info, sizeof(AURenderCallbackStruct));

}

bool MacExtenalAudio::enableIOProperty(bool input, bool enable)
{
    UInt32 enable_int = enable;
    return set_property(m_audio_unit, kAudioOutputUnitProperty_EnableIO,
            (input) ? SCOPE_INPUT : SCOPE_OUTPUT,
            (input) ? BUS_INPUT   : BUS_OUTPUT,
                        &enable_int, sizeof(enable_int));
}

bool MacExtenalAudio::formatIsValid(uint32_t format_flags, uint32_t bits)
{

    return true;
}

void MacExtenalAudio::enumDevices(std::function<bool(void *,CFStringRef, CFStringRef, AudioDeviceID)> func)
{
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    UInt32        size = 0;
    UInt32        count;
    OSStatus      stat;
    AudioDeviceID *ids;

    stat = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr,
            0, NULL, &size);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("get kAudioObjectSystemObject data size error2 :"+std::to_string(stat));
        return;
    }

    ids   = (AudioDeviceID*)malloc(size);
    count = size / sizeof(AudioDeviceID);

    stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
            0, NULL, &size, ids);
    if(checkStatus(stat))
    {
        for (UInt32 i = 0; i < count; i++)
            if (!coreaudioEnumDevice(func, ids[i]))
                break;
    }
    free(ids);
}

bool MacExtenalAudio::coreaudioEnumDevice(std::function<bool (void *,CFStringRef, CFStringRef, AudioDeviceID)> proc, AudioDeviceID id)
{
    UInt32      size      = 0;
    CFStringRef cf_name   = NULL;
    CFStringRef cf_uid    = NULL;
    bool        enum_next = true;
    OSStatus    stat;
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyStreams,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    /* check to see if it's a mac input device */
    AudioObjectGetPropertyDataSize(id, &addr, 0, NULL, &size);
    if (!size)
        return true;

    size = sizeof(CFStringRef);

    addr.mSelector = kAudioDevicePropertyDeviceUID;
    stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_uid);

    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("get audio device UID error :"+std::to_string(stat));
        return true;
    }
    addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
    stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_name);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("get audio device uname error :"+std::to_string(stat));
        return true;
    }
    enum_next = proc(this,cf_name, cf_uid, id);
    return enum_next;
}

bool MacExtenalAudio::initResampleContext()
{
    m_swr_ctx = swr_alloc();
    if(!m_swr_ctx)
    {
        OBSERVER_LOG("resample audio context is not create.");
        return false;
    }

//    AV_SAMPLE_FMT_DBL
    // set options
    av_opt_set_int(m_swr_ctx, "in_channel_layout",    3, 0);
    av_opt_set_int(m_swr_ctx, "in_sample_rate",       m_sample_rate, 0);
    av_opt_set_sample_fmt(m_swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

    av_opt_set_int(m_swr_ctx, "out_channel_layout",    3, 0);
    av_opt_set_int(m_swr_ctx, "out_sample_rate",       m_want_sample_rate, 0);
    av_opt_set_sample_fmt(m_swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);


    // initialize the resampling context
    if (swr_init(m_swr_ctx) < 0) {
        OBSERVER_LOG("resample context init error..");
        return false;
    }
    return true;
}

bool MacExtenalAudio::findDeviceIDByUid()
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

    if (m_device_name.empty())
        m_device_name = std::string("classIn");

    /* have to do this because mac output devices don't actually exist */
    if (m_device_name=="classIn") {
        {
            if (!getDefaultOutAudioDevices()) {

                return false;
            }
        }
    }

    m_device_uid=m_soundfloweruids.front();

    cf_uid = CFStringCreateWithCString(NULL, m_device_uid.c_str(),
            kCFStringEncodingUTF8);

    if (m_default_device) {
        addr.mSelector = PROPERTY_DEFAULT_DEVICE;
        stat = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                &addr, qual_size, &qual, &size, &m_device_id);
        success = (stat == noErr);
    } else {
        success = coreAudioGetDeviceUID();
    }

    if (cf_uid)
        CFRelease(cf_uid);

    return success;
    return true;
}

bool MacExtenalAudio::coreAudioGetDeviceName()
{
    CFStringRef cf_name = NULL;
    UInt32 size = sizeof(CFStringRef);

    const AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyDeviceNameCFString,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    };

    OSStatus stat = AudioObjectGetPropertyData(m_device_id, &addr,
            0, NULL, &size, &cf_name);

    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("get audio device property data error1 :"+std::to_string(stat));
        return false;
    }

    NSString *foo = (NSString *)cf_name;
    std::string name = std::string([foo UTF8String]);

    m_device_name = name;

    if (cf_name)
        CFRelease(cf_name);

    return true;
}

bool MacExtenalAudio::coreAudioGetDeviceUID()
{
    enumDevices(getDeviceId);
    return true;
}

bool MacExtenalAudio::getDefaultOutAudioDevices()
{
    enumDevices(coreaudioEnumAddDevices);

    return true;
}


bool isSoundflower(CFStringRef name)
{
    NSString *deviceName = (NSString*)name;
    return [deviceName hasPrefix:@"Soundflower"];
}

bool MacExtenalAudio::coreaudioEnumAddDevices(void *pthis,CFStringRef cf_name, CFStringRef cf_uid, AudioDeviceID id)
{
    MacExtenalAudio* data =  (MacExtenalAudio*)pthis;

    if(isSoundflower(cf_name))
    {
        NSString *foo = (NSString *)cf_uid;
        std::string uid = std::string([foo UTF8String]);
        data->m_soundfloweruids.push_back(uid);
    }
    return true;
}

bool MacExtenalAudio::getDeviceId(void *pthis,CFStringRef cf_name, CFStringRef cf_uid, AudioDeviceID id)
{
    MacExtenalAudio *that = (MacExtenalAudio*)pthis;
    QString strUID = QString::fromNSString((NSString*)cf_uid);
    if (strUID.toStdString()== that->m_device_uid) {
        that->m_device_id = id;
        that->m_device_name=QString::fromNSString((NSString*)cf_name).toStdString();
        return false;
    }

    return true;
}



void MacExtenalAudio::coreAudioUninit()
{
    coreAudioStop();
    OSStatus stat;
    stat = AudioUnitUninitialize(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put uninit unit error :"+std::to_string(stat));
    }
    resetCoreAudioUnitCallback();
    stat = AudioComponentInstanceDispose(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio component instance dispose :"+std::to_string(stat));
    }
    m_audio_unit = NULL;
    free(m_tempBufferList);
    m_tempBufferList=nullptr;
}



MacExtenalAudio::MacExtenalAudio(ICCExtenedAudioObserver *observer)
    :m_obsever(observer)
{
}

MacExtenalAudio::~MacExtenalAudio()
{
    coreAudioShutdown();
}

bool MacExtenalAudio::initAudioCapture()
{

    OSStatus stat;
    if(!findDeviceIDByUid())
    {
        return false;
    }
    if(!coreAudioGetDeviceName())
    {
        return false;
    }
    if(!coreAudioInit())
    {
        return false;
    }
    stat = enableIOProperty(true,true);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("enable input io error :"+std::to_string(stat));
        return false;
    }
    stat = enableIOProperty(false,false);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("enable output io error :"+std::to_string(stat));
        return false;
    }

    stat = set_property(m_audio_unit, kAudioOutputUnitProperty_CurrentDevice,
            SCOPE_GLOBAL, 0, &m_device_id, sizeof(m_device_id));
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("set output device property CurrentDevice error :"+std::to_string(stat));
        return false;
    }

    if(!initAudioUnitFormat())
    {
        return false;
    }
    if(!initAudioUnitBuffer())
    {
        return false;
    }
    if(!initCoreAudioUnitCallback())
    {
        return false;
    }

    stat = AudioUnitInitialize(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("audio unit init error :"+std::to_string(stat));
        return false;
    }


    m_init = true;
    return true;
}

int MacExtenalAudio::channel()
{
//    return m_ccADM->playoutChannels();
    return this->m_channels;
}

int MacExtenalAudio::sampleRate()
{
    return this->m_sample_rate;
}

void MacExtenalAudio::startAudioCapture()
{
    if(!coreAudioStart())
    {
        m_obsever->onCaptureAudioLog("audio unit start error...");
    }
}

void MacExtenalAudio::endAudioCapture()
{
    if(!coreAudioStop())
    {
        m_obsever->onCaptureAudioLog("audio capture stop error");
    }
}

void MacExtenalAudio::setAudioDataCallback(std::function<void (uint8_t *, int32_t, int32_t, int32_t)> callback)
{
    m_data_callback=callback;
}

}
