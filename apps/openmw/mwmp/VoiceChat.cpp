#include "VoiceChat.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include <SDL.h>

#include <components/misc/constants.hpp>
#include <components/openmw-mp/Base/BasePlayer.hpp>
#include <components/openmw-mp/NetworkMessages.hpp>
#include <components/openmw-mp/Packets/Player/PacketPlayerVoice.hpp>
#include <components/openmw-mp/TimedLog.hpp>

#include "Main.hpp"
#include "GUIController.hpp"
#include "LocalPlayer.hpp"
#include "Networking.hpp"
#include "PlayerList.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwsound/sound.hpp"
#include "../mwsound/sound_decoder.hpp"

namespace
{
    constexpr int sSampleRate = mwmp::VoiceFrame::SampleRate;
    constexpr std::size_t sFrameSamples = mwmp::VoiceFrame::FrameSamples; // 20 ms
    constexpr std::size_t sMaxCaptureSamples = sSampleRate;
    constexpr std::size_t sMaxPlaybackSamples = sSampleRate / 2;
    constexpr float sSpeakerTimeout = 0.75f;

    constexpr std::array<int, 89> sStepTable = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
        34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
        157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
        598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
        2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
        6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
        18500, 20350, 22385, 24623, 27086, 29794, 32767
    };
    constexpr std::array<int, 16> sIndexTable = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    };

    int clamp16(int value)
    {
        return std::max(-32768, std::min(32767, value));
    }

    std::uint8_t encodeNibble(std::int16_t sample, int& predictor, int& index)
    {
        const int step = sStepTable[index];
        int diff = static_cast<int>(sample) - predictor;
        std::uint8_t code = 0;
        if (diff < 0)
        {
            code = 8;
            diff = -diff;
        }

        int delta = step >> 3;
        if (diff >= step) { code |= 4; diff -= step; delta += step; }
        if (diff >= (step >> 1)) { code |= 2; diff -= step >> 1; delta += step >> 1; }
        if (diff >= (step >> 2)) { code |= 1; delta += step >> 2; }

        predictor += (code & 8) ? -delta : delta;
        predictor = clamp16(predictor);
        index = std::clamp(index + sIndexTable[code & 0x0f], 0, 88);
        return code & 0x0f;
    }

    std::vector<std::uint8_t> encodeAdpcm(const std::array<std::int16_t, sFrameSamples>& samples, int& rollingIndex)
    {
        std::vector<std::uint8_t> out;
        out.reserve(4 + (sFrameSamples / 2));

        int predictor = samples[0];
        int index = std::clamp(rollingIndex, 0, 88);
        out.push_back(static_cast<std::uint8_t>(predictor & 0xff));
        out.push_back(static_cast<std::uint8_t>((predictor >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(index));
        out.push_back(0);

        bool low = true;
        std::uint8_t packed = 0;
        for (std::size_t i = 1; i < samples.size(); ++i)
        {
            const std::uint8_t nibble = encodeNibble(samples[i], predictor, index);
            if (low)
            {
                packed = nibble;
                low = false;
            }
            else
            {
                packed |= static_cast<std::uint8_t>(nibble << 4);
                out.push_back(packed);
                low = true;
            }
        }
        if (!low)
            out.push_back(packed);

        rollingIndex = index;
        return out;
    }

    bool decodeAdpcm(const std::vector<std::uint8_t>& payload, std::vector<std::int16_t>& out)
    {
        if (payload.size() < 4)
            return false;

        int predictor = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(payload[0]) | (static_cast<std::uint16_t>(payload[1]) << 8));
        int index = payload[2];
        if (index < 0 || index > 88)
            return false;

        out.clear();
        out.reserve(sFrameSamples);
        out.push_back(static_cast<std::int16_t>(predictor));

        for (std::size_t byteIndex = 4; byteIndex < payload.size() && out.size() < sFrameSamples; ++byteIndex)
        {
            for (int shift : {0, 4})
            {
                if (out.size() >= sFrameSamples)
                    break;
                const std::uint8_t code = static_cast<std::uint8_t>((payload[byteIndex] >> shift) & 0x0f);
                const int step = sStepTable[index];
                int delta = step >> 3;
                if (code & 4) delta += step;
                if (code & 2) delta += step >> 1;
                if (code & 1) delta += step >> 2;
                predictor += (code & 8) ? -delta : delta;
                predictor = clamp16(predictor);
                index = std::clamp(index + sIndexTable[code], 0, 88);
                out.push_back(static_cast<std::int16_t>(predictor));
            }
        }
        return out.size() == sFrameSamples;
    }

    bool sequenceNewer(std::uint16_t sequence, std::uint16_t previous)
    {
        return static_cast<std::int16_t>(sequence - previous) > 0;
    }
}

