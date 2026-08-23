#include "Cell.hpp"

#include <components/openmw-mp/NetworkMessages.hpp>

#include <iostream>
#include "Player.hpp"
#include "Networking.hpp"
#include "Script/Script.hpp"

Cell::Cell(ESM::Cell cell) : cell(cell)
{
    cellActorList.count = 0;
    cellActorList.cell = cell;
    cellActorAIList.count = 0;
    cellActorAIList.cell = cell;
}

Cell::Iterator Cell::begin() const
{
    return players.begin();
}

Cell::Iterator Cell::end() const
{
    return players.end();
}

void Cell::addPlayer(Player *player)
{
    // Ensure the player hasn't already been added
    auto it = find(begin(), end(), player);

    if (it != end())
    {
        LOG_APPEND(TimedLog::LOG_INFO, "- Attempt to add %s to Cell %s again was ignored", player->npc.mName.c_str(), getShortDescription().c_str());
        return;
    }

    auto it2 = find(player->cells.begin(), player->cells.end(), this);
    if (it2 == player->cells.end())
    {
        LOG_APPEND(TimedLog::LOG_INFO, "- Adding %s to Player %s", getShortDescription().c_str(), player->npc.mName.c_str());

        player->cells.push_back(this);
    }

    LOG_APPEND(TimedLog::LOG_INFO, "- Adding %s to Cell %s", player->npc.mName.c_str(), getShortDescription().c_str());

    // Make the C++ interest set authoritative before Lua starts loading the cell.
    // Packets sent from OnCellLoad (authority, actor snapshots, etc.) must already
    // be able to target the newcomer.
    players.push_back(player);

    Script::Call<Script::CallbackIdentity("OnCellLoad")>(player->getId(), getShortDescription().c_str());
}

void Cell::removePlayer(Player *player, bool cleanPlayer)
{
    for (Iterator it = begin(); it != end(); it++)
    {
        if (*it == player)
        {
            if (cleanPlayer)
            {
                auto it2 = find(player->cells.begin(), player->cells.end(), this);
                if (it2 != player->cells.end())
                {
                    LOG_APPEND(TimedLog::LOG_INFO, "- Removing %s from Player %s", getShortDescription().c_str(), player->npc.mName.c_str());

                    player->cells.erase(it2);
                }
            }

            LOG_APPEND(TimedLog::LOG_INFO, "- Removing %s from Cell %s", player->npc.mName.c_str(), getShortDescription().c_str());

            // Remove the player from the C++ interest set before Lua performs an
            // authority handoff, so the departing peer is not treated as a live
            // observer of the cell while the handoff packets are sent.
            players.erase(it);

            Script::Call<Script::CallbackIdentity("OnCellUnload")>(player->getId(), getShortDescription().c_str());
            return;
        }
    }
}

void Cell::readActorList(unsigned char packetID, const mwmp::BaseActorList *newActorList)
{
    for (unsigned int i = 0; i < newActorList->count; i++)
    {
        mwmp::BaseActor newActor = newActorList->baseActors.at(i);
        mwmp::BaseActor *cellActor;

        if (containsActor(newActor.refNum, newActor.mpNum))
        {
            cellActor = getActor(newActor.refNum, newActor.mpNum);

            switch (packetID)
            {
            case ID_ACTOR_POSITION:

                cellActor->hasPositionData = true;
                cellActor->position = newActor.position;
                // Keep the movement vector together with the cached position.
                // Otherwise a cached actor can be restored with coordinates but
                // no locomotion direction and appear to slide until the next
                // fresh live movement packet arrives.
                cellActor->direction = newActor.direction;
                break;

            case ID_ACTOR_STATS_DYNAMIC:

                cellActor->hasStatsDynamicData = true;
                cellActor->creatureStats.mDynamic[0] = newActor.creatureStats.mDynamic[0];
                cellActor->creatureStats.mDynamic[1] = newActor.creatureStats.mDynamic[1];
                cellActor->creatureStats.mDynamic[2] = newActor.creatureStats.mDynamic[2];
                break;
            }
        }
        else
            cellActorList.baseActors.push_back(newActor);
    }

    cellActorList.count = cellActorList.baseActors.size();
}


void Cell::readActorAI(const mwmp::BaseActorList *newActorList)
{
    for (const mwmp::BaseActor& newActor : newActorList->baseActors)
        setActorAI(newActor);
}

bool Cell::getActorAI(int refNum, int mpNum, mwmp::BaseActor& actor) const
{
    for (const mwmp::BaseActor& cachedActor : cellActorAIList.baseActors)
    {
        if (static_cast<int>(cachedActor.refNum) == refNum && static_cast<int>(cachedActor.mpNum) == mpNum)
        {
            actor = cachedActor;
            return true;
        }
    }
    return false;
}

