#ifndef OPENMW_GUICHAT_HPP
#define OPENMW_GUICHAT_HPP

#include <list>
#include <string>
#include <vector>

#include "apps/openmw/mwgui/windowbase.hpp"

namespace MyGUI
{
    class Button;
    class ScrollBar;
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

        enum ChatChannel
        {
            CHANNEL_DEFAULT = 0,
            CHANNEL_LOCAL,
            CHANNEL_GLOBAL
        };

        enum ChatStyle
        {
            STYLE_PLAIN = 0,
            STYLE_ME,
            STYLE_DO
        };

        enum PlayerMenuTab
        {
            TAB_CHAT = 0,
            TAB_GROUP,
            TAB_HOME
        };

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

    private:
        void keyPress(MyGUI::Widget* _sender, MyGUI::KeyCode key, MyGUI::Char _char);
        void acceptCommand(MyGUI::EditBox* _sender);
        void commandTextChanged(MyGUI::EditBox* _sender);

        void setEditState(bool state);
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

        void onTabClicked(MyGUI::Widget* sender);
        void onModeClicked(MyGUI::Widget* sender);
        void onChannelClicked(MyGUI::Widget* sender);
        void onStyleClicked(MyGUI::Widget* sender);
        void onStayOpenClicked(MyGUI::Widget* sender);
        void onSendClicked(MyGUI::Widget* sender);
        void onReturnClicked(MyGUI::Widget* sender);
        void onEmojiToggleClicked(MyGUI::Widget* sender);
        void onEmojiClicked(MyGUI::Widget* sender);

        void onDragStart(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void clampToViewport(int width, int height);
        void markGeometryDirty();
        void persistGeometry();

        MyGUI::Widget* mPanelBackground;
        MyGUI::Widget* mDragHandle;
        MyGUI::Widget* mChatToolbar;
        MyGUI::Widget* mEmojiBar;
        MyGUI::Widget* mGroupPane;
        MyGUI::Widget* mHomePane;

        MyGUI::Button* mTabChat;
        MyGUI::Button* mTabGroup;
        MyGUI::Button* mTabHome;
        MyGUI::Button* mModeOoc;
        MyGUI::Button* mModeRp;
        MyGUI::Button* mChannelDefault;
        MyGUI::Button* mChannelLocal;
        MyGUI::Button* mChannelGlobal;
        MyGUI::Button* mStylePlain;
        MyGUI::Button* mStyleMe;
        MyGUI::Button* mStyleDo;
        MyGUI::Button* mStayOpenButton;
        MyGUI::Button* mSendButton;
        MyGUI::Button* mReturnButton;
        MyGUI::Button* mEmojiToggleButton;
        MyGUI::Button* mEmojiButtons[6];

        ChatWindowState windowState;
        ChatChannel chatChannel;
        ChatStyle chatStyle;
        PlayerMenuTab activeTab;
        bool rpMode;
        bool stayOpenAfterSend;
        bool emojiBarVisible;
        bool editState;
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
    };
}
#endif //OPENMW_GUICHAT_HPP
