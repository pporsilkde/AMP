#ifndef MAINDIALOG_H
#define MAINDIALOG_H


#ifndef Q_MOC_RUN
#include <components/files/configurationmanager.hpp>


#include <components/process/processinvoker.hpp>

#include <components/config/gamesettings.hpp>
#include <components/config/launchersettings.hpp>

#include <components/settings/settings.hpp>
#endif
#include "ui_mainwindow.h"

#include <components/misc/arenaherobutton.hpp>

#include <QElapsedTimer>

class QListWidgetItem;
class QStackedWidget;
class QStringList;
class QString;
class QPushButton;
class QLabel;
class QTimer;

namespace Launcher
{
    class PlayPage;
    class GraphicsPage;
    class DataFilesPage;
    class UnshieldThread;
    class SettingsPage;
    class AdvancedPage;
    class ServerDialog;

    enum FirstRunDialogResult
    {
        FirstRunDialogResultFailure,
        FirstRunDialogResultContinue,
        FirstRunDialogResultWizard
    };

#ifndef WIN32
    bool expansions(Launcher::UnshieldThread& cd);
#endif

    class MainDialog : public QMainWindow, private Ui::MainWindow
    {
        Q_OBJECT

    public:
        explicit MainDialog(QWidget *parent = nullptr);
        ~MainDialog();

        FirstRunDialogResult showFirstRunDialog();

        bool reloadSettings();
        bool writeSettings();

    public slots:
        void changePage(QListWidgetItem *current, QListWidgetItem *previous);
        void checkForUpdates();
        void play();
        void runServer();
        void stopServer();
        void help();
        void showChangelog();

    private slots:
        void wizardStarted();
        void wizardFinished(int exitCode, QProcess::ExitStatus exitStatus);
        void launchClient();
        void autoStartServerChanged(bool enabled);
        void autoRestartServerChanged(bool enabled);
        void serverRunningChanged(bool running, const QString& address, const QString& port);
        void updateServerDataFileHashes();
        void clearServerCells();
        void resetServerData();
        void updateSessionTime();
        void changeBuild();

    private:
        bool setup();
        // U021: built-in replacement for the standalone setup wizard.
        bool runBuildSetup(const QString& initialPath);
        void applyPendingBuildPath();

        void createIcons();
        void createPages();

        bool setupLauncherSettings();
        bool setupGameSettings();
        bool setupGraphicsSettings();
        bool setupGameData();
        bool loadBuildManifest();
        bool writeBuildManifest();
        void applyBuildManifestRestrictions();
        QString primaryDataDirectory() const;
        bool isLocalServerAddress(const QString& address) const;
        void writeClientEndpoint(const QString& address, const QString& port) const;
        QString resolveSelectedDataFilePath(const QString& fileName, const QStringList& selectedPaths) const;

        void setVersionLabel();
        void updateFooterServerStatus(bool running, const QString& address = QString(), const QString& port = QString());
        void loadSettings();
        void saveSettings();

        inline bool startProgram(const QString &name, bool detached = false) { return startProgram(name, QStringList(), detached); }
        bool startProgram(const QString &name, const QStringList &arguments, bool detached = false);

        void closeEvent(QCloseEvent *event) override;

        PlayPage *mPlayPage;
        GraphicsPage *mGraphicsPage;
        DataFilesPage *mDataFilesPage;
        SettingsPage *mSettingsPage;
        AdvancedPage *mAdvancedPage;

        Process::ProcessInvoker *mGameInvoker;
        Process::ProcessInvoker *mWizardInvoker;
        ServerDialog *mServerDialog;
        ArenaUi::HeroButton *mPlayButton;
        QLabel *mFooterStatusDot;
        QLabel *mFooterStatusDetail;
        QLabel *mSessionLabel;
        QTimer *mSessionTimer;
        QElapsedTimer mSessionClock;

        bool mBuildManifestLoaded;
        QString mBuildManifestPath;
        QString mBuildName;
        QString mBuildDataPath;
        QString mBuildServerAddress;
        QString mBuildServerPort;
        bool mBuildServerAddressSpecified;
        bool mBuildServerPortSpecified;
        bool mBuildComplete;
        bool mUpdateAvailable;
        bool mUpdateCheckRunning;
        QString mPendingSetupDataPath;
        QString mPendingClientAddress;
        QString mPendingClientPort;

        Files::ConfigurationManager mCfgMgr;

        Config::GameSettings mGameSettings;
        Settings::Manager mEngineSettings;
        Config::LauncherSettings mLauncherSettings;

    };
}
#endif
