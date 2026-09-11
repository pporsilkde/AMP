#include <components/misc/arenaglassicons.hpp>
#include <components/misc/arenaglasswindow.hpp>
#include "updatecontroller.hpp"
#include "maindialog.hpp"

#include <components/version/version.hpp>
#include <components/misc/helpviewer.hpp>
#include <components/config/buildmanifest.hpp>
#include <components/config/contentorder.hpp>

#include <QDate>
#include <QCoreApplication>
#include <QMessageBox>
#include <QFontDatabase>
#include <QInputDialog>
#include <QFileDialog>
#include <QCloseEvent>
#include <QTextCodec>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <boost/crc.hpp>
#include <QTimer>
#include <QElapsedTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QStyle>


#include "playpage.hpp"
#include "graphicspage.hpp"
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include "datafilespage.hpp"
#include "settingspage.hpp"
#include "advancedpage.hpp"
#include "serverdialog.hpp"

using namespace Process;

void cfgError(const QString& title, const QString& msg) {
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setText(msg);
    msgBox.exec();
}

namespace
{
    // U020 showcase layout: two cards plus a status column need a little more
    // room than the former 960x660 window. 1080x720 still fits 1366x768
    // laptop screens together with the Windows taskbar.
    constexpr int sLauncherWidth = 1080;
    constexpr int sLauncherHeight = 720;
    constexpr int sNavigationItems = 5;

