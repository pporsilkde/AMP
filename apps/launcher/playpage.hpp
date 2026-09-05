#ifndef PLAYPAGE_H
#define PLAYPAGE_H

#include "ui_playpage.h"

#include <QString>

class QWidget;
class QLabel;
class QComboBox;
class QPushButton;
class QCheckBox;

namespace Launcher
{
    class PlayPage : public QWidget, private Ui::PlayPage
    {
        Q_OBJECT

    public:
        explicit PlayPage(QWidget *parent = nullptr);

        void setBuildName(const QString& name);
        void setServerAddress(const QString& addr);
        void setServerPort(const QString& port);
        void setServerConsoleWidget(QWidget* widget);
        void setAutoStartServer(bool enabled);
        void setAutoRestartServer(bool enabled);
        void setServerRunning(bool running, const QString& address = QString(), const QString& port = QString(), bool managed = true);
        void setBuildManifestComplete(bool complete);

        QString buildName() const;
        QString serverAddress() const;
        QString serverPort() const;
        QString hostBindAddress() const;
        void setHostBindAddress(const QString& address);
        bool autoStartServer() const;
        bool autoRestartServer() const;
        void setEnforceDataFiles(bool enabled);
        bool enforceDataFiles() const;

       void switchToServerConsoleTab();
       void loadServerSettings();
       bool saveServerSettings();

        // X041: XP progression, moved here from the removed Advanced -> Arena
        // Settings tab. These are client-side defaults in settings.cfg; a
        // connected ArenaMP server stays authoritative and may override them.
        void loadXpProgressionSettings();
        bool saveXpProgressionSettings();

    signals:
        void playButtonClicked();
        void serverButtonClicked();
        void stopServerButtonClicked();
        void autoStartServerChanged(bool enabled);
        void autoRestartServerChanged(bool enabled);
        void updateHashesRequested();
        void clearServerCellsRequested();
        void resetServerDataRequested();

    private slots:
        void slotPlayClicked();
        void slotServerClicked();
        void slotStopServerClicked();
        void slotAutoStartServerToggled(bool enabled);
        void slotRefreshHostInterfaces();
        void slotUpdateHashesClicked();
        void slotEnforceRequiredToggled(bool enabled);
        void slotServerModeChanged(int index);
        void slotReloadServerSettings();
        void slotSaveServerSettings();
        void slotApplyFormToRawConfig();
        void slotSyncFormFromRawConfig();
        void slotServerSettingsModeChanged(int index);
        void slotXpRatePresetChanged(int index);
        void slotXpGainMultiplierChanged(double value);

    private:
        QString serverConfigPath() const;
        QString persistentServerConfigPath() const;
        bool writeServerConfigFile(const QString& path, const QString& text, QString* errorMessage) const;
        QString replaceRawValue(const QString& text, const QString& key, const QString& value) const;
        QString updatedConfigFromForm(const QString& input) const;
        void populateFormFromConfig(const QString& text);
        void setServerSettingsStatus(const QString& text, bool isError = false);
        void refreshHostInterfaces(const QString& preferredAddress = QString());
        void updateHostModeUi(bool enabled);
        void applyServerModePreset(int index);
        QWidget* mEmbeddedServerConsole;
        bool mSyncingServerSettingsTabs;
        QLabel* mHostInterfaceLabel;
        QComboBox* mHostInterfaceCombo;
        QPushButton* mRefreshHostInterfacesButton;
        QPushButton* mUpdateHashesButton;
        QCheckBox* mEnforceRequiredCheckBox;
        QLabel* mServerModeLabel;
        QComboBox* mServerModeCombo;
        QPushButton* mClearCellsButton;
        QPushButton* mResetServerButton;
        bool mSyncingXpControls;
    };
}
#endif
