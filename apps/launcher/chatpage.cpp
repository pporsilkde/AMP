// ArenaMP U025 — скелет страницы «Чат и голос».
//
// Подключение в maindialog.cpp:
//   sNavigationItems: 4 -> 5
//   createIcons():  пятый QListWidgetItem, иконка "chat" из arenaicons.qrc
//   createPages():  mChatPage = new ChatPage(mLauncherSettings, this);
//                   pagesWidget->addWidget(mChatPage);   // пятым: порядок
//                                                        // страниц обязан
//                                                        // совпадать с иконками
//   mChatPage->setServerEndpoint(addr, port.toUShort());
//   в play()/serverRunningChanged(): mChatPage->setGameRunning(...)
// CMakeLists.txt: chatpage.cpp, arenalinkclient.cpp, voice/voicepanel.cpp
//   в LAUNCHER; заголовки — в LAUNCHER_HEADER и LAUNCHER_HEADER_MOC.

#include "chatpage.hpp"
#include "voice/voicepanel.hpp"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QColor>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStackedLayout>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

#include <components/config/launchersettings.hpp>

using namespace Launcher;

ChatPage::ChatPage(Config::LauncherSettings& launcherSettings, QWidget* parent)
    : QWidget(parent)
    , mLauncherSettings(launcherSettings)
    , mClient(new ArenaLinkClient(this))
    , mStack(new QStackedLayout(this))
{
    setObjectName(QStringLiteral("chatPage"));
    buildLoginCard();
    buildChatView();

    connect(mClient, &ArenaLinkClient::loggedIn, this, &ChatPage::slotLoggedIn);
    connect(mClient, &ArenaLinkClient::loginFailed, this, &ChatPage::slotLoginFailed);
    connect(mClient, &ArenaLinkClient::disconnected, this, &ChatPage::slotDisconnected);
    connect(mClient, &ArenaLinkClient::channelsReceived, this, &ChatPage::slotChannelsReceived);
    connect(mClient, &ArenaLinkClient::messagesReceived, this, &ChatPage::slotMessagesReceived);
    connect(mClient, &ArenaLinkClient::messageReceived, this, &ChatPage::slotMessageReceived);
    connect(mClient, &ArenaLinkClient::presenceReceived, this, &ChatPage::slotPresenceReceived);
    connect(mClient, &ArenaLinkClient::notice, this, &ChatPage::slotNotice);
    connect(mClient, &ArenaLinkClient::voiceTicketReceived, this,
        [this](const QByteArray& ticket, quint16 port, int ttl)
        {
            emit voiceTicketReady(ticket, port, ttl);
        });

    loadSettings();
}

void ChatPage::buildLoginCard()
{
    QWidget* card = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->addStretch();

    QLabel* title = new QLabel(tr("ArenaMP chat sign-in"), card);
    title->setProperty("arenaTitle", true);
    title->setAlignment(Qt::AlignHCenter);

    QLabel* hint = new QLabel(tr(
        "The character name and password of the game server, the same as in game.\n"
        "A character is created in game only: press Play and join the server."), card);
    hint->setAlignment(Qt::AlignHCenter);
    hint->setProperty("arenaMuted", true);

    mNameEdit = new QLineEdit(card);
    mNameEdit->setPlaceholderText(tr("Character name"));
    mNameEdit->setMaxLength(32);

    mSecretEdit = new QLineEdit(card);
    mSecretEdit->setPlaceholderText(tr("Password"));
    mSecretEdit->setEchoMode(QLineEdit::Password);
    mSecretEdit->setMaxLength(64);

    mLoginButton = new QPushButton(tr("Sign in"), card);
    mLoginButton->setDefault(true);
    mUseCodeButton = new QPushButton(tr("Sign in with an in-game code (/chatlink)"), card);
    mUseCodeButton->setProperty("arenaQuiet", true);
    mLoginStatus = new QLabel(card);
    mLoginStatus->setAlignment(Qt::AlignHCenter);
    mLoginStatus->setWordWrap(true);

    for (QWidget* widget : { static_cast<QWidget*>(title), static_cast<QWidget*>(hint),
        static_cast<QWidget*>(mNameEdit), static_cast<QWidget*>(mSecretEdit),
        static_cast<QWidget*>(mLoginButton), static_cast<QWidget*>(mUseCodeButton),
        static_cast<QWidget*>(mLoginStatus) })
    {
        widget->setMaximumWidth(420);
        layout->addWidget(widget, 0, Qt::AlignHCenter);
    }
    layout->addStretch();

    connect(mLoginButton, &QPushButton::clicked, this, &ChatPage::slotLoginClicked);
    connect(mSecretEdit, &QLineEdit::returnPressed, this, &ChatPage::slotLoginClicked);
    connect(mUseCodeButton, &QPushButton::clicked, this, &ChatPage::slotToggleCodeMode);

    mStack->addWidget(card);
}

