#include "QuestItemIndex.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm/defs.hpp>
#include <components/esm/esmreader.hpp>
#include <components/esm/loadcell.hpp>
#include <components/esm/loadcont.hpp>
#include <components/esm/loadcrea.hpp>
#include <components/esm/loaddial.hpp>
#include <components/esm/loadlevlist.hpp>
#include <components/esm/loadnpc.hpp>
#include <components/esm/loadscpt.hpp>
#include <components/misc/stringops.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellref.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace
{
    std::string lower(std::string value)
    {
        Misc::StringUtils::lowerCaseInPlace(value);
        return value;
    }

    bool isItemRecordType(int type)
    {
        switch (type)
        {
            case ESM::REC_ALCH:
            case ESM::REC_APPA:
            case ESM::REC_ARMO:
            case ESM::REC_BOOK:
            case ESM::REC_CLOT:
            case ESM::REC_INGR:
            case ESM::REC_LIGH:
            case ESM::REC_LOCK:
            case ESM::REC_MISC:
            case ESM::REC_PROB:
            case ESM::REC_REPA:
            case ESM::REC_WEAP:
                return true;
            default:
                return false;
        }
    }

    bool hasWord(const std::string& haystack, const std::string& word)
    {
        std::size_t pos = haystack.find(word);
        while (pos != std::string::npos)
        {
            const bool leftOk = pos == 0 || !std::isalnum(static_cast<unsigned char>(haystack[pos - 1]));
            const std::size_t end = pos + word.size();
            const bool rightOk = end >= haystack.size() || !std::isalnum(static_cast<unsigned char>(haystack[end]));
            if (leftOk && rightOk)
                return true;
            pos = haystack.find(word, pos + 1);
        }
        return false;
    }

    std::vector<std::string> collectCommandArguments(const std::string& text, const std::string& command)
    {
        std::vector<std::string> result;
        const std::string source = lower(text);
        std::size_t pos = 0;

        while ((pos = source.find(command, pos)) != std::string::npos)
        {
            const bool leftOk = pos == 0 || !std::isalnum(static_cast<unsigned char>(source[pos - 1]));
            const std::size_t commandEnd = pos + command.size();
            const bool rightOk = commandEnd >= source.size()
                || !std::isalnum(static_cast<unsigned char>(source[commandEnd]));
            if (!leftOk || !rightOk)
            {
                pos = commandEnd;
                continue;
            }

            std::size_t cursor = commandEnd;
            while (cursor < source.size())
            {
                const char c = source[cursor];
                if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == '(' || c == '-')
                    ++cursor;
                else if (c == '>')
                    ++cursor;
                else
                    break;
            }

            if (cursor >= source.size())
                break;

            std::string id;
            if (source[cursor] == '"')
            {
                const std::size_t endQuote = source.find('"', cursor + 1);
                if (endQuote != std::string::npos)
                    id = source.substr(cursor + 1, endQuote - cursor - 1);
                pos = endQuote == std::string::npos ? cursor + 1 : endQuote + 1;
            }
            else
            {
                const std::size_t begin = cursor;
                while (cursor < source.size())
                {
                    const char c = source[cursor];
                    if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ')' || c == ';')
                        break;
                    ++cursor;
                }
                id = source.substr(begin, cursor - begin);
                pos = cursor;
            }

            if (!id.empty() && id.size() < 256)
                result.push_back(lower(id));
        }
        return result;
    }

    std::uint64_t fnv1a64(const std::string& value)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (unsigned char c : value)
        {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    // Dual modular rolling hash. Both moduli are below 2^31, so every partial
    // product stays under 2^38 and is exactly representable as a double. That is
    // what lets the Lua side of the server reproduce this hash bit for bit
    // without an integer or bitop library.
    std::string dualHash(const std::string& value)
    {
        std::uint64_t h1 = 0;
        std::uint64_t h2 = 0;
        for (unsigned char c : value)
        {
            h1 = (h1 * 131ull + c) % 2147483647ull;
            h2 = (h2 * 137ull + c) % 2147483629ull;
        }

        std::ostringstream stream;
        stream << std::hex << std::setw(8) << std::setfill('0') << h1
               << std::setw(8) << std::setfill('0') << h2;
        return stream.str();
    }

    std::string hashId(const char* prefix, const std::string& value)
    {
        std::ostringstream stream;
        stream << prefix << std::hex << std::setw(16) << std::setfill('0') << fnv1a64(value);
        return stream.str();
    }

    std::string itemSignature(const MWWorld::Ptr& item)
    {
        if (item.isEmpty())
            return std::string();

        std::ostringstream stream;
        stream << lower(item.getCellRef().getRefId()) << '|'
               << item.getCellRef().getCharge() << '|'
               << std::fixed << std::setprecision(3) << item.getCellRef().getEnchantmentCharge() << '|'
               << lower(item.getCellRef().getSoul()) << '|'
               << lower(item.getRefData().getPoisonId()) << '|'
               << item.getRefData().getPoisonCharges();
        return stream.str();
    }
}

