#include "Player.hpp"
#include "processors/ProcessorInitializer.hpp"
#include <RakPeer.h>
#include <Kbhit.h>

#include <components/misc/stringops.hpp>
#include <components/openmw-mp/NetworkMessages.hpp>
#include <components/openmw-mp/TimedLog.hpp>
#include <components/openmw-mp/Version.hpp>
#include <components/openmw-mp/Packets/PacketPreInit.hpp>

#include <iostream>
#include <algorithm>
#include <Script/Script.hpp>
#include <Script/API/TimerAPI.hpp>
#include <chrono>
#include <thread>
#include <csignal>

#include "Networking.hpp"
#include "MasterClient.hpp"
#include "Cell.hpp"
#include "CellController.hpp"
#include "processors/PlayerProcessor.hpp"
#include "processors/ActorProcessor.hpp"
#include "processors/ObjectProcessor.hpp"
#include "processors/WorldstateProcessor.hpp"

using namespace mwmp;

Networking *Networking::sThis = 0;

static int currentMpNum = 0;
static bool dataFileEnforcementState = true;
static bool scriptErrorIgnoringState = false;
bool killLoop = false;

Networking::Networking(RakNet::RakPeerInterface *peer)
    : startLocation("default"), mclient(nullptr)
{
    sThis = this;
    this->peer = peer;
    players = Players::getPlayers();

    CellController::create();

    systemPacketController = new SystemPacketController(peer);
    playerPacketController = new PlayerPacketController(peer);
    actorPacketController = new ActorPacketController(peer);
    objectPacketController = new ObjectPacketController(peer);
    worldstatePacketController = new WorldstatePacketController(peer);

    // Set send stream
    systemPacketController->SetStream(0, &bsOut);
    playerPacketController->SetStream(0, &bsOut);
    actorPacketController->SetStream(0, &bsOut);
    objectPacketController->SetStream(0, &bsOut);
    worldstatePacketController->SetStream(0, &bsOut);

    running = true;
    exitCode = 0;

    Script::Call<Script::CallbackIdentity("OnServerInit")>();

    serverPassword = TES3MP_DEFAULT_PASSW;

    ProcessorInitializer();
}

Networking::~Networking()
{
    Script::Call<Script::CallbackIdentity("OnServerExit")>(false);

    CellController::destroy();

    sThis = 0;
    delete systemPacketController;
    delete playerPacketController;
    delete actorPacketController;
    delete objectPacketController;
    delete worldstatePacketController;
}

void Networking::setServerPassword(std::string password) noexcept
{
    serverPassword = password.empty() ? TES3MP_DEFAULT_PASSW : password;
}

bool Networking::isPassworded() const
{
    return serverPassword != TES3MP_DEFAULT_PASSW;
}

void Networking::setStartLocation(const std::string& location)
{
    startLocation = location.empty() ? "default" : location;
}

const std::string& Networking::getStartLocation() const
{
    return startLocation;
}

void Networking::processSystemPacket(RakNet::Packet *packet)
{
    Player *player = Players::getPlayer(packet->guid);

    SystemPacket *myPacket = systemPacketController->GetPacket(packet->data[0]);

    if (packet->data[0] == ID_SYSTEM_HANDSHAKE)
    {
        myPacket->setSystem(&baseSystem);
        myPacket->Read();

        if (!myPacket->isPacketValid())
        {
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_ERROR, "Invalid handshake packet from client at %s", packet->systemAddress.ToString());
            kickPlayer(player->guid);
            return;
        }

        if (player->isHandshaked())
        {
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Wrong handshake with client at %s", packet->systemAddress.ToString());
            kickPlayer(player->guid);
            return;
        }

        if (baseSystem.serverPassword != serverPassword)
        {
            if (isPassworded())
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Wrong server password %s used by client at %s",
                    baseSystem.serverPassword.c_str(), packet->systemAddress.ToString());
                kickPlayer(player->guid);
                return;
            }
            else
            {
                LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Client at %s tried to join using password, despite the server not being passworded",
                    packet->systemAddress.ToString());
            }
        }
        player->setHandshake();
        return;
    }
}

