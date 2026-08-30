#include <cassert>
#include <iostream>
#include "/mnt/data/x039_work/apps/openmw/mwmp/ServerQuestRegistry.cpp"
int main() {
    using namespace mwmp;
    auto& r=ServerQuestRegistry::get();
    assert(r.handleTransport("EDITOR_CLEAR") == ServerQuestRegistry::TransportEvent::None);
    r.handleTransport("EDITOR_META\t1\t1\tarena_test\tReady");
    r.handleTransport("EDITOR_QUEST\tarena_test\tTest Quest\tdraft\tadmin\tpersonal\t3\tcaius cosades\tBalmora%2C Caius Cosades%27 House\t0-123\t10\tBring me something");
    r.handleTransport("EDITOR_TOPIC\tarena_test\ttopic_1\tgreen topic\t1");
    r.handleTransport("EDITOR_STAGE\tarena_test\t10\tJournal entry\tNPC response\t0\t0");
    r.handleTransport("EDITOR_CHOICE\tarena_test\tstage\t10\tfinish\tI have it\tadvance\t1\t20");
    r.handleTransport("EDITOR_REQ\tarena_test\tstageChoice\t10\tfinish\t1\titem\t%3E%3D\t1\tpotion_mazte_01\t\t");
    r.handleTransport("EDITOR_REWARD\tarena_test\t10\t1\tgold\t75\t\t\t\t\t");
    r.handleTransport("EDITOR_VALID\tarena_test\tW\tExample warning");
    assert(r.handleTransport("EDITOR_END") == ServerQuestRegistry::TransportEvent::EditorSyncComplete);
    const auto& m=r.getEditorModel();
    assert(m.available && m.canPublish && m.isAdmin && m.selectedQuestId=="arena_test");
    assert(m.quests.size()==1);
    const auto& q=m.quests[0];
    assert(q.topics.size()==1 && q.stages.size()==1);
    assert(q.stages[0].choices.size()==1);
    assert(q.stages[0].choices[0].requirements.size()==1);
    assert(q.stages[0].rewards.size()==1);
    assert(q.validationWarnings.size()==1);
    std::cout << "X039 registry/editor transport: OK\n";
}
