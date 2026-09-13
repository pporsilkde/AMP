// ArenaMP U025 — клиент ArenaLink.
//
// Сборка: arenalinkclient.cpp в LAUNCHER, .hpp в LAUNCHER_HEADER и
// LAUNCHER_HEADER_MOC. Qt5::Network уже подключён — новых зависимостей нет.

#include "arenalinkclient.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QTcpSocket>
#include <QTimer>

using namespace Launcher;
using namespace ArenaLink;

namespace
{
    constexpr int sPingIntervalMs = 30000;
    constexpr int sConnectTimeoutMs = 8000;
}

ArenaLinkClient::ArenaLinkClient(QObject* parent)
    : QObject(parent)
    , mSocket(new QTcpSocket(this))
    , mPingTimer(new QTimer(this))
{
    connect(mSocket, &QTcpSocket::connected, this, &ArenaLinkClient::slotConnected);
    connect(mSocket, &QTcpSocket::readyRead, this, &ArenaLinkClient::slotReadyRead);
    connect(mSocket, &QTcpSocket::disconnected, this, &ArenaLinkClient::slotDisconnected);
    connect(mSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
        this, &ArenaLinkClient::slotError);

    mPingTimer->setInterval(sPingIntervalMs);
    connect(mPingTimer, &QTimer::timeout, this, &ArenaLinkClient::slotPing);
}

ArenaLinkClient::~ArenaLinkClient()
{
    disconnectFromServer();
}

void ArenaLinkClient::setServer(const QString& host, quint16 gamePort, quint16 linkPort)
{
    mHost = host.trimmed();
    // Порт чата по умолчанию — игровой + 2 (игровой + 1 занят голосом).
    mLinkPort = linkPort != 0 ? linkPort : static_cast<quint16>(gamePort + 2);
}

void ArenaLinkClient::connectAndLogin(const QString& name, const QString& password)
{
    mPendingName = name;
    mPendingSecret = password;
    mPendingMode = AUTH_PROOF;
    emit connecting();
    mSocket->abort();
    mSocket->connectToHost(mHost, mLinkPort);
    QTimer::singleShot(sConnectTimeoutMs, this, [this]()
    {
        if (mSocket->state() == QAbstractSocket::ConnectingState)
        {
            mSocket->abort();
            emit disconnected(tr("The server is not responding"));
        }
    });
}

void ArenaLinkClient::connectAndLoginWithCode(const QString& name, const QString& code)
{
    connectAndLogin(name, code);
    mPendingMode = AUTH_CODE;
}

void ArenaLinkClient::disconnectFromServer()
{
    mPingTimer->stop();
    mProfile = LinkProfile();
    mBuffer.clear();
    mPendingSecret.clear();
    if (mSocket->state() != QAbstractSocket::UnconnectedState)
        mSocket->disconnectFromHost();
}

void ArenaLinkClient::slotConnected()
{
    send(makeHello(0, QStringLiteral("ArenaMP U025").toStdString()));
    // Дальше ждём CHALLENGE: без nonce отвечать нечем.
}

QByteArray ArenaLinkClient::makeProof(const QString& password, const QByteArray& nonce)
{
    const QByteArray key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QMessageAuthenticationCode::hash(nonce, key, QCryptographicHash::Sha256);
}

QString ArenaLinkClient::colorToHex(quint32 color)
{
    return QStringLiteral("#%1").arg(color & 0x00FFFFFFu, 6, 16, QLatin1Char('0')).toUpper();
}

void ArenaLinkClient::slotReadyRead()
{
    mBuffer.append(mSocket->readAll());

    for (;;)
    {
        FrameHeader header;
        std::string payload;
        std::size_t consumed = 0;
        bool fatal = false;
        if (!nextFrame(mBuffer.constData(), static_cast<std::size_t>(mBuffer.size()),
                header, payload, consumed, fatal))
        {
            if (fatal)
            {
                mSocket->abort();
                emit disconnected(tr("The server sent malformed data"));
            }
            return;
        }
        const QByteArray frameData(payload.data(), static_cast<int>(payload.size()));
        mBuffer.remove(0, static_cast<int>(consumed));
        handleFrame(header, frameData);
    }
}