    void repolishWidget(QWidget* widget)
    {
        if (widget == nullptr)
            return;
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    QString formatSessionTime(qint64 milliseconds)
    {
        const qint64 totalSeconds = qMax<qint64>(0, milliseconds / 1000);
        const qint64 hours = totalSeconds / 3600;
        const qint64 minutes = (totalSeconds / 60) % 60;
        const qint64 seconds = totalSeconds % 60;
        if (hours > 0)
            return QStringLiteral("%1:%2:%3").arg(hours)
                .arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
        return QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }

    bool containsGameContent(const QDir& dir)
    {
        if (!dir.exists())
            return false;

        const QStringList files = dir.entryList(
            QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QString& fileName : files)
        {
            if (fileName.endsWith(QLatin1String(".esm"), Qt::CaseInsensitive)
                || fileName.endsWith(QLatin1String(".esp"), Qt::CaseInsensitive)
                || fileName.endsWith(QLatin1String(".omwgame"), Qt::CaseInsensitive)
                || fileName.endsWith(QLatin1String(".omwaddon"), Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

    QString resolveDataFilesDirectory(const QString& selectedPath)
    {
        if (selectedPath.trimmed().isEmpty())
            return QString();

        const QString cleanPath = QDir::cleanPath(selectedPath);
        const QDir selectedDir(cleanPath);
        if (!selectedDir.exists())
            return QString();
        if (containsGameContent(selectedDir))
            return cleanPath;

        const QStringList childDirectories = selectedDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QString& childName : childDirectories)
        {
            if (childName.compare(QLatin1String("Data Files"), Qt::CaseInsensitive) != 0)
                continue;

            const QString childPath = QDir::cleanPath(selectedDir.filePath(childName));
            if (containsGameContent(QDir(childPath)))
                return childPath;
        }
        return QString();
    }

    QString formatChangelogInline(QString text)
    {
        text = text.toHtmlEscaped();
        text.replace(QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
            QStringLiteral("<b>\\1</b>"));
        text.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
            QStringLiteral("<code>\\1</code>"));
        return text;
    }

    QString changelogMarkdownToHtml(const QString& markdown)
    {
        QString html = QStringLiteral(
            "<html><head><style>"
            "body{font-family:'Segoe UI',sans-serif;font-size:10.5pt;line-height:1.45;margin:18px;color:#e8e8e8;background:#252525;}"
            "h1,h2,h3{color:#ffffff;margin-top:18px;margin-bottom:8px;}"
            "h2{font-size:17pt;border-bottom:1px solid #555;padding-bottom:5px;}"
            "h3{font-size:12.5pt;color:#f0d58a;}"
            "ul{margin-top:4px;margin-bottom:10px;} li{margin-bottom:5px;}"
            "code{font-family:Consolas,monospace;background:#333;padding:1px 4px;border-radius:2px;color:#f4f4f4;}"
            "a{color:#7fb7ff;} .paragraph{margin:6px 0 10px 0;}"
            "</style></head><body>");

        bool listOpen = false;
        const QStringList lines = markdown.split(QLatin1Char('\n'));
        for (QString line : lines)
        {
            if (line.endsWith(QLatin1Char('\r')))
                line.chop(1);
            const QString trimmed = line.trimmed();

            if (trimmed.startsWith(QStringLiteral("- ")))
            {
                if (!listOpen)
                {
                    html += QStringLiteral("<ul>");
                    listOpen = true;
                }
                html += QStringLiteral("<li>") + formatChangelogInline(trimmed.mid(2)) + QStringLiteral("</li>");
                continue;
            }

            if (listOpen)
            {
                html += QStringLiteral("</ul>");
                listOpen = false;
            }

            if (trimmed.startsWith(QStringLiteral("### ")))
                html += QStringLiteral("<h3>") + formatChangelogInline(trimmed.mid(4)) + QStringLiteral("</h3>");
            else if (trimmed.startsWith(QStringLiteral("## ")))
                html += QStringLiteral("<h2>") + formatChangelogInline(trimmed.mid(3)) + QStringLiteral("</h2>");
            else if (trimmed.startsWith(QStringLiteral("# ")))
                html += QStringLiteral("<h1>") + formatChangelogInline(trimmed.mid(2)) + QStringLiteral("</h1>");
            else if (trimmed == QStringLiteral("---"))
                html += QStringLiteral("<hr>");
            else if (!trimmed.isEmpty())
                html += QStringLiteral("<div class='paragraph'>") + formatChangelogInline(trimmed) + QStringLiteral("</div>");
        }

        if (listOpen)
            html += QStringLiteral("</ul>");
        html += QStringLiteral("</body></html>");
        return html;
    }
}

Launcher::MainDialog::MainDialog(QWidget *parent)
    : QMainWindow(parent)
    , mPlayPage(nullptr)
    , mGraphicsPage(nullptr)
    , mDataFilesPage(nullptr)
    , mSettingsPage(nullptr)
    , mAdvancedPage(nullptr)
    , mGameInvoker(nullptr)
    , mWizardInvoker(nullptr)
    , mServerDialog(nullptr)
    , mPlayButton(nullptr)
    , mFooterStatusDot(nullptr)
    , mFooterStatusDetail(nullptr)
    , mSessionLabel(nullptr)
    , mSessionTimer(nullptr)
    , mBuildManifestLoaded(false)
    , mBuildName(QStringLiteral("ArenaMP"))
    , mBuildServerAddress(QStringLiteral("127.0.0.1"))
    , mBuildServerPort(QStringLiteral("25565"))
    , mBuildServerAddressSpecified(false)
    , mBuildServerPortSpecified(false)
    , mBuildComplete(false)
    , mUpdateAvailable(false)
    , mUpdateCheckRunning(false)
    , mGameSettings (mCfgMgr)
{
    setupUi(this);
    setFixedSize(sLauncherWidth, sLauncherHeight);

    mGameInvoker = new ProcessInvoker();
    mWizardInvoker = new ProcessInvoker();
    mServerDialog = new ServerDialog(this);
    // The server console is embedded directly inside the Play page.
    // Do not wrap it in a second glass window/title bar, otherwise the
    // inner traffic-light controls and extra top margin waste space.
    connect(mWizardInvoker->getProcess(), SIGNAL(started()),
            this, SLOT(wizardStarted()));

    connect(mWizardInvoker->getProcess(), SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(wizardFinished(int,QProcess::ExitStatus)));

    iconWidget->setViewMode(QListView::IconMode);
    iconWidget->setWrapping(false);
    iconWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Just to be sure
    iconWidget->setIconSize(QSize(27, 27));
    // Five equal toolbar cells across the full window width.
    const int navigationCell = (sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems;
    iconWidget->setGridSize(QSize(navigationCell, 60));
    iconWidget->setWordWrap(false);
    iconWidget->setTextElideMode(Qt::ElideNone);
    iconWidget->setMovement(QListView::Static);

    iconWidget->setSpacing(3);
    iconWidget->setUniformItemSizes(true);
    iconWidget->setCurrentRow(0);
    iconWidget->setFlow(QListView::LeftToRight);

    // U020 footer: status dot + two-line server state, session time and the
    // global actions in a fixed, predictable order (the platform-dependent
    // QDialogButtonBox ordering is no longer used).
    buttonBox->hide();
    QPushButton *playButton = new QPushButton(tr("Play"), footerBar);
    QPushButton *changelogButton = new QPushButton(QStringLiteral("Changelog"), footerBar);
    QPushButton *serverButton = new QPushButton(tr("Run Server"), footerBar);
    QPushButton *helpButton = new QPushButton(tr("Help"), footerBar);
    changelogButton->setToolTip(tr("Open the ArenaMP changelog"));
    mPlayButton = playButton;
    playButton->setProperty("arenaPrimary", true);
    helpButton->setProperty("arenaQuiet", true);
    changelogButton->setProperty("arenaQuiet", true);
    serverButton->setProperty("arenaQuiet", true);
    playButton->setIcon(ArenaUi::glassIcon(QStringLiteral("play-dark")));
    helpButton->setIcon(ArenaUi::glassIcon(QStringLiteral("help")));
    changelogButton->setIcon(ArenaUi::glassIcon(QStringLiteral("changelog")));
    serverButton->setIcon(ArenaUi::glassIcon(QStringLiteral("server")));
    playButton->setMinimumWidth(150);
    changelogButton->setMinimumWidth(112);
    serverButton->setMinimumWidth(150);
    helpButton->setMinimumWidth(104);
    for (QPushButton* button : { playButton, changelogButton, serverButton, helpButton })
    {
        button->setProperty("arenaFooterButton", true);
        button->setCursor(Qt::PointingHandCursor);
        horizontalLayout->addWidget(button);
    }

    horizontalLayout->removeWidget(versionLabel);
    mFooterStatusDot = new QLabel(footerBar);
    mFooterStatusDot->setObjectName(QStringLiteral("footerStatusDot"));
    mFooterStatusDot->setFixedSize(12, 12);
    mFooterStatusDot->setProperty("arenaStatusDot", true);
    mFooterStatusDot->setProperty("arenaStatus", QStringLiteral("ready"));
    versionLabel->setProperty("arenaStatus", QStringLiteral("offline"));
    mFooterStatusDetail = new QLabel(footerBar);
    mFooterStatusDetail->setObjectName(QStringLiteral("footerStatusDetail"));
    mFooterStatusDetail->setProperty("arenaMuted", true);
    mFooterStatusDetail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QVBoxLayout* footerStatusText = new QVBoxLayout();
    footerStatusText->setContentsMargins(0, 0, 0, 0);
    footerStatusText->setSpacing(0);
    footerStatusText->addStretch(1);
    footerStatusText->addWidget(versionLabel);
    footerStatusText->addWidget(mFooterStatusDetail);
    footerStatusText->addStretch(1);
    QFrame* footerDivider = new QFrame(footerBar);
    footerDivider->setProperty("arenaVDivider", true);
    footerDivider->setFixedSize(1, 28);
    mSessionLabel = new QLabel(footerBar);
    mSessionLabel->setObjectName(QStringLiteral("footerSessionLabel"));
    mSessionLabel->setProperty("arenaMuted", true);
    mSessionLabel->setToolTip(tr("Uptime of the local server started from this launcher"));
    mSessionLabel->setText(tr("Session time: %1").arg(formatSessionTime(0)));
    horizontalLayout->insertWidget(0, mFooterStatusDot, 0, Qt::AlignVCenter);
    horizontalLayout->insertSpacing(1, 2);
    horizontalLayout->insertLayout(2, footerStatusText);
    horizontalLayout->insertSpacing(3, 12);
    horizontalLayout->insertWidget(4, footerDivider, 0, Qt::AlignVCenter);
    horizontalLayout->insertSpacing(5, 12);
    horizontalLayout->insertWidget(6, mSessionLabel, 0, Qt::AlignVCenter);

    mSessionTimer = new QTimer(this);
    mSessionTimer->setInterval(1000);
    connect(mSessionTimer, SIGNAL(timeout()), this, SLOT(updateSessionTime()));

    connect(playButton, SIGNAL(clicked()), this, SLOT(play()));
    connect(serverButton, SIGNAL(clicked()), this, SLOT(runServer()));
    connect(changelogButton, SIGNAL(clicked()), this, SLOT(showChangelog()));
    connect(helpButton, SIGNAL(clicked()), this, SLOT(help()));

    // Remove what's this? button
    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    createIcons();
}

Launcher::MainDialog::~MainDialog()
{
    delete mGameInvoker;
    delete mWizardInvoker;
}

void Launcher::MainDialog::createIcons()
{
    if (!QIcon::hasThemeIcon("document-new"))
        QIcon::setThemeName("tango");

    QListWidgetItem *playButton = new QListWidgetItem(iconWidget);
    playButton->setSizeHint(QSize((sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems - 4, 58));
    playButton->setIcon(ArenaUi::glassIcon(QStringLiteral("play")));
    playButton->setText(tr("Play"));
    playButton->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    playButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem *dataFilesButton = new QListWidgetItem(iconWidget);
    dataFilesButton->setSizeHint(QSize((sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems - 4, 58));
    dataFilesButton->setIcon(ArenaUi::glassIcon(QStringLiteral("browse")));
    dataFilesButton->setText(tr("Data Files"));
    dataFilesButton->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    dataFilesButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem *graphicsButton = new QListWidgetItem(iconWidget);
    graphicsButton->setSizeHint(QSize((sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems - 4, 58));
    graphicsButton->setIcon(ArenaUi::glassIcon(QStringLiteral("graphics")));
    graphicsButton->setText(tr("Graphics"));
    graphicsButton->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom | Qt::AlignAbsolute);
    graphicsButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem *settingsButton = new QListWidgetItem(iconWidget);
    settingsButton->setSizeHint(QSize((sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems - 4, 58));
    settingsButton->setIcon(ArenaUi::glassIcon(QStringLiteral("settings")));
    settingsButton->setText(tr("Settings"));
    settingsButton->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    settingsButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem *advancedButton = new QListWidgetItem(iconWidget);
    advancedButton->setSizeHint(QSize((sLauncherWidth - 20 - 18 - 12 - 30) / sNavigationItems - 4, 58));
    advancedButton->setIcon(ArenaUi::glassIcon(QStringLiteral("advanced")));
    advancedButton->setText(tr("Advanced"));
    advancedButton->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    advancedButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    connect(iconWidget,
            SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(changePage(QListWidgetItem*,QListWidgetItem*)));

}

void Launcher::MainDialog::createPages()
{
    // Avoid creating the widgets twice
    if (pagesWidget->count() != 0)
        return;

    mPlayPage = new PlayPage(this);
    mPlayPage->setBuildName(mBuildName);
    mDataFilesPage = new DataFilesPage(mCfgMgr, mGameSettings, mLauncherSettings, this);
    mGraphicsPage = new GraphicsPage(mLauncherSettings, this);
    mSettingsPage = new SettingsPage(mCfgMgr, mGameSettings, mLauncherSettings, this);
    mAdvancedPage = new AdvancedPage(mGameSettings, this);
    mPlayPage->setServerConsoleWidget(mServerDialog);
    connect(mGraphicsPage, &GraphicsPage::hardwareInfoChanged, this, [this]()
    {
        if (mPlayPage != nullptr && mGraphicsPage != nullptr)
            mPlayPage->setHardwareInfo(mGraphicsPage->hardwareGpuName(),
                mGraphicsPage->hardwareGpuDetail(), mGraphicsPage->hardwareLogicalThreads());
    });

    auto readLauncherBool = [this](const QString& key, const QString& defaultValue) -> bool
    {
        if (mLauncherSettings.getSettings().contains(key))
            return mLauncherSettings.value(key).compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        return defaultValue.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    };

    const QString localServerDefault = mBuildManifestLoaded && mBuildServerAddressSpecified
        ? QStringLiteral("false") : QStringLiteral("true");
    const bool autoStartServer = readLauncherBool(QStringLiteral("General/Server/autoStart"), localServerDefault);
    const bool autoRestartServer = readLauncherBool(QStringLiteral("General/Server/autoRestart"), localServerDefault);

    mPlayPage->setAutoStartServer(autoStartServer);
    mPlayPage->setAutoRestartServer(autoRestartServer);
    mPlayPage->setHostBindAddress(mLauncherSettings.value(
        QStringLiteral("General/Server/bindAddress"), QStringLiteral("0.0.0.0")));

    mServerDialog->setAutoRestartEnabled(autoRestartServer);

    {
        QString addr = mBuildManifestLoaded ? mBuildServerAddress : QStringLiteral("localhost");
        QString port = mBuildManifestLoaded ? mBuildServerPort : QStringLiteral("25565");

        if (!mBuildManifestLoaded)
        {
            const QString cfgPath = QString::fromUtf8(mCfgMgr.getUserConfigPath().string().c_str())
                + QStringLiteral("/tes3mp-client-default.cfg");
            QFile cfgFile(cfgPath);
            if (cfgFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&cfgFile);
                while (!in.atEnd())
                {
                    const QString line = in.readLine().trimmed();
                    if (line.startsWith(QLatin1String("destinationAddress")))
                        addr = line.section(QLatin1Char('='), 1).trimmed();
                    else if (line.startsWith(QLatin1String("port")) && !line.startsWith(QLatin1String("password")))
                        port = line.section(QLatin1Char('='), 1).trimmed();
                }
            }
        }

        if (autoStartServer)
        {
            // The editable address is the endpoint advertised/shared with other
            // players. Do not overwrite it with a LAN adapter merely because
            // Host mode is enabled. The local host client uses a separate
            // loopback/bind endpoint when launching.
            port = mServerDialog->configuredPort();
        }

        mPlayPage->setServerAddress(addr);
        mPlayPage->setServerPort(port);

        const bool managedServer = mServerDialog->isRunning();
        const bool reachableServer = managedServer
            || (autoStartServer && mServerDialog->isServerReachable(120));
        mPlayPage->setServerRunning(reachableServer, addr, port, managedServer);
        if (reachableServer)
            updateFooterServerStatus(true, addr, port);
    }

    // Add the pages to the stacked widget
    pagesWidget->addWidget(mPlayPage);
    pagesWidget->addWidget(mDataFilesPage);
    pagesWidget->addWidget(mGraphicsPage);
    pagesWidget->addWidget(mSettingsPage);
    pagesWidget->addWidget(mAdvancedPage);

    applyBuildManifestRestrictions();

    // Select the first page
    iconWidget->setCurrentItem(iconWidget->item(0), QItemSelectionModel::Select);

    connect(mPlayPage, SIGNAL(playButtonClicked()), this, SLOT(play()));
    connect(mPlayPage, SIGNAL(serverButtonClicked()), this, SLOT(runServer()));
    connect(mPlayPage, SIGNAL(stopServerButtonClicked()), this, SLOT(stopServer()));
    connect(mPlayPage, SIGNAL(autoStartServerChanged(bool)), this, SLOT(autoStartServerChanged(bool)));
    connect(mPlayPage, SIGNAL(autoRestartServerChanged(bool)), this, SLOT(autoRestartServerChanged(bool)));
    connect(mPlayPage, SIGNAL(updateHashesRequested()), this, SLOT(updateServerDataFileHashes()));
    connect(mPlayPage, SIGNAL(clearServerCellsRequested()), this, SLOT(clearServerCells()));
    connect(mPlayPage, SIGNAL(resetServerDataRequested()), this, SLOT(resetServerData()));
    connect(mServerDialog, SIGNAL(runningChanged(bool,QString,QString)),
            this, SLOT(serverRunningChanged(bool,QString,QString)));
    connect(mServerDialog, SIGNAL(autoRestartChanged(bool)),
            this, SLOT(autoRestartServerChanged(bool)));

    // Using Qt::QueuedConnection because signal is emitted in a subthread and slot is in the main thread
    connect(mDataFilesPage, SIGNAL(signalLoadedCellsChanged(QStringList)), mAdvancedPage, SLOT(slotLoadedCellsChanged(QStringList)), Qt::QueuedConnection);

}

Launcher::FirstRunDialogResult Launcher::MainDialog::showFirstRunDialog()
{
    if (!setupLauncherSettings())
        return FirstRunDialogResultFailure;

    // ArenaMP presents the setup flow as one product: on a fresh install the
    // Wizard opens first, then this same launcher instance reloads the result
    // and becomes visible. No intermediate OpenMW-style first-run dialog.
    if (mLauncherSettings.value(QStringLiteral("General/firstrun"), QStringLiteral("true")) == QLatin1String("true"))
    {
        const QStringList args { QStringLiteral("--from-launcher") };
        if (mWizardInvoker->startProcess(QStringLiteral("arenamp-wizard"), args, false)
            || mWizardInvoker->startProcess(QStringLiteral("openmw-wizard"), args, false))
            return FirstRunDialogResultWizard;
        return FirstRunDialogResultFailure;
    }

    if (!setup() || !setupGameData())
        return FirstRunDialogResultFailure;
    return FirstRunDialogResultContinue;
}

void Launcher::MainDialog::setVersionLabel()
{
    updateFooterServerStatus(false);
}

void Launcher::MainDialog::updateFooterServerStatus(bool running, const QString& address, const QString& port)
{
    if (versionLabel == nullptr)
        return;

    versionLabel->setProperty("arenaStatus", running ? QStringLiteral("online") : QStringLiteral("offline"));
    versionLabel->setText(running ? tr("Server online") : tr("Server stopped"));
    if (mFooterStatusDetail != nullptr)
        mFooterStatusDetail->setText(running && !address.isEmpty()
            ? QStringLiteral("%1:%2").arg(address, port) : tr("Ready to launch"));
    if (mFooterStatusDot != nullptr)
    {
        mFooterStatusDot->setProperty("arenaStatus", running ? QStringLiteral("online") : QStringLiteral("ready"));
        repolishWidget(mFooterStatusDot);
    }
    // Dynamic properties do not automatically repolish an existing widget.
    repolishWidget(versionLabel);

    // Session time follows the local server lifetime.
    if (running && (mSessionTimer == nullptr || !mSessionTimer->isActive()))
    {
        mSessionClock.start();
        if (mSessionTimer != nullptr)
            mSessionTimer->start();
    }
    else if (!running)
    {
        if (mSessionTimer != nullptr)
            mSessionTimer->stop();
        mSessionClock.invalidate();
    }
    updateSessionTime();
}

void Launcher::MainDialog::updateSessionTime()
{
    if (mSessionLabel == nullptr)
        return;
    const qint64 elapsed = mSessionClock.isValid() ? mSessionClock.elapsed() : 0;
    mSessionLabel->setText(tr("Session time: %1").arg(formatSessionTime(elapsed)));
}

bool Launcher::MainDialog::setup()
{
    if (!setupGameSettings())
        return false;

    loadBuildManifest();
    setVersionLabel();

    // An existing build.ini is authoritative. Its exact content list was
    // already copied into the named launcher profile by loadBuildManifest().
    // Re-synchronising from merged openmw.cfg here could select stale plugins
    // that are installed but are not present in build.ini.
    if (!mBuildManifestLoaded)
        mLauncherSettings.setContentList(mGameSettings);

    if (!setupGraphicsSettings())
        return false;

    // Now create the pages as they need the settings
    createPages();

    // Call this so we can exit on SDL errors before mainwindow is shown
    if (!mGraphicsPage->loadSettings())
        return false;

    loadSettings();

    return true;
}

bool Launcher::MainDialog::reloadSettings()
{
    if (!setupLauncherSettings())
        return false;

    if (!setupGameSettings())
        return false;

    loadBuildManifest();
    applyBuildManifestRestrictions();
    if (!mBuildManifestLoaded)
        mLauncherSettings.setContentList(mGameSettings);

    if (!setupGraphicsSettings())
        return false;

    if (!mSettingsPage->loadSettings())
        return false;

    if (!mDataFilesPage->loadSettings())
        return false;

    if (!mGraphicsPage->loadSettings())
        return false;

    if (!mAdvancedPage->loadSettings())
        return false;

    // Refresh top-level launcher widgets as well, so a build selected in the
    // Wizard (new build.ini, new build name, new endpoint, new host-mode
    // defaults) is immediately reflected without restarting the launcher.
    loadSettings();
    applyBuildManifestRestrictions();

    return true;
}

void Launcher::MainDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;

    int currentIndex = iconWidget->row(current);
    pagesWidget->setCurrentIndex(currentIndex);
    // The Play page draws its own cards and status column, so the shared
    // glass panel behind the stacked pages is disabled only there.
    const bool bare = pagesWidget->currentWidget() == mPlayPage;
    if (pagesWidget->property("arenaBare").toBool() != bare)
    {
        pagesWidget->setProperty("arenaBare", bare);
        repolishWidget(pagesWidget);
    }
    mSettingsPage->resetProgressBar();
}

bool Launcher::MainDialog::setupLauncherSettings()
{
    mLauncherSettings.clear();

    mLauncherSettings.setMultiValueEnabled(true);

    QString userPath = QString::fromUtf8(mCfgMgr.getUserConfigPath().string().c_str());
    QDir userDir(userPath);

    QStringList paths;
    paths.append(QString(Config::LauncherSettings::sLauncherConfigFileName));
    paths.append(userDir.filePath(QString(Config::LauncherSettings::sLauncherConfigFileName)));

    for (const QString &path : paths)
    {
        qDebug() << "Loading config file:" << path.toUtf8().constData();
        QFile file(path);
        if (file.exists()) {
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                cfgError(tr("Error opening OpenMW configuration file"),
                         tr("<br><b>Could not open %0 for reading</b><br><br> \
                             Please make sure you have the right permissions \
                             and try again.<br>").arg(file.fileName()));
                return false;
            }
            QTextStream stream(&file);
            stream.setCodec(QTextCodec::codecForName("UTF-8"));

            mLauncherSettings.readFile(stream);
        }
        file.close();
    }

    return true;
}

bool Launcher::MainDialog::setupGameSettings()
{
    mGameSettings.clear();

    QString localPath = QString::fromUtf8(mCfgMgr.getLocalPath().string().c_str());
    QString userPath = QString::fromUtf8(mCfgMgr.getUserConfigPath().string().c_str());
    QString globalPath = QString::fromUtf8(mCfgMgr.getGlobalPath().string().c_str());
    QDir localDir(localPath);
    QDir userDir(userPath);
    QDir globalDir(globalPath);

    // Load the user config file first, separately
    // So we can write it properly, uncontaminated
    QString path = userDir.filePath(QLatin1String("openmw.cfg"));
    QFile file(path);

    qDebug() << "Loading config file:" << path.toUtf8().constData();

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            cfgError(tr("Error opening OpenMW configuration file"),
                     tr("<br><b>Could not open %0 for reading</b><br><br> \
                         Please make sure you have the right permissions \
                         and try again.<br>").arg(file.fileName()));
            return false;
        }
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));

        mGameSettings.readUserFile(stream);
        file.close();
    }

