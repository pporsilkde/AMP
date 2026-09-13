// ArenaMP U025 — скелет ArenaLink.
//
// Что здесь настоящее: кадрирование TCP-потока, состояние клиента,
// challenge-response, правила рассылки, лимиты. Что помечено TODO:
// привязка к конкретным структурам сервера (учётки, внутриигровой чат)
// и SHA-256/HMAC — их берём из уже имеющейся в дереве реализации,
// новую зависимость ради этого не тянем.

#include "LinkServer.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using socklen_t = int;
#  define ARENA_CLOSESOCKET closesocket
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define ARENA_CLOSESOCKET ::close
#endif

using namespace mwmp;
using namespace ArenaLink;

namespace
{
    std::uint64_t nowSec()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    void randomBytes(std::uint8_t* out, std::size_t size)
    {
        static thread_local std::mt19937_64 engine{std::random_device{}()};
        for (std::size_t i = 0; i < size; ++i)
            out[i] = static_cast<std::uint8_t>(engine() & 0xFF);
    }
}

struct LinkServer::Client
{
    int socket = -1;
    std::string address;
    std::string inBuffer;
    std::string outBuffer;
    std::uint8_t nonce[sNonceSize] = {};
    bool helloDone = false;
    bool authorized = false;
    std::uint32_t userId = 0;
    std::string name;
    std::uint16_t level = 0;
    std::uint32_t color = 0xC8C8C8u;
    std::uint16_t channel = 0;
    std::uint64_t lastSeen = 0;
    std::uint64_t minuteStart = 0;
    unsigned sentThisMinute = 0;
};

LinkServer::LinkServer() = default;

LinkServer::~LinkServer()
{
    stop();
}

void LinkServer::configure(const LinkConfig& config, const LinkCallbacks& callbacks)
{
    mConfig = config;
    mCallbacks = callbacks;
}

bool LinkServer::start(unsigned short gamePort)
{
    if (!mConfig.enabled || mRunning.load())
        return false;

    mPort = mConfig.port != 0 ? mConfig.port : static_cast<unsigned short>(gamePort + 2);

    mListenSocket = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (mListenSocket < 0)
        return false;

    int reuse = 1;
    ::setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(mPort);
    address.sin_addr.s_addr = mConfig.bindAddress == "0.0.0.0"
        ? htonl(INADDR_ANY) : inet_addr(mConfig.bindAddress.c_str());

    if (::bind(mListenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || ::listen(mListenSocket, 16) != 0)
    {
        ARENA_CLOSESOCKET(mListenSocket);
        mListenSocket = -1;
        return false;
    }

    // Каналы по умолчанию. Первый зеркалит внутриигровой чат, поэтому
    // написанное в лаунчере видно в игре и наоборот; остальные живут
    // только в лаунчере.
    mChannels = {
        { 1, "Общий",        static_cast<std::uint8_t>(CHANNEL_WRITABLE | CHANNEL_MIRRORS_GAME) },
        { 2, "Поиск группы", CHANNEL_WRITABLE },
        { 3, "Торговля",     CHANNEL_WRITABLE },
        { 4, "Объявления",   static_cast<std::uint8_t>(CHANNEL_ADMIN_ONLY) },
    };
    loadHistory();

    mRunning.store(true);
    mThread = std::thread(&LinkServer::threadMain, this);
    return true;
}

void LinkServer::stop()
{
    if (!mRunning.exchange(false))
        return;
    if (mListenSocket >= 0)
    {
        ARENA_CLOSESOCKET(mListenSocket);
        mListenSocket = -1;
    }
    if (mThread.joinable())
        mThread.join();

    std::lock_guard<std::mutex> lock(mMutex);
    for (Client* client : mClients)
    {
        if (client->socket >= 0)
            ARENA_CLOSESOCKET(client->socket);
        delete client;
    }
    mClients.clear();
}

LinkServer::Stats LinkServer::stats() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    Stats copy = mStats;
    copy.connections = static_cast<unsigned>(mClients.size());
    copy.authorized = static_cast<unsigned>(std::count_if(mClients.begin(), mClients.end(),
        [](const Client* client) { return client->authorized; }));
    return copy;
}