void ArenaLinkClient::handleFrame(const FrameHeader& header, const QByteArray& payload)
{
    Reader reader(payload.constData(), static_cast<std::size_t>(payload.size()));

    switch (header.type)
    {
        case TYPE_CHALLENGE:
        {
            QByteArray nonce(static_cast<int>(sNonceSize), '\0');
            if (!reader.raw(nonce.data(), sNonceSize))
                return;
            const quint8 serverMode = reader.u8();

            // Сервер решает, каким способом принимать пароль: если учётки
            // лежат в bcrypt, proof посчитать нельзя и он попросит AUTH_PLAIN.
            quint8 mode = mPendingMode == AUTH_CODE ? AUTH_CODE : serverMode;
            std::string secret;
            if (mode == AUTH_PROOF)
            {
                const QByteArray proof = makeProof(mPendingSecret, nonce);
                secret.assign(proof.constData(), static_cast<std::size_t>(proof.size()));
            }
            else
            {
                secret = mPendingSecret.toStdString();
            }

            send(makeAuth(mPendingName.toStdString(), mode, secret));
            mPendingSecret.clear();      // пароль в памяти не держим
            break;
        }
        case TYPE_AUTH_OK:
        {
            AuthResult result;
            if (!parseAuthOk(reader, result))
                return;
            mProfile.userId = result.userId;
            mProfile.name = QString::fromStdString(result.name);
            mProfile.level = result.level;
            mProfile.color = colorToHex(result.color);
            mProfile.className = QString::fromStdString(result.className);
            mProfile.voicePort = result.voicePort;
            mProfile.authorized = true;
            mPingTimer->start();
            emit loggedIn(mProfile);
            break;
        }
        case TYPE_AUTH_FAIL:
        {
            const quint8 reason = reader.u8();
            const QString text = QString::fromStdString(reader.text16(256));
            emit loginFailed(reason, text);
            break;
        }
        case TYPE_CHANNELS:
        {
            std::vector<Channel> channels;
            if (!parseChannels(reader, channels))
                return;
            QVector<LinkChannel> out;
            out.reserve(static_cast<int>(channels.size()));
            for (const Channel& channel : channels)
            {
                LinkChannel item;
                item.id = channel.id;
                item.name = QString::fromStdString(channel.name);
                item.writable = (channel.flags & CHANNEL_WRITABLE) != 0;
                item.mirrorsGame = (channel.flags & CHANNEL_MIRRORS_GAME) != 0;
                out.push_back(item);
            }
            emit channelsReceived(out);
            break;
        }
        case TYPE_MESSAGES:
        {
            quint16 channel = 0;
            std::vector<Message> messages;
            if (!parseMessages(reader, channel, messages))
                return;
            QVector<LinkMessage> out;
            out.reserve(static_cast<int>(messages.size()));
            for (const Message& message : messages)
            {
                LinkMessage item;
                item.id = message.id;
                item.channel = message.channel;
                item.userId = message.userId;
                item.author = QString::fromStdString(message.author);
                item.level = message.level;
                item.color = colorToHex(message.color);
                item.text = QString::fromStdString(message.text);
                item.timestamp = message.timestamp;
                item.system = (message.flags & MESSAGE_SYSTEM) != 0;
                item.fromGame = (message.flags & MESSAGE_FROM_GAME) != 0;
                out.push_back(item);
            }
            emit messagesReceived(channel, out);
            break;
        }
        case TYPE_MESSAGE:
        {
            Message message;
            if (!readMessage(reader, message))
                return;
            LinkMessage item;
            item.id = message.id;
            item.channel = message.channel;
            item.userId = message.userId;
            item.author = QString::fromStdString(message.author);
            item.level = message.level;
            item.color = colorToHex(message.color);
            item.text = QString::fromStdString(message.text);
            item.timestamp = message.timestamp;
            item.system = (message.flags & MESSAGE_SYSTEM) != 0;
            item.fromGame = (message.flags & MESSAGE_FROM_GAME) != 0;
            emit messageReceived(item);
            break;
        }
        case TYPE_PRESENCE:
        case TYPE_PRESENCE_DELTA:
        {
            std::vector<Presence> players;
            if (!parsePresence(reader, players))
                return;
            QVector<LinkPresence> out;
            out.reserve(static_cast<int>(players.size()));
            for (const Presence& player : players)
            {
                LinkPresence item;
                item.userId = player.userId;
                item.name = QString::fromStdString(player.name);
                item.level = player.level;
                item.color = colorToHex(player.color);
                item.inGame = (player.flags & PRESENCE_INGAME) != 0;
                item.inVoice = (player.flags & PRESENCE_VOICE) != 0;
                out.push_back(item);
            }
            emit presenceReceived(out);
            break;
        }
        case TYPE_VOICE_TICKET:
        {
            QByteArray ticket(static_cast<int>(sTicketSize), '\0');
            if (!reader.raw(ticket.data(), sTicketSize))
                return;
            const quint16 port = reader.u16();
            const quint16 ttl = reader.u16();
            emit voiceTicketReceived(ticket, port, ttl);
            break;
        }
        case TYPE_NOTICE:
        {
            const quint8 severity = reader.u8();
            emit notice(severity, QString::fromStdString(reader.text16(512)));
            break;
        }
        case TYPE_PONG:
        default:
            break;
    }
}

void ArenaLinkClient::joinChannel(quint16 channel)
{
    Writer writer;
    writer.u16(channel);
    send(frame(TYPE_JOIN_CHANNEL, writer.data()));
}

void ArenaLinkClient::requestHistory(quint16 channel, quint64 beforeId, quint16 limit)
{
    send(makeHistoryRequest(channel, beforeId, limit));
}

void ArenaLinkClient::sendMessage(quint16 channel, const QString& text)
{
    send(makeSend(channel, text.toStdString()));
}

void ArenaLinkClient::requestVoiceTicket(quint8 scope)
{
    send(makeVoiceTicketRequest(scope));
}

void ArenaLinkClient::send(const std::string& data)
{
    if (mSocket->state() != QAbstractSocket::ConnectedState)
        return;
    mSocket->write(data.data(), static_cast<qint64>(data.size()));
}

void ArenaLinkClient::slotPing()
{
    Writer writer;
    writer.u32(static_cast<quint32>(QDateTime::currentSecsSinceEpoch()));
    send(frame(TYPE_PING, writer.data()));
}

void ArenaLinkClient::slotDisconnected()
{
    mPingTimer->stop();
    mProfile.authorized = false;
    emit disconnected(tr("Connection to the server was lost"));
}

void ArenaLinkClient::slotError()
{
    mPingTimer->stop();
    mProfile.authorized = false;
    emit disconnected(mSocket->errorString());
}
