#include "xpleveling.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

#include <components/esm/attr.hpp>
#include <components/esm/loadbook.hpp>
#include <components/esm/loadclas.hpp>
#include <components/esm/loaddial.hpp>
#include <components/esm/loadgmst.hpp>
#include <components/esm/loadinfo.hpp>
#include <components/esm/loadnpc.hpp>
#include <components/esm/loadskil.hpp>
#include <components/misc/stringops.hpp>
#include <components/settings/settings.hpp>

#include "actorutil.hpp"
#include "npcstats.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace
{
    float clampFloat(float value, float low, float high)
    {
        return std::max(low, std::min(high, value));
    }

    float positiveSetting(const char* name, float fallback)
    {
        const float value = Settings::Manager::getFloat(name, "XP Leveling");
        return value > 0.f ? value : fallback;
    }

    float nonNegativeSetting(const char* name)
    {
        return std::max(0.f, Settings::Manager::getFloat(name, "XP Leveling"));
    }

    bool showNotifications()
    {
        return Settings::Manager::getBool("show xp notifications", "XP Leveling");
    }

    std::string formatXp(float amount)
    {
        std::ostringstream stream;
        if (std::fabs(amount - std::round(amount)) < 0.05f)
            stream << static_cast<int>(std::round(amount));
        else
            stream << std::fixed << std::setprecision(1) << amount;
        return stream.str();
    }

    const ESM::Class& getPlayerClass(const MWWorld::Ptr& player)
    {
        const ESM::NPC* npc = player.get<ESM::NPC>()->mBase;
        return *MWBase::Environment::get().getWorld()->getStore().get<ESM::Class>().find(npc->mClass);
    }

    float xpRequirementForLevel(int level)
    {
        const float base = positiveSetting("base xp to level", 1000.f);
        const float perLevel = nonNegativeSetting("xp per level");
        return std::max(1.f, base + std::max(0, level - 1) * perLevel);
    }

    void notifyXp(const std::string& text)
    {
        if (showNotifications() && !text.empty())
            MWBase::Environment::get().getWindowManager()->messageBox(text);
    }

    void completeLevelUp(const MWWorld::Ptr& player)
    {
        MWMechanics::NpcStats& stats = player.getClass().getNpcStats(player);

        const MWWorld::Store<ESM::GameSetting>& gmst
            = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();

        const float endurance = stats.getAttribute(ESM::Attribute::Endurance).getBase();
        const float healthGain = endurance * gmst.find("fLevelUpHealthEndMult")->mValue.getFloat();

        MWMechanics::DynamicStat<float> health(stats.getHealth());
        health.setBase(stats.getHealth().getBase() + healthGain);
        health.setCurrent(std::max(1.f, stats.getHealth().getCurrent() + healthGain));
        stats.setHealth(health);

        stats.setLevel(stats.getLevel() + 1);
        const int points = std::max(0, Settings::Manager::getInt("skill points per level", "XP Leveling"));
        stats.addSkillPoints(points);

        std::ostringstream message;
        message << "Level " << stats.getLevel() << " reached";
        if (points > 0)
            message << ". +" << points << " Skill Points";
        notifyXp(message.str());
    }

    void addExperience(const MWWorld::Ptr& player, float amount, const std::string& notification)
    {
        if (!MWMechanics::XPLeveling::isEnabled() || player.isEmpty() || !player.getClass().isNpc())
            return;

        amount *= positiveSetting("xp gain multiplier", 1.f);
        if (!(amount > 0.f) || !std::isfinite(amount))
            return;

        MWMechanics::NpcStats& stats = player.getClass().getNpcStats(player);
        stats.setExperience(std::max(0.f, stats.getExperience()) + amount);

        if (!notification.empty())
            notifyXp(notification);

        // XP is banked immediately. No vanilla sleep gate or attribute picker is
        // involved; each completed level grants spendable skill points instead.
        for (int guard = 0; guard < 100; ++guard)
        {
            const float required = xpRequirementForLevel(stats.getLevel());
            if (stats.getExperience() + 0.0001f < required)
                break;

            stats.setExperience(std::max(0.f, stats.getExperience() - required));
            completeLevelUp(player);
        }
    }

    bool awardOnce(const MWWorld::Ptr& player, const std::string& key, float amount,
        const std::string& notification)
    {
        if (!MWMechanics::XPLeveling::isEnabled() || key.empty())
            return false;

        MWMechanics::NpcStats& stats = player.getClass().getNpcStats(player);
        if (stats.hasXpRewardKey(key))
            return false;

        if (!(amount > 0.f))
            return false;

        stats.addXpRewardKey(key);
        addExperience(player, amount, notification);
        return true;
    }

    float classAttributeProgress(const ESM::Class& class_, const ESM::Skill& skill, int skillId)
    {
        float progress = nonNegativeSetting("attribute progress misc");
        for (int i = 0; i < 5; ++i)
        {
            if (class_.mData.mSkills[i][0] == skillId)
            {
                progress = nonNegativeSetting("attribute progress minor");
                break;
            }
            if (class_.mData.mSkills[i][1] == skillId)
            {
                progress = nonNegativeSetting("attribute progress major");
                break;
            }
        }

        if (skill.mData.mSpecialization == class_.mData.mSpecialization)
            progress *= positiveSetting("attribute specialization multiplier", 1.25f);

        progress *= positiveSetting("attribute progress multiplier", 1.f);
        return progress;
    }

    void addAttributeProgress(const MWWorld::Ptr& player, int attribute, float amount)
    {
        if (attribute < 0 || attribute >= ESM::Attribute::Length || amount <= 0.f)
            return;

        MWMechanics::NpcStats& stats = player.getClass().getNpcStats(player);
        float progress = stats.getXpAttributeProgress(attribute) + amount;
        float base = stats.getAttribute(attribute).getBase();
        int gained = 0;

        while (progress >= 1.f && base < 100.f)
        {
            progress -= 1.f;
            base += 1.f;
            stats.setAttribute(attribute, std::min(100.f, base));
            ++gained;
        }

        if (base >= 100.f)
            progress = 0.f;
        stats.setXpAttributeProgress(attribute, progress);

        if (gained > 0)
        {
            const std::string& gmstId = ESM::Attribute::sGmstAttributeIds[attribute];
            const std::string name = MWBase::Environment::get().getWindowManager()
                ->getGameSettingString(gmstId, gmstId);
            std::ostringstream message;
            message << name << " increased to " << static_cast<int>(stats.getAttribute(attribute).getBase());
            notifyXp(message.str());
        }
    }

    bool playerResponsibleForKill(const MWWorld::Ptr& attacker)
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        if (attacker == player)
            return true;

        if (attacker.isEmpty())
            return false;

        std::set<MWWorld::Ptr> followers;
        MWBase::Environment::get().getMechanicsManager()->getActorsSidingWith(player, followers);
        return followers.find(attacker) != followers.end();
    }
}

