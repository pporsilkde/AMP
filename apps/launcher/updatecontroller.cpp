#include "updatecontroller.hpp"
#include <components/config/buildmanifest.hpp>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>

namespace Launcher { namespace UpdateController {
namespace {
    QString tr(const char* text) { return QCoreApplication::translate("ArenaUpdater", text); }
    bool saveJson(const QString& path, const QJsonObject& object)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        const QByteArray data = QJsonDocument(object).toJson();
        return file.write(data) == data.size() && file.flush();
    }
}

void showResult(QWidget* parent, const QString& manifestPath)
{
    if (manifestPath.isEmpty()) return;
    QFile file(QFileInfo(manifestPath).absoluteDir().filePath(QStringLiteral(".arena-update-result.json")));
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject result = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    file.remove();
    if (!result.value(QStringLiteral("ok")).toBool())
        QMessageBox::warning(parent, tr("Update failed"), result.value(QStringLiteral("message")).toString());
}

Result beforeLaunch(QWidget* parent, const QString& manifestPath, const QString& dataPath)
{
    Config::BuildManifest manifest;
    if (manifestPath.isEmpty() || !manifest.read(manifestPath)) return Result::Continue;
    const bool recoveryPending = QFileInfo::exists(QFileInfo(manifestPath).absoluteDir()
        .filePath(QStringLiteral(".arena-update-pending.json")));
    if (manifest.checkUrl.isEmpty() && !recoveryPending) return Result::Continue;

    const QString client = QCoreApplication::applicationDirPath();
    QString helper;
    QString program;
    QStringList prefix;
#ifdef Q_OS_WIN
    helper = QDir(client).filePath(QStringLiteral("arena-updater.exe"));
    const QString key = QStringLiteral("url_win");
#else
    helper = QDir(client).filePath(QStringLiteral("arena-updater"));
#ifdef Q_OS_MAC
    const QString key = QStringLiteral("url_macos");
#else
    const QString key = QStringLiteral("url_linux");
#endif
#endif
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cache);
    QTemporaryDir job(QDir(cache).filePath(QStringLiteral("arena-update-XXXXXX")));
    if (!job.isValid())
    {
        QMessageBox::warning(parent, tr("Update failed"), tr("Cannot create update staging folder."));
        return recoveryPending ? Result::Stop : Result::Continue;
    }
    if (QFileInfo::exists(helper))
    {
        // The running worker must be independent of all client files, including itself.
        program = QDir(job.path()).filePath(QFileInfo(helper).fileName());
        if (!QFile::copy(helper, program))
        {
            QMessageBox::warning(parent, tr("Update failed"),
                tr("Could not copy the updater to its staging folder."));
            return Result::Stop;
        }
        QFile::setPermissions(program, QFile::permissions(helper));
    }
    else
    {
        helper = QDir(client).filePath(QStringLiteral("arena_updater.py"));
        program = QStandardPaths::findExecutable(QStringLiteral("python3"));
#ifdef Q_OS_WIN
        if (program.isEmpty()) program = QStandardPaths::findExecutable(QStringLiteral("python"));
#endif
        if (!QFileInfo::exists(helper) || program.isEmpty())
        {
            QMessageBox::warning(parent, tr("Update failed"), tr("Updater is missing. Install the complete client package."));
            return recoveryPending ? Result::Stop : Result::Continue;
        }
        const QString script = QDir(job.path()).filePath(QStringLiteral("arena_updater.py"));
        if (!QFile::copy(helper, script))
        {
            QMessageBox::warning(parent, tr("Update failed"),
                tr("Could not copy the updater script to its staging folder."));
            return Result::Stop;
        }
        prefix << script;
    }
    QString launcher = QCoreApplication::applicationFilePath();
#ifndef Q_OS_WIN
    // Linux portable builds use a wrapper to set the bundled library paths.
    const QString wrapper = QDir(client).filePath(QStringLiteral("openmw-launcher"));
    if (QFileInfo::exists(wrapper)) launcher = wrapper;
#endif
    const QString requestPath = QDir(job.path()).filePath(QStringLiteral("request.json"));
    const QJsonObject request {
        {QStringLiteral("manifest"), QFileInfo(manifestPath).absoluteFilePath()},
        {QStringLiteral("data"), QFileInfo(dataPath).canonicalFilePath()},
        {QStringLiteral("client"), client},
        {QStringLiteral("launcher"), launcher},
        {QStringLiteral("launcher_args"), QJsonArray()},
        {QStringLiteral("engine_key"), key},
        {QStringLiteral("parent_pid"), double(QCoreApplication::applicationPid())}
    };
    if (!saveJson(requestPath, request))
    {
        QMessageBox::warning(parent, tr("Update failed"),
            tr("Could not create the updater request file."));
        return Result::Stop;
    }

