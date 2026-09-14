#ifndef OPENMW_MWMP_VOICECHAT_HPP
#define OPENMW_MWMP_VOICECHAT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <RakNetTypes.h>

namespace mwmp
{
    struct VoiceFrame;
    class VoiceStreamDecoder;

    class VoiceChat
    {
    public:
        VoiceChat();
        ~VoiceChat();

        void configure(bool enabled, const std::string& pushToTalkKey, float rangeMeters,
            float fullVolumeMeters, float sourceVolume, float micGain, float playbackGain,
            bool toggleMode);
        void init();
        void update(float dt);
        void receive(RakNet::RakNetGUID speakerGuid, const VoiceFrame& frame);
        void removeSpeaker(RakNet::RakNetGUID speakerGuid);
        void shutdown();

        bool isAvailable() const { return mAvailable; }
        bool isTransmitting() const { return mTransmitting; }

        /// U035: true when the key latches the microphone open (radio mode)
        /// instead of having to be held down.
        bool isToggleMode() const { return mToggleMode; }
        /// U035: in radio mode, whether the microphone is currently latched on.
        bool isMicOpen() const;
        /// U035: flips the radio-mode latch. Exposed so a UI button or an
        /// Android control can toggle it without synthesising a key press.
        void toggleMicrophone();

    public:
        struct Impl;

    private:
        void notifyUnavailable(const std::string& reason);
        void publishSpeakerHud();

        std::unique_ptr<Impl> mImpl;
        bool mEnabled = true;
        bool mAvailable = false;
        bool mTransmitting = false;
        std::string mPushToTalkKey = "V";
        float mRangeMeters = 30.f;
        // U035: OpenAL uses the inverse distance model, so past the reference
        // distance gain is ref/dist. The old 2 m reference made a speaker 10 m
        // away five times quieter than one standing next to you; conversation
        // range belongs inside the full-volume radius.
        float mFullVolumeMeters = 12.f;
        // Source gain handed to OpenAL. Compensates the [Sound] voice volume
        // slider (which also scales NPC dialogue) without touching it; OpenAL
        // clamps the final source gain to 1.0, so this cannot distort.
        float mSourceVolume = 2.f;
        float mMicGain = 1.f;        // digital pre-encode boost, soft-limited
        float mPlaybackGain = 1.f;   // digital post-decode boost, soft-limited
        // U035 radio mode: the push-to-talk key latches instead of being held.
        bool mToggleMode = false;
        std::uint16_t mSequence = 0;
        int mAdpcmIndex = 0;
    };
}

#endif
