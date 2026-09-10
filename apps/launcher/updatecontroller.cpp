#include "updatecontroller.hpp"

#include <components/config/buildmanifest.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>

#include <memory>

namespace Launcher { namespace UpdateController {
namespace
{
    QString tr(const char* text)
    {
        return QCoreApplication::translate("ArenaUpdater", text);
    }

    QString logPath(const QString& manifestPath)
    {
        const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!cache.isEmpty())
            QDir().mkpath(cache);
        const QStringList folders {QFileInfo(manifestPath).absolutePath(),
            QCoreApplication::applicationDirPath(), cache};
        for (const QString& folder : folders)
        {
            if (folder.isEmpty()) continue;
            QFile file(QDir(folder).filePath(QStringLiteral("Update.log")));
            if (file.size() > 4 * 1024 * 1024)
            {
                const QString previous = file.fileName() + QStringLiteral(".old");
                QFile::remove(previous);
                file.rename(previous);
                file.setFileName(QDir(folder).filePath(QStringLiteral("Update.log")));
            }
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                return file.fileName();
        }
        return QString();
    }

    void log(const QString& path, const QString& phase, const QString& message)
    {
        QFile file(path);
        if (!path.isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            const QJsonObject event {
                {QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                {QStringLiteral("pid"), double(QCoreApplication::applicationPid())},
                {QStringLiteral("phase"), phase}, {QStringLiteral("message"), message}
            };
            file.write(QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n');
        }
    }

    bool saveJson(const QString& path, const QJsonObject& object)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        const QByteArray data = QJsonDocument(object).toJson();
        return file.write(data) == data.size() && file.flush();
    }

    struct WorkerContext
    {
        std::unique_ptr<QTemporaryDir> job;
        QString program;
        QStringList prefix;
        QString requestPath;
        QString logFile;
    };

    bool createWorker(QWidget* parent, const QString& manifestPath,
        const QString& dataPath, bool quiet, WorkerContext* context)
    {
        if (context == nullptr || manifestPath.isEmpty())
            return false;

        context->logFile = logPath(manifestPath);
        log(context->logFile, QStringLiteral("launcher_prepare"), manifestPath);

        Config::BuildManifest manifest;
        if (!manifest.read(manifestPath))
        {
            log(context->logFile, QStringLiteral("error"), QStringLiteral("Cannot read build.ini"));
            return false;
        }

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

        QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cache.isEmpty())
            cache = QDir::tempPath();
        QDir().mkpath(cache);

        context->job.reset(new QTemporaryDir(QDir(cache).filePath(QStringLiteral("arena-update-XXXXXX"))));
        if (!context->job->isValid())
        {
            if (!quiet)
                QMessageBox::warning(parent, tr("Update failed"),
                    tr("Cannot create update staging folder."));
            return false;
        }

        if (QFileInfo::exists(helper))
        {
            // The detached worker must not execute an updater that is going
            // to be replaced by the engine package itself.
            program = QDir(context->job->path()).filePath(QFileInfo(helper).fileName());
            if (!QFile::copy(helper, program))
            {
                if (!quiet)
                    QMessageBox::warning(parent, tr("Update failed"),
                        tr("Could not copy the updater to its staging folder."));
                return false;
            }
            QFile::setPermissions(program, QFile::permissions(helper));
        }
        else
        {
            helper = QDir(client).filePath(QStringLiteral("arena_updater.py"));
            program = QStandardPaths::findExecutable(QStringLiteral("python3"));
#ifdef Q_OS_WIN
            if (program.isEmpty())
                program = QStandardPaths::findExecutable(QStringLiteral("python"));
#endif
            if (!QFileInfo::exists(helper) || program.isEmpty())
            {
                if (!quiet)
                    QMessageBox::warning(parent, tr("Update failed"),
                        tr("Updater is missing. Install the complete client package."));
                return false;
            }

            const QString script = QDir(context->job->path()).filePath(QStringLiteral("arena_updater.py"));
            if (!QFile::copy(helper, script))
            {
                if (!quiet)
                    QMessageBox::warning(parent, tr("Update failed"),
                        tr("Could not copy the updater script to its staging folder."));
                return false;
            }
            prefix << script;
        }

        QString launcher = QCoreApplication::applicationFilePath();
#ifndef Q_OS_WIN
        // Linux portable builds use a wrapper to set the bundled library paths.
        const QString wrapper = QDir(client).filePath(QStringLiteral("openmw-launcher"));
        if (QFileInfo::exists(wrapper))
            launcher = wrapper;
#endif

        const QString requestPath = QDir(context->job->path()).filePath(QStringLiteral("request.json"));
        const QJsonObject request {
            {QStringLiteral("manifest"), QFileInfo(manifestPath).absoluteFilePath()},
            {QStringLiteral("data"), QFileInfo(dataPath).canonicalFilePath()},
            {QStringLiteral("client"), client},
            {QStringLiteral("launcher"), launcher},
            {QStringLiteral("launcher_args"), QJsonArray()},
            {QStringLiteral("engine_key"), key},
            {QStringLiteral("log"), context->logFile},
            {QStringLiteral("parent_pid"), double(QCoreApplication::applicationPid())}
        };
        if (!saveJson(requestPath, request))
        {
            if (!quiet)
                QMessageBox::warning(parent, tr("Update failed"),
                    tr("Could not create the updater request file."));
            return false;
        }

        context->program = program;
        context->prefix = prefix;
        context->requestPath = requestPath;
        log(context->logFile, QStringLiteral("worker_ready"), program + QStringLiteral(" ") + requestPath);
        return true;
    }
}

void showResult(QWidget* parent, const QString& manifestPath)
{
    if (manifestPath.isEmpty())
        return;

    QFile file(QFileInfo(manifestPath).absoluteDir().filePath(
        QStringLiteral(".arena-update-result.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject result = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    file.remove();
    if (!result.value(QStringLiteral("ok")).toBool())
        QMessageBox::warning(parent, tr("Update failed"),
            result.value(QStringLiteral("message")).toString());
}

CheckResult checkAvailable(QWidget* parent, const QString& manifestPath, const QString& dataPath)
{
    WorkerContext context;
    if (!createWorker(parent, manifestPath, dataPath, true, &context))
    {
        log(context.logFile, QStringLiteral("check_failed"), QStringLiteral("Cannot prepare updater helper; Play remains available"));
        return CheckResult::NoUpdate;
    }

    QProgressDialog progress(tr("Checking for updates..."), tr("Cancel"), 0, 0, parent);
    progress.setWindowTitle(tr("ArenaMP update"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);

    QProcess worker;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool canceled = false;
    bool available = false;
    QByteArray lines;

    QObject::connect(&progress, &QProgressDialog::canceled, &worker, [&]() {
        canceled = true;
        log(context.logFile, QStringLiteral("check_cancelled"), QStringLiteral("User cancelled the check"));
        worker.kill();
    });
    QObject::connect(&timeout, &QTimer::timeout, &worker, [&]() {
        log(context.logFile, QStringLiteral("check_timeout"), QStringLiteral("Updater did not finish within 12 seconds"));
        worker.kill();
    });
    QObject::connect(&worker, &QProcess::readyReadStandardError, &progress, [&]() {
        log(context.logFile, QStringLiteral("worker_stderr"), QString::fromUtf8(worker.readAllStandardError()));
    });
    QObject::connect(&worker, &QProcess::readyReadStandardOutput, &progress, [&]() {
        lines += worker.readAllStandardOutput();
        int newline = -1;
        while ((newline = lines.indexOf('\n')) >= 0)
        {
            const QJsonObject event = QJsonDocument::fromJson(lines.left(newline)).object();
            lines.remove(0, newline + 1);
            const QString phase = event.value(QStringLiteral("phase")).toString();
            if (phase == QLatin1String("available"))
                available = true;
            else if (phase == QLatin1String("offline"))
                qWarning() << "Arena updater check is unavailable:"
                           << event.value(QStringLiteral("message")).toString();
        }
    });
    QObject::connect(&worker,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        &loop, &QEventLoop::quit);

    worker.start(context.program,
        context.prefix + QStringList {QStringLiteral("check"), context.requestPath});
    if (!worker.waitForStarted(3000))
    {
        log(context.logFile, QStringLiteral("check_start_failed"), worker.errorString());
        progress.close();
        return CheckResult::NoUpdate;
    }

    timeout.start(12000);
    if (worker.state() != QProcess::NotRunning)
        loop.exec();
    timeout.stop();
    progress.close();

    if (canceled)
        return CheckResult::NoUpdate;
    if (worker.exitStatus() == QProcess::NormalExit && worker.exitCode() == 10)
        available = true;
    log(context.logFile, QStringLiteral("check_finished"),
        QStringLiteral("exit=%1 status=%2 button=%3").arg(worker.exitCode()).arg(int(worker.exitStatus()))
            .arg(available ? QStringLiteral("Update") : QStringLiteral("Play")));
    return available ? CheckResult::UpdateAvailable : CheckResult::NoUpdate;
}

bool startUpdate(QWidget* parent, const QString& manifestPath, const QString& dataPath)
{
    WorkerContext context;
    if (!createWorker(parent, manifestPath, dataPath, false, &context))
    {
        log(context.logFile, QStringLiteral("update_start_failed"), QStringLiteral("Cannot prepare updater helper"));
        return false;
    }

    // The supervisor performs check/prepare/apply after this GUI exits. Keep
    // the job directory because it contains the copied worker and request.
    context.job->setAutoRemove(false);
    qint64 detachedPid = 0;
    if (!QProcess::startDetached(context.program,
        context.prefix + QStringList {QStringLiteral("update"), context.requestPath},
        context.job->path(), &detachedPid))
    {
        log(context.logFile, QStringLiteral("update_start_failed"), QStringLiteral("startDetached failed"));
        context.job->setAutoRemove(true);
        QMessageBox::warning(parent, tr("Update failed"),
            tr("Could not start the update installer."));
        return false;
    }
    qDebug() << "Arena updater supervisor started" << detachedPid;
    log(context.logFile, QStringLiteral("update_started"),
        QStringLiteral("pid=%1; launcher is closing").arg(detachedPid));
    return true;
}

}}