void LinkServer::threadMain()
{
    // Один поток, select по всем сокетам: соединений десятки, не тысячи,
    // и отдельный поток на клиента здесь не нужен.
    while (mRunning.load())
    {
        // TODO: собрать fd_set из mListenSocket и сокетов клиентов,
        // select с таймаутом 200 мс, затем acceptPending() и
        // serviceClient() для готовых.
        acceptPending();

        std::lock_guard<std::mutex> lock(mMutex);
        const std::uint64_t now = nowSec();
        for (auto it = mClients.begin(); it != mClients.end();)
        {
            Client* client = *it;
            serviceClient(*client);
            // 90 с без единого кадра — клиент мёртв. Лаунчер шлёт PING
            // раз в 30 с, так что живое соединение сюда не попадает.
            if (client->socket < 0 || now - client->lastSeen > 90)
            {
                if (client->socket >= 0)
                    ARENA_CLOSESOCKET(client->socket);
                delete client;
                it = mClients.erase(it);
            }
            else
                ++it;
        }
    }
}

void LinkServer::acceptPending()
{
    sockaddr_in from{};
    socklen_t fromSize = sizeof(from);
    const int socket = static_cast<int>(::accept(mListenSocket,
        reinterpret_cast<sockaddr*>(&from), &fromSize));
    if (socket < 0)
        return;

    std::lock_guard<std::mutex> lock(mMutex);
    if (mClients.size() >= mConfig.maxConnections)
    {
        ARENA_CLOSESOCKET(socket);
        return;
    }

    const std::string address = inet_ntoa(from.sin_addr);
    const unsigned fromAddress = static_cast<unsigned>(std::count_if(mClients.begin(), mClients.end(),
        [&address](const Client* client) { return client->address == address; }));
    if (fromAddress >= mConfig.maxConnectionsPerAddress)
    {
        ARENA_CLOSESOCKET(socket);
        return;
    }

#ifndef _WIN32
    ::fcntl(socket, F_SETFL, O_NONBLOCK);
    int noDelay = 1;
    ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
#endif

    Client* client = new Client();
    client->socket = socket;
    client->address = address;
    client->lastSeen = nowSec();
    client->minuteStart = client->lastSeen;
    randomBytes(client->nonce, sNonceSize);
    mClients.push_back(client);

    // CHALLENGE отправляем сразу: клиенту не нужно ждать HELLO-ответа,
    // а nonce уже связан с этим соединением.
    sendTo(*client, makeChallenge(client->nonce, mConfig.authMode));
}

void LinkServer::serviceClient(Client& client)
{
    char buffer[4096];
    for (;;)
    {
        const auto received = ::recv(client.socket, buffer, sizeof(buffer), 0);
        if (received > 0)
        {
            client.inBuffer.append(buffer, static_cast<std::size_t>(received));
            client.lastSeen = nowSec();
            if (client.inBuffer.size() > sMaxPayload * 4)
            {
                closeClient(client, "переполнение буфера");
                return;
            }
            continue;
        }
        if (received == 0)
        {
            closeClient(client, "клиент закрыл соединение");
            return;
        }
        break;    // EAGAIN
    }

    for (;;)
    {
        FrameHeader header;
        std::string payload;
        std::size_t consumed = 0;
        bool fatal = false;
        if (!nextFrame(client.inBuffer.data(), client.inBuffer.size(), header, payload, consumed, fatal))
        {
            if (fatal)
                closeClient(client, "битый кадр");
            return;
        }
        client.inBuffer.erase(0, consumed);
        handleFrame(client, header, payload);
        if (client.socket < 0)
            return;
    }

    // TODO: дослать client.outBuffer, если предыдущий send() ушёл не целиком.
}