    // Now the rest - priority: user > local > global
    QStringList paths;
    paths.append(globalDir.filePath(QString("openmw.cfg")));
    paths.append(localDir.filePath(QString("openmw.cfg")));
    paths.append(userDir.filePath(QString("openmw.cfg")));

    for (const QString &path2 : paths)
    {
        qDebug() << "Loading config file:" << path2.toUtf8().constData();

        file.setFileName(path2);
        if (file.exists()) {
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                cfgError(tr("Error opening OpenMW configuration file"),
                         tr("<br><b>Could not open %0 for reading</b><br><br> \
                             Please make sure you have the right permissions \
                             and try again.<br>").arg(file.fileName()));
                return false;
            }
            QTextStream stream(&file);
            stream.setCodec(QTextCodec::codecForName("UTF-8"));

            mGameSettings.readFile(stream);
            file.close();
        }
    }

    // Normalize legacy data= entries before loadBuildManifest() runs. Older
    // Wizard builds could store the Morrowind root instead of Data Files.
    const QStringList configuredDataDirs = mGameSettings.getDataDirs();
    for (const QString& configuredPath : configuredDataDirs)
    {
        const QString resolvedPath = resolveDataFilesDirectory(configuredPath);
        if (!resolvedPath.isEmpty()
            && !mGameSettings.getDataDirs().contains(resolvedPath, Qt::CaseInsensitive))
        {
            mGameSettings.setMultiValue(QLatin1String("data"), resolvedPath);
            mGameSettings.addDataDir(resolvedPath);
        }
    }

    return true;
}

