#ifndef OPENMW_GUICHAT_HPP
#define OPENMW_GUICHAT_HPP

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <components/esm/custommarkerstate.hpp>

#include "apps/openmw/mwgui/windowbase.hpp"

namespace MyGUI
{
    class Button;
    class ListBox;
    class ScrollBar;
    class TextBox;
    class Widget;
}

namespace mwmp
{
    class GUIController;

    class GUIChat : public MWGui::WindowBase
    {
        friend class GUIController;
    public:
        enum ChatWindowState
        {
            CHAT_VISIBLE = 0,
            CHAT_TRANSPARENT_30,
            CHAT_TRANSPARENT_60,
            CHAT_AUTOHIDE,
            CHAT_HIDDEN,
            CHAT_STATE_COUNT
        };

        // X054: these mirror coreChat's own channels one for one. The menu no
        // longer invents a parallel chat format - it emits the same commands a
        // player would type, so /s, /w, /sh, // and /// behave identically
        // whether they come from the toolbar or from the keyboard.
        enum ChatChannel
        {
            CHANNEL_SAY = 0,
            CHANNEL_WHISPER,
            CHANNEL_SHOUT,
            CHANNEL_LOCAL_OOC,
            CHANNEL_GLOBAL_OOC
        };

        enum ChatStyle
        {
            STYLE_PLAIN = 0,
            STYLE_ME,
            STYLE_DO,
            STYLE_TRY
        };

        enum PlayerMenuTab
        {
            TAB_CHAT = 0,
            TAB_GROUP,
            TAB_PLAYERS
        };

        // X052: only one auxiliary strip may occupy the row under the toolbar.
        enum ChatDrawer
        {
            DRAWER_NONE = 0,
            DRAWER_EMOJI,
            DRAWER_COLOR
        };

        static const int sEmojiSlotCount = 20;
        // X054: coreChat ships 40 nickname colours and /color offers all of
        // them, so the menu strip has to carry the same 40. Only the first
        // sLayoutColorSlotCount exist in tes3mp_chat.layout; the rest are
        // created at runtime, so the .layout file does not need editing.
        static const int sLayoutColorSlotCount = 16;
        static const int sColorSlotCount = 40;

        MyGUI::EditBox* mCommandLine;
        MyGUI::EditBox* mHistory;
        MyGUI::ScrollBar* mHistoryScroll;
        MyGUI::ScrollBar* mCommandScroll;

        typedef std::list<std::string> StringList;

        StringList mCommandHistory;
        StringList::iterator mCurrent;
        std::string mEditString;

        GUIChat(int x, int y, int w, int h);

        void pressedChatMode();
        void pressedSay();
        // X054: tapping the Say key restores the historic HUD caret; holding it
        // is what promotes the strip into the full Player Menu.
        void openPlayerMenu();
        bool getMenuState() const;
        void setSayKeyHeld(bool held);
        void setDelay(float newDelay);
        void setHistoryDisplayEnabled(bool enabled);

        void update(float dt);

        virtual void onOpen();
        virtual void onClose();
        virtual bool exit();

        bool getEditState();
        std::string getHistoryText() const;
        void setMainMenuOpen(bool state);

        void setFont(const std::string &fntName);
        void onResChange(int width, int height);

        void print(const std::string &msg, const std::string& color = "#FFFFFF");
        void clean();
        void printOK(const std::string &msg);
        void printError(const std::string &msg);
        void send(const std::string &str);

        // X050: consume hidden server controls for the Group tab and
        // server-authoritative party XP. Returns true when msg is not chat text.
        bool handleServerControlMessage(const std::string& msg);

    private:
        void keyPress(MyGUI::Widget* _sender, MyGUI::KeyCode key, MyGUI::Char _char);
        void acceptCommand(MyGUI::EditBox* _sender);
        void commandTextChanged(MyGUI::EditBox* _sender);

        void setEditState(bool state);
        void setMenuState(bool state);
        void setHistoryReviewState(bool state);
        void scrollHistoryToBottom();
        void updateCommandLineLayout();
        void refreshPresentation();
        void revealTemporarily();
        void showSmoothly(float targetAlpha);
        void hideSmoothly();
        void applyAlpha(float alpha);
        float getRestingAlpha() const;
        std::string getModeMessage() const;
        std::string getModeSetting() const;
        void setModeFromSetting(const std::string& mode);
        void syncSettings();

