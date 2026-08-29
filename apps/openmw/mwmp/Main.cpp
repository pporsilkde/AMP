#include <cstdlib>
/*
    Start of AMP addition
*/
#include <unordered_set>

#include <components/misc/stringops.hpp>
/*
    End of AMP addition
*/

#include <components/openmw-mp/Utils.hpp>
#include <components/openmw-mp/TimedLog.hpp>
#include <components/openmw-mp/Version.hpp>

#include <components/esm/esmwriter.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/files/escape.hpp>

#include "../mwbase/environment.hpp"

#include "../mwclass/creature.hpp"
#include "../mwclass/npc.hpp"

#include "../mwdialogue/dialoguemanagerimp.hpp"

#include "../mwgui/windowmanagerimp.hpp"

#include "../mwinput/inputmanagerimp.hpp"

#include "../mwmechanics/aitravel.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/mechanicsmanagerimp.hpp"
#include "../mwmechanics/spellcasting.hpp"

#include "../mwscript/scriptmanagerimp.hpp"

#include "../mwstate/statemanagerimp.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldimp.hpp"

#include "Main.hpp"
#include "Networking.hpp"
#include "LocalSystem.hpp"
#include "LocalPlayer.hpp"
#include "DedicatedPlayer.hpp"
#include "PlayerList.hpp"
#include "GUIController.hpp"
#include "CellController.hpp"
#include "MechanicsHelper.hpp"
#include "RecordHelper.hpp"
/*
    Start of AMP addition
*/
#include "ScriptController.hpp"
/*
    End of AMP addition
*/

using namespace mwmp;

Main *Main::pMain = 0;
std::string Main::address = "";
std::string Main::serverPassword = TES3MP_DEFAULT_PASSW;
std::string Main::resourceDir = "";

std::string Main::getResDir()
{
    return resourceDir;
}


std::string loadSettings(Settings::Manager& settings)
{
    Files::ConfigurationManager mCfgMgr;
    // Create the settings manager and load default settings file
    const std::string localdefault = (mCfgMgr.getLocalPath() / "tes3mp-client-default.cfg").string();
    const std::string globaldefault = (mCfgMgr.getGlobalPath() / "tes3mp-client-default.cfg").string();

    // prefer local
    if (boost::filesystem::exists(localdefault))
        settings.loadDefault(localdefault, false);
    else if (boost::filesystem::exists(globaldefault))
        settings.loadDefault(globaldefault, false);
    else
        throw std::runtime_error ("No default settings file found! Make sure the file \"tes3mp-client-default.cfg\" was properly installed.");

    // load user settings if they exist
    const std::string settingspath = (mCfgMgr.getUserConfigPath() / "tes3mp-client.cfg").string();
    if (boost::filesystem::exists(settingspath))
        settings.loadUser(settingspath);

    return settingspath;
}

Main::Main()
{
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "tes3mp started");
    mNetworking = new Networking();
    mLocalSystem = new LocalSystem();
    mLocalPlayer = new LocalPlayer();
    mGUIController = new GUIController();
    mCellController = new CellController();

    // X031: never fall back to the public/vanilla TES3MP server.
    server = "localhost";
    port = 25565;
}

Main::~Main()
{
    // Flush the final coalesced script-local changes while the connection and
    // ObjectList still exist. A hard connection loss cannot be recovered here,
    // but a normal quit can.
    if (mNetworking != nullptr && mLocalPlayer != nullptr && mNetworking->isConnected()
        && mLocalPlayer->isLoggedIn())
        ScriptController::flushQueuedLocalChanges(0.0f, true);

    // X022: DedicatedPlayers own MWWorld::Ptrs. They must be removed before the
    // cell/GUI controllers and, most importantly, before Engine destroys World.
    // cleanUp() also calls deleteReference() so no stale Ptr reaches a destructor.
    PlayerList::cleanUp();

    delete mNetworking;
    mNetworking = nullptr;
    delete mLocalSystem;
    mLocalSystem = nullptr;
    delete mLocalPlayer;
    mLocalPlayer = nullptr;
    delete mCellController;
    mCellController = nullptr;
    delete mGUIController;
    mGUIController = nullptr;

    // Keep this marker truthful: reaching it now means multiplayer teardown
    // actually completed, instead of merely having started.
    LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "tes3mp stopped");
}

void Main::optionsDesc(boost::program_options::options_description *desc)
{
    namespace bpo = boost::program_options;
    desc->add_options()
            ("connect", bpo::value<std::string>()->default_value(""),
                        "connect to server (e.g. --connect=127.0.0.1:25565)")
            ("password", bpo::value<std::string>()->default_value(TES3MP_DEFAULT_PASSW),
                        "сonnect to a secured server. (e.g. --password=AnyPassword");
}

void Main::configure(const boost::program_options::variables_map &variables)
{
    Main::address = variables["connect"].as<std::string>();
    Main::serverPassword = variables["password"].as<std::string>();
    resourceDir = variables["resources"].as<Files::EscapePath>().mPath.string();
}

