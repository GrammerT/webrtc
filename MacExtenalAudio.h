#ifndef MACEXTENALAUDIO_H
#define MACEXTENALAUDIO_H
#include "ICCExtenedAudio.h"
#include "audio_device_mac.h"
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>


namespace CC {

class MacExtenalAudio:public ICCExtenedAudio
{
public:
    MacExtenalAudio(ICCExtenedAudioObserver *observer);

    virtual bool initAudioCapture() override;
    virtual int channel() override;
    virtual int sampleRate() override;
    virtual void startAudioCapture() override;
    virtual void endAudioCapture() override;
    virtual void setAudioDataCallback(std::function<void(uint8_t *data,int32_t sample_num,int32_t channels,int32_t byte_per_sample)> callback) override;

    static OSStatus playbackCallback(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList *ioData);




    //! from obs.

    static OSStatus notification_callback(AudioObjectID id, UInt32 num_addresses,
                                          const AudioObjectPropertyAddress addresses[], void *data);

    static OSStatus inputCallback(
            void *data,
            AudioUnitRenderActionFlags *action_flags,
            const AudioTimeStamp *ts_data,
            UInt32 bus_num,
            UInt32 frames,
            AudioBufferList *ignored_buffers);

    bool coreAudioInit();
    void coreAudioUninit();
    void coreAudioShutdown();
    bool coreAudioStart();
    void coreAudioStop();


    bool initAudioUnitFormat();

    bool initAudioUnitBuffer();

    bool initCoreAudioUnitCallback();
    void resetCoreAudioUnitCallback();

    bool enableIOProperty(bool input,bool enable);
    bool formatIsValid(uint32_t format_flags,uint32_t bits);


    void enumDevices(std::function<bool(void *,CFStringRef cf_name,
                     CFStringRef cf_uid, AudioDeviceID id)> func);

    bool coreaudioEnumDevice(std::function<bool(void *,CFStringRef cf_name,CFStringRef cf_uid, AudioDeviceID id)> proc, AudioDeviceID id);

    bool findDeviceIDByUid();

    bool coreAudioGetDeviceName();
    bool coreAudioGetDeviceUID();

    bool getDefaultOutAudioDevices();
    static bool coreaudioEnumAddDevices(void *pthis,CFStringRef cf_name,
                                 CFStringRef cf_uid, AudioDeviceID id);
    static bool getDeviceId(void *pthis,CFStringRef cf_name, CFStringRef cf_uid,
                                   AudioDeviceID id);


//    bool haveValidOutputDevice();
//    bool enumAudioOutputDevice(AudioDeviceID id);

private:
    ICCExtenedAudioObserver *m_obsever=nullptr;
    std::shared_ptr<CC::AudioDeviceMac> m_ccADM;

    std::string m_device_name;
    std::string m_device_uid;

    AudioUnit m_audio_unit=nullptr;
    AudioDeviceID m_device_id;

    uint32_t m_sample_rate;
    uint32_t m_channels;

    bool m_default_device;
    bool m_active=false;
    bool m_init;


    AudioBufferList *m_tempBufferList;
};

}//! namespace CC

#endif // MACEXTENALAUDIO_H
