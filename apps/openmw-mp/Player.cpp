#include "Player.hpp"
#include "Networking.hpp"
#include "CoreArenaMPSecurity.hpp"

TPlayers Players::players;
TSlots Players::slots;

void Players::deletePlayer(RakNet::RakNetGUID guid)
{
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Deleting player with guid %lu", guid.g);

    if (players[guid] != 0)
    {
        CellController::get()->deletePlayer(players[guid]);

        LOG_APPEND(TimedLog::LOG_INFO, "- Emptying slot %i", players[guid]->getId());

        slots[players[guid]->getId()] = 0;
        mwmp::CoreArenaMPSecurity::ForgetPlayer(*players[guid]);
        delete players[guid];
        players.erase(guid);
    }
}

void Players::newPlayer(RakNet::RakNetGUID guid)
{
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Creating new player with guid %lu", guid.g);

    players[guid] = new Player(guid);
    players[guid]->cell.blank();
    players[guid]->npc.blank();
    players[guid]->npcStats.blank();
    players[guid]->creatureStats.blank();
    players[guid]->charClass.blank();
    players[guid]->scale = 1;
    players[guid]->isWerewolf = false;

    for (unsigned int i = 0; i < mwmp::Networking::get().maxConnections(); i++)
    {
        if (slots[i] == 0)
        {
            LOG_APPEND(TimedLog::LOG_INFO, "- Storing in slot %i", i);

            slots[i] = players[guid];
            slots[i]->setId(i);
            break;
        }
    }
}

Player *Players::getPlayer(RakNet::RakNetGUID guid)
{
    auto it = players.find(guid);
    if (it == players.end())
        return nullptr;
    return it->second;
}

TPlayers *Players::getPlayers()
{
    return &players;
}

unsigned short Players::getLastPlayerId()
{
    return slots.rbegin()->first;
}

Player::Player(RakNet::RakNetGUID guid) : BasePlayer(guid)
{
    handshakeCounter = 0;
    loadState = NOTLOADED;
    // Do not let remote clients instantiate the temporary local ESM `player`
    // record. Login/CharGen calls Networking::revealPlayer() only after the
    // final race, head, hair, sex and model have been restored.
    visibleToOthers = false;
    appearanceAuthoritative = false;
}

Player::~Player()
{

}

unsigned short Player::getId()
{
    return id;
}

void Player::setId(unsigned short id)
{
    this->id = id;
}

bool Player::isHandshaked()
{
    return handshakeCounter == std::numeric_limits<int>::max();
}

void Player::setHandshake()
{
    handshakeCounter = std::numeric_limits<int>::max();
}

void Player::incrementHandshakeAttempts()
{
    handshakeCounter++;
}

int Player::getHandshakeAttempts()
{
    return handshakeCounter;
}


void Player::setLoadState(int state)
{
    loadState = state;
}

int Player::getLoadState()
{
    return loadState;
}

void Player::setVisibleToOthers(bool state)
{
    visibleToOthers = state;
}

bool Player::isVisibleToOthers() const
{
    return visibleToOthers;
}

void Player::setAppearanceAuthoritative(bool state)
{
    appearanceAuthoritative = state;
}

bool Player::isAppearanceAuthoritative() const
{
    return appearanceAuthoritative;
}

Player *Players::getPlayer(unsigned short id)
{
    auto it = slots.find(id);
    if (it == slots.end())
        return nullptr;
    return it->second;
}

CellController::TContainer *Player::getCells()
{
    return &cells;
}

void Player::sendToLoaded(mwmp::PlayerPacket *myPacket)
{
    if (!visibleToOthers)
        return;

    std::list <Player*> plList;

    for (auto cell : cells)
        for (auto pl : *cell)
            plList.push_back(pl);

    plList.sort();
    plList.unique();

    for (auto pl : plList)
    {
        if (pl == this || !pl->isVisibleToOthers()) continue;
        myPacket->setPlayer(this);
        myPacket->Send(pl->guid);
    }
}

void Player::forEachLoaded(std::function<void(Player *pl, Player *other)> func)
{
    if (!visibleToOthers)
        return;

    std::list <Player*> plList;

    for (auto cell : cells)
    {
        for (auto pl : *cell)
        {
            if (pl != nullptr && !pl->npc.mName.empty())
                plList.push_back(pl);
        }
    }

    plList.sort();
    plList.unique();

    for (auto pl : plList)
    {
        if (pl == this || !pl->isVisibleToOthers()) continue;
        func(this, pl);
    }
}

bool Players::doesPlayerExist(RakNet::RakNetGUID guid)
{
    return players.find(guid) != players.end();
}
