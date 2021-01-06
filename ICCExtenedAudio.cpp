#include "ICCExtenedAudio.h"
#ifdef WEBRTC_WIN
#include "OWTExtenedAudio.h"
#else
#include "MacExtenalAudio.h"
#endif

std::shared_ptr<CC::ICCExtenedAudio> CC::ICCExtenedAudio::create(ICCExtenedAudioObserver *observer)
{
#ifdef WEBRTC_WIN
    auto audio = std::make_shared<CC::OWTExtenedAudio>(observer);
    return audio;
#else
    auto audio = std::make_shared<CC::MacExtenalAudio>(observer);
    return audio;
#endif
    return nullptr;
}
