#include "/mnt/data/x036_work/apps/openmw/mwmp/ServerQuestRegistry.hpp"
#include <cassert>
#include <iostream>

int main()
{
    auto& r = mwmp::ServerQuestRegistry::get();
    mwmp::ServerQuestResponse response;
    r.handleTransport("CLEAR");
    r.handleTransport("QUEST\tarena_caius_drink\tcaius cosades\tBalmora, Caius Cosades%27 House\tarena_caius_drink_topic\t%D0%BD%D0%B5%D0%BC%D0%BD%D0%BE%D0%B3%D0%BE %D0%B2%D1%8B%D0%BF%D0%B8%D0%B2%D0%BA%D0%B8\tactive\t10\tbring drink");
    auto topics = r.getTopics("Caius Cosades", "Balmora, Caius Cosades' House");
    assert(topics.size() == 1);
    assert(topics[0].token() == "@ArenaQuest:arena_caius_drink:arena_caius_drink_topic");
    auto ev = r.handleTransport("RESPONSE\tarena_caius_drink\tarena_caius_drink_topic\tcaius cosades\tBalmora, Caius Cosades%27 House\tdone\tcompleted\t20", &response);
    assert(ev == mwmp::ServerQuestRegistry::TransportEvent::Response);
    assert(response.valid && response.stage == 20 && response.text == "done");
    std::cout << "X036 registry harness: OK\n";
}
