// Transport integration fixture only. This is NOT a production account backend.
#include "apps/openmw-mp/LinkServer.hpp"
#include <iostream>
#include <cassert>
#include <cstdlib>
int main(int argc, char** argv)
{
    assert(argc == 2);
    mwmp::LinkServer server;
    mwmp::LinkConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = static_cast<unsigned short>(std::atoi(argv[1]));
    config.maxConnectionsPerAddress = 20;
    config.sendRatePerMinute = 100;
    mwmp::LinkCallbacks callbacks;
    server.configure(config, callbacks);
    assert(!server.start(25565)); // An unconfigured backend cannot accept logins.
    callbacks.findAccount = [](const std::string& name, mwmp::LinkAccount& out) {
        if (name != "Alice" && name != "Длинное Русское Имя Игрока") return false;
        out.name = name;
        out.userId = 42;
        out.level = 25;
        return true;
    };
    callbacks.verifyProof = [](const mwmp::LinkAccount&, const std::string& nonce, const std::string& proof) {
        // Exact dummy verifier tests callback dispatch, not cryptography.
        return nonce.size() == ArenaLink::sNonceSize && proof == std::string(32, 'K');
    };
    server.configure(config, callbacks);
    if (!server.start(25565)) return 2;
    std::cout << "ready" << std::endl;
    std::string stop;
    std::getline(std::cin, stop);
    server.stop();
}