bool Launcher::MainDialog::setupGameData()
{
    QStringList candidates = mGameSettings.getDataDirs();
    if (!mBuildDataPath.isEmpty())
        candidates.prepend(mBuildDataPath);

    const QString localPath = QString::fromUtf8(mCfgMgr.getLocalPath().string().c_str());
    if (!localPath.isEmpty())
    {
        candidates.append(QDir(localPath).filePath(QLatin1String("Data Files")));
        candidates.append(localPath);
    }

    QStringList dataDirs;
    for (const QString& candidate : candidates)
    {
        const QString resolvedPath = resolveDataFilesDirectory(candidate);
        if (resolvedPath.isEmpty()
            || dataDirs.contains(resolvedPath, Qt::CaseInsensitive))
            continue;

        dataDirs.append(resolvedPath);
        if (!mGameSettings.getDataDirs().contains(resolvedPath, Qt::CaseInsensitive))
        {
            mGameSettings.setMultiValue(QLatin1String("data"), resolvedPath);
            mGameSettings.addDataDir(resolvedPath);
        }
    }

    if (dataDirs.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Error detecting Morrowind installation"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::NoButton);
        msgBox.setText(tr("<br><b>Could not find the Data Files location</b><br><br> \
                                   The directory containing the data files was not found."));

        QAbstractButton *wizardButton =
                msgBox.addButton(tr("Run &Installation Wizard..."), QMessageBox::ActionRole);
        QAbstractButton *skipButton =
                msgBox.addButton(tr("Skip"), QMessageBox::RejectRole);

        Q_UNUSED(skipButton); // Suppress compiler unused warning

        msgBox.exec();

        if (msgBox.clickedButton() == wizardButton)
        {
            if (!mWizardInvoker->startProcess(QStringLiteral("arenamp-wizard"), QStringList() << QStringLiteral("--from-launcher"), false)
                && !mWizardInvoker->startProcess(QStringLiteral("openmw-wizard"), QStringList() << QStringLiteral("--from-launcher"), false))
                return false;
        }
    }

    return true;
}

QString Launcher::MainDialog::primaryDataDirectory() const
{
    const QString dataLocal = mGameSettings.getDataLocal();
    if (!dataLocal.isEmpty() && QFileInfo(dataLocal).isDir())
        return QDir::cleanPath(dataLocal);

    const QStringList dataDirs = mGameSettings.getDataDirs();
    for (auto it = dataDirs.crbegin(); it != dataDirs.crend(); ++it)
    {
        if (!Config::BuildManifest::findForDataDir(*it).isEmpty())
            return QDir::cleanPath(*it);
    }

    const QStringList filters = {
        QStringLiteral("*.esm"), QStringLiteral("*.esp"),
        QStringLiteral("*.omwgame"), QStringLiteral("*.omwaddon")
    };
    for (auto it = dataDirs.crbegin(); it != dataDirs.crend(); ++it)
    {
        QDir dir(*it);
        if (dir.exists() && !dir.entryList(filters, QDir::Files | QDir::Readable).isEmpty())
            return QDir::cleanPath(*it);
    }

    return dataDirs.isEmpty() ? QString() : QDir::cleanPath(dataDirs.last());
}

