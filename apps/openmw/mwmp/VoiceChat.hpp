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

        void configure(bool enabled, const std::string& pushToTalkKey, float rangeMeters);
        void init();
        void update(float dt);
        void receive(RakNet::RakNetGUID speakerGuid, const VoiceFrame& frame);
        void removeSpeaker(RakNet::RakNetGUID speakerGuid);
        void shutdown();

        bool isAvailable() const { return mAvailable; }
        bool isTransmitting() const { return mTransmitting; }

    public:
        struct Impl;

    private:
        void notifyUnavailable(const std::string& reason);

        std::unique_ptr<Impl> mImpl;
        bool mEnabled = true;
        bool mAvailable = false;
        bool mTransmitting = false;
        std::string mPushToTalkKey = "V";
        float mRangeMeters = 30.f;
        std::uint16_t mSequence = 0;
        int mAdpcmIndex = 0;
    };
}

#endif
