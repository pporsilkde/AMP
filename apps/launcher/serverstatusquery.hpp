#ifndef ARENAMP_SERVERSTATUSQUERY_HPP
#define ARENAMP_SERVERSTATUSQUERY_HPP
#include <QObject>
#include <QUdpSocket>
#include <QHostInfo>
#include <QTimer>
#include <QUuid>
#include <QWidget>
#include <functional>
#include <components/openmw-mp/serverstatus.hpp>

namespace Launcher
{
    class ServerStatusQuery : public QObject
    {
    public:
        enum State { NoAddress, Checking, Online, NoResponse };
        std::function<void(State, const ArenaStatus::Status&)> result;
        explicit ServerStatusQuery(QWidget* parent) : QObject(parent), mPage(parent)
        {
            mInterval.setInterval(15000);
            mDebounce.setSingleShot(true);
            mTimeout.setSingleShot(true);
            connect(&mInterval, &QTimer::timeout, this, [this]() { if (mPage->isVisible()) refresh(); });
            connect(&mDebounce, &QTimer::timeout, this, [this]() { refresh(); });
            connect(&mTimeout, &QTimer::timeout, this, [this]() { cancel(); publish(NoResponse); });
            connect(&mSocket, &QUdpSocket::readyRead, this, [this]() { receive(); });
            mInterval.start();
        }
        ~ServerStatusQuery() override { cancel(); }
        void setEndpoint(QString host, quint16 port)
        {
            host = host.trimmed();
            if (host.startsWith('[') && host.endsWith(']')) host = host.mid(1, host.size() - 2);
            if (host == mHost && port == mPort) return;
            cancel();
            mHost = host; mPort = port;
            publish(host.isEmpty() || port == 0 ? NoAddress : Checking);
            if (!host.isEmpty() && port != 0) mDebounce.start(600);
        }
        void refresh()
        {
            cancel();
            if (mHost.isEmpty() || !mPort) { publish(NoAddress); return; }
            publish(Checking);
            const unsigned generation = mGeneration;
            mToken = QUuid::createUuid().toRfc4122().left(8).toStdString();
            mTimeout.start(3000);
            mLookup = QHostInfo::lookupHost(mHost, this, [this, generation](const QHostInfo& info)
            {
                if (generation != mGeneration) return;
                mLookup = -1;
                mAddresses = info.addresses();
                if (mAddresses.isEmpty() || !mSocket.bind(QHostAddress(QHostAddress::Any), 0))
                { cancel(); publish(NoResponse); return; }
                // Support RakNet Time32 and Time64 builds, using one ephemeral socket.
                int sent = 0;
                for (const auto& address : mAddresses)
                {
                    if (address.isMulticast() || address == QHostAddress(QHostAddress::Broadcast)) continue;
                    for (unsigned size : {8u, 4u})
                    {
                        const std::string packet = ArenaStatus::ping(mToken, size);
                        if (mSocket.writeDatagram(packet.data(), static_cast<qint64>(packet.size()), address, mPort) >= 0) ++sent;
                    }
                    if (sent >= 8) break;
                }
                if (sent == 0) { cancel(); publish(NoResponse); }
            });
        }
    private:
        void publish(State state, const ArenaStatus::Status& status = {}) { if (result) result(state, status); }
        void cancel()
        {
            ++mGeneration;
            if (mLookup != -1) QHostInfo::abortHostLookup(mLookup);
            mLookup = -1;
            mTimeout.stop(); mDebounce.stop(); mSocket.abort();
        }
        void receive()
        {
            while (mSocket.hasPendingDatagrams())
            {
                const qint64 length = mSocket.pendingDatagramSize();
                if (length < 0) break;
                if (length > 512) { mSocket.readDatagram(nullptr, 0); continue; }
                QByteArray bytes(static_cast<int>(length), '\0');
                QHostAddress sender; quint16 port = 0;
                const qint64 received = mSocket.readDatagram(bytes.data(), bytes.size(), &sender, &port);
                if (received != length || port != mPort || !mAddresses.contains(sender)) continue;
                const auto status = ArenaStatus::pong(bytes.toStdString(), mToken);
                if (!status.reachable) continue;
                cancel();
                publish(Online, status);
                return;
            }
        }
        QWidget* mPage;
        QUdpSocket mSocket;
        QTimer mInterval, mDebounce, mTimeout;
        QString mHost;
        quint16 mPort = 0;
        QList<QHostAddress> mAddresses;
        std::string mToken;
        unsigned mGeneration = 0;
        int mLookup = -1;
    };
}
#endif