        // X052 caption-aware layout. Buttons are sized from their measured
        // localized caption instead of from hardcoded pixel widths, so RU and
        // EN text both fit and neither is clipped.
        void setMenuCaption(MyGUI::Widget* widget, const std::string& caption);
        void refitCaption(MyGUI::Widget* widget);
        void layoutRow(const std::vector<MyGUI::Widget*>& row, int left, int top, int width, int height, int gap);
        void layoutGrid(const std::vector<MyGUI::Widget*>& cells, int left, int top, int width,
            int rowHeight, int columns, int gap);
        void applyMenuLayout();

        // X049 player-menu shell.
        void setupPlayerMenu();
        void refreshPlayerMenu();
        void selectTab(PlayerMenuTab tab, bool persist = true);
        void updateToggleButtons();
        std::string buildOutgoingMessage(const std::string& text) const;
        void setChatChannel(ChatChannel channel);
        void setChatStyle(ChatStyle style);
        void setRpMode(bool enabled);
        void setStayOpenAfterSend(bool enabled);
        // X054: RP mode and the speech channel live on the server (coreChat),
        // so the buttons ask for a change and then redraw from the reply.
        void requestChatState();
        void applyChatState(const std::vector<std::string>& fields);

        // X050 group page. All actions travel through the existing ChatMessage
        // command path (/groupui); no new network packet is required.
        void requestGroupState();
        void sendGroupAction(const std::string& action, const std::string& argument = std::string());
        void updateGroupControls();
        void rebuildGroupInfo(const std::vector<std::string>& fields);
        void rebuildGroupRoster(const std::string& rosterField);
        std::string selectedRosterName() const;

        // X052 server player list. Mirrors what /list reports, rendered inside
        // the player menu instead of a separate server-driven dialog.
        void requestPlayerList();
        void rebuildPlayerList(const std::vector<std::string>& fields);
        void updatePlayerDetails();

        // X052 quick-insert strip and nickname colour picker.
        void setDrawer(ChatDrawer drawer);
        // X053: pull in the generated colour atlas font, if one is configured.
        void ensureChatFontLoaded();
        bool unicodeEmojiEnabled() const;
        const char* emojiSymbol(int index) const;
        void applyEmojiPalette();
        void rebuildColorPalette(const std::vector<std::string>& fields);
        void updateColorButtons();
        void requestColorState();

        void onTabClicked(MyGUI::Widget* sender);
        void onModeClicked(MyGUI::Widget* sender);
        void onChannelClicked(MyGUI::Widget* sender);
        void onStyleClicked(MyGUI::Widget* sender);
        void onStayOpenClicked(MyGUI::Widget* sender);
        void onSendClicked(MyGUI::Widget* sender);
        void onReturnClicked(MyGUI::Widget* sender);
        void onEmojiToggleClicked(MyGUI::Widget* sender);
        void onColorToggleClicked(MyGUI::Widget* sender);
        void onEmojiClicked(MyGUI::Widget* sender);
        void onColorClicked(MyGUI::Widget* sender);
        void onGroupButtonClicked(MyGUI::Widget* sender);
        void onRosterSelected(MyGUI::ListBox* sender, size_t index);
        void onRosterAccepted(MyGUI::ListBox* sender, size_t index);
        void onPlayersButtonClicked(MyGUI::Widget* sender);
        void onPlayerListSelected(MyGUI::ListBox* sender, size_t index);

