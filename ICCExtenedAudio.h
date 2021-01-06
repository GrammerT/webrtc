#ifndef __ICCExtenedAudio_H_
#define __ICCExtenedAudio_H_
#include <memory>
#include <string>
#include <functional>


namespace CC
{

	class ICCExtenedAudioObserver
	{
	public:
		virtual void onCaptureAudioLog(std::string log) = 0;
	};

class ICCExtenedAudio
{
public:
    static std::shared_ptr<ICCExtenedAudio> create(ICCExtenedAudioObserver *observer);
	virtual bool initAudioCapture() = 0;
	virtual int channel() = 0;
	virtual int sampleRate() = 0;
	virtual void startAudioCapture() = 0;
	virtual void endAudioCapture() = 0;
	virtual void setAudioDataCallback(std::function<void(uint8_t *data,int32_t sample_num,int32_t channels,int32_t byte_per_sample)> callback)=0;
};

} // end namespace CC
#endif //!__CCExtenedAudio_H_