bool Launcher::MainDialog::loadBuildManifest()
{
    mBuildManifestLoaded = false;
    mBuildManifestPath.clear();
    mBuildName = mLauncherSettings.value(QStringLiteral("General/Build/name"), QStringLiteral("ArenaMP"));
    mBuildDataPath = primaryDataDirectory();
    mBuildServerAddress = QStringLiteral("127.0.0.1");
    mBuildServerPort = QStringLiteral("25565");
    mBuildServerAddressSpecified = false;
    mBuildServerPortSpecified = false;
    mBuildComplete = false;

    if (mBuildDataPath.isEmpty())
        return false;

    const QString manifestPath = Config::BuildManifest::findForDataDir(mBuildDataPath);
    if (manifestPath.isEmpty())
    {
        // No saved build order exists yet. Apply the recommended order once,
        // then the first Launcher save creates build.ini and all later starts
        // preserve the user's exact order from that file.
        const QStringList orderedContent = Config::applyCanonicalContentOrder(
            mGameSettings.getContentList(), QDir(mBuildDataPath));
        if (!orderedContent.isEmpty())
        {
            mGameSettings.setContentList(orderedContent);
            QString profileName = mLauncherSettings.getCurrentContentListName();
            if (profileName.isEmpty())
                profileName = QStringLiteral("Default");
            QStringList profileFiles = orderedContent;
            const QStringList groundcover = mGameSettings.getGroundcoverList();
            profileFiles.append(groundcover);
            mLauncherSettings.setContentList(profileName, profileFiles, groundcover,
                !groundcover.isEmpty());
            mLauncherSettings.setCurrentContentListName(profileName);
        }
        return false;
    }

    Config::BuildManifest manifest;
    QString error;
    if (!manifest.read(manifestPath, &error))
    {
        qWarning() << "Could not read build manifest" << manifestPath << error;
        return false;
    }

    const QString resolvedDataPath = manifest.resolvedDataPath(manifestPath);
    if (QFileInfo(resolvedDataPath).isDir())
    {
        mBuildDataPath = QDir::cleanPath(resolvedDataPath);
        if (!mGameSettings.getDataDirs().contains(mBuildDataPath))
        {
            mGameSettings.setMultiValue(QStringLiteral("data"), mBuildDataPath);
            mGameSettings.addDataDir(mBuildDataPath);
        }
    }

    const QString effectiveBuildName = manifest.buildName.trimmed().isEmpty()
        ? QStringLiteral("ArenaMP") : manifest.buildName.trimmed();
    // A present build.ini always owns the enabled plug-in list, including an
    // intentionally empty one. Never merge it with launcher.cfg/openmw.cfg or
    // auto-enable other plug-ins merely because they exist in Data Files.
    const QStringList orderedContent = manifest.contentFiles;
    mGameSettings.setContentList(orderedContent);
    mGameSettings.setGroundcoverList(manifest.groundcoverFiles);

    QStringList profileFiles = orderedContent;
    profileFiles.append(manifest.groundcoverFiles);
    mLauncherSettings.setContentList(effectiveBuildName, profileFiles,
        manifest.groundcoverFiles, !manifest.groundcoverFiles.isEmpty());
    mLauncherSettings.setCurrentContentListName(effectiveBuildName);

    if (!manifest.archives.isEmpty())
    {
        mGameSettings.remove(QStringLiteral("fallback-archive"));
        for (const QString& archive : manifest.archives)
            mGameSettings.setMultiValue(QStringLiteral("fallback-archive"), archive);
    }

    // Do not manufacture English when an older manifest has no language field.
    // When the field is present, build.ini is authoritative and the translated
    // launcher UI must not replace its canonical value.
    if (manifest.languageSpecified)
    {
        const QString manifestLanguage = Config::BuildManifest::canonicalLanguage(manifest.language);
        mLauncherSettings.remove(QStringLiteral("Settings/language"));
        mLauncherSettings.setValue(QStringLiteral("Settings/language"), manifestLanguage);
        if (manifestLanguage == QLatin1String("Polish"))
            mGameSettings.setValue(QStringLiteral("encoding"), QStringLiteral("win1250"));
        else if (manifestLanguage == QLatin1String("Russian"))
            mGameSettings.setValue(QStringLiteral("encoding"), QStringLiteral("win1251"));
        else
            mGameSettings.setValue(QStringLiteral("encoding"), QStringLiteral("win1252"));
    }

    mBuildManifestLoaded = true;
    mBuildManifestPath = manifestPath;
    mBuildName = effectiveBuildName;
    mBuildServerAddress = manifest.serverAddress.trimmed().isEmpty()
        ? QStringLiteral("127.0.0.1") : manifest.serverAddress.trimmed();
    mBuildServerPort = manifest.serverPort.trimmed().isEmpty()
        ? QStringLiteral("25565") : manifest.serverPort.trimmed();
    mBuildServerAddressSpecified = manifest.serverAddressSpecified;
    mBuildServerPortSpecified = manifest.serverPortSpecified;
    mBuildComplete = manifest.complete;
    writeClientEndpoint(mBuildServerAddress, mBuildServerPort);

    if (mPlayPage != nullptr)
    {
        mPlayPage->setBuildName(mBuildName);
        mPlayPage->setServerAddress(mBuildServerAddress);
        mPlayPage->setServerPort(mBuildServerPort);
        mPlayPage->setBuildManifestComplete(mBuildComplete);
        mPlayPage->setAlternativeServer(manifest.altAddress, manifest.altPort, manifest.useAlternativeServer);
        mPlayPage->setProjectUrl(manifest.projectUrl);
        if (mBuildServerAddressSpecified)
        {
            mPlayPage->setAutoStartServer(false);
            mPlayPage->setAutoRestartServer(false);
            if (mServerDialog != nullptr)
                mServerDialog->setAutoRestartEnabled(false);
        }
    }

    QTimer::singleShot(0, this, [this, manifestPath]() { UpdateController::showResult(this, manifestPath); });
    qDebug() << "Loaded ArenaMP build manifest:" << manifestPath;
    return true;
}

bool Launcher::MainDialog::writeBuildManifest()
{
    QString dataDir = mBuildDataPath;
    if (dataDir.isEmpty() || !QFileInfo(dataDir).isDir())
        dataDir = primaryDataDirectory();
    if (dataDir.isEmpty() || !QFileInfo(dataDir).isDir())
        return true;

    // Always save the active manifest beside the selected Data Files. If an
    // older patch loaded a fallback copy beside the executable, read it as the
    // source once and migrate its values to the canonical location.
    const QString sourceManifestPath = mBuildManifestPath;
    const QString manifestPath = Config::BuildManifest::canonicalPathForDataDir(dataDir);

    Config::BuildManifest manifest;
    bool existingManifestRead = false;
    QString storedServerAddress;
    bool storedServerAddressSpecified = false;
    QString storedServerPort;
    bool storedServerPortSpecified = false;
    QString existingManifestPath = manifestPath;
    if (!sourceManifestPath.isEmpty() && QFileInfo::exists(sourceManifestPath))
        existingManifestPath = sourceManifestPath;

    if (QFileInfo::exists(existingManifestPath))
    {
        existingManifestRead = manifest.read(existingManifestPath);
        if (existingManifestRead)
        {
            storedServerAddress = manifest.serverAddress;
            storedServerAddressSpecified = manifest.serverAddressSpecified;
            storedServerPort = manifest.serverPort;
            storedServerPortSpecified = manifest.serverPortSpecified;
        }

    }

    manifest.formatVersion = 1;
    manifest.buildName = mPlayPage != nullptr ? mPlayPage->buildName() : mBuildName;
    manifest.dataPath = Config::BuildManifest::portableDataPath(manifestPath, dataDir);
    manifest.language = mLauncherSettings.value(QStringLiteral("Settings/language"), QStringLiteral("English"));
    const bool localServerModeSelected = mPlayPage != nullptr
        && mPlayPage->autoStartServer();

    if ((localServerModeSelected || mBuildComplete || (mPlayPage && mPlayPage->alternativeServer())) && existingManifestRead)
    {
        // Host mode is a launcher choice. Do not replace the distributed remote
        // endpoint in build.ini with this machine's LAN address and local port.
        manifest.serverAddress = storedServerAddress;
        manifest.serverAddressSpecified = storedServerAddressSpecified;
        manifest.serverPort = storedServerPort;
        manifest.serverPortSpecified = storedServerPortSpecified;
        manifest.vanillaServerCompatibility = false;
    }
    else if (localServerModeSelected)
    {
        // A newly generated local-host profile intentionally has no distributed
        // endpoint. This allows Host mode to stay the default until an address
        // is explicitly added to build.ini.
        manifest.serverAddress = QStringLiteral("127.0.0.1");
        manifest.serverAddressSpecified = false;
        manifest.serverPort = QStringLiteral("25565");
        manifest.serverPortSpecified = false;
        manifest.vanillaServerCompatibility = false;
    }
    else
    {
        manifest.serverAddress = mPlayPage != nullptr ? mPlayPage->serverAddress() : mBuildServerAddress;
        manifest.serverAddressSpecified = !manifest.serverAddress.trimmed().isEmpty();
        manifest.serverPort = mPlayPage != nullptr ? mPlayPage->serverPort() : mBuildServerPort;
        manifest.serverPortSpecified = !manifest.serverPort.trimmed().isEmpty();
        manifest.vanillaServerCompatibility = false;
    }
    if (mPlayPage)
    {
        manifest.useAlternativeServer = mPlayPage->alternativeServer();
        manifest.altAddress = mPlayPage->alternativeAddress();
        manifest.altPort = mPlayPage->alternativePort();
    }
    manifest.complete = mBuildComplete;
    manifest.contentFiles = mGameSettings.getContentList();
    manifest.groundcoverFiles = mGameSettings.getGroundcoverList();
    manifest.archives = Config::LauncherSettings::reverse(
        mGameSettings.values(QStringLiteral("fallback-archive")));

    QString error;
    if (!manifest.write(manifestPath, &error))
    {
        cfgError(tr("Error writing ArenaMP build manifest"),
            tr("<br><b>Could not write %1</b><br><br>%2").arg(manifestPath, error));
        return false;
    }

    mBuildManifestLoaded = true;
    mBuildManifestPath = manifestPath;
    mBuildName = manifest.buildName;
    mBuildDataPath = dataDir;
    mBuildServerAddress = manifest.serverAddress;
    mBuildServerPort = manifest.serverPort;
    mBuildServerAddressSpecified = manifest.serverAddressSpecified;
    mBuildServerPortSpecified = manifest.serverPortSpecified;
    mBuildComplete = manifest.complete;
    writeClientEndpoint(mBuildServerAddress, mBuildServerPort);
    return true;
}

