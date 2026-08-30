#include "ServerQuestEditor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include <MyGUI_InputManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_UString.h>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwbase/windowmanager.hpp"
#include "apps/openmw/mwgui/mode.hpp"

#include "../GUIController.hpp"
#include "../LocalPlayer.hpp"
#include "../Main.hpp"
#include "../Networking.hpp"
#include "../ServerQuestRegistry.hpp"

namespace
{
    std::string lowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string percentEncode(const std::string& value)
    {
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        out.reserve(value.size());
        for (unsigned char c : value)
        {
            const bool safe = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ' ';
            if (safe)
                out.push_back(static_cast<char>(c));
            else
            {
                out.push_back('%');
                out.push_back(hex[(c >> 4) & 0x0f]);
                out.push_back(hex[c & 0x0f]);
            }
        }
        return out;
    }

    std::string join(const std::vector<std::string>& fields)
    {
        std::string out = "EDITOR_CMD";
        for (const std::string& field : fields)
        {
            out.push_back('\t');
            out += percentEncode(field);
        }
        return out;
    }

    std::string requirementSummary(const mwmp::ServerQuestEditorRequirement& req)
    {
        std::ostringstream stream;
        stream << req.type;
        if (!req.refId.empty()) stream << " " << req.refId;
        else if (!req.questId.empty()) stream << " " << req.questId;
        else if (!req.key.empty()) stream << " " << req.key;
        if (!req.op.empty()) stream << " " << req.op;
        if (!req.value.empty()) stream << " " << req.value;
        return stream.str();
    }

    std::string rewardSummary(const mwmp::ServerQuestEditorReward& reward)
    {
        std::ostringstream stream;
        stream << reward.type;
        if (!reward.refId.empty()) stream << " " << reward.refId;
        if (!reward.amount.empty()) stream << " " << reward.amount;
        else if (!reward.count.empty()) stream << " x" << reward.count;
        else if (!reward.key.empty()) stream << " " << reward.key << "=" << reward.value;
        else if (!reward.value.empty()) stream << " " << reward.value;
        else if (!reward.message.empty()) stream << " " << reward.message;
        return stream.str();
    }
}