namespace mwmp
{
    QuestItemIndex& QuestItemIndex::get()
    {
        static QuestItemIndex index;
        return index;
    }

    void QuestItemIndex::addEvidence(const MWWorld::ESMStore& store, const std::string& refId, int score)
    {
        if (refId.empty() || score <= 0)
            return;

        const std::string id = lower(refId);
        if (!isItemRecordType(store.find(id)))
            return;

        Candidate& candidate = mCandidates[id];
        candidate.evidence = std::min(1000, candidate.evidence + score);
    }

    void QuestItemIndex::setEnabled(bool enabled)
    {
        if (mEnabled == enabled)
            return;

        mEnabled = enabled;
        Log(Debug::Info) << "ArenaMP Quest Item Index: oracle mode " << (enabled ? "enabled" : "disabled");
    }

    void QuestItemIndex::build()
    {
        // X013: never build unless the server asked this client to act as an
        // oracle. Classification is server side now, so an ordinary session must
        // not pay for a full ESM scan, and definitely not lazily in the middle of
        // gameplay the first time somebody opens a container.
        if (!mEnabled || mBuilt || mBuilding)
            return;

        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (world == nullptr)
            return;

        // Guard against reentry while still allowing a retry after a failure.
        // X012 set mBuilt before doing any work, so a single exception anywhere
        // below froze a half-built index for the rest of the session, silently.
        mBuilding = true;

        // Reset the reentry flag even if a malformed plugin makes the scan throw,
        // so a later call can try again instead of leaving the index dead.
        struct BuildGuard
        {
            bool& flag;
            ~BuildGuard() { flag = false; }
        } buildGuard{mBuilding};

        const MWWorld::ESMStore& store = world->getStore();

        // Dialogue is the strongest signal because Item selects are the format's
        // native representation of "the player must possess this item".
        const MWWorld::Store<ESM::Dialogue>& dialogues = store.get<ESM::Dialogue>();
        for (auto dialogueIt = dialogues.begin(); dialogueIt != dialogues.end(); ++dialogueIt)
        {
            const ESM::Dialogue& dialogue = *dialogueIt;
            for (const ESM::DialInfo& info : dialogue.mInfo)
            {
                bool questContext = dialogue.mType == ESM::Dialogue::Journal || info.mQuestStatus != ESM::DialInfo::QS_None;
                for (const ESM::DialInfo::SelectStruct& select : info.mSelects)
                {
                    if (select.mSelectRule.size() > 5 && select.mSelectRule[1] == '4')
                        questContext = true;
                }

                for (const ESM::DialInfo::SelectStruct& select : info.mSelects)
                {
                    if (select.mSelectRule.size() > 5 && select.mSelectRule[1] == '5')
                        addEvidence(store, select.mSelectRule.substr(5), questContext ? 8 : 5);
                }

                const int requirementScore = questContext ? 7 : 3;
                for (const std::string& id : collectCommandArguments(info.mResultScript, "getitemcount"))
                    addEvidence(store, id, requirementScore);
                for (const std::string& id : collectCommandArguments(info.mResultScript, "removeitem"))
                    addEvidence(store, id, requirementScore);
                for (const std::string& id : collectCommandArguments(info.mResultScript, "hasitemequipped"))
                    addEvidence(store, id, requirementScore);

                // AddItem alone is normally a reward, not a requirement. In a quest
                // result it is still useful as weak corroborating evidence if the same
                // item is referenced elsewhere.
                if (questContext)
                    for (const std::string& id : collectCommandArguments(info.mResultScript, "additem"))
                        addEvidence(store, id, 1);
            }
        }

        // Global/local scripts often implement quest state machines without any
        // dialogue Item select. Give item checks/removals a strong score only when
        // the script itself also manipulates or inspects journal state.
        const MWWorld::Store<ESM::Script>& scripts = store.get<ESM::Script>();
        for (auto scriptIt = scripts.begin(); scriptIt != scripts.end(); ++scriptIt)
        {
            const ESM::Script& script = *scriptIt;
            const std::string source = lower(script.mScriptText);
            const bool questContext = hasWord(source, "getjournalindex") || hasWord(source, "journal");
            const int requirementScore = questContext ? 7 : 2;

            for (const std::string& id : collectCommandArguments(source, "getitemcount"))
                addEvidence(store, id, requirementScore);
            for (const std::string& id : collectCommandArguments(source, "removeitem"))
                addEvidence(store, id, requirementScore);
            for (const std::string& id : collectCommandArguments(source, "hasitemequipped"))
                addEvidence(store, id, requirementScore);
            if (questContext)
                for (const std::string& id : collectCommandArguments(source, "additem"))
                    addEvidence(store, id, 1);
        }

        scanContentSources(store);

        for (const auto& [id, candidate] : mCandidates)
        {
            // The lack of an explicit QuestItem bit means false positives are much
            // worse than false negatives. Require solid quest evidence and prefer
            // items with a small number of physical sources. Very strong evidence
            // gets a slightly wider source allowance for multi-copy quest objects.
            const bool rareSource = candidate.sources > 0 && candidate.sources <= 16;
            const bool moderatelyRareSource = candidate.sources > 0 && candidate.sources <= 32;
            const bool generatedQuestItem = candidate.sources == 0 && candidate.evidence >= 12;

            if ((candidate.evidence >= 7 && rareSource)
                || (candidate.evidence >= 11 && moderatelyRareSource)
                || generatedQuestItem)
            {
                mQuestItems.insert(id);
            }
        }

        finalize();

        Log(Debug::Info) << "ArenaMP Quest Item Index: " << mQuestItems.size()
                         << " phaseable item records from " << mCandidates.size()
                         << " quest-linked candidates, hash " << mIndexHash;
    }