void Launcher::MainDialog::applyBuildManifestRestrictions()
{
    Config::BuildManifest linkManifest;
    if (mPlayPage)
        mPlayPage->setProjectUrl(Config::BuildManifest::websiteForManifest(mBuildManifestPath));
    if (mPlayPage && !mBuildManifestPath.isEmpty() && linkManifest.read(mBuildManifestPath))
    {
        mPlayPage->setAlternativeServer(linkManifest.altAddress, linkManifest.altPort, linkManifest.useAlternativeServer);

    }

    if (mPlayPage != nullptr)
    {
        mPlayPage->setBuildManifestComplete(mBuildComplete);
    }

    // build.ini stores the current user order; keep Data Files available so
    // deliberate Launcher changes can be written back to the manifest.
    if (iconWidget != nullptr && iconWidget->count() > 1)
        iconWidget->item(1)->setHidden(false);

    if (mAdvancedPage != nullptr)
        mAdvancedPage->setGameMechanicsVisible(!mBuildComplete);

}

bool Launcher::MainDialog::isLocalServerAddress(const QString& address) const
{
    const QString normalized = address.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QLatin1String("localhost")
        || normalized == QLatin1String("127.0.0.1") || normalized == QLatin1String("::1")
        || normalized == QLatin1String("0.0.0.0"))
        return true;

    return mServerDialog != nullptr
        && normalized == mServerDialog->displayAddress().trimmed().toLower();
}

void Launcher::MainDialog::writeClientEndpoint(const QString& address, const QString& port) const
{
    QDir userDir(QString::fromUtf8(mCfgMgr.getUserConfigPath().string().c_str()));
    if (!userDir.exists())
        userDir.mkpath(QStringLiteral("."));

    QFile cfgFile(userDir.filePath(QStringLiteral("tes3mp-client-default.cfg")));
    QStringList lines;
    bool foundAddress = false;
    bool foundPort = false;

    if (cfgFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream input(&cfgFile);
        input.setCodec("UTF-8");
        while (!input.atEnd())
            lines.append(input.readLine());
        cfgFile.close();
    }

    for (QString& line : lines)
    {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("destinationAddress")))
        {
            line = QStringLiteral("destinationAddress = ") + address;
            foundAddress = true;
        }
        else if (trimmed.startsWith(QLatin1String("port")) && !trimmed.startsWith(QLatin1String("password")))
        {
            line = QStringLiteral("port = ") + port;
            foundPort = true;
        }
    }

    if (!foundAddress)
        lines.append(QStringLiteral("destinationAddress = ") + address);
    if (!foundPort)
        lines.append(QStringLiteral("port = ") + port);

    if (cfgFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream output(&cfgFile);
        output.setCodec("UTF-8");
        for (const QString& line : lines)
            output << line << '\n';
    }
}

bool Launcher::MainDialog::setupGraphicsSettings()
{
    // This method is almost a copy of OMW::Engine::loadSettings().  They should definitely
    // remain consistent, and possibly be merged into a shared component.  At the very least
    // the filenames should be in the CfgMgr component.

    // Ensure to clear previous settings in case we had already loaded settings.
    mEngineSettings.clear();
    // ArenaMP uses settings-default.cfg as the canonical preset. Prefer the
    // text file so newly added fork settings are available immediately; keep
    // defaults.bin only as a compatibility fallback for incomplete packages.
    const std::string localDefault = (mCfgMgr.getLocalPath() / "defaults.bin").string();
    const std::string globalDefault = (mCfgMgr.getGlobalPath() / "defaults.bin").string();
    const std::string localDefaultCfg = (mCfgMgr.getLocalPath() / "settings-default.cfg").string();
    const std::string globalDefaultCfg = (mCfgMgr.getGlobalPath() / "settings-default.cfg").string();
    std::string defaultPath;
    bool defaultIsTextCfg = false;

    if (boost::filesystem::exists(localDefaultCfg))
    {
        defaultPath = localDefaultCfg;
        defaultIsTextCfg = true;
    }
    else if (boost::filesystem::exists(globalDefaultCfg))
    {
        defaultPath = globalDefaultCfg;
        defaultIsTextCfg = true;
    }
    else if (boost::filesystem::exists(localDefault))
        defaultPath = localDefault;
    else if (boost::filesystem::exists(globalDefault))
        defaultPath = globalDefault;
    else {
        cfgError(tr("Error reading OpenMW configuration file"),
                 tr("<br><b>Could not find defaults.bin or settings-default.cfg</b><br><br>                      The problem may be due to an incomplete installation of OpenMW.<br>                      Reinstalling OpenMW may resolve the problem."));
        return false;
    }

    try {
        mEngineSettings.loadDefault(defaultPath, !defaultIsTextCfg);
    }
    catch (std::exception& e) {
        std::string msg = std::string("<br><b>Error reading default settings</b><br><br>") + e.what();
        cfgError(tr("Error reading OpenMW configuration file"), tr(msg.c_str()));
        return false;
    }

    // Always configure the canonical user settings path. The explicit preset
    // Apply button may need to create settings.cfg after a clean Wizard run.
    const std::string userPath = mCfgMgr.getPrimarySettingsPath().string();
    mEngineSettings.setUserSettingsPath(userPath);

    // User settings are not required to exist.
    if (!boost::filesystem::exists(userPath))
        return true;

    try {
        mEngineSettings.loadUser(userPath);
    }
    catch (std::exception& e) {
        std::string msg = std::string("<br><b>Error reading settings.cfg</b><br><br>") + e.what();
        cfgError(tr("Error reading OpenMW configuration file"), tr(msg.c_str()));
        return false;
    }

    return true;
}

void Launcher::MainDialog::loadSettings()
{
    int posX = mLauncherSettings.value(QString("General/MainWindow/posx")).toInt();
    int posY = mLauncherSettings.value(QString("General/MainWindow/posy")).toInt();

    // Keep the page layouts stable and ignore stale width/height values.
    setFixedSize(sLauncherWidth, sLauncherHeight);
    move(posX, posY);

    if (mPlayPage != nullptr && mServerDialog != nullptr)
    {
        auto readLauncherBool = [this](const QString& key, const QString& defaultValue) -> bool
        {
            if (mLauncherSettings.getSettings().contains(key))
                return mLauncherSettings.value(key).compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
            return defaultValue.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        };

        const QString localServerDefault = mBuildManifestLoaded && mBuildServerAddressSpecified
            ? QStringLiteral("false") : QStringLiteral("true");
        const bool autoStart = readLauncherBool(QStringLiteral("General/Server/autoStart"), localServerDefault);
        const bool autoRestart = readLauncherBool(QStringLiteral("General/Server/autoRestart"), localServerDefault);
        mPlayPage->setAutoStartServer(autoStart);
        mPlayPage->setAutoRestartServer(autoRestart);
        mPlayPage->setBuildName(mBuildName);

        mServerDialog->setAutoRestartEnabled(autoRestart);
    }
}

void Launcher::MainDialog::saveSettings()
{
    QString width = QString::number(sLauncherWidth);
    QString height = QString::number(sLauncherHeight);

    mLauncherSettings.remove(QString("General/MainWindow/width"));
    mLauncherSettings.remove(QString("General/MainWindow/height"));
    mLauncherSettings.setValue(QString("General/MainWindow/width"), width);
    mLauncherSettings.setValue(QString("General/MainWindow/height"), height);

    QString posX = QString::number(this->pos().x());
    QString posY = QString::number(this->pos().y());

    mLauncherSettings.setValue(QString("General/MainWindow/posx"), posX);
    mLauncherSettings.setValue(QString("General/MainWindow/posy"), posY);

    mLauncherSettings.setValue(QString("General/firstrun"), QString("false"));

    if (mPlayPage != nullptr)
    {
        mLauncherSettings.remove(QStringLiteral("General/Server/autoStart"));
        mLauncherSettings.setValue(QStringLiteral("General/Server/autoStart"),
            mPlayPage->autoStartServer() ? QStringLiteral("true") : QStringLiteral("false"));
        mLauncherSettings.remove(QStringLiteral("General/Server/autoRestart"));
        mLauncherSettings.setValue(QStringLiteral("General/Server/autoRestart"),
            mPlayPage->autoRestartServer() ? QStringLiteral("true") : QStringLiteral("false"));
        mLauncherSettings.remove(QStringLiteral("General/Server/vanillaBuild"));
        mLauncherSettings.remove(QStringLiteral("General/Server/bindAddress"));
        mLauncherSettings.setValue(QStringLiteral("General/Server/bindAddress"),
            mPlayPage->hostBindAddress());
        mLauncherSettings.remove(QStringLiteral("General/Chat/hideHistory"));
        mLauncherSettings.setValue(QStringLiteral("General/Build/name"), mPlayPage->buildName());
    }

}