        void onDragStart(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void applyHudGeometry(int width, int height);
        void applyPanelGeometry(int width, int height);
        void applyStateGeometry();
        void syncInteractiveInputMode();
        void clampToViewport(int width, int height);
        void markGeometryDirty();
        void persistGeometry();

        MyGUI::Widget* mPanelBackground;
        MyGUI::Widget* mDragHandle;
        MyGUI::Widget* mChatToolbar;
        MyGUI::Widget* mEmojiBar;
        MyGUI::Widget* mColorBar;
        MyGUI::Widget* mGroupPane;
        MyGUI::Widget* mPlayersPane;
        MyGUI::Widget* mPlayersActionRow;
        MyGUI::Widget* mGroupActionRow1;
        MyGUI::Widget* mGroupActionRow2;
        MyGUI::Widget* mGroupActionRow3;
        MyGUI::EditBox* mGroupInfo;
        MyGUI::EditBox* mPlayersInfo;
        MyGUI::ListBox* mGroupRoster;
        MyGUI::ListBox* mPlayersList;
        MyGUI::TextBox* mGroupRosterLabel;
        MyGUI::TextBox* mGroupNameLabel;
        MyGUI::TextBox* mGroupTargetLabel;
        MyGUI::TextBox* mColorBarLabel;
        MyGUI::EditBox* mGroupNameEdit;
        MyGUI::EditBox* mGroupTargetEdit;

        MyGUI::Button* mTabChat;
        MyGUI::Button* mTabGroup;
        MyGUI::Button* mTabPlayers;
        MyGUI::Button* mPlayersRefreshButton;
        MyGUI::Button* mPlayersOpenListButton;
        MyGUI::Button* mPlayersInviteButton;
        MyGUI::Button* mModeOoc;
        MyGUI::Button* mModeRp;
        MyGUI::Button* mChannelSay;
        MyGUI::Button* mChannelWhisper;
        MyGUI::Button* mChannelShout;
        MyGUI::Button* mChannelLocalOoc;
        MyGUI::Button* mChannelGlobalOoc;
        MyGUI::Button* mStylePlain;
        MyGUI::Button* mStyleMe;
        MyGUI::Button* mStyleDo;
        MyGUI::Button* mStyleTry;
        MyGUI::Button* mStayOpenButton;
        MyGUI::Button* mSendButton;
        MyGUI::Button* mReturnButton;
        MyGUI::Button* mEmojiToggleButton;
        MyGUI::Button* mColorToggleButton;
        MyGUI::Button* mEmojiButtons[sEmojiSlotCount];
        MyGUI::Button* mColorButtons[sColorSlotCount];
        MyGUI::Button* mGroupCreateButton;
        MyGUI::Button* mGroupRefreshButton;
        MyGUI::Button* mGroupInviteButton;
        MyGUI::Button* mGroupLeaveButton;
        MyGUI::Button* mGroupDisbandButton;
        MyGUI::Button* mGroupKickButton;
        MyGUI::Button* mGroupLeaderButton;
        MyGUI::Button* mGroupJournalButton;
        MyGUI::Button* mGroupTopicsButton;
        MyGUI::Button* mGroupAcceptButton;
        MyGUI::Button* mGroupDeclineButton;

        ChatWindowState windowState;
        ChatChannel chatChannel;
        ChatStyle chatStyle;
        // Y049: server-restored personal markers and synthetic group markers.
        // Personal markers are installed into WindowManager's editable marker
        // collection; group markers stay in GUIController's transient layer.
        std::map<std::string, ESM::CustomMarker> mPersonalMapMarkers;
        std::map<std::string, ESM::CustomMarker> mGroupMapMarkers;

        PlayerMenuTab activeTab;
        bool rpMode;
        bool stayOpenAfterSend;
        ChatDrawer activeDrawer;
        std::string emojiFontName;
        std::string menuFontName;
        std::vector<std::string> groupRosterNames;
        std::vector<std::string> playerListNames;
        std::vector<std::string> playerListDetails;
        std::vector<std::pair<unsigned int, std::string>> colorPalette;
        std::vector<std::string> colorNames;
        int selectedColorIndex;
        std::map<MyGUI::Widget*, std::string> menuCaptions;
        bool groupInGroup;
        bool groupIsLeader;
        bool groupJournalSync;
        bool groupTopicSync;
        bool groupPendingInvite;
        bool editState;
        bool menuState;
        bool sayKeyHeld;
        bool historyReviewState;
        bool mainMenuOpen;
        bool historyDisplayEnabled;
        bool externalHistoryDisplayEnabled;
        bool hideAfterFade;
        bool geometryDirty;
        float geometrySaveDelay;
        float delay;
        float revealTime;
        float currentAlpha;
        float targetAlpha;
        MyGUI::IntPoint dragStartMouse;
        MyGUI::IntPoint dragStartWindow;
        MyGUI::IntCoord panelCoord;
    };
}
#endif //OPENMW_GUICHAT_HPP