void Networking::processPlayerPacket(RakNet::Packet *packet)
{
    Player *player = Players::getPlayer(packet->guid);

    PlayerPacket *myPacket = playerPacketController->GetPacket(packet->data[0]);

    if (!player->isHandshaked())
    {
        player->incrementHandshakeAttempts();
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Have not completed handshake with client at %s", packet->systemAddress.ToString());
        LOG_APPEND(TimedLog::LOG_WARN, "- Attempts so far: %i", player->getHandshakeAttempts());

        if (player->getHandshakeAttempts() > 20)
            kickPlayer(player->guid, false);
        else if (player->getHandshakeAttempts() > 5)
            kickPlayer(player->guid, true);

        return;
    }

    if (packet->data[0] == ID_LOADED)
    {
        player->setLoadState(Player::LOADED);

        unsigned short pid = Players::getPlayer(packet->guid)->getId();
        Script::Call<Script::CallbackIdentity("OnPlayerConnect")>(pid);

        if (player->getLoadState() == Player::KICKED) // kicked inside in OnPlayerConnect
        {
            playerPacketController->GetPacket(ID_USER_DISCONNECTED)->setPlayer(Players::getPlayer(packet->guid));
            playerPacketController->GetPacket(ID_USER_DISCONNECTED)->Send(false);
            Players::deletePlayer(packet->guid);
            return;
        }
    }
    else if (packet->data[0] == ID_PLAYER_BASEINFO)
    {
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Received ID_PLAYER_BASEINFO about %s", player->npc.mName.c_str());

        myPacket->setPlayer(player);
        myPacket->Read();
        player->language = player->language == "RU" ? "RU" : "EN";
        LOG_APPEND(TimedLog::LOG_INFO, "- Client language: %s", player->language.c_str());
        if (player->isVisibleToOthers())
            myPacket->Send(true);
        else
            LOG_APPEND(TimedLog::LOG_INFO, "- Presence is still hidden; BaseInfo kept server-side only");
    }

    if (player->getLoadState() == Player::NOTLOADED)
        return;
    else if (player->getLoadState() == Player::LOADED)
    {
        player->setLoadState(Player::POSTLOADED);
        newPlayer(packet->guid);
        return;
    }


    if (!PlayerProcessor::Process(*packet))
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Unhandled PlayerPacket with identifier %i has arrived", packet->data[0]);

}

void Networking::processActorPacket(RakNet::Packet *packet)
{
    Player *player = Players::getPlayer(packet->guid);

    if (!player->isHandshaked() || player->getLoadState() != Player::POSTLOADED)
        return;

    if (!ActorProcessor::Process(*packet, baseActorList))
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Unhandled ActorPacket with identifier %i has arrived", packet->data[0]);

}

void Networking::processObjectPacket(RakNet::Packet *packet)
{
    Player *player = Players::getPlayer(packet->guid);

    if (!player->isHandshaked() || player->getLoadState() != Player::POSTLOADED)
        return;

    if (!ObjectProcessor::Process(*packet, baseObjectList))
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Unhandled ObjectPacket with identifier %i has arrived", packet->data[0]);

}

void Networking::processWorldstatePacket(RakNet::Packet *packet)
{
    Player *player = Players::getPlayer(packet->guid);

    if (!player->isHandshaked() || player->getLoadState() != Player::POSTLOADED)
        return;

    if (!WorldstateProcessor::Process(*packet, baseWorldstate))
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Unhandled WorldstatePacket with identifier %i has arrived", packet->data[0]);

}