void ChatPage::buildChatView()
{
    QWidget* view = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(view);

    QHBoxLayout* header = new QHBoxLayout();
    mProfileLabel = new QLabel(view);
    mLogoutButton = new QPushButton(tr("Sign out"), view);
    mLogoutButton->setProperty("arenaQuiet", true);
    header->addWidget(mProfileLabel, 1);
    header->addWidget(mLogoutButton);
    outer->addLayout(header);

    QHBoxLayout* body = new QHBoxLayout();

    // Левая колонка: каналы, затем управление голосом.
    QWidget* left = new QWidget(view);
    left->setFixedWidth(200);
    QVBoxLayout* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    mChannels = new QListWidget(left);
    mVoicePanel = new VoicePanel(left);
    leftLayout->addWidget(mChannels, 1);
    leftLayout->addWidget(mVoicePanel);

    mHistory = new QTextBrowser(view);
    mHistory->setOpenExternalLinks(true);

    mPlayers = new QListWidget(view);
    mPlayers->setFixedWidth(170);

    body->addWidget(left);
    body->addWidget(mHistory, 1);
    body->addWidget(mPlayers);
    outer->addLayout(body, 1);

    QHBoxLayout* footer = new QHBoxLayout();
    mInput = new QLineEdit(view);
    mInput->setPlaceholderText(tr("Message…"));
    mInput->setMaxLength(static_cast<int>(ArenaLink::sMaxText));
    mSendButton = new QPushButton(tr("Send"), view);
    footer->addWidget(mInput, 1);
    footer->addWidget(mSendButton);
    outer->addLayout(footer);

    connect(mSendButton, &QPushButton::clicked, this, &ChatPage::slotSendClicked);
    connect(mInput, &QLineEdit::returnPressed, this, &ChatPage::slotSendClicked);
    connect(mLogoutButton, &QPushButton::clicked, this, &ChatPage::slotLogoutClicked);
    connect(mChannels, &QListWidget::currentRowChanged, this, &ChatPage::slotChannelChanged);

    mStack->addWidget(view);
}

void ChatPage::loadSettings(const QString& gameSettingsPath)
{
    if (!gameSettingsPath.trimmed().isEmpty())
        mGameSettingsPath = gameSettingsPath;

    // Launcher/Chat/lastName remains only as a fallback for old configurations.
    // The authoritative ArenaMP account is [Login] name/password in settings.cfg.
    mNameEdit->setText(mLauncherSettings.value(QStringLiteral("Chat/lastName")));
    refreshLoginFromGameSettings();

    if (mVoicePanel != nullptr)
        mVoicePanel->loadSettings(mLauncherSettings);
}

void ChatPage::refreshLoginFromGameSettings()
{
    if (mGameSettingsPath.trimmed().isEmpty())
        return;

    QFile file(mGameSettingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString accountName;
    QString accountPassword;
    QString section;
    QTextStream input(&file);
    input.setCodec("UTF-8");

    while (!input.atEnd())
    {
        QString line = input.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';')))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
        {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        if (section.compare(QStringLiteral("Login"), Qt::CaseInsensitive) != 0)
            continue;

        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;

        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        if (key.compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0)
            accountName = value;
        else if (key.compare(QStringLiteral("password"), Qt::CaseInsensitive) == 0)
            accountPassword = value;
    }

    if (!accountName.isEmpty())
        mNameEdit->setText(accountName);

    // Never copy the password into launcher.cfg. Keep it only in memory and
    // show it masked in the normal password mode. /chatlink code mode keeps
    // its own temporary value untouched until the user switches back.
    mGamePassword = accountPassword;
    if (!mCodeMode)
        mSecretEdit->setText(mGamePassword);
}