namespace MWMechanics
{
    namespace XPLeveling
    {
        bool isEnabled()
        {
            return Settings::Manager::getBool("enabled", "XP Leveling");
        }

        float getXpForNextLevel(const MWWorld::Ptr& player)
        {
            if (player.isEmpty() || !player.getClass().isActor())
                return xpRequirementForLevel(1);
            return xpRequirementForLevel(std::max(1, player.getClass().getCreatureStats(player).getLevel()));
        }

        int getSkillPointCost(float skillBase)
        {
            if (skillBase < 50.f)
                return 1;
            if (skillBase < 75.f)
                return 2;
            if (skillBase < 90.f)
                return 3;
            return 4;
        }

        void awardSkillUse(const MWWorld::Ptr& player, int skillId, int usageType,
            float extraFactor, const ESM::Class& class_)
        {
            if (!isEnabled() || player.isEmpty() || player != MWMechanics::getPlayer())
                return;
            if (skillId < 0 || skillId >= ESM::Skill::Length || usageType >= 4)
                return;

            const ESM::Skill* skill = MWBase::Environment::get().getWorld()->getStore().get<ESM::Skill>().find(skillId);
            float organicGain = 1.f;
            if (usageType >= 0)
                organicGain = skill->mData.mUseValue[usageType];

            organicGain *= std::max(0.f, extraFactor);
            if (!(organicGain > 0.f))
                return;

            const NpcStats& stats = player.getClass().getNpcStats(player);
            const float requirement = std::max(1.f, stats.getSkillProgressRequirement(skillId, class_));
            const float equivalentXp = positiveSetting("xp per skill level equivalent", 50.f);

            // The use-value already contains the action-specific difficulty signal
            // OpenMW uses for progression (and many ArenaMW systems pass a richer
            // extraFactor). Normalising by the current skill requirement makes the
            // pipeline generic across all 27 skills without any skill-ID tables.
            float xp = equivalentXp * organicGain / requirement;

            const float globalSkillXp = Settings::Manager::getFloat("global XP gain multiplier", "Game");
            if (globalSkillXp > 0.f)
                xp *= globalSkillXp;

            addExperience(player, xp, std::string());
        }