void LinkServer::handleFrame(Client& client, const FrameHeader& header, const std::string& payload)
{
    Reader reader(payload);

    // До авторизации принимаем ровно два типа кадров: всё остальное —
    // повод закрыть соединение, а не молча проигнорировать.
    if (!client.authorized && header.type != TYPE_HELLO && header.type != TYPE_AUTH
        && header.type != TYPE_PING)
    {
        closeClient(client, "кадр до авторизации");
        return;
    }

    switch (header.type)
    {
        case TYPE_HELLO:
        {
            const std::uint16_t protocol = reader.u16();
            reader.u8();                          // вид клиента: ПК/Android
            reader.text(32);                      // версия клиента, для логов
            if (protocol != sProtocol)
            {
                sendTo(client, makeAuthFail(FAIL_VERSION, "Версия лаунчера не совпадает с сервером"));
                closeClient(client, "версия протокола");
                return;
            }
            client.helloDone = true;
            break;
        }
        case TYPE_AUTH:      handleAuth(client, reader); break;
        case TYPE_SEND:      handleSend(client, reader); break;
        case TYPE_HISTORY_REQ: handleHistory(client, reader); break;
        case TYPE_JOIN_CHANNEL:
        {
            const std::uint16_t channel = reader.u16();
            const auto found = std::find_if(mChannels.begin(), mChannels.end(),
                [channel](const Channel& c) { return c.id == channel; });
            if (found != mChannels.end())
            {
                client.channel = channel;
                // TODO: отдать последние mConfig.historyOnJoin сообщений канала.
            }
            break;
        }
        case TYPE_VOICE_TICKET_REQ: handleVoiceTicket(client, reader); break;
        case TYPE_PING:
        {
            Writer writer;
            writer.u32(reader.u32());
            sendTo(client, frame(TYPE_PONG, writer.data()));
            break;
        }
        default:
            break;
    }
}

void LinkServer::handleAuth(Client& client, Reader& reader)
{
    const std::string name = reader.text(sMaxNick);
    const std::uint8_t mode = reader.u8();
    const std::string secret = reader.text(128);
    if (!reader.ok() || name.empty())
    {
        closeClient(client, "некорректный AUTH");
        return;
    }

    if (!throttleAuth(client.address))
    {
        ++mStats.rejectedAuth;
        sendTo(client, makeAuthFail(FAIL_RATE, "Слишком много попыток, подождите"));
        closeClient(client, "перебор пароля");
        return;
    }

    LinkAccount account;
    if (mCallbacks.findAccount == nullptr || !mCallbacks.findAccount(name, account))
    {
        sendTo(client, makeAuthFail(FAIL_NO_ACCOUNT,
            "Персонаж не найден. Зайдите на сервер и создайте его в игре."));
        return;
    }
    if (account.banned)
    {
        sendTo(client, makeAuthFail(FAIL_BANNED, "Учётная запись заблокирована"));
        return;
    }

    bool verified = false;
    switch (mode)
    {
        case AUTH_PROOF:
            // proof = HMAC-SHA256(sha256(пароль), nonce). Пароль по сети
            // не идёт вообще. Если у учётки нет passwordSha256 (bcrypt в
            // хранилище) — клиенту заранее выдан AUTH_PLAIN в CHALLENGE.
            // TODO: сравнение постоянного времени с посчитанным HMAC.
            verified = !account.passwordSha256.empty() && secret.size() == sProofSize;
            break;
        case AUTH_PLAIN:
            verified = mCallbacks.verifyPassword != nullptr
                && mCallbacks.verifyPassword(name, secret);
            break;
        case AUTH_CODE:
            verified = mCallbacks.verifyLinkCode != nullptr
                && mCallbacks.verifyLinkCode(name, secret);
            break;
        default:
            break;
    }

    if (!verified)
    {
        ++mStats.rejectedAuth;
        sendTo(client, makeAuthFail(FAIL_BAD_CREDENTIALS, "Неверный пароль"));
        return;
    }

    client.authorized = true;
    client.userId = account.userId;
    client.name = account.name;
    client.level = account.level;
    client.color = account.color;
    client.channel = mChannels.empty() ? 0 : mChannels.front().id;

    AuthResult result;
    result.userId = account.userId;
    result.name = account.name;
    result.level = account.level;
    result.color = account.color;
    result.className = account.className;
    result.voicePort = 0;                 // TODO: VoiceServer::port(), 0 если голос выключен
    sendTo(client, makeAuthOk(result));
    sendTo(client, makeChannels(mChannels));
    // TODO: PRESENCE всем, история текущего канала этому клиенту.
}

