#ifndef MACEXTENALAUDIO_H
#define MACEXTENALAUDIO_H
#include "ICCExtenedAudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>
#include <vector>


struct SwrContext;

namespace CC {

enum audio_format {
    AUDIO_FORMAT_UNKNOWN,

    AUDIO_FORMAT_U8BIT,
    AUDIO_FORMAT_16BIT,
    AUDIO_FORMAT_32BIT,
    AUDIO_FORMAT_FLOAT,

    AUDIO_FORMAT_U8BIT_PLANAR,
    AUDIO_FORMAT_16BIT_PLANAR,
    AUDIO_FORMAT_32BIT_PLANAR,
    AUDIO_FORMAT_FLOAT_PLANAR,
};

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