    void QuestItemIndex::finalize()
    {
        mSortedEntries.assign(mQuestItems.begin(), mQuestItems.end());

        // Plain byte order, not a locale collation. The server rehashes the
        // payload in the exact order it arrives, and two clients with identical
        // content must produce identical bytes for their uploads to agree.
        std::sort(mSortedEntries.begin(), mSortedEntries.end());

        std::string payload;
        payload.reserve(mSortedEntries.size() * 24);
        for (const std::string& entry : mSortedEntries)
        {
            if (!payload.empty())
                payload.push_back('\n');
            payload.append(entry);
        }

        mIndexHash = dualHash(payload);
        mBuilding = false;
        mBuilt = true;
    }

    const std::vector<std::string>& QuestItemIndex::getSortedEntries()
    {
        build();
        return mSortedEntries;
    }

    const std::string& QuestItemIndex::getIndexHash()
    {
        build();
        return mIndexHash;
    }

    std::string QuestItemIndex::arenaHash(const std::string& value)
    {
        return dualHash(value);
    }

    std::string QuestItemIndex::getContentKey()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (world == nullptr)
            return std::string();

        std::string payload;
        for (const std::string& file : world->getContentFiles())
        {
            if (!payload.empty())
                payload.push_back('\n');
            payload.append(lower(file));
        }
        return dualHash(payload);
    }

    void QuestItemIndex::scanContentSources(const MWWorld::ESMStore& store)
    {
        if (mCandidates.empty())
            return;

        // Map base container/NPC/creature ids to the candidate quest records they
        // carry. We count a placed carrier as one physical source per candidate.
        std::unordered_map<std::string, std::vector<std::string>> carrierItems;

        auto collectInventory = [&](const std::string& carrierId, const ESM::InventoryList& inventory)
        {
            std::unordered_set<std::string> unique;
            for (const ESM::ContItem& item : inventory.mList)
            {
                const std::string itemId = lower(item.mItem);
                if (mCandidates.find(itemId) != mCandidates.end())
                    unique.insert(itemId);
            }
            if (!unique.empty())
                carrierItems[lower(carrierId)] = std::vector<std::string>(unique.begin(), unique.end());
        };

        for (auto it = store.get<ESM::Container>().begin(); it != store.get<ESM::Container>().end(); ++it)
            collectInventory(it->mId, it->mInventory);
        for (auto it = store.get<ESM::NPC>().begin(); it != store.get<ESM::NPC>().end(); ++it)
            collectInventory(it->mId, it->mInventory);
        for (auto it = store.get<ESM::Creature>().begin(); it != store.get<ESM::Creature>().end(); ++it)
            collectInventory(it->mId, it->mInventory);

        // Anything reachable from an item leveled list is repeatable/random
        // content, not a unique authored quest source. Mark those candidates as
        // common even if there are no direct placed references in cells.
        std::unordered_map<std::string, std::vector<std::string>> leveledEntries;
        for (auto it = store.get<ESM::ItemLevList>().begin(); it != store.get<ESM::ItemLevList>().end(); ++it)
        {
            std::vector<std::string>& entries = leveledEntries[lower(it->mId)];
            for (const ESM::LevelledListBase::LevelItem& entry : it->mList)
                entries.push_back(lower(entry.mId));
        }

        std::function<void(const std::string&, std::unordered_set<std::string>&)> markLeveledCandidates;
        markLeveledCandidates = [&](const std::string& id, std::unordered_set<std::string>& visiting)
        {
            auto candidate = mCandidates.find(id);
            if (candidate != mCandidates.end())
                candidate->second.sources = std::max(candidate->second.sources, 1000);

            auto list = leveledEntries.find(id);
            if (list == leveledEntries.end() || !visiting.insert(id).second)
                return;
            for (const std::string& child : list->second)
                markLeveledCandidates(child, visiting);
            visiting.erase(id);
        };

        for (const auto& entry : leveledEntries)
        {
            std::unordered_set<std::string> visiting;
            markLeveledCandidates(entry.first, visiting);
        }

        auto countRef = [&](const std::string& rawId)
        {
            const std::string id = lower(rawId);
            auto candidate = mCandidates.find(id);
            if (candidate != mCandidates.end())
                candidate->second.sources = std::min(100000, candidate->second.sources + 1);

            auto carrier = carrierItems.find(id);
            if (carrier != carrierItems.end())
            {
                for (const std::string& itemId : carrier->second)
                {
                    auto item = mCandidates.find(itemId);
                    if (item != mCandidates.end())
                        item->second.sources = std::min(100000, item->second.sources + 1);
                }
            }
        };

        ESM::ESMReader reader;
        auto scanCell = [&](const ESM::Cell& cell)
        {
            std::map<ESM::RefNum, std::string> liveRefs;
            for (std::size_t contextIndex = 0; contextIndex < cell.mContextList.size(); ++contextIndex)
            {
                try
                {
                    cell.restore(reader, static_cast<int>(contextIndex));
                    ESM::CellRef ref;
                    ref.mRefNum.mContentFile = ESM::RefNum::RefNum_NoContentFile;
                    bool deleted = false;
                    while (cell.getNextRef(reader, ref, deleted))
                    {
                        if (std::find(cell.mMovedRefs.begin(), cell.mMovedRefs.end(), ref.mRefNum) != cell.mMovedRefs.end())
                            continue;
                        if (deleted)
                            liveRefs.erase(ref.mRefNum);
                        else
                            liveRefs[ref.mRefNum] = ref.mRefID;
                    }
                }
                catch (const std::exception& e)
                {
                    Log(Debug::Warning) << "Quest Item Index skipped refs in " << cell.getShortDescription()
                                        << ": " << e.what();
                }
            }

            for (const auto& [refNum, id] : liveRefs)
                countRef(id);
            for (const auto& leased : cell.mLeasedRefs)
                if (!leased.second)
                    countRef(leased.first.mRefID);
        };

        const MWWorld::Store<ESM::Cell>& cells = store.get<ESM::Cell>();
        for (auto it = cells.intBegin(); it != cells.intEnd(); ++it)
            scanCell(*it);
        for (auto it = cells.extBegin(); it != cells.extEnd(); ++it)
            scanCell(*it);
    }

    bool QuestItemIndex::isQuestItem(const std::string& refId)
    {
        if (!mEnabled)
            return false;

        build();
        return mQuestItems.find(lower(refId)) != mQuestItems.end();
    }

    std::string QuestItemIndex::makeWorldSourceId(const MWWorld::Ptr& ptr)
    {
        if (ptr.isEmpty())
            return std::string();

        // Use the authored multiplayer identity instead of the current position.
        // Quest props can be moved by scripts/placement and NPC/container carriers
        // obviously move all the time; a position based ID would therefore create a
        // fresh personal copy after every move. RefNum + content file + mpNum stays
        // stable for the lifetime of the reference and across normal saves.
        const ESM::RefNum& refNum = ptr.getCellRef().getRefNum();
        const std::string key = lower(ptr.getCellRef().getRefId()) + '|'
            + std::to_string(refNum.mContentFile) + ':' + std::to_string(refNum.mIndex) + ':'
            + std::to_string(ptr.getCellRef().getMpNum());
        return hashId("qiw:", key);
    }

    std::string QuestItemIndex::makeContainerSourceId(const MWWorld::Ptr& container, const MWWorld::Ptr& item)
    {
        if (container.isEmpty() || item.isEmpty())
            return std::string();

        const ESM::RefNum& refNum = container.getCellRef().getRefNum();
        const std::string key = lower(container.getCellRef().getRefId()) + '|'
            + std::to_string(refNum.mContentFile) + ':' + std::to_string(refNum.mIndex) + ':'
            + std::to_string(container.getCellRef().getMpNum()) + '|' + itemSignature(item);
        return hashId("qic:", key);
    }

    std::string QuestItemIndex::makeContainerSourceId(const MWWorld::Ptr& container, const std::string& itemRefId)
    {
        if (container.isEmpty() || itemRefId.empty())
            return std::string();

        const ESM::RefNum& refNum = container.getCellRef().getRefNum();
        const std::string key = lower(container.getCellRef().getRefId()) + '|'
            + std::to_string(refNum.mContentFile) + ':' + std::to_string(refNum.mIndex) + ':'
            + std::to_string(container.getCellRef().getMpNum()) + '|' + lower(itemRefId);
        return hashId("qic:", key);
    }

    std::string QuestItemIndex::makeContainerSourceIdFallback(const std::string& cellDescription,
        const std::string& containerRefId, unsigned int refNum, unsigned int mpNum,
        const MWWorld::Ptr& item)
    {
        const std::string key = lower(cellDescription) + '|' + lower(containerRefId) + '|'
            + std::to_string(refNum) + '-' + std::to_string(mpNum) + '|'
            + (item.isEmpty() ? std::string() : itemSignature(item));
        return hashId("qic:", key);
    }
}
