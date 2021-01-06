#ifndef AUDIO_DEVICE_AUDIO_DEVICE_MAC_H_
#define AUDIO_DEVICE_AUDIO_DEVICE_MAC_H_

#include <memory>
#include "audio_mixer_manager_mac.h"

#include <AudioToolbox/AudioConverter.h>
#include <CoreAudio/CoreAudio.h>
#include <mach/semaphore.h>
#include <thread>

struct PaUtilRingBuffer;

//namespace rtc {
//class PlatformThread;
//}  // namespace rtc

namespace CC {

class ICCExtenedAudioObserver;

const uint32_t N_REC_SAMPLES_PER_SEC = 48000;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 48000;

const uint32_t N_REC_CHANNELS = 1;   // default is mono recording
const uint32_t N_PLAY_CHANNELS = 2;  // default is stereo playout
const uint32_t N_DEVICE_CHANNELS = 64;

const int kBufferSizeMs = 10;

const uint32_t ENGINE_REC_BUF_SIZE_IN_SAMPLES =
    N_REC_SAMPLES_PER_SEC * kBufferSizeMs / 1000;
const uint32_t ENGINE_PLAY_BUF_SIZE_IN_SAMPLES =
    N_PLAY_SAMPLES_PER_SEC * kBufferSizeMs / 1000;

const int N_BLOCKS_IO = 2;
const int N_BUFFERS_IN = 2;   // Must be at least N_BLOCKS_IO.
const int N_BUFFERS_OUT = 3;  // Must be at least N_BLOCKS_IO.

const uint32_t TIMER_PERIOD_MS = 2 * 10 * N_BLOCKS_IO * 1000000;

const uint32_t REC_BUF_SIZE_IN_SAMPLES =
    ENGINE_REC_BUF_SIZE_IN_SAMPLES * N_DEVICE_CHANNELS * N_BUFFERS_IN;
const uint32_t PLAY_BUF_SIZE_IN_SAMPLES =
    ENGINE_PLAY_BUF_SIZE_IN_SAMPLES * N_PLAY_CHANNELS * N_BUFFERS_OUT;

const int kGetMicVolumeIntervalMs = 1000;


static const int kAdmMaxDeviceNameSize = 128;
static const int kAdmMaxFileNameSize = 512;
static const int kAdmMaxGuidSize = 128;

static const int kAdmMinPlayoutBufferSizeMs = 10;
static const int kAdmMaxPlayoutBufferSizeMs = 250;


class AudioDeviceMac {
 public:
  AudioDeviceMac();
  ~AudioDeviceMac();


    int32_t playoutChannels();
    int32_t playoutSampleRate();
    void setObserver(ICCExtenedAudioObserver *observer);
    void setDataCallback(std::function<void (uint8_t *, int32_t, int32_t, int32_t)> callback);

  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(
      int audioLayer) const;

  // Main initializaton and termination
  bool Init();
  int32_t Terminate();
  bool Initialized() const;



  // Device enumeration
  int16_t PlayoutDevices();
  int16_t RecordingDevices();
  int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]);
  int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]);

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index);
//  int32_t SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
  int32_t SetRecordingDevice(uint16_t index);
//  int32_t SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device);

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available);
  int32_t InitPlayout();
  bool PlayoutIsInitialized() const;
  int32_t RecordingIsAvailable(bool& available);
  int32_t InitRecording();
  bool RecordingIsInitialized() const;

  // Audio transport control
  int32_t StartPlayout();
  int32_t StopPlayout();
  bool Playing() const;
  int32_t StartRecording();
  int32_t StopRecording();
  bool Recording() const;

  // Audio mixer initialization
  int32_t InitSpeaker();
  bool SpeakerIsInitialized() const;
  int32_t InitMicrophone();
  bool MicrophoneIsInitialized() const;

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available);
  int32_t SetSpeakerVolume(uint32_t volume);
  int32_t SpeakerVolume(uint32_t& volume) const;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const;

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool& available);
  int32_t SetMicrophoneVolume(uint32_t volume);
  int32_t MicrophoneVolume(uint32_t& volume) const;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const;

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool& available);
  int32_t SetMicrophoneMute(bool enable);
  int32_t MicrophoneMute(bool& enabled) const;

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available);
  int32_t SetSpeakerMute(bool enable);
  int32_t SpeakerMute(bool& enabled) const;

  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available);
  int32_t SetStereoPlayout(bool enable);
  int32_t StereoPlayout(bool& enabled) const;
  int32_t StereoRecordingIsAvailable(bool& available);
  int32_t SetStereoRecording(bool enable);
  int32_t StereoRecording(bool& enabled) const;

  // Delay information and control
  int32_t PlayoutDelay(uint16_t& delayMS) const;

//  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 private:
  int32_t MicrophoneIsAvailable(bool& available);
  int32_t SpeakerIsAvailable(bool& available);

  static void AtomicSet32(int32_t* theValue, int32_t newValue);
  static int32_t AtomicGet32(int32_t* theValue);