        void awardKill(const MWWorld::Ptr& victim, const MWWorld::Ptr& attacker)
        {
            if (!isEnabled() || victim.isEmpty() || attacker.isEmpty() || victim == attacker
                || victim == MWMechanics::getPlayer())
                return;
            if (!victim.getClass().isActor() || !playerResponsibleForKill(attacker))
                return;

            const MWWorld::Ptr player = MWMechanics::getPlayer();
            const int victimLevel = std::max(1, victim.getClass().getCreatureStats(victim).getLevel());
            const int playerLevel = std::max(1, player.getClass().getCreatureStats(player).getLevel());

            const float base = nonNegativeSetting("kill base xp");
            const float perLevel = nonNegativeSetting("kill xp per victim level");
            const float relativeDanger = clampFloat(
                std::sqrt(static_cast<float>(victimLevel) / static_cast<float>(playerLevel)), 0.35f, 2.25f);
            const float xp = (base + perLevel * victimLevel) * relativeDanger;

            std::ostringstream message;
            message << "+" << formatXp(xp * positiveSetting("xp gain multiplier", 1.f)) << " XP - defeated "
                    << victim.getClass().getName(victim);
            addExperience(player, xp, message.str());
        }

        void awardQuestProgress(const std::string& questId, int journalIndex, bool completed)
        {
            if (!isEnabled() || questId.empty() || journalIndex <= 0)
                return;

            const MWWorld::Ptr player = MWMechanics::getPlayer();
            if (player.isEmpty() || !player.getClass().isNpc())
                return;

            const ESM::Dialogue* dialogue = MWBase::Environment::get().getWorld()
                ->getStore().get<ESM::Dialogue>().search(questId);
            if (!dialogue)
                return;

            std::string questName;
            for (const ESM::DialInfo& info : dialogue->mInfo)
            {
                if (info.mQuestStatus == ESM::DialInfo::QS_Name && !info.mResponse.empty())
                {
                    questName = info.mResponse;
                    break;
                }
            }
            if (questName.empty())
                questName = questId;

            NpcStats& stats = player.getClass().getNpcStats(player);
            const std::string lowerId = Misc::StringUtils::lowerCase(questId);
            const std::string stageKey = "quest-stage:" + lowerId + ":" + std::to_string(journalIndex);
            const std::string completionKey = "quest-complete:" + lowerId;

            float xp = 0.f;
            if (!stats.hasXpRewardKey(stageKey))
            {
                stats.addXpRewardKey(stageKey);
                xp += nonNegativeSetting("quest xp per stage");
            }

            if (completed && !stats.hasXpRewardKey(completionKey))
            {
                stats.addXpRewardKey(completionKey);
                xp += nonNegativeSetting("quest base xp");
            }

            if (!(xp > 0.f))
                return;

            std::ostringstream message;
            message << "+" << formatXp(xp * positiveSetting("xp gain multiplier", 1.f))
                    << " XP - " << (completed ? "quest completed: " : "quest progress: ")
                    << questName;
            addExperience(player, xp, message.str());
        }

