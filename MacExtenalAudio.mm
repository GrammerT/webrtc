#include "MacExtenalAudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardware.h>
#include <QString>


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



#define kOutputBus 0
#define kInputBus 1


#define MAX_AUDIO_MIXES     4
#define MAX_AUDIO_CHANNELS  2
#define AUDIO_OUTPUT_FRAMES 1024

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

#if 1
#include <stdio.h>
#ifdef WIN32
static FILE *g_pFile = nullptr;
#else
static FILE *g_pFile = fopen("source.pcm", "wb");
#endif
#endif
OSStatus MacExtenalAudio::playbackCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
    MacExtenalAudio *pThis=(MacExtenalAudio*)inRefCon;
    pThis->m_obsever->onCaptureAudioLog("playcallback...");
    for (int i=0; i < ioData->mNumberBuffers; i++) { // in practice we will only ever have 1 buffer, since audio format is mono
        AudioBuffer buffer = ioData->mBuffers[i];
        pThis->m_obsever->onCaptureAudioLog("buffer have data.channels:"+std::to_string(buffer.mNumberChannels)+",samples size:"+std::to_string(buffer.mDataByteSize));

#if 1

#if 0
                    static FILE *g_pFile;
                    if (!g_pFile)
                    {
                        fopen_s(&g_pFile, "e:/source.pcm", "wb");
                    }
#endif
                    if (g_pFile)
                    {
                        fwrite((const int8_t*)buffer.mData,buffer.mDataByteSize, 1, g_pFile);
                        fflush(g_pFile);
                    }
#endif

    }

    return noErr;
}

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

//    if (!ca_success(stat, ca, "input_callback", "audio retrieval")){
//        unsigned long cur_ms = clock();
//        if (!ca->prev_update_ts || cur_ms - ca->prev_update_ts > 5000000) {
//            blog(LOG_WARNING, "Prepare reset the coreaudio module.");
//            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//                blog(LOG_WARNING, "Reset the coreaudio module.");
//                obs_data_t * settings = obs_source_get_settings(ca->source);
//                coreaudio_update(ca, settings);
//                obs_data_release(settings);
//            });
//            ca->prev_update_ts = cur_ms;
//        }
//        return noErr;
//    }

//    for (UInt32 i = 0; i < pThis->m_tempBufferList->mNumberBuffers; i++)
//        audio.data[i] = pThis->m_tempBufferList->mBuffers[i].mData;

//    audio.frames          = frames;
//    audio.speakers        = ca->speakers;
//    audio.format          = ca->format;
//    audio.samples_per_sec = ca->sample_rate;
//    audio.timestamp       = ts_data->mHostTime;

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

void MacExtenalAudio::coreAudioStop()
{
    OSStatus stat;
    stat = AudioOutputUnitStop(m_audio_unit);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("auduio out put unit stop error :"+std::to_string(stat));
    }
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
//    if (desc.mChannelsPerFrame > 8)
    {

            desc.mSampleRate=48000;

            desc.mChannelsPerFrame = 2;
            desc.mBytesPerFrame = 2 * desc.mBitsPerChannel / 8;
            desc.mBytesPerPacket =
                    desc.mFramesPerPacket * desc.mBytesPerFrame;
    }

    stat = set_property(m_audio_unit, kAudioUnitProperty_StreamFormat,
            SCOPE_OUTPUT, BUS_INPUT, &desc, size);
    if(!checkStatus(stat))
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

    m_sample_rate=desc.mSampleRate;
    m_channels=2;


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
//    bool planar = (format_flags & kAudioFormatFlagIsNonInterleaved) != 0;

//    if (format_flags & kAudioFormatFlagIsFloat)
//        return planar ? AUDIO_FORMAT_FLOAT_PLANAR : AUDIO_FORMAT_FLOAT;

//    if (!(format_flags & kAudioFormatFlagIsSignedInteger) && bits == 8)
//        return planar ? AUDIO_FORMAT_U8BIT_PLANAR : AUDIO_FORMAT_U8BIT;

//    /* not float?  not signed int?  no clue, fail */
//    if ((format_flags & kAudioFormatFlagIsSignedInteger) == 0)
//        return AUDIO_FORMAT_UNKNOWN;

//    if (bits == 16)
//        return planar ? AUDIO_FORMAT_16BIT_PLANAR : AUDIO_FORMAT_16BIT;
//    else if (bits == 32)
//        return planar ? AUDIO_FORMAT_32BIT_PLANAR : AUDIO_FORMAT_32BIT;

//    return AUDIO_FORMAT_UNKNOWN;
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
//                ca->no_devices = true;
                return false;
            }
        }
    }

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
//    char name[1024];

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
//    QString name = QString::fromNSString((const NSString*)cf_name);


//    if (!cf_to_cstr(cf_name, name, 1024)) {
//        blog(LOG_WARNING, "[coreaudio_get_device_name] failed to "
//                          "convert name to cstr for some reason");
//        return false;
//    }

//    bfree(ca->device_name);
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