bool Launcher::MainDialog::writeSettings()
{
    // Now write all config files
    saveSettings();
    mDataFilesPage->saveSettings();

    // Y001: GraphicsPage performs a merge-safe save: it first captures only
    // controls changed relative to the page snapshot, then reloads settings.cfg
    // and replays just those keys. This makes manual Water/Terrain/PBR/shadow
    // changes apply on Play without overwriting unrelated in-game settings.
    if (!mGraphicsPage->saveSettings())
        return false;

    mSettingsPage->saveSettings();
    if (!mPlayPage->saveServerSettings())
        return false;

    QString userPath = QString::fromUtf8(mCfgMgr.getUserConfigPath().string().c_str());
    QDir dir(userPath);

    if (!dir.exists()) {
        if (!dir.mkpath(userPath)) {
            cfgError(tr("Error creating OpenMW configuration directory"),
                     tr("<br><b>Could not create %0</b><br><br> \
                         Please make sure you have the right permissions \
                         and try again.<br>").arg(userPath));
            return false;
        }
    }

    // Game settings
    QFile file(dir.filePath(QString("openmw.cfg")));

    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        // File cannot be opened or created
        cfgError(tr("Error writing OpenMW configuration file"),
                 tr("<br><b>Could not open or create %0 for writing</b><br><br> \
                     Please make sure you have the right permissions \
                     and try again.<br>").arg(file.fileName()));
        return false;
    }


    mGameSettings.writeFileWithComments(file);
    file.close();

    // Launcher settings
    file.setFileName(dir.filePath(QString(Config::LauncherSettings::sLauncherConfigFileName)));

    if (!file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate)) {
        // File cannot be opened or created
        cfgError(tr("Error writing Launcher configuration file"),
                 tr("<br><b>Could not open or create %0 for writing</b><br><br> \
                     Please make sure you have the right permissions \
                     and try again.<br>").arg(file.fileName()));
        return false;
    }

    QTextStream stream(&file);
    stream.setDevice(&file);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));

    mLauncherSettings.writeFile(stream);
    file.close();

    if (!writeBuildManifest())
        return false;

    return true;
}

void Launcher::MainDialog::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}

void Launcher::MainDialog::wizardStarted()
{
    hide();
}

void Launcher::MainDialog::wizardFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0 || exitStatus == QProcess::CrashExit)
        return qApp->quit();

    // The Wizard has just replaced openmw.cfg and launcher.cfg. Reload both
    // before validating the selected Data Files directory.
    if (!setup() || !reloadSettings())
        return qApp->quit();

    if (setupGameData())
    {
        show();
        raise();
        activateWindow();
        QTimer::singleShot(0, this, SLOT(checkForUpdates()));
    }
}

void Launcher::MainDialog::checkForUpdates()
{
    // A reopened launcher must compare the actual installed revisions again.
    // The resume argument only identifies the return from the updater; it is
    // not proof that the requested content/engine was installed successfully.
    if (mUpdateCheckRunning)
        return;

    mUpdateAvailable = false;
    if (mPlayButton == nullptr || mPlayPage == nullptr || mBuildManifestPath.isEmpty())
        return;

    mUpdateCheckRunning = true;
    mPlayButton->setEnabled(false);
    mPlayButton->setText(tr("Checking for updates..."));
    mPlayPage->setPlayButtonState(tr("Checking for updates..."), false);
    mPlayPage->setUpdateState(true, false);

    const UpdateController::CheckResult result = UpdateController::checkAvailable(
        this, mBuildManifestPath, mBuildDataPath);

    mUpdateCheckRunning = false;
    mUpdateAvailable = result == UpdateController::CheckResult::UpdateAvailable;
    mPlayButton->setText(mUpdateAvailable ? tr("Update") : tr("Play"));
    mPlayButton->setIcon(ArenaUi::glassIcon(mUpdateAvailable ? QStringLiteral("update-dark") : QStringLiteral("play-dark")));
    mPlayButton->setEnabled(true);
    mPlayPage->setPlayButtonState(mUpdateAvailable ? tr("Update") : tr("Start game"), true);
    mPlayPage->setUpdateState(false, mUpdateAvailable);
}

void Launcher::MainDialog::play()
{
    if (mUpdateCheckRunning)
        return;

    if (mUpdateAvailable)
    {
        if (!UpdateController::startUpdate(this, mBuildManifestPath, mBuildDataPath))
            return;

        mUpdateAvailable = false;
        mPlayButton->setEnabled(false);
        mPlayPage->setPlayButtonState(tr("Update"), false);
        hide();
        close();
        QCoreApplication::exit(0);
        return;
    }

    const bool alternate = mPlayPage->alternativeServer();
    bool portOk = false;
    const int port = mPlayPage->alternativePort().toInt(&portOk);
    if (alternate && (mPlayPage->alternativeAddress().isEmpty() || !portOk || port < 1 || port > 65535))
    {
        QMessageBox::warning(this, tr("Invalid server address"), tr("Enter an address and a port from 1 to 65535."));
        return;
    }

    if (!writeSettings())
        return qApp->quit();

    if (!mGameSettings.hasMaster())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("No game file selected"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setText(tr("<br><b>You do not have a game file selected.</b><br><br> "
                          "ArenaMP will not start without a game file selected.<br>"));
        msgBox.exec();
        return;
    }

    mPendingClientAddress = alternate ? mPlayPage->alternativeAddress() : mPlayPage->serverAddress();
    mPendingClientPort = alternate ? mPlayPage->alternativePort() : mPlayPage->serverPort();

    bool startedNow = false;
    const bool localServerMode = mPlayPage->autoStartServer();
    if (localServerMode)
    {
        // Host mode overrides the remote endpoint for this launch only.
        // The build.ini endpoint remains unchanged.
        mServerDialog->setAutoRestartEnabled(mPlayPage->autoRestartServer());

        if (!mServerDialog->isRunning())
        {
            QString bindError;
            if (!mServerDialog->setConfiguredLocalAddress(mPlayPage->hostBindAddress(), &bindError))
            {
                QMessageBox::warning(this, tr("Invalid server interface"), bindError);
                mPendingClientAddress.clear();
                mPendingClientPort.clear();
                return;
            }

            QString portError;
            if (!mServerDialog->setConfiguredPort(mPlayPage->serverPort(), &portError))
            {
                QMessageBox::warning(this, tr("Invalid server port"), portError);
                mPendingClientAddress.clear();
                mPendingClientPort.clear();
                return;
            }
        }

        // The host's own client must use a local endpoint. The value in the
        // Server Address field may intentionally be a public WAN address and
        // may not support NAT loopback on the user's router.
        mPendingClientAddress = mServerDialog->localConnectAddress();
        mPendingClientPort = mServerDialog->configuredPort();
        writeClientEndpoint(mPendingClientAddress, mPendingClientPort);

        if (!mServerDialog->isRunning())
        {
            if (mServerDialog->isServerReachable())
            {
                mPlayPage->setServerRunning(true, mPendingClientAddress,
                    mPendingClientPort, false);
            }
            else
            {
                startedNow = mServerDialog->startServer();
                if (!startedNow)
                {
                    mPendingClientAddress.clear();
                    mPendingClientPort.clear();
                    return;
                }
            }
        }
    }

    if (startedNow)
        QTimer::singleShot(900, this, SLOT(launchClient()));
    else
        launchClient();
}

void Launcher::MainDialog::launchClient()
{
    const QString address = mPendingClientAddress.trimmed().isEmpty()
        ? mPlayPage->serverAddress() : mPendingClientAddress.trimmed();
    const QString port = mPendingClientPort.trimmed().isEmpty()
        ? mPlayPage->serverPort() : mPendingClientPort.trimmed();

    writeClientEndpoint(address, port);

    QStringList arguments;
    arguments.append(QLatin1String("--connect=") + address + QLatin1String(":") + port);
    mPendingClientAddress.clear();
    mPendingClientPort.clear();

    if (mGameInvoker->startProcess(QLatin1String("tes3mp"), arguments, true))
    {
        if (mServerDialog != nullptr && mServerDialog->isRunning())
            return;
        qApp->quit();
    }
}

void Launcher::MainDialog::runServer()
{
    if (!writeSettings())
        return;

    mServerDialog->setAutoRestartEnabled(mPlayPage->autoRestartServer());

    if (!mServerDialog->isRunning())
    {
        QString bindError;
        if (!mServerDialog->setConfiguredLocalAddress(mPlayPage->hostBindAddress(), &bindError))
        {
            QMessageBox::warning(this, tr("Invalid server interface"), bindError);
            return;
        }

        QString portError;
        if (!mServerDialog->setConfiguredPort(mPlayPage->serverPort(), &portError))
        {
            QMessageBox::warning(this, tr("Invalid server port"), portError);
            return;
        }
    }

    mPlayPage->switchToServerConsoleTab();
    mServerDialog->startServer();
}