void ChatPage::saveSettings()
{
    mLauncherSettings.setValue(QStringLiteral("Chat/lastName"), mNameEdit->text().trimmed());
    if (mVoicePanel != nullptr)
        mVoicePanel->saveSettings(mLauncherSettings);
}


bool ChatPage::voiceEnabled() const
{
    return mVoicePanel != nullptr && mVoicePanel->voiceEnabled();
}

QString ChatPage::pushToTalkKey() const
{
    return mVoicePanel != nullptr ? mVoicePanel->pushToTalkKey() : QStringLiteral("V");
}

void ChatPage::setServerEndpoint(const QString& host, quint16 gamePort)
{
    // Порты: игровой (RakNet) не трогаем, голос = +1, чат = +2.
    mClient->setServer(host, gamePort);
}

void ChatPage::setGameRunning(bool running)
{
    mGameRunning = running;
    if (mVoicePanel != nullptr)
        mVoicePanel->setGameRunning(running);
}

void ChatPage::slotLoginClicked()
{
    const QString name = mNameEdit->text().trimmed();
    const QString secret = mSecretEdit->text();
    if (name.isEmpty() || secret.isEmpty())
    {
        setStatus(tr("Enter the character name and password"), true);
        return;
    }
    mLoginButton->setEnabled(false);
    setStatus(tr("Connecting to the game server…"));
    if (mCodeMode)
        mClient->connectAndLoginWithCode(name, secret);
    else
        mClient->connectAndLogin(name, secret);
}

void ChatPage::slotToggleCodeMode()
{
    mCodeMode = !mCodeMode;
    mSecretEdit->setEchoMode(mCodeMode ? QLineEdit::Normal : QLineEdit::Password);
    mSecretEdit->setPlaceholderText(mCodeMode ? tr("In-game code") : tr("Password"));
    if (mCodeMode)
        mSecretEdit->clear();
    else
    {
        refreshLoginFromGameSettings();
        mSecretEdit->setText(mGamePassword);
    }
    mUseCodeButton->setText(mCodeMode ? tr("Sign in with a password")
                                      : tr("Sign in with an in-game code (/chatlink)"));
}

void ChatPage::slotLoggedIn(const LinkProfile& profile)
{
    mLoginButton->setEnabled(true);
    mSecretEdit->clear();
    saveSettings();

    mProfileLabel->setText(tr("%1 · level %2").arg(profile.name).arg(profile.level));
    mStack->setCurrentIndex(1);
    mHistory->clear();

}

void ChatPage::slotLoginFailed(quint8 reason, const QString& text)
{
    mLoginButton->setEnabled(true);
    if (!text.isEmpty())
    {
        setStatus(text, true);
        return;
    }
    switch (reason)
    {
        case ArenaLink::FAIL_NO_ACCOUNT:
            setStatus(tr("Character not found. Join the server and create it in game."), true);
            break;
        case ArenaLink::FAIL_BANNED:
            setStatus(tr("This account is blocked"), true);
            break;
        case ArenaLink::FAIL_RATE:
            setStatus(tr("Too many attempts, please wait"), true);
            break;
        case ArenaLink::FAIL_CODE_EXPIRED:
            setStatus(tr("The code expired, get a new one with /chatlink"), true);
            break;
        case ArenaLink::FAIL_VERSION:
            setStatus(tr("Launcher version does not match the server"), true);
            break;
        default:
            setStatus(tr("Wrong name or password"), true);
            break;
    }
}

void ChatPage::slotDisconnected(const QString& reason)
{
    mLoginButton->setEnabled(true);
    mStack->setCurrentIndex(0);
    setStatus(reason, true);
    if (mVoicePanel != nullptr)
        mVoicePanel->setVoiceAvailable(false);
}

void ChatPage::slotLogoutClicked()
{
    mClient->disconnectFromServer();
    mStack->setCurrentIndex(0);
    refreshLoginFromGameSettings();
    setStatus(QString());
}

void ChatPage::slotChannelsReceived(const QVector<LinkChannel>& channels)
{
    mChannelList = channels;
    mChannels->clear();
    for (const LinkChannel& channel : channels)
    {
        // Канал-зеркало помечаем: игрок должен понимать, что его увидят
        // и в игре тоже.
        mChannels->addItem(channel.mirrorsGame
            ? QStringLiteral("%1  ↔").arg(channel.name) : channel.name);
    }
    if (!channels.isEmpty())
        mChannels->setCurrentRow(0);
}