bool Main::init(std::vector<std::string> &content, std::vector<std::string> &groundcover, Files::Collections &collections)
{
    assert(!pMain);
    pMain = new Main();

    Settings::Manager manager;
    loadSettings(manager);

    int logLevel = manager.getInt("logLevel", "General");
    TimedLog::SetLevel(logLevel);
    if (address.empty())
    {
        pMain->server = manager.getString("destinationAddress", "General");
        pMain->port = (unsigned short) manager.getInt("port", "General");

        serverPassword = manager.getString("password", "General");
        if (serverPassword.empty())
            serverPassword = TES3MP_DEFAULT_PASSW;
    }
    else
    {
        size_t delimPos = address.find(':');
        pMain->server = address.substr(0, delimPos);
        pMain->port = atoi(address.substr(delimPos + 1).c_str());
    }
    get().mLocalSystem->serverPassword = serverPassword;

    pMain->mNetworking->connect(pMain->server, pMain->port, content, groundcover, collections);

    return pMain->mNetworking->isConnected();
}

void Main::postInit()
{
    pMain->mGUIController->setupChat();

    const MWBase::Environment &environment = MWBase::Environment::get();
    environment.getStateManager()->newGame(true);
    MWBase::Environment::get().getMechanicsManager()->toggleAI();
    RecordHelper::createPlaceholderInteriorCell();
}

bool Main::isInitialized()
{
    return pMain != nullptr;
}

void Main::destroy()
{
    assert(pMain);

    delete pMain;
    pMain = 0;
}

void Main::frame(float dt)
{
    get().getNetworking()->update();

    PlayerList::update(dt);
    get().getCellController()->updateDedicated(dt);
    get().updateWorld(dt);

    /*
        Start of AMP addition

        Send whatever script local changes have accumulated since the last flush
    */
    ScriptController::flushQueuedLocalChanges(dt);
    /*
        End of AMP addition
    */

    get().getGUIController()->update(dt);
}

void Main::updateWorld(float dt) const
{

    if (!mLocalPlayer->processCharGen())
        return;

    static bool init = true;
    if (init)
    {
        init = false;
        getLocalPlayer()->updateLanguage();
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Sending ID_PLAYER_BASEINFO to server (language %s)",
            getLocalPlayer()->language.c_str());

        mNetworking->getPlayerPacket(ID_PLAYER_BASEINFO)->setPlayer(getLocalPlayer());
        mNetworking->getPlayerPacket(ID_LOADED)->setPlayer(getLocalPlayer());
        mNetworking->getPlayerPacket(ID_PLAYER_BASEINFO)->Send();
        mNetworking->getPlayerPacket(ID_LOADED)->Send();
        mLocalPlayer->updateStatsDynamic(true);
        get().getGUIController()->setChatVisible(true);
    }
    else
    {
        mLocalPlayer->update();
        mCellController->updateLocal(false);
    }
}

const Main &Main::get()
{
    return *pMain;
}

Networking *Main::getNetworking() const
{
    return mNetworking;
}

LocalSystem *Main::getLocalSystem() const
{
    return mLocalSystem;
}

LocalPlayer *Main::getLocalPlayer() const
{
    return mLocalPlayer;
}

GUIController *Main::getGUIController() const
{
    return mGUIController;
}

CellController *Main::getCellController() const
{
    return mCellController;
}

/*
    Start of AMP change

    These two used to walk a std::vector of strings with an exact, case sensitive comparison,
    once per script per frame.

    Case sensitivity was an outright bug: script IDs come out of the content files with
    whatever capitalisation the author used, while the server sends the list as written in
    config.synchronizedClientScriptIds, so a mismatch in a single letter silently dropped a
    script out of synchronization with no diagnostic anywhere.

    Both lookups are now case insensitive and backed by a hash set that is rebuilt only when
    the server sends us a new list
*/
namespace
{
    std::unordered_set<std::string> sPacketScriptIds;
    std::unordered_set<std::string> sPacketGlobalIds;
    bool sPacketIdCachesValid = false;

    void rebuildPacketIdCaches(mwmp::BaseWorldstate *worldstate)
    {
        sPacketScriptIds.clear();
        sPacketGlobalIds.clear();

        for (const auto &scriptId : worldstate->synchronizedClientScriptIds)
            sPacketScriptIds.insert(Misc::StringUtils::lowerCase(scriptId));

        for (const auto &globalId : worldstate->synchronizedClientGlobalIds)
            sPacketGlobalIds.insert(Misc::StringUtils::lowerCase(globalId));

        sPacketIdCachesValid = true;
    }
}

void Main::invalidatePacketScriptCache()
{
    sPacketIdCachesValid = false;
}

bool Main::isValidPacketScript(std::string scriptId)
{
    mwmp::BaseWorldstate *worldstate = get().getNetworking()->getWorldstate();

    if (!sPacketIdCachesValid)
        rebuildPacketIdCaches(worldstate);

    return sPacketScriptIds.count(Misc::StringUtils::lowerCase(scriptId)) > 0;
}

bool Main::isValidPacketGlobal(std::string globalId)
{
    mwmp::BaseWorldstate *worldstate = get().getNetworking()->getWorldstate();

    if (!sPacketIdCachesValid)
        rebuildPacketIdCaches(worldstate);

    return sPacketGlobalIds.count(Misc::StringUtils::lowerCase(globalId)) > 0;
}
/*
    End of AMP change
*/