    QProgressDialog progress(tr("Checking for updates..."), tr("Cancel"), 0, 0, parent);
    progress.setWindowTitle(tr("ArenaMP update"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    QProcess worker;
    QEventLoop loop;
    QByteArray lines;
    QString error;
    bool canceled = false;
    bool checking = true;
    // A hard deadline covers DNS and slow responses for check.ini. Downloads
    // use worker socket timeouts and remain cancellable while staging.
    QTimer checkTimeout;
    checkTimeout.setSingleShot(true);
    QObject::connect(&checkTimeout, &QTimer::timeout, &worker, [&]() {
        if (checking) worker.kill();
    });
    QObject::connect(&progress, &QProgressDialog::canceled, &worker, [&]() {
        canceled = true; worker.kill();
    });
    QObject::connect(&worker, &QProcess::readyReadStandardOutput, &progress, [&]() {
        lines += worker.readAllStandardOutput();
        int newline;
        while ((newline = lines.indexOf('\n')) >= 0)
        {
            const QJsonObject event = QJsonDocument::fromJson(lines.left(newline)).object();
            lines.remove(0, newline + 1);
            const QString phase = event.value(QStringLiteral("phase")).toString();
            if (phase == QLatin1String("package"))
            {
                checking = false;
                checkTimeout.stop();
                progress.setLabelText(event.value(QStringLiteral("kind")).toString() == QLatin1String("engine")
                    ? tr("Downloading engine update...") : tr("Downloading content update..."));
                progress.setRange(0, 0);
            }
            else if (phase == QLatin1String("download"))
            {
                const double total = event.value(QStringLiteral("total")).toDouble(-1);
                if (total > 0)
                {
                    progress.setRange(0, 100);
                    progress.setValue(int(event.value(QStringLiteral("done")).toDouble() * 100 / total));
                }
            }
            else if (phase == QLatin1String("extract"))
            {
                progress.setRange(0, 0);
                progress.setLabelText(tr("Preparing update files..."));
            }
            else if (phase == QLatin1String("error") || phase == QLatin1String("blocked"))
            {
                checking = false;
                error = event.value(QStringLiteral("message")).toString();
            }
        }
    });
    QObject::connect(&worker, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    worker.start(program, prefix + QStringList {QStringLiteral("prepare"), requestPath});
    if (!worker.waitForStarted(3000))
    {
        QMessageBox::warning(parent, tr("Update failed"), worker.errorString());
        return recoveryPending ? Result::Stop : Result::Continue;
    }
    checkTimeout.start(12000);
    if (worker.state() != QProcess::NotRunning) loop.exec();
    checkTimeout.stop();
    progress.close();
    const QString pendingPath = QFileInfo(manifestPath).absoluteDir().filePath(QStringLiteral(".arena-update-pending.json"));
    if (canceled)
    {
        // Preparation does not mutate target files. Our own prepared marker is disposable.
        QFile marker(pendingPath);
        if (marker.open(QIODevice::ReadOnly))
        {
            const auto record = QJsonDocument::fromJson(marker.readAll()).object();
            marker.close();
            if (record.value(QStringLiteral("job")).toString() == job.path()) marker.remove();
        }
        return Result::Stop;
    }
    if (worker.exitStatus() == QProcess::NormalExit && worker.exitCode() == 10)
    {
        // A detached helper waits for THIS PID, so the launcher and its loaded DLLs
        // have exited before any replacement. The helper opens the new launcher.
        // Keep the staging directory as the helper's working directory. This is
        // important on Windows: starting a copied .exe from the install folder
        // can leave the helper locked by the launcher process, and a relative
        // helper dependency must resolve beside the copied executable.
        job.setAutoRemove(false);
        qint64 detachedPid = 0;
        const bool started = QProcess::startDetached(
            program,
            prefix + QStringList {QStringLiteral("apply"), requestPath},
            job.path(),
            &detachedPid);
        if (started)
        {
            // Close the visible window before returning to the event loop. The
            // detached helper waits for this process PID, then replaces files.
            // Hiding/closing also prevents a second Play click during apply.
            if (parent != nullptr)
            {
                parent->setEnabled(false);
                parent->hide();
                parent->close();
            }
            // The apply helper is waiting for this PID.  Use exit(), rather
            // than only posting quit(), so this path cannot be held open by a
            // secondary Qt window/event loop (for example the server console
            // or a platform modal dialog).  No game is launched from this
            // process; the helper starts a fresh launcher after committing.
            QCoreApplication::exit(0);
            return Result::Restarting;
        }
        // No installer started, so target files are untouched and preparation can be discarded.
        job.setAutoRemove(true);
        QFile::remove(pendingPath);
        QMessageBox::warning(parent, tr("Update failed"), tr("Could not start the update installer."));
        return Result::Stop;
    }
    {
        QFile pending(pendingPath);
        if (pending.open(QIODevice::ReadOnly))
        {
            const auto record = QJsonDocument::fromJson(pending.readAll()).object();
            pending.close();
            if (record.value(QStringLiteral("job")).toString() == job.path()) pending.remove();
        }
    }
    if (worker.exitCode() == 20 || (recoveryPending && QFileInfo::exists(pendingPath)))
    {
        if (error.isEmpty())
            error = tr("The update was blocked because another client is still running.");
        QMessageBox::warning(parent, tr("Update failed"), error);
        return Result::Stop;
    }
    // A network/check-only failure is non-blocking by design: check.ini may be
    // unavailable and the installed client must still start. Once package
    // preparation has begun, however, a failed worker is fatal because the
    // update may have staged a transaction that must not be ignored.
    if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0)
    {
        if (checking && !recoveryPending)
        {
            qWarning() << "Arena updater check failed; continuing with installed client"
                       << worker.exitCode() << worker.errorString();
            return Result::Continue;
        }
        if (error.isEmpty())
            error = QString::fromUtf8(worker.readAllStandardError()).trimmed();
        if (error.isEmpty())
            error = tr("The updater stopped unexpectedly (exit code %1).").arg(worker.exitCode());
        QMessageBox::warning(parent, tr("Update failed"), error);
        return Result::Stop;
    }
    if (!error.isEmpty())
    {
        QMessageBox::warning(parent, tr("Update failed"), error);
        return Result::Stop;
    }
    // check.ini unavailable or malformed: connect using the current installation.
    return Result::Continue;
}
}}
