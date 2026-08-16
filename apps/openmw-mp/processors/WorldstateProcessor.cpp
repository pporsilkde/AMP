#include "WorldstateProcessor.hpp"
#include "Networking.hpp"
#include "CoreArenaMPSecurity.hpp"

using namespace mwmp;

template<class T>
typename BasePacketProcessor<T>::processors_t BasePacketProcessor<T>::processors;

void WorldstateProcessor::Do(WorldstatePacket &packet, Player &player, BaseWorldstate &worldstate)
{
    packet.Send(true);
}

bool WorldstateProcessor::Process(RakNet::Packet &packet, BaseWorldstate &worldstate) noexcept
{
    worldstate.guid = packet.guid;

    for (auto &processor : processors)
    {
        if (processor.first == packet.data[0])
        {
            Player *player = Players::getPlayer(packet.guid);
            if (player == nullptr)
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                    "CoreArenaMP security: ignored worldstate packet from an unknown connection");
                return true;
            }
            WorldstatePacket *myPacket = Networking::get().getWorldstatePacketController()->GetPacket(packet.data[0]);

            myPacket->setWorldstate(&worldstate);
            worldstate.isValid = true;

            if (!processor.second->avoidReading)
            {
                myPacket->Read();
                if (!myPacket->isPacketValid())
                {
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                        "CoreArenaMP security: rejected malformed worldstate packet before processing");
                    return true;
                }
            }

            if (worldstate.isValid && CoreArenaMPSecurity::ValidateWorldstatePacket(*player, worldstate, packet.data[0]))
                processor.second->Do(*myPacket, *player, worldstate);
            else
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_ERROR, "Received %s that failed integrity check and was ignored!", processor.second->strPacketID.c_str());
            
            return true;
        }
    }
    return false;
}