void LinkServer::handleSend(Client& client, Reader& reader)
{
    const std::uint16_t channelId = reader.u16();
    const std::string text = reader.text16(sMaxText);
    if (!reader.ok() || text.empty())
        return;

    if (!throttleSend(client))
    {
        ++mStats.rejectedRate;
        sendTo(client, makeNotice(1, "Слишком часто. Подождите немного."));
        return;
    }

    const auto found = std::find_if(mChannels.begin(), mChannels.end(),
        [channelId](const Channel& c) { return c.id == channelId; });
    if (found == mChannels.end() || (found->flags & CHANNEL_WRITABLE) == 0)
    {
        sendTo(client, makeNotice(1, "В этот канал писать нельзя"));
        return;
    }
    // TODO: проверка админского канала и серверного mute — та же, что в игре.

    Message message;
    message.id = mNextMessageId++;
    message.timestamp = static_cast<std::uint32_t>(nowSec());
    message.channel = channelId;
    message.userId = client.userId;
    message.author = client.name;
    // Уровень и цвет пишутся снимком: история не перекрашивается, когда
    // игрок сменил цвет через /chatcolor или взял уровень.
    message.level = client.level;
    message.color = client.color;
    message.text = text;

    mHistory[channelId].push_back(message);
    while (mHistory[channelId].size() > mConfig.historyPerChannel)
        mHistory[channelId].pop_front();
    appendHistory(message);
    ++mStats.messages;

    broadcast(channelId, makeMessage(message));

    if ((found->flags & CHANNEL_MIRRORS_GAME) != 0 && mCallbacks.pushToGameChat != nullptr)
        mCallbacks.pushToGameChat(client.name, text);
}

void LinkServer::handleHistory(Client& client, Reader& reader)
{
    const std::uint16_t channelId = reader.u16();
    const std::uint64_t beforeId = reader.u64();
    const std::uint16_t limit = reader.u16();
    if (!reader.ok())
        return;

    const auto found = mHistory.find(channelId);
    if (found == mHistory.end())
        return;

    std::vector<Message> batch;
    const unsigned wanted = std::min<unsigned>(limit == 0 ? mConfig.historyOnJoin : limit, 100);
    for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
    {
        if (beforeId != 0 && it->id >= beforeId)
            continue;
        batch.push_back(*it);
        if (batch.size() >= wanted)
            break;
    }
    std::reverse(batch.begin(), batch.end());

    // Кадр ограничен sMaxPayload, а сообщение может быть на 4000 символов,
    // поэтому отдаём порциями, а не одним куском.
    std::vector<Message> chunk;
    std::size_t chunkBytes = 0;
    for (const Message& message : batch)
    {
        const std::size_t size = message.text.size() + message.author.size() + 32;
        if (chunkBytes + size > sMaxPayload - 64 || chunk.size() >= 255)
        {
            sendTo(client, makeMessages(channelId, chunk));
            chunk.clear();
            chunkBytes = 0;
        }
        chunk.push_back(message);
        chunkBytes += size;
    }
    if (!chunk.empty())
        sendTo(client, makeMessages(channelId, chunk));
}

void LinkServer::handleVoiceTicket(Client& client, Reader& reader)
{
    const std::uint8_t scope = reader.u8();
    if (!reader.ok() || mCallbacks.issueVoiceTicket == nullptr)
        return;

    std::uint8_t ticket[sTicketSize] = {};
    std::uint16_t port = 0;
    if (!mCallbacks.issueVoiceTicket(client.userId, scope, ticket, port))
    {
        sendTo(client, makeNotice(1, "Голосовой сервер недоступен"));
        return;
    }
    sendTo(client, makeVoiceTicket(ticket, port, 120));
}

void LinkServer::publishFromGame(const std::string& author, std::uint32_t userId,
    std::uint16_t level, std::uint32_t color, const std::string& text)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const Channel& channel : mChannels)
    {
        if ((channel.flags & CHANNEL_MIRRORS_GAME) == 0)
            continue;

        Message message;
        message.id = mNextMessageId++;
        message.timestamp = static_cast<std::uint32_t>(nowSec());
        message.channel = channel.id;
        message.userId = userId;
        message.author = author;
        message.level = level;
        message.color = color;
        message.flags = MESSAGE_FROM_GAME;
        message.text = text;

        mHistory[channel.id].push_back(message);
        while (mHistory[channel.id].size() > mConfig.historyPerChannel)
            mHistory[channel.id].pop_front();
        appendHistory(message);
        broadcast(channel.id, makeMessage(message));
    }
}

