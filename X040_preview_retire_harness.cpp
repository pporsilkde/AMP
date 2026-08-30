// X040 logic harness: models the retire-queue lifetime rule without OSG.
// Verifies that a preview graph destroyed on frame N is never released before
// frame N+3, and that a forced drain releases everything.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <mutex>
#include <vector>

namespace {
struct RetiredPreview { int id; unsigned int mRetiredFrame; };
std::mutex m; std::vector<RetiredPreview> q;
std::atomic<unsigned int> sCurrentPreviewFrame{0};
const unsigned int sRetireFrameMargin = 3;
std::vector<std::pair<int,unsigned int>> released; // id, frame released
}

void collect(unsigned int currentFrame, bool force=false)
{
    sCurrentPreviewFrame.store(currentFrame);
    std::vector<RetiredPreview> ready;
    { std::lock_guard<std::mutex> l(m);
      if (q.empty()) return;
      for (std::size_t i=q.size(); i>0; --i) {
        RetiredPreview& c=q[i-1];
        bool expired = currentFrame >= c.mRetiredFrame + sRetireFrameMargin;
        if (!force && !expired) continue;
        ready.push_back(c); q.erase(q.begin()+(i-1));
      } }
    for (auto& r: ready) released.push_back({r.id, currentFrame});
}
void retire(int id)
{ std::lock_guard<std::mutex> l(m); q.push_back({id, sCurrentPreviewFrame.load()}); }

int main()
{
    // Reproduce the observed CharGen sequence: a preview destroyed on almost
    // every frame while the renderer is a few frames behind.
    for (unsigned int f=1; f<=12; ++f) { collect(f); if (f==2||f==3||f==4||f==5) retire((int)f); }
    for (auto& r: released) {
        unsigned int retiredOn = (unsigned int)r.first;
        printf("preview %d retired on frame %u, released on frame %u\n", r.first, retiredOn, r.second);
        assert(r.second >= retiredOn + sRetireFrameMargin);
    }
    assert(released.size()==4);

    // Shutdown path: anything still queued must go, regardless of frame number.
    released.clear(); collect(100); retire(99); collect(0, true);
    assert(released.size()==1 && released[0].first==99);
    printf("forced drain released preview 99\n");

    // Empty queue must not fabricate work.
    released.clear(); collect(200); assert(released.empty());
    printf("ALL OK\n");
    return 0;
}
