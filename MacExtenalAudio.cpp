#include "MacExtenalAudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardware.h>
namespace CC {
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

}

bool MacExtenalAudio::coreAuduioInit()
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

    m_init = true;
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
    if (desc.mChannelsPerFrame > 8) {
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

bool MacExtenalAudio::haveValidOutputDevice()
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
        m_obsever->onCaptureAudioLog("get kAudioObjectSystemObject data size error :"+std::to_string(stat));
        return false;
    }
    ids   = (AudioDeviceID*)malloc(size);
    count = size / sizeof(AudioDeviceID);

    stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
            0, NULL, &size, ids);
    if(!checkStatus(stat))
    {
        m_obsever->onCaptureAudioLog("get kAudioObjectSystemObject data size error :"+std::to_string(stat));
        return false;
    }

    for (UInt32 i = 0; i < count; i++)
    {
        if (!coreaudio_enum_device(proc, param, ids[i]))
            break;
    }

}

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
    if(findDeviceIDByUid())
    {
        return false;
    }
    if(coreAudioGetDeviceName())
    {
        return false;
    }
    if(coreAuduioInit())
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
//    m_ccADM->StartPlayout();
    OSStatus status = AudioOutputUnitStart(m_audioUnit);
    if(!checkStatus(status))
    {
//        return false;
    }

}

void MacExtenalAudio::endAudioCapture()
{
//    m_ccADM->StopPlayout();
    OSStatus status = AudioOutputUnitStop(m_audioUnit);
    if(!checkStatus(status))
    {
//        return false;
    }
}

void MacExtenalAudio::setAudioDataCallback(std::function<void (uint8_t *, int32_t, int32_t, int32_t)> callback)
{

}

}