void Cell::setActorAI(const mwmp::BaseActor& actor)
{
    for (mwmp::BaseActor& cachedActor : cellActorAIList.baseActors)
    {
        if (cachedActor.refNum == actor.refNum && cachedActor.mpNum == actor.mpNum)
        {
            cachedActor = actor;
            cellActorAIList.count = static_cast<unsigned int>(cellActorAIList.baseActors.size());
            return;
        }
    }

    cellActorAIList.baseActors.push_back(actor);
    cellActorAIList.count = static_cast<unsigned int>(cellActorAIList.baseActors.size());
}

void Cell::removeActorAI(int refNum, int mpNum)
{
    for (auto it = cellActorAIList.baseActors.begin(); it != cellActorAIList.baseActors.end();)
    {
        if (static_cast<int>(it->refNum) == refNum && static_cast<int>(it->mpNum) == mpNum)
            it = cellActorAIList.baseActors.erase(it);
        else
            ++it;
    }
    cellActorAIList.count = static_cast<unsigned int>(cellActorAIList.baseActors.size());
}

bool Cell::containsActor(int refNum, int mpNum)
{
    for (unsigned int i = 0; i < cellActorList.baseActors.size(); i++)
    {
        mwmp::BaseActor actor = cellActorList.baseActors.at(i);

        if (actor.refNum == refNum && actor.mpNum == mpNum)
            return true;
    }
    return false;
}

mwmp::BaseActor *Cell::getActor(int refNum, int mpNum)
{
    for (unsigned int i = 0; i < cellActorList.baseActors.size(); i++)
    {
        mwmp::BaseActor *actor = &cellActorList.baseActors.at(i);

        if (actor->refNum == refNum && actor->mpNum == mpNum)
            return actor;
    }
    return 0;
}

void Cell::removeActors(const mwmp::BaseActorList *newActorList)
{
    for (std::vector<mwmp::BaseActor>::iterator it = cellActorList.baseActors.begin(); it != cellActorList.baseActors.end();)
    {
        int refNum = (*it).refNum;
        int mpNum = (*it).mpNum;

        bool foundActor = false;

        for (unsigned int i = 0; i < newActorList->count; i++)
        {
            mwmp::BaseActor newActor = newActorList->baseActors.at(i);

            if (newActor.refNum == refNum && newActor.mpNum == mpNum)
            {
                removeActorAI(refNum, mpNum);
                it = cellActorList.baseActors.erase(it);
                foundActor = true;
                break;
            }
        }

        if (!foundActor)
            it++;
    }

    cellActorList.count = cellActorList.baseActors.size();
}

RakNet::RakNetGUID *Cell::getAuthority()
{
    return &authorityGuid;
}

void Cell::setAuthority(const RakNet::RakNetGUID& guid)
{
    authorityGuid = guid;
}

mwmp::BaseActorList *Cell::getActorList()
{
    return &cellActorList;
}

mwmp::BaseActorList *Cell::getActorAIList()
{
    return &cellActorAIList;
}

Cell::TPlayers Cell::getPlayers() const
{
    return players;
}

void Cell::sendToLoaded(mwmp::ActorPacket *actorPacket, mwmp::BaseActorList *baseActorList) const
{
    if (players.empty())
        return;

    std::list <Player*> plList;

    for (auto pl : players)
    {
        if (pl != nullptr && !pl->npc.mName.empty())
            plList.push_back(pl);
    }

    plList.sort();
    plList.unique();

    for (auto pl : plList)
    {
        if (pl->guid == baseActorList->guid) continue;

        actorPacket->setActorList(baseActorList);

        // Send the packet to this eligible guid
        actorPacket->Send(pl->guid);
    }
}

void Cell::sendCachedActorAI(const RakNet::RakNetGUID& authority) const
{
    if (cellActorAIList.baseActors.empty())
        return;

    mwmp::BaseActorList aiSnapshot = cellActorAIList;
    aiSnapshot.guid = authority;
    aiSnapshot.cell = cell;
    aiSnapshot.count = static_cast<unsigned int>(aiSnapshot.baseActors.size());

    mwmp::ActorPacket *actorPacket = mwmp::Networking::get().getActorPacketController()->GetPacket(ID_ACTOR_AI);
    actorPacket->setActorList(&aiSnapshot);

    // Always send directly to the authority first, then to the rest of the
    // loaded-cell interest set. The duplicate filter in sendToLoaded skips the
    // authority because aiSnapshot.guid is set to that peer.
    actorPacket->Send(authority);
    sendToLoaded(actorPacket, &aiSnapshot);
}

void Cell::sendToLoaded(mwmp::ObjectPacket *objectPacket, mwmp::BaseObjectList *baseObjectList) const
{
    if (players.empty())
        return;

    std::list <Player*> plList;

    for (auto pl : players)
    {
        if (pl != nullptr && !pl->npc.mName.empty())
            plList.push_back(pl);
    }

    plList.sort();
    plList.unique();

    for (auto pl : plList)
    {
        if (pl->guid == baseObjectList->guid) continue;

        objectPacket->setObjectList(baseObjectList);

        // Send the packet to this eligible guid
        objectPacket->Send(pl->guid);
    }
}

std::string Cell::getShortDescription() const
{
    return cell.getShortDescription();
}
