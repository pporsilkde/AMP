#include "ObjectProcessor.hpp"
#include "Networking.hpp"
#include "Cell.hpp"
#include "CellController.hpp"
#include "CoreArenaMPSecurity.hpp"

using namespace mwmp;

template<class T>
typename BasePacketProcessor<T>::processors_t BasePacketProcessor<T>::processors;

void ObjectProcessor::Do(ObjectPacket &packet, Player &player, BaseObjectList &objectList)
{
    packet.Send(true);
}

void ObjectProcessor::SendToLoadedCell(ObjectPacket &packet, BaseObjectList &objectList)
{
    Cell* cell = CellController::get()->getCell(&objectList.cell);
    if (cell != nullptr)
        cell->sendToLoaded(&packet, &objectList);
}

bool ObjectProcessor::Process(RakNet::Packet &packet, BaseObjectList &objectList) noexcept
{
    // Clear our BaseObjectList before loading new data in it
    objectList.cell.blank();
    objectList.baseObjects.clear();
    objectList.guid = packet.guid;

    for (auto &processor : processors)
    {
        if (processor.first == packet.data[0])
        {
            Player *player = Players::getPlayer(packet.guid);
            if (player == nullptr)
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                    "CoreArenaMP security: ignored object packet from an unknown connection");
                return true;
            }

            ObjectPacket *myPacket = Networking::get().getObjectPacketController()->GetPacket(packet.data[0]);

            myPacket->setObjectList(&objectList);
            objectList.isValid = true;

            if (!processor.second->avoidReading)
            {
                myPacket->Read();
                if (!myPacket->isPacketValid())
                {
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,
                        "CoreArenaMP security: rejected malformed object packet before processing");
                    return true;
                }
            }

            if (objectList.isValid && CoreArenaMPSecurity::ValidateObjectPacket(*player, objectList, packet.data[0]))
                processor.second->Do(*myPacket, *player, objectList);
            else
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_ERROR, "Received %s that failed integrity check and was ignored!", processor.second->strPacketID.c_str());
            
            return true;
        }
    }
    return false;
}