bool MacExtenalAudio::coreaudioEnumAddDevices(void *pthis,CFStringRef cf_name, CFStringRef cf_uid, AudioDeviceID id)
{
    MacExtenalAudio* data =  (MacExtenalAudio*)pthis;
//	struct device_item item;

//	memset(&item, 0, sizeof(item));

//	if (!cf_to_dstr(cf_name, &item.name))
//		goto fail;
//	if (!cf_to_dstr(cf_uid,  &item.value))
//		goto fail;

//    bool isOutput = (astrstri(item.value.array, "output") == NULL);
//    if ((data->input || !device_is_input(item.value.array)) && isOutput)
//        device_list_add(data->list, &item);

//    if (!data->input && !isOutput)
//    {
//        device_list_add(data->list, &item);
//    }

//fail:
//    device_item_free(&item);

//    UNUSED_PARAMETER(id);
    return true;
}

bool MacExtenalAudio::getDeviceId(void *pthis,CFStringRef cf_name, CFStringRef cf_uid, AudioDeviceID id)
{
    MacExtenalAudio *that = (MacExtenalAudio*)pthis;
    QString strUID = QString::fromNSString((NSString*)cf_name);
    if (strUID.toStdString()== that->m_device_name) {
        that->m_device_id = id;
        that->m_device_uid=QString::fromNSString((NSString*)cf_uid).toStdString();
        return false;
    }

    return true;
}

//bool MacExtenalAudio::haveValidOutputDevice()
//{
//    AudioObjectPropertyAddress addr = {
//        kAudioHardwarePropertyDevices,
//        kAudioObjectPropertyScopeGlobal,
//        kAudioObjectPropertyElementMaster
//    };

//    UInt32        size = 0;
//    UInt32        count;
//    OSStatus      stat;
//    AudioDeviceID *ids;

//    stat = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr,
//            0, NULL, &size);
//    if(!checkStatus(stat))
//    {
//        m_obsever->onCaptureAudioLog("get kAudioObjectSystemObject data size error :"+std::to_string(stat));
//        return false;
//    }
//    ids   = (AudioDeviceID*)malloc(size);
//    count = size / sizeof(AudioDeviceID);

//    stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
//            0, NULL, &size, ids);
//    if(!checkStatus(stat))
//    {
//        m_obsever->onCaptureAudioLog("get kAudioObjectSystemObject data size error :"+std::to_string(stat));
//        return false;
//    }

//    for (UInt32 i = 0; i < count; i++)
//    {
//        if (!enumAudioOutputDevice(ids[i]))
//            break;
//    }

//}

//bool MacExtenalAudio::enumAudioOutputDevice(AudioDeviceID id)
//{
//    UInt32      size      = 0;
//    CFStringRef cf_name   = NULL;
//    CFStringRef cf_uid    = NULL;
//    bool        enum_next = true;
//    OSStatus    stat;
//    AudioObjectPropertyAddress addr = {
//        kAudioDevicePropertyStreams,
//        kAudioObjectPropertyScopeGlobal,
//        kAudioObjectPropertyElementMaster
//    };

//    /* check to see if it's a mac input device */
//    AudioObjectGetPropertyDataSize(id, &addr, 0, NULL, &size);
//    if (!size)
//        return true;

//    size = sizeof(CFStringRef);

//    addr.mSelector = kAudioDevicePropertyDeviceUID;
//    stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_uid);
//    if(!checkStatus(stat))
//    {
//        m_obsever->onCaptureAudioLog("get audio uid error2 :"+std::to_string(stat));
//        return true;
//    }
//    addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
//    stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_name);
//    if(!checkStatus(stat))
//    {
//        m_obsever->onCaptureAudioLog("get audio cname error2 :"+std::to_string(stat));
//        return enum_next;
//    }
////    if (!enum_success(stat, "get audio device name"))
////        goto fail;

//    enum_next = proc(param, cf_name, cf_uid, id);

//    return enum_next;
//}

//void MacExtenalAudio::audioObjectRemovePerprotyListener(AudioObjectPropertyAddress *addr, AURenderCallbackStruct clallback)
//{

//}

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
//    free(m_tempBuffer);
}



MacExtenalAudio::MacExtenalAudio(ICCExtenedAudioObserver *observer)
    :m_obsever(observer)
{
//    m_ccADM = std::make_shared<CC::AudioDeviceMac>();
//    m_ccADM->setObserver(m_obsever);
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

    if(!coreAudioStart())
    {
        m_obsever->onCaptureAudioLog("audio unit start error...");
        return false;
    }
    m_init = true;
    return true;
}

int MacExtenalAudio::channel()
{
    return m_ccADM->playoutChannels();
}

int MacExtenalAudio::sampleRate()
{
    return m_ccADM->playoutSampleRate();
}

void MacExtenalAudio::startAudioCapture()
{
////    m_ccADM->StartPlayout();
//    OSStatus status = AudioOutputUnitStart(m_audioUnit);
//    if(!checkStatus(status))
//    {
////        return false;
//    }

}

void MacExtenalAudio::endAudioCapture()
{
//    m_ccADM->StopPlayout();
//    OSStatus status = AudioOutputUnitStop(m_audioUnit);
//    if(!checkStatus(status))
//    {
////        return false;
//    }
}

void MacExtenalAudio::setAudioDataCallback(std::function<void (uint8_t *, int32_t, int32_t, int32_t)> callback)
{

}

}