bool Networking::preInit(RakNet::Packet *packet, RakNet::BitStream &bsIn)
{
    if (packet->data[0] != ID_GAME_PREINIT)
    {
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "%s sent wrong first packet (ID_GAME_PREINIT was expected)",
                           packet->systemAddress.ToString());
        peer->CloseConnection(packet->systemAddress, true);
        return false;
    }

    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Received ID_GAME_PREINIT from %s", packet->systemAddress.ToString());
    PacketPreInit::PluginContainer dataFiles;
    PacketPreInit::PluginContainer clientGroundcover;

    PacketPreInit packetPreInit(peer);
    packetPreInit.SetReadStream(&bsIn);
    packetPreInit.setChecksums(&dataFiles);
    packetPreInit.setGroundcoverChecksums(&clientGroundcover);
    packetPreInit.Read();

    if (!packetPreInit.isPacketValid())
    {
        LOG_APPEND(TimedLog::LOG_ERROR, "- Packet was invalid");
        peer->CloseConnection(packet->systemAddress, false);
        return false;
    }

    const bool syncCapable = packetPreInit.supportsManifestSync();
    const uint8_t phase = packetPreInit.getManifestPhase();

    // ArenaMP FIX12: when enforcement is enabled, a new client first receives
    // the authoritative content order and optional groundcover list. No player
    // object is created yet, so this remains entirely before world/login state.
    if (dataFileEnforcementState && syncCapable && phase == PacketPreInit::PHASE_REQUEST)
    {
        LOG_APPEND(TimedLog::LOG_INFO,
            "- Sending launch manifest before hash verification (%d content, %d optional groundcover)",
            static_cast<int>(samples.size()), static_cast<int>(groundcoverSamples.size()));
        RakNet::BitStream manifestStream;
        packetPreInit.SetSendStream(&manifestStream);
        packetPreInit.setChecksums(&samples);
        packetPreInit.setGroundcoverChecksums(&groundcoverSamples);
        packetPreInit.setStartLocation(startLocation);
        packetPreInit.setManifestPhase(PacketPreInit::PHASE_MANIFEST);
        packetPreInit.Send(packet->systemAddress);
        return false;
    }

    bool matches = samples.size() == dataFiles.size();
    if (matches)
    {
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const PacketPreInit::PluginPair& required = samples[i];
            const PacketPreInit::PluginPair& supplied = dataFiles[i];
            const unsigned suppliedHash = supplied.second.empty() ? 0 : supplied.second.front();
            LOG_APPEND(TimedLog::LOG_INFO, "- idx: %i\tchecksum: %X\tfile: %s",
                static_cast<int>(i), suppliedHash, supplied.first.c_str());

            if (!Misc::StringUtils::ciEqual(required.first, supplied.first) || supplied.second.empty())
            {
                matches = false;
                break;
            }

            // An empty required hash list means that any checksum is accepted.
            if (!required.second.empty()
                && std::find(required.second.begin(), required.second.end(), suppliedHash) == required.second.end())
            {
                matches = false;
                break;
            }
        }
    }

    RakNet::BitStream responseStream;
    packetPreInit.SetSendStream(&responseStream);
    packetPreInit.setStartLocation(startLocation);

    if (dataFileEnforcementState && !matches)
    {
        LOG_APPEND(TimedLog::LOG_INFO, "- Client was not allowed to connect due to incompatible data files");
        packetPreInit.setChecksums(&samples);
        packetPreInit.setGroundcoverChecksums(&groundcoverSamples);
        if (syncCapable)
            packetPreInit.setManifestPhase(PacketPreInit::PHASE_REJECTED);
        packetPreInit.Send(packet->systemAddress);
        peer->CloseConnection(packet->systemAddress, true);
        return false;
    }

    LOG_APPEND(TimedLog::LOG_INFO, "- Client was allowed to connect");
    PacketPreInit::PluginContainer empty;
    packetPreInit.setChecksums(&empty);
    packetPreInit.setGroundcoverChecksums(&empty);
    if (syncCapable)
        packetPreInit.setManifestPhase(PacketPreInit::PHASE_ACCEPTED);
    packetPreInit.Send(packet->systemAddress);

    Players::newPlayer(packet->guid);
    systemPacketController->SetStream(&bsIn, nullptr);
    systemPacketController->GetPacket(ID_SYSTEM_HANDSHAKE)->RequestData(packet->guid);
    return true;
}

void Networking::update(RakNet::Packet *packet, RakNet::BitStream &bsIn)
{
    if (systemPacketController->ContainsPacket(packet->data[0]))
    {
        systemPacketController->SetStream(&bsIn, nullptr);
        processSystemPacket(packet);
    }
    else if (playerPacketController->ContainsPacket(packet->data[0]))
    {
        playerPacketController->SetStream(&bsIn, nullptr);
        processPlayerPacket(packet);
    }
    else if (actorPacketController->ContainsPacket(packet->data[0]))
    {
        actorPacketController->SetStream(&bsIn, 0);
        processActorPacket(packet);
    }
    else if (objectPacketController->ContainsPacket(packet->data[0]))
    {
        objectPacketController->SetStream(&bsIn, 0);
        processObjectPacket(packet);
    }
    else if (worldstatePacketController->ContainsPacket(packet->data[0]))
    {
        worldstatePacketController->SetStream(&bsIn, 0);
        processWorldstatePacket(packet);
    }
    else
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Unhandled RakNet packet with identifier %i has arrived", packet->data[0]);
}