        void awardLocationDiscovery(const MWWorld::Ptr& player,
            const std::string& rewardKey, const std::string& displayName)
        {
            if (!isEnabled() || player.isEmpty() || player != MWMechanics::getPlayer()
                || rewardKey.empty() || displayName.empty())
                return;

            const float xp = nonNegativeSetting("location discovery xp");
            std::ostringstream message;
            message << "+" << formatXp(xp * positiveSetting("xp gain multiplier", 1.f)) << " XP - discovered " << displayName;
            awardOnce(player, "location:" + rewardKey, xp, message.str());
        }

        void awardBookRead(const MWWorld::Ptr& player, const ESM::Book& book)
        {
            if (!isEnabled() || player.isEmpty() || player != MWMechanics::getPlayer()
                || book.mData.mIsScroll)
                return;

            const bool skillBook = book.mData.mSkillId >= 0 && book.mData.mSkillId < ESM::Skill::Length;
            const float xp = nonNegativeSetting(skillBook ? "skill book xp" : "lore book xp");
            if (!(xp > 0.f))
                return;

            std::ostringstream message;
            message << "+" << formatXp(xp * positiveSetting("xp gain multiplier", 1.f)) << " XP - read "
                    << (book.mName.empty() ? book.mId : book.mName);
            awardOnce(player, "book:" + Misc::StringUtils::lowerCase(book.mId), xp, message.str());
        }

        void applyDeathPenalty(const MWWorld::Ptr& player)
        {
            if (!isEnabled() || player.isEmpty() || !player.getClass().isNpc())
                return;

            NpcStats& stats = player.getClass().getNpcStats(player);
            const float fraction = clampFloat(Settings::Manager::getFloat(
                "death xp loss fraction", "XP Leveling"), 0.f, 1.f);
            const float loss = std::min(stats.getExperience(), std::max(0.f, stats.getExperience() * fraction));
            if (!(loss > 0.f))
                return;

            stats.setExperience(std::max(0.f, stats.getExperience() - loss));
            std::ostringstream message;
            message << "Death: -" << formatXp(loss) << " XP";
            notifyXp(message.str());
        }

        bool spendSkillPoints(const MWWorld::Ptr& player, int skillId)
        {
            if (!isEnabled() || player.isEmpty() || !player.getClass().isNpc())
                return false;
            if (skillId < 0 || skillId >= ESM::Skill::Length)
                return false;

            NpcStats& stats = player.getClass().getNpcStats(player);
            const float before = stats.getSkill(skillId).getBase();
            if (before >= 100.f)
            {
                notifyXp("Skill is already at 100");
                return false;
            }

            const int cost = getSkillPointCost(before);
            if (stats.getSkillPoints() < cost)
            {
                std::ostringstream message;
                message << "Not enough Skill Points (need " << cost << ")";
                notifyXp(message.str());
                return false;
            }

            const ESM::Class& class_ = getPlayerClass(player);
            stats.increaseSkill(skillId, class_, false, false);
            const float after = stats.getSkill(skillId).getBase();
            if (after <= before)
                return false;

            if (!stats.spendSkillPoints(cost))
            {
                // Defensive rollback: this should be unreachable because the
                // balance check occurs immediately above.
                stats.getSkill(skillId).setBase(before);
                return false;
            }

            const ESM::Skill* skill = MWBase::Environment::get().getWorld()
                ->getStore().get<ESM::Skill>().find(skillId);
            addAttributeProgress(player, skill->mData.mAttribute,
                classAttributeProgress(class_, *skill, skillId));

            std::ostringstream message;
            message << "Skill Points: " << stats.getSkillPoints() << " remaining";
            notifyXp(message.str());
            return true;
        }
    }
}
