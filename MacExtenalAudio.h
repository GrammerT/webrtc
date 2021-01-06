#ifndef MACEXTENALAUDIO_H
#define MACEXTENALAUDIO_H
#include "ICCExtenedAudio.h"
#include "audio_device_mac.h"

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

private:
    ICCExtenedAudioObserver *m_obsever;
    std::shared_ptr<CC::AudioDeviceMac> m_ccADM;
};

}//! namespace CC

#endif // MACEXTENALAUDIO_H