void Networking::newPlayer(RakNet::RakNetGUID guid)
{
    playerPacketController->GetPacket(ID_PLAYER_BASEINFO)->RequestData(guid);
    playerPacketController->GetPacket(ID_PLAYER_STATS_DYNAMIC)->RequestData(guid);
    playerPacketController->GetPacket(ID_PLAYER_POSITION)->RequestData(guid);
    playerPacketController->GetPacket(ID_PLAYER_CELL_CHANGE)->RequestData(guid);
    playerPacketController->GetPacket(ID_PLAYER_EQUIPMENT)->RequestData(guid);

    // ArenaMP presence gate: do not publish already-visible players to a client
    // while it is still authenticating or running CharGen. DedicatedPlayer objects
    // created at this stage used to be based on the local ESM "player" template and
    // could keep that Dunmer body even after the final BaseInfo arrived.
    // revealPlayer() performs a bidirectional authoritative snapshot exchange once
    // the client is actually ready to exist in the multiplayer world.
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO,
        "Deferring remote player snapshots for %lu until authentication/CharGen is complete", guid.g);
}

void Networking::revealPlayer(Player* player)
{
    if (player == nullptr || player->isVisibleToOthers())
        return;

    player->setVisibleToOthers(true);
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Revealing authenticated player %s (%i) to other clients",
        player->npc.mName.c_str(), player->getId());
    LOG_APPEND(TimedLog::LOG_INFO, "- Appearance snapshot: race=%s head=%s hair=%s model=%s flags=%u",
        player->npc.mRace.c_str(), player->npc.mHead.c_str(), player->npc.mHair.c_str(),
        player->npc.mModel.c_str(), static_cast<unsigned int>(player->npc.mFlags));

    const unsigned char packetIds[] = {
        ID_PLAYER_BASEINFO,
        ID_PLAYER_SHAPESHIFT,
        ID_PLAYER_STATS_DYNAMIC,
        ID_PLAYER_ATTRIBUTE,
        ID_PLAYER_SKILL,
        ID_PLAYER_POSITION,
        ID_PLAYER_CELL_CHANGE,
        ID_PLAYER_EQUIPMENT
    };

    const auto sendSnapshot = [this, &packetIds](Player* subject, RakNet::RakNetGUID destination)
    {
        if (subject == nullptr)
            return;

        for (unsigned char packetId : packetIds)
        {
            PlayerPacket* packet = playerPacketController->GetPacket(packetId);
            packet->setPlayer(subject);
            packet->Send(destination);
        }
    };

    // Exchange complete snapshots in BOTH directions. This is deliberately done
    // only after the joining player has finished authentication/CharGen, so neither
    // side ever has to instantiate a temporary default-Dunmer representation.
    for (const auto& entry : *players)
    {
        Player* other = entry.second;
        if (other == nullptr || other == player || !other->isVisibleToOthers()
            || other->getLoadState() != Player::POSTLOADED)
            continue;

        sendSnapshot(player, other->guid);
        sendSnapshot(other, player->guid);

        LOG_APPEND(TimedLog::LOG_INFO,
            "- Exchanged appearance snapshots: %s <-> %s",
            player->npc.mName.c_str(), other->npc.mName.c_str());
    }
}

void Networking::disconnectPlayer(RakNet::RakNetGUID guid)
{
    Player *player = Players::getPlayer(guid);
    if (!player)
        return;
    Script::Call<Script::CallbackIdentity("OnPlayerDisconnect")>(player->getId());

    if (player->isVisibleToOthers())
    {
        playerPacketController->GetPacket(ID_USER_DISCONNECTED)->setPlayer(player);
        playerPacketController->GetPacket(ID_USER_DISCONNECTED)->Send(true);
    }
    Players::deletePlayer(guid);
}

PlayerPacketController *Networking::getPlayerPacketController() const
{
    return playerPacketController;
}

ActorPacketController *Networking::getActorPacketController() const
{
    return actorPacketController;
}

ObjectPacketController *Networking::getObjectPacketController() const
{
    return objectPacketController;
}

WorldstatePacketController *Networking::getWorldstatePacketController() const
{
    return worldstatePacketController;
}

BaseActorList *Networking::getReceivedActorList()
{
    return &baseActorList;
}

BaseObjectList *Networking::getReceivedObjectList()
{
    return &baseObjectList;
}

BaseWorldstate *Networking::getReceivedWorldstate()
{
    return &baseWorldstate;
}

int Networking::getCurrentMpNum()
{
    return currentMpNum;
}

void Networking::setCurrentMpNum(int value)
{
    currentMpNum = value;
}

int Networking::incrementMpNum()
{
    currentMpNum++;
    Script::Call<Script::CallbackIdentity("OnMpNumIncrement")>(currentMpNum);
    return currentMpNum;
}

bool Networking::getDataFileEnforcementState()
{
    return dataFileEnforcementState;
}

void Networking::setDataFileEnforcementState(bool state)
{
    dataFileEnforcementState = state;
}

bool Networking::getScriptErrorIgnoringState()
{
    return scriptErrorIgnoringState;
}

