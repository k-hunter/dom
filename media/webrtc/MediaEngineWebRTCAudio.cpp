/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"
#include <stdio.h>
#include <algorithm>
#include "mozilla/Assertions.h"
#include "MediaTrackConstraints.h"
#include "mtransport/runnable_utils.h"

// scoped_ptr.h uses FF
#ifdef FF
#undef FF
#endif
#include "webrtc/modules/audio_device/opensl/single_rw_fifo.h"

#define CHANNELS 1
#define ENCODING "L16"
#define DEFAULT_PORT 5555

#define SAMPLE_RATE(freq) ((freq)*2*8) // bps, 16-bit samples
#define SAMPLE_LENGTH(freq) (((freq)*10)/1000)

// These are restrictions from the webrtc.org code
#define MAX_CHANNELS 2
#define MAX_SAMPLING_FREQ 48000 // Hz - multiple of 100

#define MAX_AEC_FIFO_DEPTH 200 // ms - multiple of 10
static_assert(!(MAX_AEC_FIFO_DEPTH % 10), "Invalid MAX_AEC_FIFO_DEPTH");

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOG_FRAMES(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

/**
 * Webrtc microphone source source.
 */
NS_IMPL_ISUPPORTS0(MediaEngineWebRTCMicrophoneSource)
NS_IMPL_ISUPPORTS0(MediaEngineWebRTCAudioCaptureSource)

// XXX temp until MSG supports registration
StaticRefPtr<AudioOutputObserver> gFarendObserver;

AudioOutputObserver::AudioOutputObserver()
  : mPlayoutFreq(0)
  , mPlayoutChannels(0)
  , mChunkSize(0)
  , mSaved(nullptr)
  , mSamplesSaved(0)
{
  // Buffers of 10ms chunks
  mPlayoutFifo = new webrtc::SingleRwFifo(MAX_AEC_FIFO_DEPTH/10);
}

AudioOutputObserver::~AudioOutputObserver()
{
  Clear();
  free(mSaved);
  mSaved = nullptr;
}

void
AudioOutputObserver::Clear()
{
  while (mPlayoutFifo->size() > 0) {
    free(mPlayoutFifo->Pop());
  }
  // we'd like to touch mSaved here, but we can't if we might still be getting callbacks
}

FarEndAudioChunk *
AudioOutputObserver::Pop()
{
  return (FarEndAudioChunk *) mPlayoutFifo->Pop();
}

uint32_t
AudioOutputObserver::Size()
{
  return mPlayoutFifo->size();
}

void
AudioOutputObserver::MixerCallback(AudioDataValue* aMixedBuffer,
                                   AudioSampleFormat aFormat,
                                   uint32_t aChannels,
                                   uint32_t aFrames,
                                   uint32_t aSampleRate)
{
  if (gFarendObserver) {
    gFarendObserver->InsertFarEnd(aMixedBuffer, aFrames, false,
                                  aSampleRate, aChannels, aFormat);
  }
}

// static
void
AudioOutputObserver::InsertFarEnd(const AudioDataValue *aBuffer, uint32_t aFrames, bool aOverran,
                                  int aFreq, int aChannels, AudioSampleFormat aFormat)
{
  if (mPlayoutChannels != 0) {
    if (mPlayoutChannels != static_cast<uint32_t>(aChannels)) {
      MOZ_CRASH();
    }
  } else {
    MOZ_ASSERT(aChannels <= MAX_CHANNELS);
    mPlayoutChannels = static_cast<uint32_t>(aChannels);
  }
  if (mPlayoutFreq != 0) {
    if (mPlayoutFreq != static_cast<uint32_t>(aFreq)) {
      MOZ_CRASH();
    }
  } else {
    MOZ_ASSERT(aFreq <= MAX_SAMPLING_FREQ);
    MOZ_ASSERT(!(aFreq % 100), "Sampling rate for far end data should be multiple of 100.");
    mPlayoutFreq = aFreq;
    mChunkSize = aFreq/100; // 10ms
  }

#ifdef LOG_FAREND_INSERTION
  static FILE *fp = fopen("insertfarend.pcm","wb");
#endif

  if (mSaved) {
    // flag overrun as soon as possible, and only once
    mSaved->mOverrun = aOverran;
    aOverran = false;
  }
  // Rechunk to 10ms.
  // The AnalyzeReverseStream() and WebRtcAec_BufferFarend() functions insist on 10ms
  // samples per call.  Annoying...
  while (aFrames) {
    if (!mSaved) {
      mSaved = (FarEndAudioChunk *) moz_xmalloc(sizeof(FarEndAudioChunk) +
                                                (mChunkSize * aChannels - 1)*sizeof(int16_t));
      mSaved->mSamples = mChunkSize;
      mSaved->mOverrun = aOverran;
      aOverran = false;
    }
    uint32_t to_copy = mChunkSize - mSamplesSaved;
    if (to_copy > aFrames) {
      to_copy = aFrames;
    }

    int16_t *dest = &(mSaved->mData[mSamplesSaved * aChannels]);
    ConvertAudioSamples(aBuffer, dest, to_copy * aChannels);

#ifdef LOG_FAREND_INSERTION
    if (fp) {
      fwrite(&(mSaved->mData[mSamplesSaved * aChannels]), to_copy * aChannels, sizeof(int16_t), fp);
    }
#endif
    aFrames -= to_copy;
    mSamplesSaved += to_copy;
    aBuffer += to_copy * aChannels;

    if (mSamplesSaved >= mChunkSize) {
      int free_slots = mPlayoutFifo->capacity() - mPlayoutFifo->size();
      if (free_slots <= 0) {
        // XXX We should flag an overrun for the reader.  We can't drop data from it due to
        // thread safety issues.
        break;
      } else {
        mPlayoutFifo->Push((int8_t *) mSaved); // takes ownership
        mSaved = nullptr;
        mSamplesSaved = 0;
      }
    }
  }
}

void
MediaEngineWebRTCMicrophoneSource::GetName(nsAString& aName)
{
  if (mInitDone) {
    aName.Assign(mDeviceName);
  }

  return;
}

void
MediaEngineWebRTCMicrophoneSource::GetUUID(nsACString& aUUID)
{
  if (mInitDone) {
    aUUID.Assign(mDeviceUUID);
  }

  return;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Config(bool aEchoOn, uint32_t aEcho,
                                          bool aAgcOn, uint32_t aAGC,
                                          bool aNoiseOn, uint32_t aNoise,
                                          int32_t aPlayoutDelay)
{
  LOG(("Audio config: aec: %d, agc: %d, noise: %d",
       aEchoOn ? aEcho : -1,
       aAgcOn ? aAGC : -1,
       aNoiseOn ? aNoise : -1));

  bool update_echo = (mEchoOn != aEchoOn);
  bool update_agc = (mAgcOn != aAgcOn);
  bool update_noise = (mNoiseOn != aNoiseOn);
  mEchoOn = aEchoOn;
  mAgcOn = aAgcOn;
  mNoiseOn = aNoiseOn;

  if ((webrtc::EcModes) aEcho != webrtc::kEcUnchanged) {
    if (mEchoCancel != (webrtc::EcModes) aEcho) {
      update_echo = true;
      mEchoCancel = (webrtc::EcModes) aEcho;
    }
  }
  if ((webrtc::AgcModes) aAGC != webrtc::kAgcUnchanged) {
    if (mAGC != (webrtc::AgcModes) aAGC) {
      update_agc = true;
      mAGC = (webrtc::AgcModes) aAGC;
    }
  }
  if ((webrtc::NsModes) aNoise != webrtc::kNsUnchanged) {
    if (mNoiseSuppress != (webrtc::NsModes) aNoise) {
      update_noise = true;
      mNoiseSuppress = (webrtc::NsModes) aNoise;
    }
  }
  mPlayoutDelay = aPlayoutDelay;

  if (mInitDone) {
    int error;

    if (update_echo &&
      0 != (error = mVoEProcessing->SetEcStatus(mEchoOn, (webrtc::EcModes) aEcho))) {
      LOG(("%s Error setting Echo Status: %d ",__FUNCTION__, error));
      // Overhead of capturing all the time is very low (<0.1% of an audio only call)
      if (mEchoOn) {
        if (0 != (error = mVoEProcessing->SetEcMetricsStatus(true))) {
          LOG(("%s Error setting Echo Metrics: %d ",__FUNCTION__, error));
        }
      }
    }
    if (update_agc &&
      0 != (error = mVoEProcessing->SetAgcStatus(mAgcOn, (webrtc::AgcModes) aAGC))) {
      LOG(("%s Error setting AGC Status: %d ",__FUNCTION__, error));
    }
    if (update_noise &&
      0 != (error = mVoEProcessing->SetNsStatus(mNoiseOn, (webrtc::NsModes) aNoise))) {
      LOG(("%s Error setting NoiseSuppression Status: %d ",__FUNCTION__, error));
    }
  }
  return NS_OK;
}

// GetBestFitnessDistance returns the best distance the capture device can offer
// as a whole, given an accumulated number of ConstraintSets.
// Ideal values are considered in the first ConstraintSet only.
// Plain values are treated as Ideal in the first ConstraintSet.
// Plain values are treated as Exact in subsequent ConstraintSets.
// Infinity = UINT32_MAX e.g. device cannot satisfy accumulated ConstraintSets.
// A finite result may be used to calculate this device's ranking as a choice.

uint32_t MediaEngineWebRTCMicrophoneSource::GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId)
{
  uint32_t distance = 0;

  for (const MediaTrackConstraintSet* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, false, aDeviceId);
    break; // distance is read from first entry only
  }
  return distance;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                            const MediaEnginePrefs &aPrefs,
                                            const nsString& aDeviceId)
{
  AssertIsOnOwningThread();
  if (mState == kReleased) {
    if (mInitDone) {
      ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw(webrtc::VoEHardware::GetInterface(mVoiceEngine));
      if (!ptrVoEHw || ptrVoEHw->SetRecordingDevice(mCapIndex)) {
        return NS_ERROR_FAILURE;
      }
      mState = kAllocated;
      LOG(("Audio device %d allocated", mCapIndex));
    } else {
      LOG(("Audio device is not initalized"));
      return NS_ERROR_FAILURE;
    }
  } else if (MOZ_LOG_TEST(GetMediaManagerLog(), LogLevel::Debug)) {
    MonitorAutoLock lock(mMonitor);
    if (mSources.IsEmpty()) {
      LOG(("Audio device %d reallocated", mCapIndex));
    } else {
      LOG(("Audio device %d allocated shared", mCapIndex));
    }
  }
  ++mNrAllocations;
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Deallocate()
{
  AssertIsOnOwningThread();
  --mNrAllocations;
  MOZ_ASSERT(mNrAllocations >= 0, "Double-deallocations are prohibited");
  if (mNrAllocations == 0) {
    // If empty, no callbacks to deliver data should be occuring
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }

    mState = kReleased;
    LOG(("Audio device %d deallocated", mCapIndex));
  } else {
    LOG(("Audio device %d deallocated but still in use", mCapIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Start(SourceMediaStream *aStream,
                                         TrackID aID)
{
  AssertIsOnOwningThread();
  if (!mInitDone || !aStream) {
    return NS_ERROR_FAILURE;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
  }

  AudioSegment* segment = new AudioSegment();
  aStream->AddAudioTrack(aID, mSampleFrequency, 0, segment, SourceMediaStream::ADDTRACK_QUEUED);

  // XXX Make this based on the pref.
  aStream->RegisterForAudioMixing();
  LOG(("Start audio for stream %p", aStream));

  if (mState == kStarted) {
    MOZ_ASSERT(aID == mTrackID);
    return NS_OK;
  }
  mState = kStarted;
  mTrackID = aID;

  // Make sure logger starts before capture
  AsyncLatencyLogger::Get(true);

  // Register output observer
  // XXX
  MOZ_ASSERT(gFarendObserver);
  gFarendObserver->Clear();

  // Configure audio processing in webrtc code
  Config(mEchoOn, webrtc::kEcUnchanged,
         mAgcOn, webrtc::kAgcUnchanged,
         mNoiseOn, webrtc::kNsUnchanged,
         mPlayoutDelay);

  if (mVoEBase->StartReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StartSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Attach external media processor, so this::Process will be called.
  mVoERender->RegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel, *this);

  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  AssertIsOnOwningThread();
  {
    MonitorAutoLock lock(mMonitor);

    if (!mSources.RemoveElement(aSource)) {
      // Already stopped - this is allowed
      return NS_OK;
    }

    aSource->EndTrack(aID);

    if (!mSources.IsEmpty()) {
      return NS_OK;
    }
    if (mState != kStarted) {
      return NS_ERROR_FAILURE;
    }
    if (!mVoEBase) {
      return NS_ERROR_FAILURE;
    }

    mState = kStopped;
  }

  mVoERender->DeRegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel);

  if (mVoEBase->StopSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StopReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Restart(const dom::MediaTrackConstraints& aConstraints,
                                           const MediaEnginePrefs &aPrefs,
                                           const nsString& aDeviceId)
{
  return NS_OK;
}

void
MediaEngineWebRTCMicrophoneSource::NotifyPull(MediaStreamGraph *aGraph,
                                              SourceMediaStream *aSource,
                                              TrackID aID,
                                              StreamTime aDesiredTime)
{
  // Ignore - we push audio data
  LOG_FRAMES(("NotifyPull, desired = %ld", (int64_t) aDesiredTime));
}

void
MediaEngineWebRTCMicrophoneSource::Init()
{
  mVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);

  mVoEBase->Init();

  mVoERender = webrtc::VoEExternalMedia::GetInterface(mVoiceEngine);
  if (!mVoERender) {
    return;
  }
  mVoENetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
  if (!mVoENetwork) {
    return;
  }

  mVoEProcessing = webrtc::VoEAudioProcessing::GetInterface(mVoiceEngine);
  if (!mVoEProcessing) {
    return;
  }

  mChannel = mVoEBase->CreateChannel();
  if (mChannel < 0) {
    return;
  }
  mNullTransport = new NullTransport();
  if (mVoENetwork->RegisterExternalTransport(mChannel, *mNullTransport)) {
    return;
  }

  mSampleFrequency = MediaEngine::DEFAULT_SAMPLE_RATE;
  LOG(("%s: sampling rate %u", __FUNCTION__, mSampleFrequency));

  // Check for availability.
  ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw(webrtc::VoEHardware::GetInterface(mVoiceEngine));
  if (!ptrVoEHw || ptrVoEHw->SetRecordingDevice(mCapIndex)) {
    return;
  }

#ifndef MOZ_B2G
  // Because of the permission mechanism of B2G, we need to skip the status
  // check here.
  bool avail = false;
  ptrVoEHw->GetRecordingDeviceStatus(avail);
  if (!avail) {
    return;
  }
#endif // MOZ_B2G

  // Set "codec" to PCM, 32kHz on 1 channel
  ScopedCustomReleasePtr<webrtc::VoECodec> ptrVoECodec(webrtc::VoECodec::GetInterface(mVoiceEngine));
  if (!ptrVoECodec) {
    return;
  }

  webrtc::CodecInst codec;
  strcpy(codec.plname, ENCODING);
  codec.channels = CHANNELS;
  MOZ_ASSERT(mSampleFrequency == 16000 || mSampleFrequency == 32000);
  codec.rate = SAMPLE_RATE(mSampleFrequency);
  codec.plfreq = mSampleFrequency;
  codec.pacsize = SAMPLE_LENGTH(mSampleFrequency);
  codec.pltype = 0; // Default payload type

  if (!ptrVoECodec->SetSendCodec(mChannel, codec)) {
    mInitDone = true;
  }
}

void
MediaEngineWebRTCMicrophoneSource::Shutdown()
{
  if (!mInitDone) {
    // duplicate these here in case we failed during Init()
    if (mChannel != -1 && mVoENetwork) {
      mVoENetwork->DeRegisterExternalTransport(mChannel);
    }

    delete mNullTransport;
    mNullTransport = nullptr;
    return;
  }

  if (mState == kStarted) {
    SourceMediaStream *source;
    bool empty;

    while (1) {
      {
        MonitorAutoLock lock(mMonitor);
        empty = mSources.IsEmpty();
        if (empty) {
          break;
        }
        source = mSources[0];
      }
      Stop(source, kAudioTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  if (mState == kAllocated || mState == kStopped) {
    Deallocate();
  }

  mVoEBase->Terminate();
  if (mChannel != -1) {
    mVoENetwork->DeRegisterExternalTransport(mChannel);
  }

  delete mNullTransport;
  mNullTransport = nullptr;

  mVoEProcessing = nullptr;
  mVoENetwork = nullptr;
  mVoERender = nullptr;
  mVoEBase = nullptr;

  mState = kReleased;
  mInitDone = false;
}

typedef int16_t sample;

void
MediaEngineWebRTCMicrophoneSource::Process(int channel,
                                           webrtc::ProcessingTypes type,
                                           sample *audio10ms, int length,
                                           int samplingFreq, bool isStereo)
{
  // On initial capture, throw away all far-end data except the most recent sample
  // since it's already irrelevant and we want to keep avoid confusing the AEC far-end
  // input code with "old" audio.
  if (!mStarted) {
    mStarted  = true;
    while (gFarendObserver->Size() > 1) {
      free(gFarendObserver->Pop()); // only call if size() > 0
    }
  }

  while (gFarendObserver->Size() > 0) {
    FarEndAudioChunk *buffer = gFarendObserver->Pop(); // only call if size() > 0
    if (buffer) {
      int length = buffer->mSamples;
      int res = mVoERender->ExternalPlayoutData(buffer->mData,
                                                gFarendObserver->PlayoutFrequency(),
                                                gFarendObserver->PlayoutChannels(),
                                                mPlayoutDelay,
                                                length);
      free(buffer);
      if (res == -1) {
        return;
      }
    }
  }

  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted)
    return;

  uint32_t len = mSources.Length();
  for (uint32_t i = 0; i < len; i++) {
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create(length * sizeof(sample));

    sample* dest = static_cast<sample*>(buffer->Data());
    memcpy(dest, audio10ms, length * sizeof(sample));

    nsAutoPtr<AudioSegment> segment(new AudioSegment());
    nsAutoTArray<const sample*,1> channels;
    channels.AppendElement(dest);
    segment->AppendFrames(buffer.forget(), channels, length);
    TimeStamp insertTime;
    segment->GetStartTime(insertTime);

    if (mSources[i]) {
      // Make sure we include the stream and the track.
      // The 0:1 is a flag to note when we've done the final insert for a given input block.
      LogTime(AsyncLatencyLogger::AudioTrackInsertion, LATENCY_STREAM_ID(mSources[i].get(), mTrackID),
              (i+1 < len) ? 0 : 1, insertTime);

      // This is safe from any thread, and is safe if the track is Finished
      // or Destroyed.
      // Note: due to evil magic, the nsAutoPtr<AudioSegment>'s ownership transfers to
      // the Runnable (AutoPtr<> = AutoPtr<>)
      RUN_ON_THREAD(mThread, WrapRunnable(mSources[i], &SourceMediaStream::AppendToTrack,
                                          mTrackID, segment, (AudioSegment *) nullptr),
                    NS_DISPATCH_NORMAL);
    }
  }

  return;
}

void
MediaEngineWebRTCAudioCaptureSource::GetName(nsAString &aName)
{
  aName.AssignLiteral("AudioCapture");
}
void
MediaEngineWebRTCAudioCaptureSource::GetUUID(nsACString &aUUID)
{
  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    aUUID.AssignLiteral("");
    return;
  }


  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  aUUID.Assign(Substring(asciiString, 1, NSID_LENGTH - 3));
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Start(SourceMediaStream *aMediaStream,
                                           TrackID aId)
{
  AssertIsOnOwningThread();
  aMediaStream->AddTrack(aId, 0, new AudioSegment());
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Stop(SourceMediaStream *aMediaStream,
                                          TrackID aId)
{
  AssertIsOnOwningThread();
  aMediaStream->EndAllTrackAndFinish();
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Restart(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId)
{
  return NS_OK;
}

uint32_t
MediaEngineWebRTCAudioCaptureSource::GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId)
{
  // There is only one way of capturing audio for now, and it's always adequate.
  return 0;
}
}
