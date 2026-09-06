#ifndef OPENMW_GUICONTROLLER_HPP
#define OPENMW_GUICONTROLLER_HPP

#include <components/settings/settings.hpp>

#include "apps/openmw/mwgui/mode.hpp"

#include <components/openmw-mp/Base/BasePlayer.hpp>
#include "GUI/PlayerMarkerCollection.hpp"
#include "GUI/TextInputDialog.hpp"
#include "GUI/GUILogin.hpp"

namespace MyGUI
{
    class Widget;
    class ProgressBar;
    class TextBox;
}

namespace MWGui
{
    class LocalMapBase;
    class MapWindow;
}

namespace mwmp
{
    class GUIDialogList;
    class GUIChat;
    class ServerQuestEditorWindow;
    class GUIController
    {
    public:
        enum GM
        {
            GM_VR_MetaMenu = MWGui::GM_PlayerAnimationMenu + 1, // Put this dummy GuiMode here because it's used in VR
            GM_TES3MP_InputBox,
            GM_TES3MP_ListBox,
            GM_ARENAMP_QuestEditor,
            GM_ARENAMP_PlayerMenu

        };
        GUIController();
        ~GUIController();
        void cleanUp();

        void refreshGuiMode(MWGui::GuiMode guiMode);

        void setupChat();

        void printChatMessage(std::string &msg);
        void setChatVisible(bool chatVisible);

        void showMessageBox(const BasePlayer::GUIMessageBox &guiMessageBox);
        void showCustomMessageBox(const BasePlayer::GUIMessageBox &guiMessageBox);
        void showInputBox(const BasePlayer::GUIMessageBox &guiMessageBox);

        void showDialogList(const BasePlayer::GUIMessageBox &guiMessageBox);

        // X039: real MyGUI server quest editor, opened by the hidden quest transport.
        void showServerQuestEditor();
        void refreshServerQuestEditor();

        /// Returns 0 if there was no events
        bool pressedKey(int key);

        void changeChatMode();

        bool getChatEditState();
        std::string getChatHistoryText() const;
        bool getChatHistoryCoord(int& x, int& y, int& width, int& height) const;
        void setChatMainMenuOpen(bool state);

        void update(float dt);

        void processCustomMessageBoxInput(int pressedButton);

        void WM_UpdateVisible(MWGui::GuiMode mode);

        void updatePlayersMarkers(MWGui::LocalMapBase *localMapBase);
        void updateGlobalMapMarkerTooltips(MWGui::MapWindow *pWindow);

        ESM::CustomMarker createMarker(const RakNet::RakNetGUID &guid);
        PlayerMarkerCollection mPlayerMarkers;
    private:
        void setGlobalMapMarkerTooltip(MWGui::MapWindow *mapWindow ,MyGUI::Widget* markerWidget, int x, int y);

    private:
        // X054: tap-vs-hold arbitration for the Say key. The press itself opens
        // the historic HUD caret; only a sustained hold promotes it to the full
        // Player Menu.
        void updateSayKeyHold(float dt);

        // Y050: hidden server restart control envelope and borderless HUD overlay.
        bool handleEmbeddedRestartControl(const std::string& message);
        void ensureEmbeddedRestartHud();
        void destroyEmbeddedRestartHud();

        GUIChat *mChat;
        int keySay;
        int keyChatMode;
        bool sayHoldArmed;
        bool sayHoldTriggered;
        float sayHoldTime;
        float sayHoldThreshold;

        long id;
        TextInputDialog *mInputBox;
        GUILogin *mAccountLoginBox;
        GUIDialogList *mListBox;
        ServerQuestEditorWindow *mServerQuestEditor;
        bool mPreLoginPasswordAutoSubmitted;

        MyGUI::Widget* mEmbeddedRestartHud;
        MyGUI::ProgressBar* mEmbeddedRestartProgress;
        MyGUI::TextBox* mEmbeddedRestartTitle;
        MyGUI::TextBox* mEmbeddedRestartMessage;
        int mEmbeddedRestartTotalSeconds;
        void onInputBoxDone(MWGui::WindowBase* parWindow);
        void onAccountLoginDone(MWGui::WindowBase* parWindow);
        void submitInputReply(const std::string& rawText);
        //MyGUI::Widget *oldFocusWidget, *currentFocusWidget;
    };
}

#endif //OPENMW_GUICONTROLLER_HPP