void Networking::setScriptErrorIgnoringState(bool state)
{
    scriptErrorIgnoringState = state;
}

const Networking &Networking::get()
{
    return *sThis;
}


Networking *Networking::getPtr()
{
    return sThis;
}

RakNet::SystemAddress Networking::getSystemAddress(RakNet::RakNetGUID guid)
{
    return peer->GetSystemAddressFromGuid(guid);
}

void Networking::stopServer(int code)
{
    running = false;
    exitCode = code;
}

void signalHandler(int signum) 
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    //15 is SIGTERM(Normal OS stop call), 2 is SIGINT(Ctrl+C)
    if(signum == 15 || signum == 2)
    {
        killLoop = true;
    }
}

int Networking::mainLoop()
{
    RakNet::Packet *packet;

#ifndef _WIN32
    struct sigaction sigIntHandler;
    
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
#endif
    
    while (running && !killLoop)
    {
#ifndef _WIN32
        sigaction(SIGTERM, &sigIntHandler, NULL);
        sigaction(SIGINT, &sigIntHandler, NULL);
#endif
        if (kbhit() && getch() == '\n')
            break;
        for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
        {
            if (getMasterClient()->Process(packet))
                continue;

            switch (packet->data[0])
            {
                case ID_REMOTE_DISCONNECTION_NOTIFICATION:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Client at %s has disconnected", packet->systemAddress.ToString());
                    break;
                case ID_REMOTE_CONNECTION_LOST:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Client at %s has lost connection", packet->systemAddress.ToString());
                    break;
                case ID_REMOTE_NEW_INCOMING_CONNECTION:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Client at %s has connected", packet->systemAddress.ToString());
                    break;
                case ID_CONNECTION_REQUEST_ACCEPTED:    // client to server
                {
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Our connection request has been accepted");
                    break;
                }
                case ID_NEW_INCOMING_CONNECTION:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "A connection is incoming from %s", packet->systemAddress.ToString());
                    break;
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "The server is full");
                    break;
                case ID_DISCONNECTION_NOTIFICATION:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN,  "Client at %s has disconnected", packet->systemAddress.ToString());
                    disconnectPlayer(packet->guid);
                    break;
                case ID_CONNECTION_LOST:
                    LOG_MESSAGE_SIMPLE(TimedLog::LOG_WARN, "Client at %s has lost connection", packet->systemAddress.ToString());
                    disconnectPlayer(packet->guid);
                    break;
                case ID_SND_RECEIPT_ACKED:
                case ID_CONNECTED_PING:
                case ID_UNCONNECTED_PING:
                    break;
                default:
                {
                    RakNet::BitStream bsIn(&packet->data[1], packet->length, false);
                    bsIn.IgnoreBytes((unsigned int) RakNet::RakNetGUID::size()); // Ignore GUID from received packet


                    if (Players::doesPlayerExist(packet->guid))
                        update(packet, bsIn);
                    else
                        preInit(packet, bsIn);
                    break;
                }
            }
        }
        TimerAPI::Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    TimerAPI::Terminate();
    return exitCode;
}

void Networking::kickPlayer(RakNet::RakNetGUID guid, bool sendNotification)
{
    peer->CloseConnection(guid, sendNotification);
}

void Networking::banAddress(const char *ipAddress)
{
    peer->AddToBanList(ipAddress);
}

void Networking::unbanAddress(const char *ipAddress)
{
    peer->RemoveFromBanList(ipAddress);
}

unsigned short Networking::numberOfConnections() const
{
    return peer->NumberOfConnections();
}

unsigned int Networking::maxConnections() const
{
    return peer->GetMaximumIncomingConnections();
}

int Networking::getAvgPing(RakNet::AddressOrGUID addr) const
{
    return peer->GetAveragePing(addr);
}

unsigned short Networking::getPort() const
{
    return peer->GetMyBoundAddress().GetPort();
}

MasterClient *Networking::getMasterClient()
{
    return mclient;
}

void Networking::InitQuery(std::string queryAddr, unsigned short queryPort)
{
    mclient = new MasterClient(peer, queryAddr, queryPort);
}

void Networking::postInit()
{
    Script::Call<Script::CallbackIdentity("OnRequestDataFileList")>();
    Script::Call<Script::CallbackIdentity("OnServerPostInit")>();
}

PacketPreInit::PluginContainer &Networking::getSamples()
{
    return samples;
}

PacketPreInit::PluginContainer &Networking::getGroundcoverSamples()
{
    return groundcoverSamples;
}