void ChatPage::slotChannelChanged(int row)
{
    if (row < 0 || row >= mChannelList.size())
        return;
    mCurrentChannel = mChannelList[row].id;
    mHistory->clear();
    mSendButton->setEnabled(mChannelList[row].writable);
    mInput->setEnabled(mChannelList[row].writable);
    mClient->joinChannel(mCurrentChannel);
    mClient->requestHistory(mCurrentChannel, 0, 50);
}

void ChatPage::slotMessagesReceived(quint16 channel, const QVector<LinkMessage>& messages)
{
    if (channel != mCurrentChannel)
        return;
    for (const LinkMessage& message : messages)
        appendMessage(message);
}

void ChatPage::slotMessageReceived(const LinkMessage& message)
{
    if (message.channel == mCurrentChannel)
        appendMessage(message);
    // TODO: непрочитанные — жирным в списке каналов.
}

void ChatPage::slotPresenceReceived(const QVector<LinkPresence>& players)
{
    mPlayers->clear();
    for (const LinkPresence& player : players)
    {
        QListWidgetItem* item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(player.level).arg(player.name), mPlayers);
        item->setForeground(QColor(safeColor(player.color)));
        if (player.inGame)
            item->setToolTip(tr("In game"));
    }
}

void ChatPage::slotNotice(quint8 severity, const QString& text)
{
    LinkMessage message;
    message.system = true;
    message.text = text;
    message.timestamp = QDateTime::currentSecsSinceEpoch();
    message.channel = mCurrentChannel;
    appendMessage(message);
    if (severity >= 2)
        setStatus(text, true);
}

void ChatPage::appendMessage(const LinkMessage& message)
{
    QScrollBar* scrollBar = mHistory->verticalScrollBar();
    const bool atBottom = scrollBar == nullptr || scrollBar->value() >= scrollBar->maximum() - 8;
    mHistory->append(renderMessageHtml(message));
    if (atBottom)
        mHistory->moveCursor(QTextCursor::End);
}

QString ChatPage::safeColor(const QString& value)
{
    // Цвет приходит от сервера и попадает прямо в style="color:...",
    // поэтому пропускаем только #RRGGBB.
    static const QRegularExpression pattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return pattern.match(value).hasMatch() ? value.toUpper() : QStringLiteral("#C8C8C8");
}

QString ChatPage::renderMessageHtml(const LinkMessage& message)
{
    // Ничего из сети не попадает в HTML без экранирования.
    const QString author = message.author.toHtmlEscaped();
    const QString text = message.text.toHtmlEscaped();
    const QString time = QDateTime::fromSecsSinceEpoch(message.timestamp)
        .toString(QStringLiteral("HH:mm"));

    if (message.system)
        return QStringLiteral("<div style='color:#9A8F7A'><i>%1</i></div>").arg(text);

    const QString badge = message.level > 0
        ? QStringLiteral("<span style='background:#2B2318;color:#E8C87A;"
                         "padding:0 4px;border-radius:3px'>%1</span> ").arg(message.level)
        : QString();

    // Пришедшее из игры помечаем, иначе непонятно, почему человек «молчит»
    // в лаунчере, но пишет.
    const QString source = message.fromGame
        ? QStringLiteral("<span style='color:#6E6A62'> %1</span>").arg(tr("from game").toHtmlEscaped()) : QString();

    return QStringLiteral(
        "<div style='margin-bottom:4px'>%1"
        "<span style='color:%2;font-weight:bold'>%3</span>"
        "<span style='color:#6E6A62'> %4</span>%5<br>"
        "<span style='color:#DCD6CC'>%6</span></div>")
        .arg(badge, safeColor(message.color), author, time, source, text);
}

void ChatPage::slotSendClicked()
{
    const QString text = mInput->text().trimmed();
    if (text.isEmpty() || mCurrentChannel == 0 || !mClient->authorized())
        return;
    mInput->clear();
    // Эхо не рисуем: сервер вернёт сообщение пушем всем, включая автора,
    // иначе порядок в ленте разъедется.
    mClient->sendMessage(mCurrentChannel, text);
}

void ChatPage::setStatus(const QString& text, bool error)
{
    if (mLoginStatus == nullptr)
        return;
    mLoginStatus->setText(text);
    mLoginStatus->setStyleSheet(error ? QStringLiteral("color:#E06A6A") : QString());
}