void LinkServer::publishPresence(const LinkAccount& account, bool online)
{
    std::lock_guard<std::mutex> lock(mMutex);
    Presence presence;
    presence.userId = account.userId;
    presence.name = account.name;
    presence.level = account.level;
    presence.color = account.color;
    presence.flags = static_cast<std::uint8_t>(online ? (PRESENCE_ONLINE | PRESENCE_INGAME) : 0);

    // Уровень и цвет живого игрока обновляются здесь же: лаунчер
    // перекрашивает ник, не переспрашивая.
    for (Client* client : mClients)
    {
        if (!client->authorized)
            continue;
        if (client->userId == account.userId)
        {
            client->level = account.level;
            client->color = account.color;
        }
        sendTo(*client, frame(TYPE_PRESENCE_DELTA, makePresence({presence}).substr(sHeaderSize)));
    }
}

void LinkServer::disconnectUser(std::uint32_t userId, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (Client* client : mClients)
    {
        if (client->userId != userId)
            continue;
        sendTo(*client, makeNotice(2, reason));
        closeClient(*client, reason);
    }
}

void LinkServer::sendTo(Client& client, const std::string& data)
{
    if (client.socket < 0)
        return;
    const auto sent = ::send(client.socket, data.data(), static_cast<int>(data.size()), 0);
    if (sent < 0)
    {
        // TODO: EAGAIN — сложить остаток в client.outBuffer и дослать,
        // когда select скажет, что сокет готов на запись.
        return;
    }
    if (static_cast<std::size_t>(sent) < data.size())
        client.outBuffer.append(data, static_cast<std::size_t>(sent), data.size() - static_cast<std::size_t>(sent));
}

void LinkServer::broadcast(std::uint16_t channel, const std::string& data)
{
    for (Client* client : mClients)
    {
        // Сообщение уходит всем авторизованным, а не только тем, кто
        // сейчас смотрит канал: иначе непрочитанное не посчитать.
        if (client->authorized)
            sendTo(*client, data);
    }
    (void)channel;
}

void LinkServer::closeClient(Client& client, const std::string&)
{
    if (client.socket >= 0)
        ARENA_CLOSESOCKET(client.socket);
    client.socket = -1;
    client.authorized = false;
}

void LinkServer::loadHistory()
{
    if (mConfig.storageDir.empty())
        return;
    // TODO: прочитать хвост server/data/chat/<channel>.jsonl (последние
    // historyPerChannel строк) и восстановить mNextMessageId.
}

void LinkServer::appendHistory(const Message& message)
{
    if (mConfig.storageDir.empty())
        return;
    std::ofstream file(mConfig.storageDir + "/" + std::to_string(message.channel) + ".jsonl",
        std::ios::app | std::ios::binary);
    if (!file)
        return;
    // TODO: экранирование кавычек и управляющих символов в author/text.
    file << "{\"id\":" << message.id
         << ",\"ts\":" << message.timestamp
         << ",\"uid\":" << message.userId
         << ",\"lvl\":" << message.level
         << ",\"color\":" << message.color
         << ",\"flags\":" << static_cast<unsigned>(message.flags)
         << ",\"name\":\"" << message.author
         << "\",\"text\":\"" << message.text << "\"}\n";
}

bool LinkServer::throttleAuth(const std::string& address)
{
    const std::uint64_t now = nowSec();
    auto& entry = mAuthAttempts[address];
    if (entry.second > now)
        return false;
    if (++entry.first >= mConfig.authTriesBeforeBlock)
    {
        entry.first = 0;
        entry.second = now + mConfig.authBlockSeconds;
        return false;
    }
    return true;
}

bool LinkServer::throttleSend(Client& client)
{
    const std::uint64_t now = nowSec();
    if (now - client.minuteStart >= 60)
    {
        client.minuteStart = now;
        client.sentThisMinute = 0;
    }
    return ++client.sentThisMinute <= mConfig.sendRatePerMinute;
}
