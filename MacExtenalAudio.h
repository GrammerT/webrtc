#ifndef MACEXTENALAUDIO_H
#define MACEXTENALAUDIO_H
#include "ICCExtenedAudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>
#include <vector>


struct SwrContext;

namespace CC {

class MacExtenalAudio:public ICCExtenedAudio
{
public:
    MacExtenalAudio(ICCExtenedAudioObserver *observer);
    ~MacExtenalAudio();

    virtual bool initAudioCapture() override;
    virtual int channel() override;
    virtual int sampleRate() override;
    virtual void startAudioCapture() override;
    virtual void endAudioCapture() override;
    virtual void setAudioDataCallback(std::function<void(uint8_t *data,int32_t sample_num,int32_t channels,int32_t byte_per_sample)> callback) override;



private:
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
    bool coreAudioStop();


    bool initAudioUnitFormat();

    bool initAudioUnitBuffer();

    bool initCoreAudioUnitCallback();
    void resetCoreAudioUnitCallback();

    bool enableIOProperty(bool input,bool enable);
    bool formatIsValid(uint32_t format_flags,uint32_t bits);

    void enumDevices(std::function<bool(void *,CFStringRef cf_name,
                     CFStringRef cf_uid, AudioDeviceID id)> func);

    bool coreaudioEnumDevice(std::function<bool(void *,CFStringRef cf_name,CFStringRef cf_uid, AudioDeviceID id)> proc,
                             AudioDeviceID id);

    bool initResampleContext();


    bool findDeviceIDByUid();
    bool coreAudioGetDeviceName();
    bool coreAudioGetDeviceUID();
    bool getDefaultOutAudioDevices();

    static bool coreaudioEnumAddDevices(void *pthis,CFStringRef cf_name,
                                 CFStringRef cf_uid, AudioDeviceID id);
    static bool getDeviceId(void *pthis,CFStringRef cf_name, CFStringRef cf_uid,
                                   AudioDeviceID id);

private:
    ICCExtenedAudioObserver *m_obsever=nullptr;
    std::vector<std::string> m_soundfloweruids;

    std::string m_device_name;
    std::string m_device_uid;

    AudioBufferList *m_tempBufferList;
    AudioUnit m_audio_unit=nullptr;
    AudioDeviceID m_device_id;

    uint32_t m_byte_per_frame;
    uint32_t m_sample_rate;
    uint32_t m_channels;


    int m_dst_nb_samples;
    uint32_t m_want_byte_per_frame=2;
    uint32_t m_want_sample_rate=48000;
    uint32_t m_want_channels = 2;

    std::function<void (uint8_t *, int32_t, int32_t, int32_t)> m_data_callback=nullptr;

    struct SwrContext* m_swr_ctx ;

    bool m_default_device=false;
    bool m_active=false;
    bool m_init;
};

}//! namespace CC

#endif // MACEXTENALAUDIO_H