namespace mwmp
{
    class VoiceStreamDecoder final : public MWSound::Sound_Decoder
    {
    public:
        VoiceStreamDecoder() : Sound_Decoder(nullptr) {}
        void open(const std::string&) override {}
        void close() override {}
        std::string getName() override { return "ArenaMP proximity voice"; }
        void getInfo(int* samplerate, MWSound::ChannelConfig* chans, MWSound::SampleType* type) override
        {
            *samplerate = sSampleRate;
            *chans = MWSound::ChannelConfig_Mono;
            *type = MWSound::SampleType_Int16;
        }
        size_t read(char* buffer, size_t bytes) override
        {
            const std::size_t requested = bytes / sizeof(std::int16_t);
            auto* output = reinterpret_cast<std::int16_t*>(buffer);
            std::size_t copied = 0;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                while (copied < requested && !mSamples.empty())
                {
                    output[copied++] = mSamples.front();
                    mSamples.pop_front();
                }
            }
            std::fill(output + copied, output + requested, 0);
            mOffset += requested;
            return requested * sizeof(std::int16_t);
        }
        size_t getSampleOffset() override { return mOffset; }

        void push(const std::vector<std::int16_t>& samples)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mSamples.size() + samples.size() > sMaxPlaybackSamples)
            {
                const std::size_t overflow = mSamples.size() + samples.size() - sMaxPlaybackSamples;
                for (std::size_t i = 0; i < overflow && !mSamples.empty(); ++i)
                    mSamples.pop_front();
            }
            mSamples.insert(mSamples.end(), samples.begin(), samples.end());
        }
        void pushSilence(std::size_t samples)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            samples = std::min(samples, sMaxPlaybackSamples);
            while (mSamples.size() + samples > sMaxPlaybackSamples && !mSamples.empty())
                mSamples.pop_front();
            mSamples.insert(mSamples.end(), samples, 0);
        }
        std::size_t queuedSamples() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mSamples.size();
        }
    private:
        mutable std::mutex mMutex;
        std::deque<std::int16_t> mSamples;
        std::size_t mOffset = 0;
    };

    float voiceLipLevel(const std::int16_t* samples, std::size_t count)
    {
        if (samples == nullptr || count == 0)
            return 0.f;

        double sumSquares = 0.0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const double normalized = static_cast<double>(samples[i]) / 32768.0;
            sumSquares += normalized * normalized;
        }

        // Native head talk morphs expect the same approximate 0..1 loudness range
        // as voiced dialogue. A small gain keeps ordinary microphones expressive
        // without making background noise hold the mouth open.
        const float rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
        const float gated = std::max(0.f, rms - 0.012f);
        return std::clamp(gated * 5.f, 0.f, 1.f);
    }

    struct VoiceChat::Impl
    {
        struct Speaker
        {
            std::shared_ptr<VoiceStreamDecoder> decoder;
            MWSound::Stream* stream = nullptr;
            float age = 0.f;
            std::uint16_t lastSequence = 0;
            bool haveSequence = false;
            float lipLevel = 0.f;
        };

        SDL_AudioDeviceID captureDevice = 0;
        SDL_AudioStream* captureConverter = nullptr;
        SDL_Scancode pushToTalk = SDL_SCANCODE_V;
        bool ownsAudioSubsystem = false;
        std::mutex captureMutex;
        std::deque<std::int16_t> captureSamples;
        std::unordered_map<std::uint64_t, Speaker> speakers;
        float localLipLevel = 0.f;
        float localLipAge = 1.f;
    };

    namespace
    {
        void appendCaptureSamples(VoiceChat::Impl* impl, const std::int16_t* samples, std::size_t count)
        {
            if (impl == nullptr || samples == nullptr || count == 0)
                return;
            std::lock_guard<std::mutex> lock(impl->captureMutex);
            if (impl->captureSamples.size() + count > sMaxCaptureSamples)
            {
                const std::size_t overflow = impl->captureSamples.size() + count - sMaxCaptureSamples;
                for (std::size_t i = 0; i < overflow && !impl->captureSamples.empty(); ++i)
                    impl->captureSamples.pop_front();
            }
            impl->captureSamples.insert(impl->captureSamples.end(), samples, samples + count);
        }

        void captureCallback(void* userdata, Uint8* stream, int len)
        {
            auto* impl = static_cast<VoiceChat::Impl*>(userdata);
            if (impl == nullptr || stream == nullptr || len <= 0)
                return;

            if (impl->captureConverter == nullptr)
            {
                appendCaptureSamples(impl, reinterpret_cast<const std::int16_t*>(stream),
                    static_cast<std::size_t>(len) / sizeof(std::int16_t));
                return;
            }

            if (SDL_AudioStreamPut(impl->captureConverter, stream, len) != 0)
                return;

            std::array<std::int16_t, 2048> converted{};
            for (;;)
            {
                const int available = SDL_AudioStreamAvailable(impl->captureConverter);
                if (available < static_cast<int>(sizeof(std::int16_t)))
                    break;
                const int wanted = std::min<int>(available, static_cast<int>(sizeof(converted)));
                const int got = SDL_AudioStreamGet(impl->captureConverter, converted.data(), wanted);
                if (got <= 0)
                    break;
                appendCaptureSamples(impl, converted.data(), static_cast<std::size_t>(got) / sizeof(std::int16_t));
            }
        }
    }

    VoiceChat::VoiceChat() : mImpl(new Impl) {}
    VoiceChat::~VoiceChat() { shutdown(); }

    void VoiceChat::configure(bool enabled, const std::string& pushToTalkKey, float rangeMeters)
    {
        mEnabled = enabled;
        mPushToTalkKey = pushToTalkKey.empty() ? "V" : pushToTalkKey;
        mRangeMeters = std::clamp(rangeMeters, 3.f, 100.f);
        const SDL_Scancode code = SDL_GetScancodeFromName(mPushToTalkKey.c_str());
        mImpl->pushToTalk = code == SDL_SCANCODE_UNKNOWN ? SDL_SCANCODE_V : code;
    }

    void VoiceChat::init()
    {
        if (!mEnabled || mAvailable)
            return;

        if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
        {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Voice: SDL audio init failed: %s", SDL_GetError());
                return;
            }
            mImpl->ownsAudioSubsystem = true;
        }

        SDL_AudioSpec desired{};
        desired.freq = sSampleRate;
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = static_cast<Uint16>(sFrameSamples);
        desired.callback = captureCallback;
        desired.userdata = mImpl.get();

        SDL_AudioSpec obtained{};
        const int allowedChanges = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE
            | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
        mImpl->captureDevice = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &desired, &obtained, allowedChanges);
        if (mImpl->captureDevice == 0)
        {
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Voice: microphone open failed: %s", SDL_GetError());
            return;
        }
        if (obtained.freq != sSampleRate || obtained.format != AUDIO_S16SYS || obtained.channels != 1)
        {
            mImpl->captureConverter = SDL_NewAudioStream(obtained.format, obtained.channels, obtained.freq,
                AUDIO_S16SYS, 1, sSampleRate);
            if (mImpl->captureConverter == nullptr)
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Voice: microphone conversion setup failed: %s", SDL_GetError());
                SDL_CloseAudioDevice(mImpl->captureDevice);
                mImpl->captureDevice = 0;
                return;
            }
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO,
                "Voice: converting microphone capture from %d Hz/format %u/%u ch to 16 kHz mono S16",
                obtained.freq, static_cast<unsigned int>(obtained.format), static_cast<unsigned int>(obtained.channels));
        }

        SDL_PauseAudioDevice(mImpl->captureDevice, 1);
        mAvailable = true;
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Voice: proximity PTT ready (%s, %.1f m)",
            SDL_GetScancodeName(mImpl->pushToTalk), mRangeMeters);
    }

    void VoiceChat::update(float dt)
    {
        if (!mEnabled)
            return;
        if (!mAvailable)
            init();
        if (!mAvailable)
            return;

        LocalPlayer* local = Main::get().getLocalPlayer();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        GUIController* gui = Main::get().getGUIController();
        const bool chatEditing = gui != nullptr && gui->getChatEditState();
        const bool pressed = local != nullptr && local->isLoggedIn() && !chatEditing
            && keys != nullptr && keys[mImpl->pushToTalk];
        if (pressed != mTransmitting)
        {
            // Pause first so the capture callback cannot race the frame/converter
            // reset. This also guarantees that a new PTT press never replays the
            // tail of the previous transmission.
            SDL_PauseAudioDevice(mImpl->captureDevice, 1);
            {
                std::lock_guard<std::mutex> lock(mImpl->captureMutex);
                mImpl->captureSamples.clear();
            }
            if (mImpl->captureConverter != nullptr)
                SDL_AudioStreamClear(mImpl->captureConverter);
            mTransmitting = pressed;
            if (pressed)
                SDL_PauseAudioDevice(mImpl->captureDevice, 0);
            else
            {
                mImpl->localLipLevel = 0.f;
                mImpl->localLipAge = 1.f;
                if (local != nullptr)
                {
                    MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
                    if (soundManager != nullptr)
                        soundManager->clearVoiceLipSync(local->getPlayerPtr());
                }
            }
        }

        mImpl->localLipAge += std::max(0.f, dt);
        bool capturedVoiceFrame = false;
        if (mTransmitting && local != nullptr)
        {
            for (;;)
            {
                std::array<std::int16_t, sFrameSamples> frame{};
                {
                    std::lock_guard<std::mutex> lock(mImpl->captureMutex);
                    if (mImpl->captureSamples.size() < sFrameSamples)
                        break;
                    for (std::size_t i = 0; i < sFrameSamples; ++i)
                    {
                        frame[i] = mImpl->captureSamples.front();
                        mImpl->captureSamples.pop_front();
                    }
                }

                mImpl->localLipLevel = voiceLipLevel(frame.data(), frame.size());
                mImpl->localLipAge = 0.f;
                capturedVoiceFrame = true;
                MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
                if (soundManager != nullptr)
                    soundManager->setVoiceLipSyncLevel(local->getPlayerPtr(), mImpl->localLipLevel);

                local->voiceFrame.sequence = ++mSequence;
                local->voiceFrame.codec = VoiceFrame::CodecImaAdpcm16k;
                local->voiceFrame.mode = VoiceFrame::ModeProximity;
                local->voiceFrame.payload = encodeAdpcm(frame, mAdpcmIndex);
                PlayerPacket* packet = Main::get().getNetworking()->getPlayerPacket(ID_PLAYER_VOICE);
                packet->setPlayer(local);
                packet->Send();
            }
        }

        MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
        if (mTransmitting && local != nullptr && !capturedVoiceFrame && mImpl->localLipAge > 0.10f)
        {
            mImpl->localLipLevel = 0.f;
            soundManager->clearVoiceLipSync(local->getPlayerPtr());
        }

        for (auto it = mImpl->speakers.begin(); it != mImpl->speakers.end();)
        {
            Impl::Speaker& speaker = it->second;
            speaker.age += dt;
            DedicatedPlayer* player = PlayerList::getPlayer(RakNet::RakNetGUID(it->first));
            if (speaker.age > sSpeakerTimeout || player == nullptr || player->getPtr().isEmpty())
            {
                if (player != nullptr && !player->getPtr().isEmpty() && soundManager != nullptr)
                    soundManager->clearVoiceLipSync(player->getPtr());
                if (speaker.stream != nullptr && soundManager != nullptr)
                    soundManager->stopTrack(speaker.stream);
                it = mImpl->speakers.erase(it);
                continue;
            }

            // Hold each 20 ms voice level briefly and then close the mouth smoothly
            // if packets stop. This follows actual speech energy instead of playing a
            // body animation, so movement/combat animation layers stay untouched.
            float lipLevel = speaker.lipLevel;
            if (speaker.age > 0.06f)
                lipLevel *= std::clamp(1.f - (speaker.age - 0.06f) / 0.12f, 0.f, 1.f);
            if (soundManager != nullptr)
                soundManager->setVoiceLipSyncLevel(player->getPtr(), lipLevel);

            const osg::Vec3f pos = MWBase::Environment::get().getWorld()->getActorHeadTransform(player->getPtr()).getTrans();
            if (speaker.stream != nullptr)
                speaker.stream->setPosition(pos);
            ++it;
        }
    }

    void VoiceChat::receive(RakNet::RakNetGUID speakerGuid, const VoiceFrame& frame)
    {
        if (!mEnabled || frame.codec != VoiceFrame::CodecImaAdpcm16k || frame.payload.empty())
            return;

        DedicatedPlayer* player = PlayerList::getPlayer(speakerGuid);
        MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
        if (player == nullptr || player->getPtr().isEmpty() || soundManager == nullptr)
            return;

        const std::uint64_t key = speakerGuid.g;
        Impl::Speaker& speaker = mImpl->speakers[key];
        if (speaker.haveSequence && !sequenceNewer(frame.sequence, speaker.lastSequence))
            return;

        if (!speaker.decoder)
            speaker.decoder = std::make_shared<VoiceStreamDecoder>();

        if (speaker.haveSequence)
        {
            const std::uint16_t gap = static_cast<std::uint16_t>(frame.sequence - speaker.lastSequence);
            if (gap > 1 && gap <= 4)
                speaker.decoder->pushSilence(static_cast<std::size_t>(gap - 1) * sFrameSamples);
        }

        std::vector<std::int16_t> decoded;
        if (!decodeAdpcm(frame.payload, decoded))
            return;
        speaker.decoder->push(decoded);
        speaker.lipLevel = voiceLipLevel(decoded.data(), decoded.size());
        soundManager->setVoiceLipSyncLevel(player->getPtr(), speaker.lipLevel);
        speaker.lastSequence = frame.sequence;
        speaker.haveSequence = true;
        speaker.age = 0.f;

        // Prebuffer three 20 ms frames. The low-latency OpenAL stream also uses
        // exactly three 20 ms buffers, preventing the normal music-stream
        // prequeue (6 x 125 ms) from adding ~750 ms of voice delay.
        if (speaker.stream == nullptr && speaker.decoder->queuedSamples() >= sFrameSamples * 3)
        {
            const osg::Vec3f pos = MWBase::Environment::get().getWorld()->getActorHeadTransform(player->getPtr()).getTrans();
            speaker.stream = soundManager->playTrack3D(speaker.decoder, pos,
                2.f * Constants::UnitsPerMeter, mRangeMeters * Constants::UnitsPerMeter, 1.f, MWSound::Type::Voice);
        }
    }

    void VoiceChat::removeSpeaker(RakNet::RakNetGUID speakerGuid)
    {
        const auto it = mImpl->speakers.find(speakerGuid.g);
        if (it == mImpl->speakers.end())
            return;
        MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
        DedicatedPlayer* player = PlayerList::getPlayer(speakerGuid);
        if (player != nullptr && !player->getPtr().isEmpty() && soundManager != nullptr)
            soundManager->clearVoiceLipSync(player->getPtr());
        if (it->second.stream != nullptr && soundManager != nullptr)
            soundManager->stopTrack(it->second.stream);
        mImpl->speakers.erase(it);
    }

    void VoiceChat::shutdown()
    {
        if (!mImpl)
            return;
        MWBase::SoundManager* soundManager = nullptr;
        if (MWBase::Environment::get().getSoundManager() != nullptr)
            soundManager = MWBase::Environment::get().getSoundManager();
        if (soundManager != nullptr)
        {
            for (auto& entry : mImpl->speakers)
            {
                DedicatedPlayer* player = PlayerList::getPlayer(RakNet::RakNetGUID(entry.first));
                if (player != nullptr && !player->getPtr().isEmpty())
                    soundManager->clearVoiceLipSync(player->getPtr());
                if (entry.second.stream != nullptr)
                    soundManager->stopTrack(entry.second.stream);
            }
            LocalPlayer* local = Main::isInitialized() ? Main::get().getLocalPlayer() : nullptr;
            if (local != nullptr)
                soundManager->clearVoiceLipSync(local->getPlayerPtr());
        }
        mImpl->speakers.clear();
        if (mImpl->captureDevice != 0)
        {
            SDL_CloseAudioDevice(mImpl->captureDevice);
            mImpl->captureDevice = 0;
        }
        if (mImpl->captureConverter != nullptr)
        {
            SDL_FreeAudioStream(mImpl->captureConverter);
            mImpl->captureConverter = nullptr;
        }
        if (mImpl->ownsAudioSubsystem)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            mImpl->ownsAudioSubsystem = false;
        }
        mAvailable = false;
        mTransmitting = false;
    }
}
