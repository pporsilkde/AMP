#ifndef OPENMW_PROCESSORPLAYERVOICE_HPP
#define OPENMW_PROCESSORPLAYERVOICE_HPP

#include <chrono>
#include <cstdint>
#include <cmath>
#include <set>
#include <unordered_map>
#include <components/misc/constants.hpp>
#include <components/openmw-mp/Packets/Player/PacketPlayerVoice.hpp>
#include "../PlayerProcessor.hpp"
#include "../../Networking.hpp"

namespace mwmp
{
    class ProcessorPlayerVoice : public PlayerProcessor
    {
    public:
        ProcessorPlayerVoice() { BPP_INIT(ID_PLAYER_VOICE) }

        void Do(PlayerPacket& packet, Player& player) override
        {
            if (!packet.isPacketValid() || !Networking::get().isVoiceEnabled() || !player.isVisibleToOthers())
                return;

            const VoiceFrame& frame = player.voiceFrame;
            if (frame.codec != VoiceFrame::CodecImaAdpcm16k || frame.mode != VoiceFrame::ModeProximity
                || frame.payload.size() != VoiceFrame::ImaAdpcmPayloadBytes
                || frame.payload.size() > PacketPlayerVoice::MaxPayloadBytes)
                return;

            // 50 packets/s is the expected 20 ms frame rate. Allow short jitter/bursts
            // but refuse a client that tries to turn voice into an arbitrary UDP flood.
            struct RateState
            {
                std::chrono::steady_clock::time_point start;
                unsigned int count = 0;
            };
            static std::unordered_map<std::uint64_t, RateState> rate;
            const auto now = std::chrono::steady_clock::now();
            RateState& state = rate[player.guid.g];
            if (state.start.time_since_epoch().count() == 0
                || now - state.start >= std::chrono::seconds(1))
            {
                state.start = now;
                state.count = 0;
            }
            if (++state.count > 75)
                return;

            const float maxUnits = Networking::get().getVoiceRangeMeters() * Constants::UnitsPerMeter;
            const float maxDistance2 = maxUnits * maxUnits;
            const std::set<RakNet::RakNetGUID> recipients = player.getLoadedPlayerGuids();
            for (const RakNet::RakNetGUID& guid : recipients)
            {
                Player* other = Players::getPlayer(guid);
                if (other == nullptr || !other->isVisibleToOthers())
                    continue;

                // Interiors are isolated acoustic spaces. Exteriors may cross cell
                // borders naturally as long as their world-space distance is close.
                if (player.cell.isExterior() != other->cell.isExterior())
                    continue;
                if (!player.cell.isExterior() && player.cell.mName != other->cell.mName)
                    continue;

                const float dx = player.position.pos[0] - other->position.pos[0];
                const float dy = player.position.pos[1] - other->position.pos[1];
                const float dz = player.position.pos[2] - other->position.pos[2];
                if (dx * dx + dy * dy + dz * dz > maxDistance2)
                    continue;

                packet.setPlayer(&player);
                packet.Send(other->guid);
            }
        }
    };
}

#endif
