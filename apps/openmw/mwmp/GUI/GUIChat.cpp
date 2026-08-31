#include "GUIChat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_ScrollBar.h>
#include <MyGUI_TextBox.h>
#include <MyGUI_Widget.h>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwgui/windowmanagerimp.hpp"
#include "apps/openmw/mwinput/inputmanagerimp.hpp"
#include "apps/openmw/mwmechanics/xpserverbridge.hpp"
#include <components/openmw-mp/TimedLog.hpp>
#include <components/settings/settings.hpp>

#include "../Networking.hpp"
#include "../Main.hpp"
#include "../LocalPlayer.hpp"
#include "../GUIController.hpp"

namespace
{
    constexpr float sFullyVisibleAlpha = 1.f;
    constexpr float sThirtyPercentTransparentAlpha = 0.7f;
    constexpr float sSixtyPercentTransparentAlpha = 0.4f;
    constexpr float sFadeSpeed = 4.f;
    constexpr float sGeometrySaveDebounce = 0.45f;
    constexpr int sMinimumPanelWidth = 620;
    constexpr int sMinimumPanelHeight = 380;

    std::string localizeArena(const std::string& key)
    {
        return MyGUI::LanguageManager::getInstance().replaceTags("#{arenamp=" + key + "}");
    }

    std::vector<std::string> splitControlFields(const std::string& value, char delimiter)
    {
        std::vector<std::string> result;
        std::string current;
        for (char c : value)
        {
            if (c == delimiter)
            {
                result.push_back(current);
                current.clear();
            }
            else if (c != '\r' && c != '\n')
                current.push_back(c);
        }
        result.push_back(current);
        return result;
    }

    std::string unescapeControlField(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                result.push_back(value[i]);
                continue;
            }
            const char next = value[++i];
            if (next == 'n') result.push_back('\n');
            else if (next == 't') result.push_back('\t');
            else result.push_back(next);
        }
        return result;
    }

    bool controlBool(const std::vector<std::string>& fields, std::size_t index)
    {
        return index < fields.size() && fields[index] == "1";
    }

    float moveTowards(float value, float target, float maximumDelta)
    {
        if (value < target)
            return std::min(value + maximumDelta, target);
        return std::max(value - maximumDelta, target);
    }
}

namespace mwmp
{
    GUIChat::GUIChat(int x, int y, int w, int h)
        : WindowBase("tes3mp_chat.layout")
        , mCommandLine(nullptr)
        , mHistory(nullptr)
        , mHistoryScroll(nullptr)
        , mCommandScroll(nullptr)
        , mPanelBackground(nullptr)
        , mDragHandle(nullptr)
        , mChatToolbar(nullptr)
        , mEmojiBar(nullptr)
        , mGroupPane(nullptr)
        , mHomePane(nullptr)
        , mGroupInfo(nullptr)
        , mGroupNameLabel(nullptr)
        , mGroupTargetLabel(nullptr)
        , mGroupNameEdit(nullptr)
        , mGroupTargetEdit(nullptr)
        , mTabChat(nullptr)
        , mTabGroup(nullptr)
        , mTabHome(nullptr)
        , mModeOoc(nullptr)
        , mModeRp(nullptr)
        , mChannelDefault(nullptr)
        , mChannelLocal(nullptr)
        , mChannelGlobal(nullptr)
        , mStylePlain(nullptr)
        , mStyleMe(nullptr)
        , mStyleDo(nullptr)
        , mStayOpenButton(nullptr)
        , mSendButton(nullptr)
        , mReturnButton(nullptr)
        , mEmojiToggleButton(nullptr)
        , mEmojiButtons{}
        , mGroupCreateButton(nullptr)
        , mGroupRefreshButton(nullptr)
        , mGroupInviteButton(nullptr)
        , mGroupLeaveButton(nullptr)
        , mGroupDisbandButton(nullptr)
        , mGroupKickButton(nullptr)
        , mGroupLeaderButton(nullptr)
        , mGroupJournalButton(nullptr)
        , mGroupTopicsButton(nullptr)
        , mGroupAcceptButton(nullptr)
        , mGroupDeclineButton(nullptr)
        , windowState(CHAT_TRANSPARENT_30)
        , chatChannel(CHANNEL_DEFAULT)
        , chatStyle(STYLE_PLAIN)
        , activeTab(TAB_CHAT)
        , rpMode(false)
        , stayOpenAfterSend(false)
        , emojiBarVisible(false)
        , groupInGroup(false)
        , groupIsLeader(false)
        , groupJournalSync(true)
        , groupTopicSync(true)
        , groupPendingInvite(false)
        , editState(false)
        , historyReviewState(false)
        , mainMenuOpen(false)
        , historyDisplayEnabled(true)
        , externalHistoryDisplayEnabled(true)
        , hideAfterFade(false)
        , geometryDirty(false)
        , geometrySaveDelay(0.f)
        , delay(3.f)
        , revealTime(0.f)
        , currentAlpha(sThirtyPercentTransparentAlpha)
        , targetAlpha(sThirtyPercentTransparentAlpha)
        , dragStartMouse(0, 0)
        , dragStartWindow(0, 0)
    {
        // X049 migration: the old passive chat was commonly only ~260 px wide.
        // The activated player menu needs enough width for the channel/style bar.
        // Keep the saved position while upgrading too-small legacy dimensions.
        const int panelWidth = std::max(sMinimumPanelWidth, w);
        const int panelHeight = std::max(sMinimumPanelHeight, h);
        setCoord(x, y, panelWidth, panelHeight);

        getWidget(mCommandLine, "edit_Command");
        getWidget(mHistory, "list_History");

        for (size_t i = 0; i < mHistory->getChildCount(); ++i)
        {
            if (MyGUI::ScrollBar* scroll = mHistory->getChildAt(i)->castType<MyGUI::ScrollBar>(false))
            {
                mHistoryScroll = scroll;
                break;
            }
        }
        if (mHistoryScroll)
        {
            mHistoryScroll->setVisible(false);
            mHistoryScroll->setNeedMouseFocus(false);
        }

        for (size_t i = 0; i < mCommandLine->getChildCount(); ++i)
        {
            if (MyGUI::ScrollBar* scroll = mCommandLine->getChildAt(i)->castType<MyGUI::ScrollBar>(false))
            {
                mCommandScroll = scroll;
                break;
            }
        }
        if (mCommandScroll)
        {
            mCommandScroll->setVisible(false);
            mCommandScroll->setNeedMouseFocus(false);
        }

        mCommandLine->eventEditSelectAccept += MyGUI::newDelegate(this, &GUIChat::acceptCommand);
        mCommandLine->eventKeyButtonPressed += MyGUI::newDelegate(this, &GUIChat::keyPress);
        mCommandLine->eventEditTextChange += MyGUI::newDelegate(this, &GUIChat::commandTextChanged);
        mCommandLine->setEditMultiLine(true);
        mCommandLine->setEditWordWrap(true);
        mCommandLine->setOverflowToTheLeft(false);

        setTitle("ArenaMP");

        mHistory->setOverflowToTheLeft(false);
        mHistory->setEditWordWrap(true);
        mHistory->setEditReadOnly(true);
        mHistory->setTextShadow(true);
        mHistory->setTextShadowColour(MyGUI::Colour::Black);
        mHistory->setNeedKeyFocus(false);
        setFont(Settings::Manager::getString("font", "Chat"));

        setupPlayerMenu();
        syncSettings();

        const MyGUI::IntSize view = MyGUI::RenderManager::getInstance().getViewSize();
        clampToViewport(view.width, view.height);
        if (panelWidth != w || panelHeight != h)
            persistGeometry();

        mCommandLine->setVisible(false);
        updateCommandLineLayout();
        refreshPlayerMenu();
        applyAlpha(currentAlpha);
    }

