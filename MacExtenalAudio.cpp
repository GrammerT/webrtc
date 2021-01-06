#include "MacExtenalAudio.h"

namespace CC {



MacExtenalAudio::MacExtenalAudio(ICCExtenedAudioObserver *observer)
    :m_obsever(observer)
{
    m_ccADM = std::make_shared<CC::AudioDeviceMac>();
}

bool MacExtenalAudio::initAudioCapture()
{
    m_ccADM->Init();
    m_ccADM->SetPlayoutDevice(0);
    m_ccADM->InitPlayout();

//    m_ccADM->Init();
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
    m_ccADM->StartPlayout();
}

void MacExtenalAudio::endAudioCapture()
{
    m_ccADM->StopPlayout();
}

void MacExtenalAudio::setAudioDataCallback(std::function<void (uint8_t *, int32_t, int32_t, int32_t)> callback)
{

}

}