namespace mwmp
{
    ServerQuestEditorWindow::ServerQuestEditorWindow()
        : WindowModal("arenamp_serverquesteditor.layout")
    {
        getWidget(mTitle, "EditorTitle");
        getWidget(mQuestSearch, "QuestSearch");
        getWidget(mQuestList, "QuestList");
        getWidget(mQuestCounter, "QuestCounter");
        getWidget(mNewQuestId, "NewQuestId");
        getWidget(mNewQuestName, "NewQuestName");
        getWidget(mNewQuestButton, "NewQuestButton");
        getWidget(mCloneQuestButton, "CloneQuestButton");
        getWidget(mDeleteQuestButton, "DeleteQuestButton");
        getWidget(mTabs, "EditorTabs");

        getWidget(mQuestId, "QuestId");
        getWidget(mQuestStatus, "QuestStatus");
        getWidget(mQuestName, "QuestName");
        getWidget(mProgressMode, "ProgressMode");
        getWidget(mQuestVersion, "QuestVersion");
        getWidget(mQuestAuthor, "QuestAuthor");
        getWidget(mGiverRefId, "GiverRefId");
        getWidget(mGiverCell, "GiverCell");
        getWidget(mGiverUnique, "GiverUnique");
        getWidget(mInitialStage, "InitialStage");
        getWidget(mPickGiverButton, "PickGiverButton");
        getWidget(mClearUniqueButton, "ClearUniqueButton");
        getWidget(mSaveOverviewButton, "SaveOverviewButton");

        getWidget(mTopicList, "TopicList");
        getWidget(mTopicId, "TopicId");
        getWidget(mTopicText, "TopicText");
        getWidget(mTopicEnabled, "TopicEnabled");
        getWidget(mNewTopicButton, "NewTopicButton");
        getWidget(mSaveTopicButton, "SaveTopicButton");
        getWidget(mDeleteTopicButton, "DeleteTopicButton");
        getWidget(mOfferDialogue, "OfferDialogue");
        getWidget(mSaveOfferButton, "SaveOfferButton");
        getWidget(mOfferChoiceList, "OfferChoiceList");
        getWidget(mOfferChoiceId, "OfferChoiceId");
        getWidget(mOfferChoiceAction, "OfferChoiceAction");
        getWidget(mOfferChoiceTarget, "OfferChoiceTarget");
        getWidget(mOfferChoiceText, "OfferChoiceText");
        getWidget(mNewOfferChoiceButton, "NewOfferChoiceButton");
        getWidget(mSaveOfferChoiceButton, "SaveOfferChoiceButton");
        getWidget(mDeleteOfferChoiceButton, "DeleteOfferChoiceButton");

        getWidget(mStageList, "StageList");
        getWidget(mStageIndex, "StageIndex");
        getWidget(mStageInitialButton, "StageInitialButton");
        getWidget(mStageCompleteButton, "StageCompleteButton");
        getWidget(mStageFailButton, "StageFailButton");
        getWidget(mStageJournal, "StageJournal");
        getWidget(mStageDialogue, "StageDialogue");
        getWidget(mNewStageButton, "NewStageButton");
        getWidget(mSaveStageButton, "SaveStageButton");
        getWidget(mDeleteStageButton, "DeleteStageButton");
        getWidget(mStageChoiceList, "StageChoiceList");
        getWidget(mStageChoiceId, "StageChoiceId");
        getWidget(mStageChoiceAction, "StageChoiceAction");
        getWidget(mStageChoiceTarget, "StageChoiceTarget");
        getWidget(mStageChoiceText, "StageChoiceText");
        getWidget(mNewStageChoiceButton, "NewStageChoiceButton");
        getWidget(mSaveStageChoiceButton, "SaveStageChoiceButton");
        getWidget(mDeleteStageChoiceButton, "DeleteStageChoiceButton");
        getWidget(mNextStageTarget, "NextStageTarget");
        getWidget(mAddNextButton, "AddNextButton");
        getWidget(mDeleteNextButton, "DeleteNextButton");

        getWidget(mLogicTarget, "LogicTarget");
        getWidget(mLogicContext, "LogicContext");
        getWidget(mRequirementList, "RequirementList");
        getWidget(mRequirementType, "RequirementType");
        getWidget(mRequirementOp, "RequirementOp");
        getWidget(mRequirementValue, "RequirementValue");
        getWidget(mRequirementRef, "RequirementRef");
        getWidget(mAddRequirementButton, "AddRequirementButton");
        getWidget(mDeleteRequirementButton, "DeleteRequirementButton");
        getWidget(mRewardList, "RewardList");
        getWidget(mRewardType, "RewardType");
        getWidget(mRewardValueA, "RewardValueA");
        getWidget(mRewardValueB, "RewardValueB");
        getWidget(mAddRewardButton, "AddRewardButton");
        getWidget(mDeleteRewardButton, "DeleteRewardButton");

        getWidget(mValidationText, "ValidationText");
        getWidget(mValidateButton, "ValidateButton");
        getWidget(mPublishButton, "PublishButton");
        getWidget(mDisableButton, "DisableButton");
        getWidget(mRefreshButton, "RefreshButton");
        getWidget(mNotice, "EditorNotice");
        getWidget(mCloseButton, "CloseButton");

        mProgressMode->addItem("personal");
        mProgressMode->addItem("party");
        mProgressMode->addItem("server");
        mTopicEnabled->addItem("true");
        mTopicEnabled->addItem("false");
        for (const char* action : { "none", "start", "advance" })
        {
            mOfferChoiceAction->addItem(action);
            mStageChoiceAction->addItem(action);
        }
        mLogicTarget->addItem("Stage requirements");
        mLogicTarget->addItem("Selected stage choice");
        mLogicTarget->addItem("Selected offer choice");
        // X042: expose the expanded server vocabulary in Quest Studio. Nested
        // all/any/not groups remain an advanced JSON feature until the graph
        // editor gets a dedicated boolean-tree widget.
        for (const char* type : { "level", "item", "gold", "questStage", "questState",
                 "questCompleted", "questNotStarted", "playerVariable", "serverVariable", "staffRank",
                 "skill", "attribute", "faction", "factionRank", "reputation", "bounty", "race",
                 "class", "cell", "global", "vanillaJournal", "realTime", "cooldown" })
            mRequirementType->addItem(type);
        for (const char* op : { ">=", "<=", "=", "!=", ">", "<" })
            mRequirementOp->addItem(op);
        for (const char* type : { "gold", "xp", "item", "giveItem", "takeItem", "setPlayerVariable",
                 "message", "setServerVariable", "addSpell", "removeSpell", "setReputation", "setBounty",
                 "teleport", "playSound", "messageBox", "setVanillaJournal" })
            mRewardType->addItem(type);

        mProgressMode->setIndexSelected(0);
        mTopicEnabled->setIndexSelected(0);
        mOfferChoiceAction->setIndexSelected(0);
        mStageChoiceAction->setIndexSelected(0);
        mLogicTarget->setIndexSelected(0);
        mRequirementType->setIndexSelected(0);
        mRequirementOp->setIndexSelected(0);
        mRewardType->setIndexSelected(0);

        mOfferDialogue->setEditMultiLine(true);
        mOfferDialogue->setEditWordWrap(true);
        mStageJournal->setEditMultiLine(true);
        mStageJournal->setEditWordWrap(true);
        mStageDialogue->setEditMultiLine(true);
        mStageDialogue->setEditWordWrap(true);
        mStageChoiceText->setEditMultiLine(true);
        mStageChoiceText->setEditWordWrap(true);
        mValidationText->setEditStatic(true);
        mValidationText->setEditReadOnly(true);
        mValidationText->setEditMultiLine(true);
        mValidationText->setEditWordWrap(true);

        mCloseButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyClose);
        mQuestSearch->eventEditTextChange += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySearchChanged);
        mQuestList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyQuestSelected);
        mNewQuestButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyNewQuest);
        mCloneQuestButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyCloneQuest);
        mDeleteQuestButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteQuest);
        mSaveOverviewButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveOverview);
        mPickGiverButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyPickGiver);
        mClearUniqueButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyClearUnique);

        mTopicList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyTopicSelected);
        mNewTopicButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyNewTopic);
        mSaveTopicButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveTopic);
        mDeleteTopicButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteTopic);
        mSaveOfferButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveOffer);
        mOfferChoiceList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyOfferChoiceSelected);
        mNewOfferChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyNewOfferChoice);
        mSaveOfferChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveOfferChoice);
        mDeleteOfferChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteOfferChoice);

        mStageList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyStageSelected);
        mStageInitialButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyToggleInitial);
        mStageCompleteButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyToggleComplete);
        mStageFailButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyToggleFail);
        mNewStageButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyNewStage);
        mSaveStageButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveStage);
        mDeleteStageButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteStage);
        mStageChoiceList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyStageChoiceSelected);
        mNewStageChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyNewStageChoice);
        mSaveStageChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifySaveStageChoice);
        mDeleteStageChoiceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteStageChoice);
        mAddNextButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyAddNext);
        mDeleteNextButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteNext);

        mLogicTarget->eventComboChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyLogicTargetChanged);
        mRequirementList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyRequirementSelected);
        mAddRequirementButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyAddRequirement);
        mDeleteRequirementButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteRequirement);
        mRewardList->eventListChangePosition += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyRewardSelected);
        mAddRewardButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyAddReward);
        mDeleteRewardButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDeleteReward);

        mValidateButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyValidate);
        mPublishButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyPublish);
        mDisableButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyDisable);
        mRefreshButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ServerQuestEditorWindow::notifyRefresh);

        center();
    }

    void ServerQuestEditorWindow::onOpen()
    {
        WindowModal::onOpen();
        center();
        refreshFromRegistry();
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mQuestSearch);
    }

    bool ServerQuestEditorWindow::exit()
    {
        notifyClose(nullptr);
        return false;
    }

    void ServerQuestEditorWindow::sendCommand(const std::vector<std::string>& fields)
    {
        LocalPlayer* localPlayer = Main::get().getLocalPlayer();
        if (localPlayer == nullptr)
            return;

        localPlayer->guiMessageBox.id = ServerQuestRegistry::TransportGuiId;
        localPlayer->guiMessageBox.type = BasePlayer::GUIMessageBox::CustomMessageBox;
        localPlayer->guiMessageBox.label.clear();
        localPlayer->guiMessageBox.note.clear();
        localPlayer->guiMessageBox.buttons.clear();
        localPlayer->guiMessageBox.data = join(fields);

        auto* playerPacket = Main::get().getNetworking()->getPlayerPacket(ID_GUI_MESSAGEBOX);
        playerPacket->setPlayer(localPlayer);
        playerPacket->Send();
    }

    std::string ServerQuestEditorWindow::boolText(bool value) { return value ? "yes" : "no"; }

    int ServerQuestEditorWindow::parseInt(const std::string& value, int fallback)
    {
        if (value.empty()) return fallback;
        char* end = nullptr;
        const long result = std::strtol(value.c_str(), &end, 10);
        return end != value.c_str() && *end == '\0' ? static_cast<int>(result) : fallback;
    }

    void ServerQuestEditorWindow::setComboValue(MyGUI::ComboBox* combo, const std::string& value)
    {
        const std::string wanted = lowerAscii(value);
        for (std::size_t i = 0; i < combo->getItemCount(); ++i)
        {
            if (lowerAscii(combo->getItemNameAt(i).asUTF8()) == wanted)
            {
                combo->setIndexSelected(i);
                return;
            }
        }
        if (combo->getItemCount() > 0)
            combo->setIndexSelected(0);
    }

    std::string ServerQuestEditorWindow::comboValue(MyGUI::ComboBox* combo) const
    {
        const std::size_t index = combo->getIndexSelected();
        if (index == MyGUI::ITEM_NONE || index >= combo->getItemCount())
            return std::string();
        return combo->getItemNameAt(index).asUTF8();
    }

    const ServerQuestEditorQuest* ServerQuestEditorWindow::selectedQuest() const
    {
        const ServerQuestEditorModel& model = ServerQuestRegistry::get().getEditorModel();
        for (const ServerQuestEditorQuest& quest : model.quests)
            if (quest.id == mSelectedQuestId)
                return &quest;
        return nullptr;
    }

    const ServerQuestEditorStage* ServerQuestEditorWindow::selectedStage() const
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (quest == nullptr) return nullptr;
        for (const ServerQuestEditorStage& stage : quest->stages)
            if (stage.index == mSelectedStageIndex) return &stage;
        return nullptr;
    }

    const ServerQuestEditorChoice* ServerQuestEditorWindow::selectedOfferChoice() const
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (quest == nullptr) return nullptr;
        for (const ServerQuestEditorChoice& choice : quest->offerChoices)
            if (choice.id == mSelectedOfferChoiceId) return &choice;
        return nullptr;
    }

    const ServerQuestEditorChoice* ServerQuestEditorWindow::selectedStageChoice() const
    {
        const ServerQuestEditorStage* stage = selectedStage();
        if (stage == nullptr) return nullptr;
        for (const ServerQuestEditorChoice& choice : stage->choices)
            if (choice.id == mSelectedStageChoiceId) return &choice;
        return nullptr;
    }

    void ServerQuestEditorWindow::refreshFromRegistry()
    {
        mRefreshing = true;
        const ServerQuestEditorModel& model = ServerQuestRegistry::get().getEditorModel();
        if (!model.selectedQuestId.empty())
            mSelectedQuestId = model.selectedQuestId;
        rebuildQuestList();
        rebuildQuestDetails();
        updateNotice();
        mRefreshing = false;
    }

    void ServerQuestEditorWindow::rebuildQuestList()
    {
        const ServerQuestEditorModel& model = ServerQuestRegistry::get().getEditorModel();
        const std::string filter = lowerAscii(mQuestSearch->getOnlyText());
        mQuestList->removeAllItems();
        mVisibleQuestIds.clear();
        std::size_t selected = MyGUI::ITEM_NONE;

        for (const ServerQuestEditorQuest& quest : model.quests)
        {
            const std::string searchable = lowerAscii(quest.id + " " + quest.name + " " + quest.status);
            if (!filter.empty() && searchable.find(filter) == std::string::npos)
                continue;
            std::string prefix = quest.status == "published" ? "[PUB] " : quest.status == "disabled" ? "[OFF] " : "[DRAFT] ";
            mQuestList->addItem(prefix + (quest.name.empty() ? quest.id : quest.name));
            if (quest.id == mSelectedQuestId)
                selected = mVisibleQuestIds.size();
            mVisibleQuestIds.push_back(quest.id);
        }
        mQuestCounter->setCaption(std::to_string(mVisibleQuestIds.size()) + " quest(s)");
        if (selected != MyGUI::ITEM_NONE)
            mQuestList->setIndexSelected(selected);
    }

    void ServerQuestEditorWindow::clearEditorFields()
    {
        mQuestId->setCaption(""); mQuestStatus->setCaption(""); mQuestName->setCaption("");
        mQuestVersion->setCaption(""); mQuestAuthor->setCaption(""); mGiverRefId->setCaption("");
        mGiverCell->setCaption(""); mGiverUnique->setCaption(""); mInitialStage->setCaption("");
        mTopicList->removeAllItems(); mOfferChoiceList->removeAllItems(); mStageList->removeAllItems();
        mStageChoiceList->removeAllItems(); mRequirementList->removeAllItems(); mRewardList->removeAllItems();
        mOfferDialogue->setCaption(""); mValidationText->setCaption("");
    }

    void ServerQuestEditorWindow::rebuildQuestDetails()
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        const ServerQuestEditorModel& model = ServerQuestRegistry::get().getEditorModel();
        if (quest == nullptr)
        {
            clearEditorFields();
            return;
        }
        mQuestId->setCaption(quest->id);
        mQuestStatus->setCaption(quest->status);
        mQuestName->setCaption(quest->name);
        setComboValue(mProgressMode, quest->progressMode);
        mQuestVersion->setCaption(std::to_string(quest->version));
        mQuestAuthor->setCaption(quest->author);
        mGiverRefId->setCaption(quest->giverRefId);
        mGiverCell->setCaption(quest->cell);
        mGiverUnique->setCaption(quest->uniqueIndex);
        mInitialStage->setCaption(std::to_string(quest->initialStage));
        mOfferDialogue->setCaption(quest->offerDialogue);
        mDeleteQuestButton->setEnabled(model.isAdmin);
        mPublishButton->setEnabled(model.canPublish);
        rebuildTopics();
        rebuildOfferChoices();
        rebuildStages();
        rebuildLogic();
        rebuildValidation();
    }

    void ServerQuestEditorWindow::rebuildTopics()
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        mTopicList->removeAllItems();
        if (quest == nullptr) return;
        std::size_t selected = MyGUI::ITEM_NONE;
        for (std::size_t i = 0; i < quest->topics.size(); ++i)
        {
            const ServerQuestEditorTopic& topic = quest->topics[i];
            mTopicList->addItem(std::string(topic.enabled ? "[ON] " : "[OFF] ") + topic.text);
            if (topic.id == mSelectedTopicId) selected = i;
        }
        if (selected != MyGUI::ITEM_NONE)
        {
            mTopicList->setIndexSelected(selected);
            const ServerQuestEditorTopic& topic = quest->topics[selected];
            mTopicId->setCaption(topic.id);
            mTopicText->setCaption(topic.text);
            setComboValue(mTopicEnabled, topic.enabled ? "true" : "false");
        }
        else
        {
            mSelectedTopicId.clear();
            mTopicId->setCaption("");
            mTopicText->setCaption("");
            setComboValue(mTopicEnabled, "true");
        }
    }

    void ServerQuestEditorWindow::rebuildOfferChoices()
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        mOfferChoiceList->removeAllItems();
        if (quest == nullptr) return;
        std::size_t selected = MyGUI::ITEM_NONE;
        for (std::size_t i = 0; i < quest->offerChoices.size(); ++i)
        {
            const ServerQuestEditorChoice& choice = quest->offerChoices[i];
            mOfferChoiceList->addItem(choice.text.empty() ? choice.id : choice.text);
            if (choice.id == mSelectedOfferChoiceId) selected = i;
        }
        if (selected != MyGUI::ITEM_NONE)
        {
            mOfferChoiceList->setIndexSelected(selected);
            const ServerQuestEditorChoice& choice = quest->offerChoices[selected];
            mOfferChoiceId->setCaption(choice.id);
            mOfferChoiceText->setCaption(choice.text);
            setComboValue(mOfferChoiceAction, choice.action);
            mOfferChoiceTarget->setCaption(choice.hasTargetStage ? std::to_string(choice.targetStage) : "");
        }
        else
        {
            mSelectedOfferChoiceId.clear();
            mOfferChoiceId->setCaption("");
            mOfferChoiceText->setCaption("");
            setComboValue(mOfferChoiceAction, "none");
            mOfferChoiceTarget->setCaption("");
        }
    }

    void ServerQuestEditorWindow::rebuildStages()
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        mStageList->removeAllItems();
        if (quest == nullptr) return;
        const ServerQuestEditorStage* selectedStagePtr = nullptr;
        std::size_t selected = MyGUI::ITEM_NONE;
        for (std::size_t i = 0; i < quest->stages.size(); ++i)
        {
            const ServerQuestEditorStage& stage = quest->stages[i];
            std::string label = "#" + std::to_string(stage.index);
            if (stage.index == quest->initialStage) label += " [initial]";
            if (stage.complete) label += " [complete]";
            if (stage.fail) label += " [fail]";
            mStageList->addItem(label);
            if (stage.index == mSelectedStageIndex)
            {
                selected = i;
                selectedStagePtr = &stage;
            }
        }
        if (selectedStagePtr == nullptr && !quest->stages.empty())
        {
            selected = 0;
            selectedStagePtr = &quest->stages[0];
            mSelectedStageIndex = selectedStagePtr->index;
        }
        if (selectedStagePtr != nullptr)
        {
            mStageList->setIndexSelected(selected);
            mStageIndex->setCaption(std::to_string(selectedStagePtr->index));
            mStageJournal->setCaption(selectedStagePtr->journal);
            mStageDialogue->setCaption(selectedStagePtr->dialogue);
            mStageInitial = selectedStagePtr->index == quest->initialStage;
            mStageComplete = selectedStagePtr->complete;
            mStageFail = selectedStagePtr->fail;
            mStageInitialButton->setCaption("Initial: " + boolText(mStageInitial));
            mStageCompleteButton->setCaption("Complete: " + boolText(mStageComplete));
            mStageFailButton->setCaption("Fail: " + boolText(mStageFail));
        }
        else
        {
            mSelectedStageIndex = 0;
            mStageIndex->setCaption("");
            mStageJournal->setCaption("");
            mStageDialogue->setCaption("");
            mStageInitial = mStageComplete = mStageFail = false;
            mStageInitialButton->setCaption("Initial: no");
            mStageCompleteButton->setCaption("Complete: no");
            mStageFailButton->setCaption("Fail: no");
        }
        rebuildStageChoices();
    }

    void ServerQuestEditorWindow::rebuildStageChoices()
    {
        const ServerQuestEditorStage* stage = selectedStage();
        mStageChoiceList->removeAllItems();
        if (stage == nullptr) return;
        std::size_t selected = MyGUI::ITEM_NONE;
        for (std::size_t i = 0; i < stage->choices.size(); ++i)
        {
            const ServerQuestEditorChoice& choice = stage->choices[i];
            mStageChoiceList->addItem(choice.text.empty() ? choice.id : choice.text);
            if (choice.id == mSelectedStageChoiceId) selected = i;
        }
        if (selected != MyGUI::ITEM_NONE)
        {
            mStageChoiceList->setIndexSelected(selected);
            const ServerQuestEditorChoice& choice = stage->choices[selected];
            mStageChoiceId->setCaption(choice.id);
            mStageChoiceText->setCaption(choice.text);
            setComboValue(mStageChoiceAction, choice.action);
            mStageChoiceTarget->setCaption(choice.hasTargetStage ? std::to_string(choice.targetStage) : "");
        }
        else
        {
            mSelectedStageChoiceId.clear();
            mStageChoiceId->setCaption("");
            mStageChoiceText->setCaption("");
            setComboValue(mStageChoiceAction, "none");
            mStageChoiceTarget->setCaption("");
        }
    }

    void ServerQuestEditorWindow::rebuildLogic()
    {
        mRequirementList->removeAllItems();
        mRewardList->removeAllItems();
        mSelectedRequirementIndex = -1;
        mSelectedRewardIndex = -1;

        const ServerQuestEditorStage* stage = selectedStage();
        if (stage == nullptr)
        {
            mLogicContext->setCaption("Select a stage first");
            return;
        }

        const std::size_t mode = mLogicTarget->getIndexSelected();
        const std::vector<ServerQuestEditorRequirement>* requirements = nullptr;
        if (mode == 1)
        {
            const ServerQuestEditorChoice* choice = selectedStageChoice();
            if (choice != nullptr)
            {
                requirements = &choice->requirements;
                mLogicContext->setCaption("Stage #" + std::to_string(stage->index) + " choice: " + choice->id);
            }
            else mLogicContext->setCaption("Select a stage choice");
        }
        else if (mode == 2)
        {
            const ServerQuestEditorChoice* choice = selectedOfferChoice();
            if (choice != nullptr)
            {
                requirements = &choice->requirements;
                mLogicContext->setCaption("Offer choice: " + choice->id);
            }
            else mLogicContext->setCaption("Select an offer choice");
        }
        else
        {
            requirements = &stage->requirements;
            mLogicContext->setCaption("Stage #" + std::to_string(stage->index) + " requirements");
        }

        if (requirements != nullptr)
            for (const ServerQuestEditorRequirement& req : *requirements)
                mRequirementList->addItem(requirementSummary(req));
        for (const ServerQuestEditorReward& reward : stage->rewards)
            mRewardList->addItem(rewardSummary(reward));
    }

    void ServerQuestEditorWindow::rebuildValidation()
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (quest == nullptr)
        {
            mValidationText->setCaption("");
            return;
        }
        std::ostringstream stream;
        stream << "Quest: " << quest->id << "\nStatus: " << quest->status << "\n\n";
        stream << "Errors: " << quest->validationErrors.size() << "\n";
        for (const std::string& error : quest->validationErrors) stream << "ERROR: " << error << "\n";
        stream << "\nWarnings: " << quest->validationWarnings.size() << "\n";
        for (const std::string& warning : quest->validationWarnings) stream << "WARN: " << warning << "\n";
        if (quest->validationErrors.empty() && quest->validationWarnings.empty())
            stream << "No validation issues reported.\n";
        mValidationText->setCaption(stream.str());
    }

    void ServerQuestEditorWindow::updateNotice()
    {
        const ServerQuestEditorModel& model = ServerQuestRegistry::get().getEditorModel();
        std::string text = model.notice;
        if (text.empty()) text = "Server-authoritative editor. Every edit returns Published quests to Draft.";
        mNotice->setCaption(text);
    }

    void ServerQuestEditorWindow::selectQuestById(const std::string& id, bool requestDetails)
    {
        if (id == mSelectedQuestId && !requestDetails) return;
        mSelectedQuestId = id;
        mSelectedTopicId.clear();
        mSelectedStageIndex = 0;
        mSelectedOfferChoiceId.clear();
        mSelectedStageChoiceId.clear();
        if (requestDetails && !id.empty()) sendCommand({ "select", id });
        rebuildQuestList();
        rebuildQuestDetails();
    }

    void ServerQuestEditorWindow::selectStageByIndex(int stage)
    {
        mSelectedStageIndex = stage;
        mSelectedStageChoiceId.clear();
        rebuildStages();
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifyClose(MyGUI::Widget*)
    {
        setVisible(false);
        MWBase::Environment::get().getWindowManager()->removeGuiMode(
            static_cast<MWGui::GuiMode>(GUIController::GM_ARENAMP_QuestEditor));
    }

    void ServerQuestEditorWindow::notifySearchChanged(MyGUI::EditBox*)
    {
        if (!mRefreshing) rebuildQuestList();
    }

    void ServerQuestEditorWindow::notifyQuestSelected(MyGUI::ListBox*, std::size_t index)
    {
        if (mRefreshing || index == MyGUI::ITEM_NONE || index >= mVisibleQuestIds.size()) return;
        selectQuestById(mVisibleQuestIds[index], true);
    }

    void ServerQuestEditorWindow::notifyNewQuest(MyGUI::Widget*)
    {
        const std::string id = mNewQuestId->getOnlyText();
        const std::string name = mNewQuestName->getOnlyText();
        if (!id.empty() && !name.empty()) sendCommand({ "new", id, name });
    }

    void ServerQuestEditorWindow::notifyCloneQuest(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        const std::string id = mNewQuestId->getOnlyText();
        const std::string name = mNewQuestName->getOnlyText();
        if (!id.empty() && !name.empty()) sendCommand({ "clone", mSelectedQuestId, id, name });
    }

    void ServerQuestEditorWindow::notifyDeleteQuest(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty()) sendCommand({ "delete", mSelectedQuestId });
    }

    void ServerQuestEditorWindow::notifySaveOverview(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        sendCommand({ "overview", mSelectedQuestId, mQuestName->getOnlyText(), comboValue(mProgressMode),
            mGiverRefId->getOnlyText(), mGiverCell->getOnlyText(), mGiverUnique->getOnlyText(), mInitialStage->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyPickGiver(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        sendCommand({ "pick_giver", mSelectedQuestId });
        notifyClose(nullptr);
    }

    void ServerQuestEditorWindow::notifyClearUnique(MyGUI::Widget*)
    {
        mGiverUnique->setCaption("");
        notifySaveOverview(nullptr);
    }

    void ServerQuestEditorWindow::notifyTopicSelected(MyGUI::ListBox*, std::size_t index)
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (mRefreshing || quest == nullptr || index == MyGUI::ITEM_NONE || index >= quest->topics.size()) return;
        const ServerQuestEditorTopic& topic = quest->topics[index];
        mSelectedTopicId = topic.id;
        mTopicId->setCaption(topic.id);
        mTopicText->setCaption(topic.text);
        setComboValue(mTopicEnabled, topic.enabled ? "true" : "false");
    }

    void ServerQuestEditorWindow::notifyNewTopic(MyGUI::Widget*)
    {
        mSelectedTopicId.clear();
        mTopicList->setIndexSelected(MyGUI::ITEM_NONE);
        mTopicId->setCaption(""); mTopicText->setCaption(""); setComboValue(mTopicEnabled, "true");
    }

    void ServerQuestEditorWindow::notifySaveTopic(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        sendCommand({ "topic_upsert", mSelectedQuestId, mSelectedTopicId, mTopicId->getOnlyText(),
            mTopicText->getOnlyText(), comboValue(mTopicEnabled) });
    }

    void ServerQuestEditorWindow::notifyDeleteTopic(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && !mSelectedTopicId.empty())
            sendCommand({ "topic_delete", mSelectedQuestId, mSelectedTopicId });
    }

    void ServerQuestEditorWindow::notifySaveOffer(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty()) sendCommand({ "offer", mSelectedQuestId, mOfferDialogue->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyOfferChoiceSelected(MyGUI::ListBox*, std::size_t index)
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (mRefreshing || quest == nullptr || index == MyGUI::ITEM_NONE || index >= quest->offerChoices.size()) return;
        const ServerQuestEditorChoice& choice = quest->offerChoices[index];
        mSelectedOfferChoiceId = choice.id;
        mOfferChoiceId->setCaption(choice.id);
        mOfferChoiceText->setCaption(choice.text);
        setComboValue(mOfferChoiceAction, choice.action);
        mOfferChoiceTarget->setCaption(choice.hasTargetStage ? std::to_string(choice.targetStage) : "");
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifyNewOfferChoice(MyGUI::Widget*)
    {
        mSelectedOfferChoiceId.clear();
        mOfferChoiceList->setIndexSelected(MyGUI::ITEM_NONE);
        mOfferChoiceId->setCaption(""); mOfferChoiceText->setCaption("");
        mOfferChoiceTarget->setCaption(""); setComboValue(mOfferChoiceAction, "none");
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifySaveOfferChoice(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        sendCommand({ "choice_upsert", mSelectedQuestId, "offer", "0", mSelectedOfferChoiceId,
            mOfferChoiceId->getOnlyText(), comboValue(mOfferChoiceAction), mOfferChoiceTarget->getOnlyText(),
            mOfferChoiceText->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyDeleteOfferChoice(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && !mSelectedOfferChoiceId.empty())
            sendCommand({ "choice_delete", mSelectedQuestId, "offer", "0", mSelectedOfferChoiceId });
    }

    void ServerQuestEditorWindow::notifyStageSelected(MyGUI::ListBox*, std::size_t index)
    {
        const ServerQuestEditorQuest* quest = selectedQuest();
        if (mRefreshing || quest == nullptr || index == MyGUI::ITEM_NONE || index >= quest->stages.size()) return;
        selectStageByIndex(quest->stages[index].index);
    }

    void ServerQuestEditorWindow::notifyToggleInitial(MyGUI::Widget*)
    {
        mStageInitial = !mStageInitial;
        mStageInitialButton->setCaption("Initial: " + boolText(mStageInitial));
    }

    void ServerQuestEditorWindow::notifyToggleComplete(MyGUI::Widget*)
    {
        mStageComplete = !mStageComplete;
        if (mStageComplete) mStageFail = false;
        mStageCompleteButton->setCaption("Complete: " + boolText(mStageComplete));
        mStageFailButton->setCaption("Fail: " + boolText(mStageFail));
    }

    void ServerQuestEditorWindow::notifyToggleFail(MyGUI::Widget*)
    {
        mStageFail = !mStageFail;
        if (mStageFail) mStageComplete = false;
        mStageCompleteButton->setCaption("Complete: " + boolText(mStageComplete));
        mStageFailButton->setCaption("Fail: " + boolText(mStageFail));
    }

    void ServerQuestEditorWindow::notifyNewStage(MyGUI::Widget*)
    {
        mSelectedStageIndex = 0;
        mSelectedStageChoiceId.clear();
        mStageList->setIndexSelected(MyGUI::ITEM_NONE);
        mStageIndex->setCaption("");
        mStageJournal->setCaption("");
        mStageDialogue->setCaption("");
        mStageInitial = mStageComplete = mStageFail = false;
        mStageInitialButton->setCaption("Initial: no");
        mStageCompleteButton->setCaption("Complete: no");
        mStageFailButton->setCaption("Fail: no");
        rebuildStageChoices();
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifySaveStage(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty()) return;
        sendCommand({ "stage_upsert", mSelectedQuestId, std::to_string(mSelectedStageIndex), mStageIndex->getOnlyText(),
            mStageJournal->getOnlyText(), mStageDialogue->getOnlyText(), mStageInitial ? "1" : "0",
            mStageComplete ? "1" : "0", mStageFail ? "1" : "0" });
    }

    void ServerQuestEditorWindow::notifyDeleteStage(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && mSelectedStageIndex != 0)
            sendCommand({ "stage_delete", mSelectedQuestId, std::to_string(mSelectedStageIndex) });
    }

    void ServerQuestEditorWindow::notifyStageChoiceSelected(MyGUI::ListBox*, std::size_t index)
    {
        const ServerQuestEditorStage* stage = selectedStage();
        if (mRefreshing || stage == nullptr || index == MyGUI::ITEM_NONE || index >= stage->choices.size()) return;
        const ServerQuestEditorChoice& choice = stage->choices[index];
        mSelectedStageChoiceId = choice.id;
        mStageChoiceId->setCaption(choice.id);
        mStageChoiceText->setCaption(choice.text);
        setComboValue(mStageChoiceAction, choice.action);
        mStageChoiceTarget->setCaption(choice.hasTargetStage ? std::to_string(choice.targetStage) : "");
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifyNewStageChoice(MyGUI::Widget*)
    {
        mSelectedStageChoiceId.clear();
        mStageChoiceList->setIndexSelected(MyGUI::ITEM_NONE);
        mStageChoiceId->setCaption(""); mStageChoiceText->setCaption("");
        mStageChoiceTarget->setCaption(""); setComboValue(mStageChoiceAction, "none");
        rebuildLogic();
    }

    void ServerQuestEditorWindow::notifySaveStageChoice(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty() || mSelectedStageIndex == 0) return;
        sendCommand({ "choice_upsert", mSelectedQuestId, "stage", std::to_string(mSelectedStageIndex),
            mSelectedStageChoiceId, mStageChoiceId->getOnlyText(), comboValue(mStageChoiceAction),
            mStageChoiceTarget->getOnlyText(), mStageChoiceText->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyDeleteStageChoice(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && mSelectedStageIndex != 0 && !mSelectedStageChoiceId.empty())
            sendCommand({ "choice_delete", mSelectedQuestId, "stage", std::to_string(mSelectedStageIndex), mSelectedStageChoiceId });
    }

    void ServerQuestEditorWindow::notifyAddNext(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && mSelectedStageIndex != 0 && !mNextStageTarget->getOnlyText().empty())
            sendCommand({ "next_add", mSelectedQuestId, std::to_string(mSelectedStageIndex), mNextStageTarget->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyDeleteNext(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty() && mSelectedStageIndex != 0 && !mNextStageTarget->getOnlyText().empty())
            sendCommand({ "next_delete", mSelectedQuestId, std::to_string(mSelectedStageIndex), mNextStageTarget->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyLogicTargetChanged(MyGUI::ComboBox*, std::size_t)
    {
        if (!mRefreshing) rebuildLogic();
    }

    void ServerQuestEditorWindow::notifyRequirementSelected(MyGUI::ListBox*, std::size_t index)
    {
        if (index == MyGUI::ITEM_NONE) mSelectedRequirementIndex = -1;
        else mSelectedRequirementIndex = static_cast<int>(index);
    }

    void ServerQuestEditorWindow::notifyAddRequirement(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty() || mSelectedStageIndex == 0) return;
        const std::size_t mode = mLogicTarget->getIndexSelected();
        std::string scope = "stage";
        std::string choiceId;
        if (mode == 1)
        {
            scope = "stageChoice";
            choiceId = mSelectedStageChoiceId;
            if (choiceId.empty()) return;
        }
        else if (mode == 2)
        {
            scope = "offerChoice";
            choiceId = mSelectedOfferChoiceId;
            if (choiceId.empty()) return;
        }
        sendCommand({ "require_add", mSelectedQuestId, scope, std::to_string(mSelectedStageIndex), choiceId,
            comboValue(mRequirementType), comboValue(mRequirementOp), mRequirementValue->getOnlyText(),
            mRequirementRef->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyDeleteRequirement(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty() || mSelectedRequirementIndex < 0) return;
        const std::size_t mode = mLogicTarget->getIndexSelected();
        std::string scope = mode == 1 ? "stageChoice" : mode == 2 ? "offerChoice" : "stage";
        std::string choiceId = mode == 1 ? mSelectedStageChoiceId : mode == 2 ? mSelectedOfferChoiceId : "";
        sendCommand({ "require_delete", mSelectedQuestId, scope, std::to_string(mSelectedStageIndex), choiceId,
            std::to_string(mSelectedRequirementIndex + 1) });
    }

    void ServerQuestEditorWindow::notifyRewardSelected(MyGUI::ListBox*, std::size_t index)
    {
        if (index == MyGUI::ITEM_NONE) mSelectedRewardIndex = -1;
        else mSelectedRewardIndex = static_cast<int>(index);
    }

    void ServerQuestEditorWindow::notifyAddReward(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty() || mSelectedStageIndex == 0) return;
        sendCommand({ "reward_add", mSelectedQuestId, std::to_string(mSelectedStageIndex), comboValue(mRewardType),
            mRewardValueA->getOnlyText(), mRewardValueB->getOnlyText() });
    }

    void ServerQuestEditorWindow::notifyDeleteReward(MyGUI::Widget*)
    {
        if (mSelectedQuestId.empty() || mSelectedStageIndex == 0 || mSelectedRewardIndex < 0) return;
        sendCommand({ "reward_delete", mSelectedQuestId, std::to_string(mSelectedStageIndex),
            std::to_string(mSelectedRewardIndex + 1) });
    }

    void ServerQuestEditorWindow::notifyValidate(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty()) sendCommand({ "validate", mSelectedQuestId });
    }

    void ServerQuestEditorWindow::notifyPublish(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty()) sendCommand({ "publish", mSelectedQuestId });
    }

    void ServerQuestEditorWindow::notifyDisable(MyGUI::Widget*)
    {
        if (!mSelectedQuestId.empty()) sendCommand({ "disable", mSelectedQuestId });
    }

    void ServerQuestEditorWindow::notifyRefresh(MyGUI::Widget*)
    {
        sendCommand({ "refresh", mSelectedQuestId });
    }
}
