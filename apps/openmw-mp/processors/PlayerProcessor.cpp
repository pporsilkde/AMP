#include "PlayerProcessor.hpp"
#include "Networking.hpp"
#include "CoreArenaMPSecurity.hpp"

using namespace mwmp;

template<class T>
typename BasePacketProcessor<T>::processors_t BasePacketProcessor<T>::processors;

bool PlayerProcessor::Process(RakNet::Packet &packet) noexcept
{
    for (auto &processor : processors)
    {
        if (processor.first == packet.data[0])
        {
            Player *player = Players::getPlayer(packet.guid);
            if (player == nullptr)
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                    "CoreArenaMP security: ignored player packet from an unknown connection");
                return true;
            }

            PlayerPacket *myPacket = Networking::get().getPlayerPacketController()->GetPacket(packet.data[0]);
            myPacket->setPlayer(player);

            RakNet::BitStream rollbackStream;
            bool hasRollback = false;

            if (!processor.second->avoidReading)
            {
                // Serialize the current authoritative fields for this exact
                // packet type. BasePlayer itself is intentionally non-copyable
                // (CreatureStats owns an AI sequence), while packet-level
                // serialization gives us a precise transactional rollback.
                myPacket->Packet(&rollbackStream, true);
                hasRollback = true;

                CoreArenaMPSecurity::CapturePlayerState(*player, packet.data[0]);
                myPacket->Read();
            }

            const auto rollbackPlayerPacket = [&]()
            {
                if (!hasRollback)
                    return;
                rollbackStream.IgnoreBytes(BasePacket::headerSize());
                myPacket->Packet(&rollbackStream, false);
                myPacket->setPlayer(player);
            };

            if (!processor.second->avoidReading && !myPacket->isPacketValid())
            {
                rollbackPlayerPacket();
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                    "CoreArenaMP security: rejected malformed player packet before processing");
                if (packet.data[0] == ID_PLAYER_POSITION)
                    myPacket->Send(player->guid);
                return true;
            }

            if (!CoreArenaMPSecurity::ValidatePlayerPacket(*player, packet.data[0]))
            {
                rollbackPlayerPacket();
                if (packet.data[0] == ID_PLAYER_POSITION)
                    myPacket->Send(player->guid);
                return true;
            }

            processor.second->Do(*myPacket, *player);
            return true;
        }
    }
    return false;
}