    void GUIChat::setupPlayerMenu()
    {
        getWidget(mPanelBackground, "PanelBackground");
        getWidget(mDragHandle, "DragHandle");
        getWidget(mChatToolbar, "ChatToolbar");
        getWidget(mEmojiBar, "EmojiBar");
        getWidget(mGroupPane, "GroupPane");
        getWidget(mHomePane, "HomePane");

        getWidget(mTabChat, "TabChat");
        getWidget(mTabGroup, "TabGroup");
        getWidget(mTabHome, "TabHome");
        getWidget(mModeOoc, "ModeOoc");
        getWidget(mModeRp, "ModeRp");
        getWidget(mChannelDefault, "ChannelDefault");
        getWidget(mChannelLocal, "ChannelLocal");
        getWidget(mChannelGlobal, "ChannelGlobal");
        getWidget(mStylePlain, "StylePlain");
        getWidget(mStyleMe, "StyleMe");
        getWidget(mStyleDo, "StyleDo");
        getWidget(mStayOpenButton, "StayOpenButton");
        getWidget(mSendButton, "SendButton");
        getWidget(mReturnButton, "ReturnButton");
        getWidget(mEmojiToggleButton, "EmojiToggle");
        getWidget(mEmojiButtons[0], "Emoji1");
        getWidget(mEmojiButtons[1], "Emoji2");
        getWidget(mEmojiButtons[2], "Emoji3");
        getWidget(mEmojiButtons[3], "Emoji4");
        getWidget(mEmojiButtons[4], "Emoji5");
        getWidget(mEmojiButtons[5], "Emoji6");

        getWidget(mGroupInfo, "GroupInfo");
        getWidget(mGroupNameLabel, "GroupNameLabel");
        getWidget(mGroupTargetLabel, "GroupTargetLabel");
        getWidget(mGroupNameEdit, "GroupNameEdit");
        getWidget(mGroupTargetEdit, "GroupTargetEdit");
        getWidget(mGroupCreateButton, "GroupCreateButton");
        getWidget(mGroupRefreshButton, "GroupRefreshButton");
        getWidget(mGroupInviteButton, "GroupInviteButton");
        getWidget(mGroupLeaveButton, "GroupLeaveButton");
        getWidget(mGroupDisbandButton, "GroupDisbandButton");
        getWidget(mGroupKickButton, "GroupKickButton");
        getWidget(mGroupLeaderButton, "GroupLeaderButton");
        getWidget(mGroupJournalButton, "GroupJournalButton");
        getWidget(mGroupTopicsButton, "GroupTopicsButton");
        getWidget(mGroupAcceptButton, "GroupAcceptButton");
        getWidget(mGroupDeclineButton, "GroupDeclineButton");

        MyGUI::TextBox* title = nullptr;
        MyGUI::TextBox* groupTitle = nullptr;
        MyGUI::TextBox* homeTitle = nullptr;
        MyGUI::EditBox* homeInfo = nullptr;
        getWidget(title, "PlayerMenuTitle");
        getWidget(groupTitle, "GroupTitle");
        getWidget(homeTitle, "HomeTitle");
        getWidget(homeInfo, "HomeInfo");

        title->setCaption(localizeArena("chat.menu.title"));
        mTabChat->setCaption(localizeArena("chat.tab.chat"));
        mTabGroup->setCaption(localizeArena("chat.tab.group"));
        mTabHome->setCaption(localizeArena("chat.tab.home"));
        mReturnButton->setCaption(localizeArena("chat.return_game"));
        mModeOoc->setCaption("OOC");
        mModeRp->setCaption("RP");
        mChannelDefault->setCaption(localizeArena("chat.channel.default"));
        mChannelLocal->setCaption(localizeArena("chat.channel.local"));
        mChannelGlobal->setCaption(localizeArena("chat.channel.global"));
        mStylePlain->setCaption(localizeArena("chat.style.plain"));
        mStyleMe->setCaption("/me");
        mStyleDo->setCaption("/do");
        mEmojiToggleButton->setCaption(localizeArena("chat.emoji"));
        mStayOpenButton->setCaption(localizeArena("chat.stay_after_send"));
        mSendButton->setCaption(localizeArena("chat.send"));

        groupTitle->setCaption(localizeArena("chat.group.title"));
        mGroupInfo->setCaption(localizeArena("chat.group.loading"));
        mGroupNameLabel->setCaption(localizeArena("chat.group.name"));
        mGroupTargetLabel->setCaption(localizeArena("chat.group.target"));
        mGroupCreateButton->setCaption(localizeArena("chat.group.create"));
        mGroupRefreshButton->setCaption(localizeArena("chat.group.refresh"));
        mGroupInviteButton->setCaption(localizeArena("chat.group.invite"));
        mGroupLeaveButton->setCaption(localizeArena("chat.group.leave"));
        mGroupDisbandButton->setCaption(localizeArena("chat.group.disband"));
        mGroupKickButton->setCaption(localizeArena("chat.group.kick"));
        mGroupLeaderButton->setCaption(localizeArena("chat.group.leader"));
        mGroupAcceptButton->setCaption(localizeArena("chat.group.accept"));
        mGroupDeclineButton->setCaption(localizeArena("chat.group.decline"));
        homeTitle->setCaption(localizeArena("chat.home.title"));
        homeInfo->setCaption(localizeArena("chat.home.placeholder"));

        const char* symbols[6] = { "\xE2\x98\xBA", "\xE2\x99\xA5", "\xE2\x98\x85", "\xE2\x98\x80", "\xE2\x98\x95", "\xE2\x9C\x93" };
        for (int i = 0; i < 6; ++i)
            mEmojiButtons[i]->setCaption(symbols[i]);

        mTabChat->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onTabClicked);
        mTabGroup->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onTabClicked);
        mTabHome->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onTabClicked);
        mModeOoc->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onModeClicked);
        mModeRp->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onModeClicked);
        mChannelDefault->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onChannelClicked);
        mChannelLocal->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onChannelClicked);
        mChannelGlobal->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onChannelClicked);
        mStylePlain->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onStyleClicked);
        mStyleMe->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onStyleClicked);
        mStyleDo->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onStyleClicked);
        mStayOpenButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onStayOpenClicked);
        mSendButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onSendClicked);
        mReturnButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onReturnClicked);
        mEmojiToggleButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onEmojiToggleClicked);
        for (int i = 0; i < 6; ++i)
            mEmojiButtons[i]->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onEmojiClicked);

        mGroupCreateButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupRefreshButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupInviteButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupLeaveButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupDisbandButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupKickButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupLeaderButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupJournalButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupTopicsButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupAcceptButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);
        mGroupDeclineButton->eventMouseButtonClick += MyGUI::newDelegate(this, &GUIChat::onGroupButtonClicked);

        mDragHandle->eventMouseButtonPressed += MyGUI::newDelegate(this, &GUIChat::onDragStart);
        mDragHandle->eventMouseDrag += MyGUI::newDelegate(this, &GUIChat::onDrag);

        // Keep the title bar draggable even though its label occupies part of it.
        title->setNeedMouseFocus(false);
        mGroupInfo->setEditReadOnly(true);
        homeInfo->setEditReadOnly(true);
        updateGroupControls();
    }

    void GUIChat::onOpen()
    {
        applyAlpha(currentAlpha);
    }

    void GUIChat::onClose()
    {
        if (geometryDirty)
            persistGeometry();
    }

    bool GUIChat::exit()
    {
        if (editState)
        {
            setEditState(false);
            return false;
        }
        return true;
    }

    bool GUIChat::getEditState()
    {
        return editState;
    }

    void GUIChat::acceptCommand(MyGUI::EditBox*)
    {
        if (!editState || activeTab != TAB_CHAT)
            return;

        const std::string cm = mCommandLine->getOnlyText();

        // With "stay in menu" enabled, an empty Enter is harmless. Without it,
        // retain the historical quick-cancel behaviour.
        if (cm.empty())
        {
            if (!stayOpenAfterSend)
                setEditState(false);
            return;
        }

        LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Player: %s", cm.c_str());

        if (mCommandHistory.empty() || mCommandHistory.back() != cm)
            mCommandHistory.push_back(cm);
        mCurrent = mCommandHistory.end();
        mEditString.clear();

        mCommandLine->setCaption("");
        const std::string outgoing = buildOutgoingMessage(cm);
        send(outgoing);

        if (stayOpenAfterSend)
        {
            selectTab(TAB_CHAT, false);
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
            updateCommandLineLayout();
        }
        else
            setEditState(false);
    }

    void GUIChat::onResChange(int width, int height)
    {
        // X049: never reset to the historical full-width rectangle on resolution
        // changes. Clamp the player's saved/moved panel instead.
        clampToViewport(width, height);
        updateCommandLineLayout();
    }

    void GUIChat::setFont(const std::string &fntName)
    {
        mHistory->setFontName(fntName);
        mCommandLine->setFontName(fntName);
    }

    void GUIChat::print(const std::string &msg, const std::string &color)
    {
        if (msg.empty())
        {
            clean();
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "Chat cleaned");
        }
        else
        {
            mHistory->addText(color + msg);
            if (!historyReviewState)
                scrollHistoryToBottom();
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_INFO, "%s", msg.c_str());

            if (historyDisplayEnabled && windowState != CHAT_HIDDEN && !mainMenuOpen)
                revealTemporarily();
        }
    }

    void GUIChat::printOK(const std::string &msg)
    {
        print(msg + "\n", "#FF00FF");
    }

    void GUIChat::printError(const std::string &msg)
    {
        print(msg + "\n", "#FF2222");
    }

    bool GUIChat::handleServerControlMessage(const std::string& msg)
    {
        static const std::string groupPrefix = "@@AMP_GROUP@@";
        static const std::string xpPrefix = "@@AMP_XP@@";

        if (msg.compare(0, groupPrefix.size(), groupPrefix) == 0)
        {
            std::vector<std::string> fields = splitControlFields(msg.substr(groupPrefix.size()), '\t');
            for (std::string& field : fields)
                field = unescapeControlField(field);
            if (!fields.empty() && fields[0] == "STATE")
                rebuildGroupInfo(fields);
            return true;
        }

        if (msg.compare(0, xpPrefix.size(), xpPrefix) == 0)
        {
            std::vector<std::string> fields = splitControlFields(msg.substr(xpPrefix.size()), '\t');
            if (fields.size() >= 2)
            {
                char* end = nullptr;
                const float amount = std::strtof(fields[0].c_str(), &end);
                const bool scaled = fields[1] == "1";
                std::string reason = fields.size() > 3 ? unescapeControlField(fields[3]) : std::string();
                if (reason.empty() && fields.size() > 2)
                {
                    const std::string kind = fields[2];
                    reason = kind == "kill" ? localizeArena("chat.group.xp.kill")
                        : localizeArena("chat.group.xp.quest");
                }
                if (end != fields[0].c_str() && amount > 0.f)
                    MWMechanics::XPLeveling::awardServer(amount, scaled, reason);
            }
            return true;
        }

        return false;
    }

    void GUIChat::send(const std::string &str)
    {
        LocalPlayer *localPlayer = Main::get().getLocalPlayer();
        Networking *networking = Main::get().getNetworking();

        localPlayer->chatMessage = str;
        networking->getPlayerPacket(ID_CHAT_MESSAGE)->setPlayer(localPlayer);
        networking->getPlayerPacket(ID_CHAT_MESSAGE)->Send();
    }

    std::string GUIChat::buildOutgoingMessage(const std::string& text) const
    {
        // User-entered slash commands always bypass the visual mode selector.
        // This keeps /help and every existing server command 100% compatible.
        if (!text.empty() && text[0] == '/')
            return text;

        if (chatChannel == CHANNEL_DEFAULT && chatStyle == STYLE_PLAIN && !rpMode)
            return text;

        std::string channel = "default";
        if (chatChannel == CHANNEL_LOCAL)
            channel = "local";
        else if (chatChannel == CHANNEL_GLOBAL)
            channel = "global";

        std::string style = "plain";
        if (chatStyle == STYLE_ME)
            style = "me";
        else if (chatStyle == STYLE_DO)
            style = "do";

        return "/ampchat " + channel + " " + style + " " + (rpMode ? "1 " : "0 ") + text;
    }

    void GUIChat::clean()
    {
        mHistory->setCaption("");
        scrollHistoryToBottom();
    }

    void GUIChat::pressedChatMode()
    {
        if (!historyDisplayEnabled)
        {
            MWBase::Environment::get().getWindowManager()->messageBox(localizeArena("chat.history_hidden"));
            return;
        }

        windowState = static_cast<ChatWindowState>((static_cast<int>(windowState) + 1) % CHAT_STATE_COUNT);
        Settings::Manager::setString("mode", "Chat", getModeSetting());
        Settings::Manager::saveUser();
        revealTime = windowState == CHAT_AUTOHIDE ? delay : 0.f;

        const std::string chatMode = getModeMessage();
        LOG_MESSAGE_SIMPLE(TimedLog::LOG_VERBOSE, "Switch chat mode to %s", chatMode.c_str());
        MWBase::Environment::get().getWindowManager()->messageBox(chatMode);

        if (windowState == CHAT_HIDDEN)
        {
            setHistoryReviewState(false);
            if (editState)
                setEditState(false);
        }

        refreshPresentation();
    }

    void GUIChat::setEditState(bool state)
    {
        if (state && historyReviewState)
            setHistoryReviewState(false);

        if (editState == state)
        {
            if (state && activeTab == TAB_CHAT)
                MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
            return;
        }

        editState = state;
        emojiBarVisible = false;

        if (editState)
        {
            selectTab(TAB_CHAT, false);
            revealTime = 0.f;
            MWBase::Environment::get().getInputManager()->changeInputMode(true);
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
            mMainWidget->setNeedMouseFocus(true);
        }
        else
        {
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(nullptr);
            mMainWidget->setNeedMouseFocus(false);
            if (!mainMenuOpen && !historyReviewState)
                MWBase::Environment::get().getInputManager()->changeInputMode(false);
            if (windowState == CHAT_AUTOHIDE)
                revealTime = delay;
            if (geometryDirty)
                persistGeometry();
        }

        refreshPlayerMenu();
        updateCommandLineLayout();
        refreshPresentation();
    }

    void GUIChat::commandTextChanged(MyGUI::EditBox*)
    {
        updateCommandLineLayout();
    }

    void GUIChat::updateCommandLineLayout()
    {
        if (!mMainWidget || !mCommandLine || !mHistory)
            return;

        const int mainWidth = mMainWidget->getWidth();
        const int mainHeight = mMainWidget->getHeight();

        if (!editState)
        {
            mHistory->setCoord(8, 8, std::max(32, mainWidth - 16), std::max(20, mainHeight - 16));
            mCommandLine->setVisible(false);
            return;
        }

        if (activeTab != TAB_CHAT)
        {
            mCommandLine->setVisible(false);
            return;
        }

        constexpr int lineHeight = 20;
        constexpr int maxVisibleLines = 5;
        constexpr int inputPadding = 8;
        constexpr int sideMargin = 10;
        constexpr int bottomFooterHeight = 46;
        constexpr int gap = 8;

        const std::string text = mCommandLine->getOnlyText();
        int explicitLines = 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n'));
        const int measuredHeight = std::max(lineHeight, mCommandLine->getTextSize().height);
        const int measuredLines = std::max(1, (measuredHeight + lineHeight - 1) / lineHeight);
        const int contentLines = std::max(explicitLines, measuredLines);
        const int visibleLines = std::min(maxVisibleLines, contentLines);
        const int commandHeight = inputPadding + visibleLines * lineHeight;
        const int commandBottom = std::max(150, mainHeight - bottomFooterHeight);
        const int commandTop = std::max(118, commandBottom - commandHeight);

        mCommandLine->setCoord(sideMargin, commandTop,
            std::max(32, mainWidth - sideMargin * 2), std::max(28, commandBottom - commandTop));
        mCommandLine->setEditMultiLine(true);
        mCommandLine->setEditWordWrap(true);
        mCommandLine->setOverflowToTheLeft(false);

        const int historyTop = emojiBarVisible ? 114 : 78;
        const int historyHeight = std::max(40, commandTop - gap - historyTop);
        mHistory->setCoord(sideMargin, historyTop,
            std::max(32, mainWidth - sideMargin * 2), historyHeight);

        if (mCommandScroll)
        {
            const bool overflow = contentLines > maxVisibleLines;
            mCommandScroll->setVisible(overflow);
            mCommandScroll->setNeedMouseFocus(overflow);
        }
    }

    void GUIChat::scrollHistoryToBottom()
    {
        const size_t range = mHistory->getVScrollRange();
        if (range > 0)
            mHistory->setVScrollPosition(range - 1);
        mHistory->setTextCursor(mHistory->getCaption().size());
    }

    void GUIChat::setHistoryReviewState(bool state)
    {
        if (historyReviewState == state)
            return;

        historyReviewState = state;
        if (state)
        {
            if (editState)
                setEditState(false);
            mMainWidget->setNeedMouseFocus(true);
            mHistory->setNeedMouseFocus(true);
            mHistory->setNeedKeyFocus(true);
            if (mHistoryScroll)
            {
                mHistoryScroll->setVisible(true);
                mHistoryScroll->setNeedMouseFocus(true);
            }
            MWBase::Environment::get().getInputManager()->changeInputMode(true);
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mHistory);
        }
        else
        {
            mMainWidget->setNeedMouseFocus(false);
            mHistory->setNeedMouseFocus(false);
            mHistory->setNeedKeyFocus(false);
            if (mHistoryScroll)
            {
                mHistoryScroll->setVisible(false);
                mHistoryScroll->setNeedMouseFocus(false);
            }
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(nullptr);
            if (!mainMenuOpen && !editState)
                MWBase::Environment::get().getInputManager()->changeInputMode(false);
            scrollHistoryToBottom();
            if (windowState == CHAT_AUTOHIDE)
                revealTime = delay;
        }

        updateCommandLineLayout();
        refreshPresentation();
    }

    std::string GUIChat::getHistoryText() const
    {
        return mHistory->getCaption().asUTF8();
    }

    void GUIChat::setMainMenuOpen(bool state)
    {
        if (mainMenuOpen == state)
            return;

        mainMenuOpen = state;
        if (state)
        {
            editState = false;
            emojiBarVisible = false;
            mCommandLine->setVisible(false);
            historyReviewState = false;
            mMainWidget->setNeedMouseFocus(false);
            mHistory->setNeedMouseFocus(false);
            mHistory->setNeedKeyFocus(false);
            if (mHistoryScroll)
            {
                mHistoryScroll->setVisible(false);
                mHistoryScroll->setNeedMouseFocus(false);
            }
            currentAlpha = 0.f;
            targetAlpha = 0.f;
            hideAfterFade = false;
            refreshPlayerMenu();
            applyAlpha(0.f);
            setVisible(false);
        }
        else
        {
            updateCommandLineLayout();
            refreshPlayerMenu();
            refreshPresentation();
        }
    }

    void GUIChat::pressedSay()
    {
        if (!editState)
            LOG_MESSAGE_SIMPLE(TimedLog::LOG_VERBOSE, "Opening ArenaMP player chat menu.");

        setEditState(true);
        selectTab(TAB_CHAT, false);
        mCommandLine->setVisible(true);
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
    }

    void GUIChat::keyPress(MyGUI::Widget*, MyGUI::KeyCode key, MyGUI::Char)
    {
        if (key == MyGUI::KeyCode::Escape)
        {
            setEditState(false);
            return;
        }

        if (key == MyGUI::KeyCode::Return || key == MyGUI::KeyCode::NumpadEnter)
        {
            acceptCommand(mCommandLine);
            return;
        }

        updateCommandLineLayout();

        const bool multiRow = mCommandLine->getOnlyText().find('\n') != std::string::npos
            || mCommandLine->getTextSize().height > 24;
        if (multiRow && (key == MyGUI::KeyCode::ArrowUp || key == MyGUI::KeyCode::ArrowDown))
            return;

        if (mCommandHistory.empty())
            return;

        if (key == MyGUI::KeyCode::ArrowUp)
        {
            if (mCurrent == mCommandHistory.end())
                mEditString = mCommandLine->getOnlyText();

            if (mCurrent != mCommandHistory.begin())
            {
                --mCurrent;
                mCommandLine->setCaption(*mCurrent);
            }
        }
        else if (key == MyGUI::KeyCode::ArrowDown)
        {
            if (mCurrent != mCommandHistory.end())
            {
                ++mCurrent;
                if (mCurrent != mCommandHistory.end())
                    mCommandLine->setCaption(*mCurrent);
                else
                    mCommandLine->setCaption(mEditString);
            }
        }
    }

    void GUIChat::update(float dt)
    {
        syncSettings();
        if (editState && activeTab == TAB_CHAT)
            updateCommandLineLayout();

        if (geometryDirty)
        {
            geometrySaveDelay = std::max(0.f, geometrySaveDelay - std::max(0.f, dt));
            if (geometrySaveDelay <= 0.f)
                persistGeometry();
        }

        if (mainMenuOpen)
            return;

        if (revealTime > 0.f && !editState && !historyReviewState)
        {
            revealTime = std::max(0.f, revealTime - dt);
            if (revealTime == 0.f)
                refreshPresentation();
        }

        if (!isVisible())
            return;

        currentAlpha = moveTowards(currentAlpha, targetAlpha, std::max(0.f, dt) * sFadeSpeed);
        applyAlpha(currentAlpha);

        if (hideAfterFade && currentAlpha <= 0.001f)
        {
            currentAlpha = 0.f;
            targetAlpha = 0.f;
            hideAfterFade = false;
            applyAlpha(0.f);
            setVisible(false);
        }
    }

    void GUIChat::refreshPresentation()
    {
        if (mainMenuOpen)
            return;

        refreshPlayerMenu();

        // Input/player-menu mode is always fully opaque regardless of HUD opacity.
        if (editState)
        {
            showSmoothly(sFullyVisibleAlpha);
            return;
        }

        if (!historyDisplayEnabled)
        {
            hideSmoothly();
            return;
        }

        if (historyReviewState)
        {
            showSmoothly(sFullyVisibleAlpha);
            return;
        }

        if (revealTime > 0.f && windowState != CHAT_HIDDEN)
        {
            showSmoothly(sFullyVisibleAlpha);
            return;
        }

        const float restingAlpha = getRestingAlpha();
        if (restingAlpha > 0.f)
            showSmoothly(restingAlpha);
        else
            hideSmoothly();
    }

    void GUIChat::refreshPlayerMenu()
    {
        const bool menuVisible = editState && !mainMenuOpen;
        const bool chatVisible = menuVisible && activeTab == TAB_CHAT;

        mPanelBackground->setVisible(menuVisible);
        mDragHandle->setVisible(menuVisible);
        mChatToolbar->setVisible(chatVisible);
        mStayOpenButton->setVisible(chatVisible);
        mSendButton->setVisible(chatVisible);
        mEmojiBar->setVisible(chatVisible && emojiBarVisible);
        mGroupPane->setVisible(menuVisible && activeTab == TAB_GROUP);
        mHomePane->setVisible(menuVisible && activeTab == TAB_HOME);

        mCommandLine->setVisible(chatVisible);
        mHistory->setVisible(historyDisplayEnabled && (!menuVisible || chatVisible));

        mHistory->setNeedMouseFocus(chatVisible);
        if (mHistoryScroll)
        {
            mHistoryScroll->setVisible(chatVisible || historyReviewState);
            mHistoryScroll->setNeedMouseFocus(chatVisible || historyReviewState);
        }

        updateToggleButtons();
        updateGroupControls();
    }

    void GUIChat::selectTab(PlayerMenuTab tab, bool persist)
    {
        activeTab = tab;
        if (persist)
        {
            Settings::Manager::setInt("player menu tab", "Chat", static_cast<int>(activeTab));
            Settings::Manager::saveUser();
        }

        emojiBarVisible = false;
        refreshPlayerMenu();
        updateCommandLineLayout();

        if (!editState)
            return;

        if (activeTab == TAB_CHAT)
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
        else
        {
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(nullptr);
            if (activeTab == TAB_GROUP)
                requestGroupState();
        }
    }

    void GUIChat::updateToggleButtons()
    {
        mTabChat->setStateSelected(activeTab == TAB_CHAT);
        mTabGroup->setStateSelected(activeTab == TAB_GROUP);
        mTabHome->setStateSelected(activeTab == TAB_HOME);
        mModeOoc->setStateSelected(!rpMode);
        mModeRp->setStateSelected(rpMode);
        mChannelDefault->setStateSelected(chatChannel == CHANNEL_DEFAULT);
        mChannelLocal->setStateSelected(chatChannel == CHANNEL_LOCAL);
        mChannelGlobal->setStateSelected(chatChannel == CHANNEL_GLOBAL);
        mStylePlain->setStateSelected(chatStyle == STYLE_PLAIN);
        mStyleMe->setStateSelected(chatStyle == STYLE_ME);
        mStyleDo->setStateSelected(chatStyle == STYLE_DO);
        mStayOpenButton->setStateSelected(stayOpenAfterSend);
        mEmojiToggleButton->setStateSelected(emojiBarVisible);
    }

    void GUIChat::setChatChannel(ChatChannel channel)
    {
        chatChannel = channel;
        const char* setting = chatChannel == CHANNEL_LOCAL ? "local"
            : (chatChannel == CHANNEL_GLOBAL ? "global" : "default");
        Settings::Manager::setString("send channel", "Chat", setting);
        Settings::Manager::saveUser();
        updateToggleButtons();
    }

    void GUIChat::setChatStyle(ChatStyle style)
    {
        chatStyle = style;
        const char* setting = chatStyle == STYLE_ME ? "me" : (chatStyle == STYLE_DO ? "do" : "plain");
        Settings::Manager::setString("send style", "Chat", setting);
        Settings::Manager::saveUser();
        updateToggleButtons();
    }

    void GUIChat::setRpMode(bool enabled)
    {
        rpMode = enabled;
        Settings::Manager::setBool("rp mode", "Chat", rpMode);
        Settings::Manager::saveUser();
        updateToggleButtons();
    }

    void GUIChat::setStayOpenAfterSend(bool enabled)
    {
        stayOpenAfterSend = enabled;
        Settings::Manager::setBool("stay after send", "Chat", stayOpenAfterSend);
        Settings::Manager::saveUser();
        updateToggleButtons();
    }

    void GUIChat::requestGroupState()
    {
        send("/groupui state");
    }

    void GUIChat::sendGroupAction(const std::string& action, const std::string& argument)
    {
        std::string command = "/groupui " + action;
        if (!argument.empty())
            command += " " + argument;
        send(command);
    }

    void GUIChat::updateGroupControls()
    {
        if (mGroupCreateButton == nullptr)
            return;

        const bool leaderTools = groupInGroup && groupIsLeader;
        mGroupNameLabel->setVisible(!groupInGroup);
        mGroupNameEdit->setVisible(!groupInGroup);
        mGroupCreateButton->setVisible(!groupInGroup);

        mGroupTargetLabel->setVisible(leaderTools);
        mGroupTargetEdit->setVisible(leaderTools);
        mGroupInviteButton->setVisible(leaderTools);
        mGroupKickButton->setVisible(leaderTools);
        mGroupLeaderButton->setVisible(leaderTools);

        mGroupJournalButton->setVisible(groupInGroup);
        mGroupTopicsButton->setVisible(groupInGroup);
        mGroupLeaveButton->setVisible(groupInGroup);
        mGroupDisbandButton->setVisible(leaderTools);

        mGroupAcceptButton->setVisible(!groupInGroup && groupPendingInvite);
        mGroupDeclineButton->setVisible(!groupInGroup && groupPendingInvite);
        mGroupRefreshButton->setVisible(true);

        mGroupJournalButton->setStateSelected(groupJournalSync);
        mGroupTopicsButton->setStateSelected(groupTopicSync);
        mGroupJournalButton->setCaption(localizeArena("chat.group.sync_journal") + ": "
            + localizeArena(groupJournalSync ? "chat.group.on" : "chat.group.off"));
        mGroupTopicsButton->setCaption(localizeArena("chat.group.sync_topics") + ": "
            + localizeArena(groupTopicSync ? "chat.group.on" : "chat.group.off"));
    }

    void GUIChat::rebuildGroupInfo(const std::vector<std::string>& fields)
    {
        groupInGroup = controlBool(fields, 1);
        groupIsLeader = controlBool(fields, 2);
        groupJournalSync = controlBool(fields, 3);
        groupTopicSync = controlBool(fields, 4);
        groupPendingInvite = controlBool(fields, 5);

        const std::string groupName = fields.size() > 6 ? fields[6] : std::string();
        const std::string leaderName = fields.size() > 7 ? fields[7] : std::string();
        const std::string membersField = fields.size() > 8 ? fields[8] : std::string();
        const std::string inviteFrom = fields.size() > 9 ? fields[9] : std::string();
        const std::string inviteGroup = fields.size() > 10 ? fields[10] : std::string();

        std::ostringstream info;
        if (!groupInGroup)
        {
            info << localizeArena("chat.group.not_member");
            if (groupPendingInvite)
            {
                info << "\n\n" << localizeArena("chat.group.invite_pending") << "\n"
                     << localizeArena("chat.group.inviter") << ": " << inviteFrom << "\n"
                     << localizeArena("chat.group.group") << ": " << inviteGroup;
            }
        }
        else
        {
            info << localizeArena("chat.group.group") << ": " << groupName << "\n"
                 << localizeArena("chat.group.role") << ": "
                 << localizeArena(groupIsLeader ? "chat.group.role_leader" : "chat.group.role_member") << "\n"
                 << localizeArena("chat.group.leader_name") << ": " << leaderName << "\n"
                 << localizeArena("chat.group.xp_rule") << "\n\n"
                 << localizeArena("chat.group.members") << ":";

            const std::vector<std::string> members = splitControlFields(membersField, ';');
            for (const std::string& encoded : members)
            {
                if (encoded.empty())
                    continue;
                const std::vector<std::string> member = splitControlFields(encoded, '^');
                const std::string name = !member.empty() ? member[0] : "?";
                const bool online = member.size() > 1 && member[1] == "1";
                const bool sameCell = member.size() > 2 && member[2] == "1";
                const bool leader = member.size() > 3 && member[3] == "1";
                info << "\n" << (leader ? "* " : "- ") << name << " ["
                     << localizeArena(!online ? "chat.group.offline" : (sameCell ? "chat.group.same_cell" : "chat.group.other_cell"))
                     << "]";
            }
        }

        mGroupInfo->setCaption(info.str());
        updateGroupControls();
    }

    void GUIChat::onTabClicked(MyGUI::Widget* sender)
    {
        if (sender == mTabGroup)
            selectTab(TAB_GROUP);
        else if (sender == mTabHome)
            selectTab(TAB_HOME);
        else
            selectTab(TAB_CHAT);
    }

    void GUIChat::onModeClicked(MyGUI::Widget* sender)
    {
        setRpMode(sender == mModeRp);
    }

    void GUIChat::onChannelClicked(MyGUI::Widget* sender)
    {
        if (sender == mChannelLocal)
            setChatChannel(CHANNEL_LOCAL);
        else if (sender == mChannelGlobal)
            setChatChannel(CHANNEL_GLOBAL);
        else
            setChatChannel(CHANNEL_DEFAULT);
    }

    void GUIChat::onStyleClicked(MyGUI::Widget* sender)
    {
        if (sender == mStyleMe)
            setChatStyle(STYLE_ME);
        else if (sender == mStyleDo)
            setChatStyle(STYLE_DO);
        else
            setChatStyle(STYLE_PLAIN);
    }

    void GUIChat::onStayOpenClicked(MyGUI::Widget*)
    {
        setStayOpenAfterSend(!stayOpenAfterSend);
    }

    void GUIChat::onSendClicked(MyGUI::Widget*)
    {
        acceptCommand(mCommandLine);
    }

    void GUIChat::onReturnClicked(MyGUI::Widget*)
    {
        setEditState(false);
    }

    void GUIChat::onEmojiToggleClicked(MyGUI::Widget*)
    {
        emojiBarVisible = !emojiBarVisible;
        refreshPlayerMenu();
        updateCommandLineLayout();
    }

    void GUIChat::onEmojiClicked(MyGUI::Widget* sender)
    {
        static const char* symbols[6] = { "\xE2\x98\xBA", "\xE2\x99\xA5", "\xE2\x98\x85", "\xE2\x98\x80", "\xE2\x98\x95", "\xE2\x9C\x93" };
        for (int i = 0; i < 6; ++i)
        {
            if (sender != mEmojiButtons[i])
                continue;

            MyGUI::UString text = mCommandLine->getCaption();
            text.insert(text.size(), MyGUI::UString(symbols[i]));
            mCommandLine->setCaption(text);
            mCommandLine->setTextCursor(text.size());
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
            updateCommandLineLayout();
            break;
        }
    }

    void GUIChat::onGroupButtonClicked(MyGUI::Widget* sender)
    {
        if (sender == mGroupRefreshButton)
            requestGroupState();
        else if (sender == mGroupCreateButton)
        {
            sendGroupAction("create", mGroupNameEdit->getOnlyText());
            mGroupNameEdit->setCaption("");
        }
        else if (sender == mGroupInviteButton)
            sendGroupAction("invite", mGroupTargetEdit->getOnlyText());
        else if (sender == mGroupKickButton)
            sendGroupAction("kick", mGroupTargetEdit->getOnlyText());
        else if (sender == mGroupLeaderButton)
            sendGroupAction("leader", mGroupTargetEdit->getOnlyText());
        else if (sender == mGroupJournalButton)
            sendGroupAction("journal");
        else if (sender == mGroupTopicsButton)
            sendGroupAction("topics");
        else if (sender == mGroupLeaveButton)
            sendGroupAction("leave");
        else if (sender == mGroupDisbandButton)
            sendGroupAction("disband");
        else if (sender == mGroupAcceptButton)
            sendGroupAction("accept");
        else if (sender == mGroupDeclineButton)
            sendGroupAction("decline");
    }

    void GUIChat::onDragStart(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left || !editState)
            return;
        dragStartMouse = MyGUI::InputManager::getInstance().getMousePosition();
        dragStartWindow = mMainWidget->getPosition();
    }

    void GUIChat::onDrag(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left || !editState)
            return;

        const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
        const MyGUI::IntPoint delta = mouse - dragStartMouse;
        mMainWidget->setPosition(dragStartWindow + delta);
        const MyGUI::IntSize view = MyGUI::RenderManager::getInstance().getViewSize();
        clampToViewport(view.width, view.height);
        markGeometryDirty();
    }

    void GUIChat::clampToViewport(int width, int height)
    {
        if (!mMainWidget || width <= 0 || height <= 0)
            return;

        MyGUI::IntSize size = mMainWidget->getSize();
        size.width = std::min(std::max(sMinimumPanelWidth, size.width), std::max(sMinimumPanelWidth, width));
        size.height = std::min(std::max(sMinimumPanelHeight, size.height), std::max(sMinimumPanelHeight, height));
        if (width < sMinimumPanelWidth)
            size.width = width;
        if (height < sMinimumPanelHeight)
            size.height = height;
        mMainWidget->setSize(size);

        MyGUI::IntPoint pos = mMainWidget->getPosition();
        pos.left = std::max(0, std::min(pos.left, std::max(0, width - size.width)));
        pos.top = std::max(0, std::min(pos.top, std::max(0, height - size.height)));
        mMainWidget->setPosition(pos);
    }

    void GUIChat::markGeometryDirty()
    {
        geometryDirty = true;
        geometrySaveDelay = sGeometrySaveDebounce;
    }

    void GUIChat::persistGeometry()
    {
        if (!mMainWidget)
            return;

        const MyGUI::IntCoord coord = mMainWidget->getCoord();
        Settings::Manager::setInt("x", "Chat", coord.left);
        Settings::Manager::setInt("y", "Chat", coord.top);
        Settings::Manager::setInt("w", "Chat", coord.width);
        Settings::Manager::setInt("h", "Chat", coord.height);
        Settings::Manager::setInt("panel layout version", "Chat", 1);
        Settings::Manager::saveUser();
        geometryDirty = false;
        geometrySaveDelay = 0.f;
    }

    void GUIChat::revealTemporarily()
    {
        revealTime = delay;
        refreshPresentation();
    }

    void GUIChat::showSmoothly(float alpha)
    {
        targetAlpha = std::max(0.f, std::min(1.f, alpha));
        hideAfterFade = false;

        if (!isVisible())
        {
            currentAlpha = 0.f;
            applyAlpha(0.f);
            setVisible(true);
        }
    }

    void GUIChat::hideSmoothly()
    {
        targetAlpha = 0.f;
        hideAfterFade = isVisible();

        if (!isVisible())
        {
            currentAlpha = 0.f;
            hideAfterFade = false;
        }
    }

    void GUIChat::applyAlpha(float alpha)
    {
        // Player-menu chrome stays readable while the passive history still obeys
        // the legacy opacity/autohide state machine.
        if (editState)
        {
            mHistory->setAlpha(1.f);
            mCommandLine->setAlpha(1.f);
            mPanelBackground->setAlpha(1.f);
            mDragHandle->setAlpha(1.f);
            mChatToolbar->setAlpha(1.f);
            mGroupPane->setAlpha(1.f);
            mHomePane->setAlpha(1.f);
        }
        else
        {
            mHistory->setAlpha(alpha);
            mCommandLine->setAlpha(alpha);
        }
    }

    float GUIChat::getRestingAlpha() const
    {
        switch (windowState)
        {
            case CHAT_VISIBLE: return sFullyVisibleAlpha;
            case CHAT_TRANSPARENT_30: return sThirtyPercentTransparentAlpha;
            case CHAT_TRANSPARENT_60: return sSixtyPercentTransparentAlpha;
            default: return 0.f;
        }
    }

    std::string GUIChat::getModeMessage() const
    {
        switch (windowState)
        {
            case CHAT_VISIBLE: return localizeArena("chat.mode.visible");
            case CHAT_TRANSPARENT_30: return localizeArena("chat.mode.opacity_30");
            case CHAT_TRANSPARENT_60: return localizeArena("chat.mode.opacity_60");
            case CHAT_AUTOHIDE: return localizeArena("chat.mode.autohide");
            case CHAT_HIDDEN: return localizeArena("chat.mode.hidden");
            default: return localizeArena("chat.mode.visible");
        }
    }

    std::string GUIChat::getModeSetting() const
    {
        switch (windowState)
        {
            case CHAT_VISIBLE: return "visible";
            case CHAT_TRANSPARENT_30: return "transparent30";
            case CHAT_TRANSPARENT_60: return "transparent60";
            case CHAT_AUTOHIDE: return "autohide";
            case CHAT_HIDDEN: return "hidden";
            default: return "transparent30";
        }
    }

    void GUIChat::setModeFromSetting(const std::string& mode)
    {
        if (mode == "visible")
            windowState = CHAT_VISIBLE;
        else if (mode == "transparent60")
            windowState = CHAT_TRANSPARENT_60;
        else if (mode == "autohide")
            windowState = CHAT_AUTOHIDE;
        else if (mode == "hidden")
            windowState = CHAT_HIDDEN;
        else
            windowState = CHAT_TRANSPARENT_30;
    }

    void GUIChat::syncSettings()
    {
        const bool enabled = externalHistoryDisplayEnabled && Settings::Manager::getBool("enabled", "Chat");
        const float configuredDelay = std::max(0.f, Settings::Manager::getFloat("delay", "Chat"));
        const std::string configuredMode = Settings::Manager::getString("mode", "Chat");

        bool changed = false;
        if (historyDisplayEnabled != enabled)
        {
            historyDisplayEnabled = enabled;
            if (!enabled && historyReviewState)
                setHistoryReviewState(false);
            changed = true;
        }
        if (std::abs(delay - configuredDelay) > 0.001f)
        {
            delay = configuredDelay;
            revealTime = std::min(revealTime, delay);
            changed = true;
        }

        const ChatWindowState previousState = windowState;
        setModeFromSetting(configuredMode);
        if (windowState != previousState)
        {
            if (windowState == CHAT_HIDDEN && !editState)
            {
                if (historyReviewState)
                    setHistoryReviewState(false);
            }
            revealTime = windowState == CHAT_AUTOHIDE ? delay : 0.f;
            changed = true;
        }

        const std::string channel = Settings::Manager::getString("send channel", "Chat");
        const ChatChannel configuredChannel = channel == "local" ? CHANNEL_LOCAL
            : (channel == "global" ? CHANNEL_GLOBAL : CHANNEL_DEFAULT);
        if (chatChannel != configuredChannel)
        {
            chatChannel = configuredChannel;
            changed = true;
        }

        const std::string style = Settings::Manager::getString("send style", "Chat");
        const ChatStyle configuredStyle = style == "me" ? STYLE_ME : (style == "do" ? STYLE_DO : STYLE_PLAIN);
        if (chatStyle != configuredStyle)
        {
            chatStyle = configuredStyle;
            changed = true;
        }

        const bool configuredRp = Settings::Manager::getBool("rp mode", "Chat");
        if (rpMode != configuredRp)
        {
            rpMode = configuredRp;
            changed = true;
        }

        const bool configuredStay = Settings::Manager::getBool("stay after send", "Chat");
        if (stayOpenAfterSend != configuredStay)
        {
            stayOpenAfterSend = configuredStay;
            changed = true;
        }

        if (changed)
        {
            updateToggleButtons();
            refreshPresentation();
        }
    }

    void GUIChat::setHistoryDisplayEnabled(bool enabled)
    {
        externalHistoryDisplayEnabled = enabled;
        syncSettings();
    }

    void GUIChat::setDelay(float newDelay)
    {
        delay = std::max(0.f, newDelay);
        if (revealTime > delay)
            revealTime = delay;
    }
}