void Launcher::MainDialog::stopServer()
{
    if (mServerDialog != nullptr)
        mServerDialog->stopServer();
}


QString Launcher::MainDialog::resolveSelectedDataFilePath(const QString& fileName, const QStringList& selectedPaths) const
{
    for (const QString& path : selectedPaths)
    {
        if (QFileInfo(path).fileName().compare(fileName, Qt::CaseInsensitive) == 0)
            return path;
    }

    QStringList searchDirs;
    if (!mGameSettings.getDataLocal().isEmpty())
        searchDirs << mGameSettings.getDataLocal();
    searchDirs << mGameSettings.getDataDirs();
    for (const QString& dirPath : searchDirs)
    {
        const QString candidate = QDir(dirPath).filePath(fileName);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

void Launcher::MainDialog::updateServerDataFileHashes()
{
    if (mPlayPage == nullptr || !mPlayPage->autoStartServer())
        return;

    // Hash generation and strict verification are one operation in Host mode.
    mPlayPage->setEnforceDataFiles(true);
    if (!mPlayPage->saveServerSettings())
        return;

    // Pull the current UI selection into GameSettings first. This guarantees
    // that JSON order is exactly the order visible on the Data Files page.
    mDataFilesPage->saveSettings();
    const QStringList selectedPaths = mDataFilesPage->selectedFilePaths();
    const QStringList contentFiles = mGameSettings.getContentList();
    const QStringList groundcoverFiles = mGameSettings.getGroundcoverList();

    if (contentFiles.isEmpty())
    {
        QMessageBox::warning(this, tr("Update Hash"), tr("No content files are selected."));
        return;
    }

    auto crc32ForPath = [](const QString& path, quint32* value) -> bool
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        boost::crc_32_type crc;
        while (!file.atEnd())
        {
            const QByteArray chunk = file.read(1024 * 1024);
            if (chunk.isEmpty() && file.error() != QFile::NoError)
                return false;
            crc.process_bytes(chunk.constData(), static_cast<std::size_t>(chunk.size()));
        }
        *value = crc.checksum();
        return true;
    };

    auto makeEntry = [&](const QString& fileName, QJsonObject* entry, QString* error) -> bool
    {
        const QString path = resolveSelectedDataFilePath(fileName, selectedPaths);
        if (path.isEmpty())
        {
            *error = tr("Selected data file was not found: %1").arg(fileName);
            return false;
        }
        quint32 crc = 0;
        if (!crc32ForPath(path, &crc))
        {
            *error = tr("Could not calculate CRC32 for %1").arg(QDir::toNativeSeparators(path));
            return false;
        }
        const QString hash = QStringLiteral("0x")
            + QString::number(crc, 16).rightJustified(8, QLatin1Char('0')).toUpper();
        QJsonArray hashes;
        hashes.append(hash);
        entry->insert(fileName, hashes);
        return true;
    };

    QJsonArray contentArray;
    QJsonArray groundcoverArray;
    QString error;
    for (const QString& fileName : contentFiles)
    {
        QJsonObject entry;
        if (!makeEntry(fileName, &entry, &error))
        {
            QMessageBox::critical(this, tr("Update Hash"), error);
            return;
        }
        contentArray.append(entry);
    }
    for (const QString& fileName : groundcoverFiles)
    {
        QJsonObject entry;
        if (!makeEntry(fileName, &entry, &error))
        {
            QMessageBox::critical(this, tr("Update Hash"), error);
            return;
        }
        groundcoverArray.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), 2);
    root.insert(QStringLiteral("content"), contentArray);
    root.insert(QStringLiteral("groundcover"), groundcoverArray);

    const QString path = mServerDialog->requiredDataFilesPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, tr("Update Hash"),
            tr("Could not write %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (output.write(json) != json.size() || !output.commit())
    {
        QMessageBox::critical(this, tr("Update Hash"),
            tr("Could not finish writing %1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    // Keep build.ini in the same order as the generated server manifest.
    writeBuildManifest();

    QString status = tr("Server manifest updated.\n\nRequired content: %1\nOptional groundcover: %2\n\n%3")
        .arg(contentFiles.size()).arg(groundcoverFiles.size())
        .arg(QDir::toNativeSeparators(path));
    if (mServerDialog->isRunning())
        status += tr("\n\nRestart the server to apply the new manifest.");
    QMessageBox::information(this, tr("Update Hash"), status);
}

void Launcher::MainDialog::clearServerCells()
{
    if (mServerDialog == nullptr)
        return;
    if (mServerDialog->isRunning())
    {
        QMessageBox::warning(this, tr("Clear server cells"),
            tr("Stop the server before clearing persistent data."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::warning(this,
        tr("Clear server cells?"),
        tr("This will delete all saved cell state. Player accounts and world data will be kept. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!mServerDialog->clearPersistentCells(&error))
    {
        QMessageBox::critical(this, tr("Clear server cells"),
            tr("Server data reset failed: %1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Clear server cells"), tr("All saved server cells were cleared."));
}

void Launcher::MainDialog::resetServerData()
{
    if (mServerDialog == nullptr)
        return;
    if (mServerDialog->isRunning())
    {
        QMessageBox::warning(this, tr("Full server reset"),
            tr("Stop the server before clearing persistent data."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::critical(this,
        tr("Full server reset?"),
        tr("This will delete player accounts, cells, world state, maps, custom data, record stores and the server database. requiredDataFiles.json and banlist.json will be kept. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!mServerDialog->resetPersistentServerData(&error))
    {
        QMessageBox::critical(this, tr("Full server reset"),
            tr("Server data reset failed: %1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Full server reset"), tr("Server gameplay data was fully reset."));
}

void Launcher::MainDialog::autoStartServerChanged(bool enabled)
{
    mLauncherSettings.remove(QStringLiteral("General/Server/autoStart"));
    mLauncherSettings.setValue(QStringLiteral("General/Server/autoStart"),
        enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

void Launcher::MainDialog::autoRestartServerChanged(bool enabled)
{
    if (mPlayPage != nullptr && mPlayPage->autoRestartServer() != enabled)
        mPlayPage->setAutoRestartServer(enabled);
    if (mServerDialog != nullptr && mServerDialog->autoRestartEnabled() != enabled)
        mServerDialog->setAutoRestartEnabled(enabled);

    mLauncherSettings.remove(QStringLiteral("General/Server/autoRestart"));
    mLauncherSettings.setValue(QStringLiteral("General/Server/autoRestart"),
        enabled ? QStringLiteral("true") : QStringLiteral("false"));
}



void Launcher::MainDialog::serverRunningChanged(bool running, const QString& address, const QString& port)
{
    if (mPlayPage != nullptr)
    {
        mPlayPage->setServerRunning(running, address, port);
    }

    updateFooterServerStatus(running, address, port);
}

void Launcher::MainDialog::showChangelog()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("CHANGELOG.txt")),
        QDir(appDir).filePath(QStringLiteral("CHANGELOG.md")),
        QDir::current().filePath(QStringLiteral("CHANGELOG.txt")),
        QDir::current().filePath(QStringLiteral("CHANGELOG.md"))
    };

    QString changelogPath;
    for (const QString& candidate : candidates)
    {
        if (QFileInfo(candidate).isFile())
        {
            changelogPath = candidate;
            break;
        }
    }

    if (changelogPath.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("Changelog"),
            tr("CHANGELOG.txt was not found in the ArenaMP installation."));
        return;
    }

    QFile file(changelogPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, QStringLiteral("Changelog"),
            tr("Could not open the changelog file: %1").arg(changelogPath));
        return;
    }

    QTextStream stream(&file);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    const QString markdown = stream.readAll();

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("arenaChangelogDialog"));
    dialog.setWindowTitle(QStringLiteral("%1 — Changelog").arg(mBuildName));
    dialog.resize(860, 600);
    dialog.setMinimumSize(620, 420);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QLabel* header = new QLabel(QStringLiteral("<b>%1 — Changelog</b>").arg(mBuildName.toHtmlEscaped()), &dialog);
    QFont headerFont = header->font();
    headerFont.setPointSizeF(headerFont.pointSizeF() + 2.0);
    header->setFont(headerFont);
    layout->addWidget(header);

    QTextBrowser* browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->setHtml(changelogMarkdownToHtml(markdown));
    layout->addWidget(browser, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    ArenaUi::installGlassWindow(dialog);
    dialog.exec();
}

void Launcher::MainDialog::help()
{
    Misc::HelpViewer::openHelp("reference/index.html");
}