//  static void logCAMsg(const rtc::LoggingSeverity sev,
//                       const char* msg,
//                       const char* err);

  int32_t GetNumberDevices(const AudioObjectPropertyScope scope,
                           AudioDeviceID scopedDeviceIds[],
                           const uint32_t deviceListLength);

  int32_t GetDeviceName(const AudioObjectPropertyScope scope,
                        const uint16_t index,
                        char* name,
                        char* guid);

  int32_t InitDevice(uint16_t userDeviceIndex,
                     AudioDeviceID& deviceId,
                     bool isInput);

  // Always work with our preferred playout format inside VoE.
  // Then convert the output to the OS setting using an AudioConverter.
  OSStatus SetDesiredPlayoutFormat();

  static OSStatus objectListenerProc(
      AudioObjectID objectId,
      UInt32 numberAddresses,
      const AudioObjectPropertyAddress addresses[],
      void* clientData);

  OSStatus implObjectListenerProc(AudioObjectID objectId,
                                  UInt32 numberAddresses,
                                  const AudioObjectPropertyAddress addresses[]);

  int32_t HandleDeviceChange();

  int32_t HandleStreamFormatChange(AudioObjectID objectId,
                                   AudioObjectPropertyAddress propertyAddress);

  int32_t HandleDataSourceChange(AudioObjectID objectId,
                                 AudioObjectPropertyAddress propertyAddress);

  int32_t HandleProcessorOverload(AudioObjectPropertyAddress propertyAddress);

  static OSStatus deviceIOProc(AudioDeviceID device,
                               const AudioTimeStamp* now,
                               const AudioBufferList* inputData,
                               const AudioTimeStamp* inputTime,
                               AudioBufferList* outputData,
                               const AudioTimeStamp* outputTime,
                               void* clientData);

  static OSStatus outConverterProc(
      AudioConverterRef audioConverter,
      UInt32* numberDataPackets,
      AudioBufferList* data,
      AudioStreamPacketDescription** dataPacketDescription,
      void* userData);

  static OSStatus inDeviceIOProc(AudioDeviceID device,
                                 const AudioTimeStamp* now,
                                 const AudioBufferList* inputData,
                                 const AudioTimeStamp* inputTime,
                                 AudioBufferList* outputData,
                                 const AudioTimeStamp* outputTime,
                                 void* clientData);

  static OSStatus inConverterProc(
      AudioConverterRef audioConverter,
      UInt32* numberDataPackets,
      AudioBufferList* data,
      AudioStreamPacketDescription** dataPacketDescription,
      void* inUserData);

  OSStatus implDeviceIOProc(const AudioBufferList* inputData,
                            const AudioTimeStamp* inputTime,
                            AudioBufferList* outputData,
                            const AudioTimeStamp* outputTime);

  OSStatus implOutConverterProc(UInt32* numberDataPackets,
                                AudioBufferList* data);

  OSStatus implInDeviceIOProc(const AudioBufferList* inputData,
                              const AudioTimeStamp* inputTime);

  OSStatus implInConverterProc(UInt32* numberDataPackets,
                               AudioBufferList* data);

  static void RunCapture(void*);
  static void RunRender(void*);
  bool CaptureWorkerThread();
  bool RenderWorkerThread();

//  bool KeyPressed();

  void startCaptureAudioThread();
  void stopCaptureAudioThread();

  void startRenderAudioThread();
  void stopRenderAudioThread();
//  AudioDeviceBuffer* _ptrAudioBuffer;

//  rtc::CriticalSection _critSect;

//  rtc::Event _stopEventRec;
//  rtc::Event _stopEvent;

  // TODO(pbos): Replace with direct members, just start/stop, no need to
  // recreate the thread.
  // Only valid/running between calls to StartRecording and StopRecording.
//  std::unique_ptr<rtc::PlatformThread> capture_worker_thread_;
    std::thread capture_worker_thread_;
    bool m_stop_capture_thread;
  // Only valid/running between calls to StartPlayout and StopPlayout.
//  std::unique_ptr<rtc::PlatformThread> render_worker_thread_;
    std::thread render_worker_thread_;
    bool m_stop_render_thread;

  AudioMixerManagerMac _mixerManager;

  uint16_t _inputDeviceIndex;
  uint16_t _outputDeviceIndex;
  AudioDeviceID _inputDeviceID;
  AudioDeviceID _outputDeviceID;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
  AudioDeviceIOProcID _inDeviceIOProcID;
  AudioDeviceIOProcID _deviceIOProcID;
#endif
  bool _inputDeviceIsSpecified;
  bool _outputDeviceIsSpecified;

  uint8_t _recChannels;
  uint8_t _playChannels;

  Float32* _captureBufData;
  SInt16* _renderBufData;

  SInt16 _renderConvertData[PLAY_BUF_SIZE_IN_SAMPLES];

  bool _initialized;
  bool _isShutDown;
  bool _recording;
  bool _playing;
  bool _recIsInitialized;
  bool _playIsInitialized;

  // Atomically set varaibles
  int32_t _renderDeviceIsAlive;
  int32_t _captureDeviceIsAlive;

  bool _twoDevices;
  bool _doStop;  // For play if not shared device or play+rec if shared device
  bool _doStopRec;  // For rec if not shared device
  bool _macBookPro;
  bool _macBookProPanRight;

  AudioConverterRef _captureConverter;
  AudioConverterRef _renderConverter;

  AudioStreamBasicDescription _outStreamFormat;
  AudioStreamBasicDescription _outDesiredFormat;
  AudioStreamBasicDescription _inStreamFormat;
  AudioStreamBasicDescription _inDesiredFormat;

  uint32_t _captureLatencyUs;
  uint32_t _renderLatencyUs;

  // Atomically set variables
  mutable int32_t _captureDelayUs;
  mutable int32_t _renderDelayUs;

  int32_t _renderDelayOffsetSamples;

  PaUtilRingBuffer* _paCaptureBuffer;
  PaUtilRingBuffer* _paRenderBuffer;

  semaphore_t _renderSemaphore;
  semaphore_t _captureSemaphore;

  int _captureBufSizeSamples;
  int _renderBufSizeSamples;

  // Typing detection
  // 0x5c is key "9", after that comes function keys.
  bool prev_key_state_[0x5d];


  std::function<void (uint8_t *, int32_t, int32_t, int32_t)> m_audio_data_callback=nullptr;
  ICCExtenedAudioObserver *m_observer;

};

}  // namespace CC

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_MAC_AUDIO_DEVICE_MAC_H_
