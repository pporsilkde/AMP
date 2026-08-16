#ifndef OPENMW_PROCESSOROBJECTDIALOGUECHOICE_HPP
#define OPENMW_PROCESSOROBJECTDIALOGUECHOICE_HPP

#include "../ObjectProcessor.hpp"
#include "../../CoreArenaMPSecurity.hpp"
#include <apps/openmw-mp/Networking.hpp>

namespace mwmp
{
    class ProcessorObjectDialogueChoice : public ObjectProcessor
    {
    public:
        ProcessorObjectDialogueChoice()
        {
            BPP_INIT(ID_OBJECT_DIALOGUE_CHOICE)
        }

        void Do(ObjectPacket &packet, Player &player, BaseObjectList &objectList) override
        {
            if (!CoreArenaMPSecurity::ValidateObjectInteraction(player, objectList, ID_OBJECT_DIALOGUE_CHOICE))
                return;

            LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Received %s from %s", strPacketID.c_str(), player.npc.mName.c_str());

            Script::Call<Script::CallbackIdentity("OnObjectDialogueChoice")>(player.getId(), objectList.cell.getShortDescription().c_str());
        }
    };
}

#endif //OPENMW_PROCESSOROBJECTDIALOGUECHOICE_HPP
