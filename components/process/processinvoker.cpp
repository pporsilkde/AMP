#include "processinvoker.hpp"

#include <QMessageBox>
#include <QStringList>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>

Process::ProcessInvoker::ProcessInvoker()
{
    mProcess = new QProcess(this);

    connect(mProcess, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(processError(QProcess::ProcessError)));

    connect(mProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(processFinished(int,QProcess::ExitStatus)));


    mName = QString();
    mArguments = QStringList();
}

Process::ProcessInvoker::~ProcessInvoker()
{
}

//void Process::ProcessInvoker::setProcessName(const QString &name)
//{
//    mName = name;
//}

//void Process::ProcessInvoker::setProcessArguments(const QStringList &arguments)
//{
//    mArguments = arguments;
//}

QProcess* Process::ProcessInvoker::getProcess()
{
    return mProcess;
}

//QString Process::ProcessInvoker::getProcessName()
//{
//    return mName;
//}

//QStringList Process::ProcessInvoker::getProcessArguments()
//{
//    return mArguments;
//}

bool Process::ProcessInvoker::startProcess(const QString &name, const QStringList &arguments, bool detached)
{
    //    mProcess = new QProcess(this);
    mName = name;
    mArguments = arguments;

    // Never depend on the process current directory. The portable Linux
    // package is normally started through a wrapper, Steam/desktop launchers
    // may choose another cwd, and an update/restart can change it as well.
    // Resolve executables beside openmw-launcher instead.
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    QStringList candidates;
#ifdef Q_OS_WIN
    candidates << name + QLatin1String(".exe");
    // Some older ArenaMP packages used the explicit client suffix.  Keep it
    // as a compatibility fallback so an updater cannot leave Play apparently
    // inert merely because the executable was renamed.
    candidates << name + QLatin1String("-client.exe");
#else
    candidates << name;
#ifndef Q_OS_MAC
    // A raw x86_64 binary is accepted when the portable wrapper was not
    // generated (for example when running directly from a build tree).
    candidates << name + QLatin1String(".x86_64");
#endif
    candidates << name + QLatin1String("-client");
#ifndef Q_OS_MAC
    candidates << name + QLatin1String("-client.x86_64");
#endif
#endif

    QString path;
    // Release packages keep the client beside the launcher.  The current and
    // parent directories are inexpensive compatibility fallbacks for older
    // layouts where the launcher lived in a bin/ subdirectory.
    QStringList searchDirectories;
    searchDirectories << applicationDir.absolutePath();
    const QString currentDirectory = QDir::currentPath();
    if (!currentDirectory.isEmpty() && !searchDirectories.contains(currentDirectory, Qt::CaseInsensitive))
        searchDirectories << currentDirectory;
    const QString parentDirectory = applicationDir.absoluteFilePath(QStringLiteral(".."));
    if (!parentDirectory.isEmpty() && !searchDirectories.contains(parentDirectory, Qt::CaseInsensitive))
        searchDirectories << parentDirectory;

    for (const QString &directory : searchDirectories)
    {
        for (const QString &candidate : candidates)
        {
            const QString absolute = QDir(directory).absoluteFilePath(candidate);
            if (QFileInfo::exists(absolute))
            {
                path = absolute;
                break;
            }
        }
        if (!path.isEmpty()) break;
    }
    if (path.isEmpty() && !candidates.isEmpty())
        path = applicationDir.absoluteFilePath(candidates.first());

    QFileInfo info(path);
    const QString workingDirectory = info.absoluteDir().absolutePath();

    if (!info.exists()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Error starting executable"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setText(tr("<html><head/><body><p><b>Could not find %1</b></p> \
                          <p>The application is not found.</p> \
                          <p>Please make sure OpenMW is installed correctly and try again.</p></body></html>").arg(info.fileName()));
        msgBox.exec();
        return false;
    }

#ifndef Q_OS_WIN
    if (!info.isExecutable()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Error starting executable"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setText(tr("<html><head/><body><p><b>Could not start %1</b></p> \
                          <p>The application is not executable.</p> \
                          <p>Please make sure you have the right permissions and try again.</p></body></html>").arg(info.fileName()));
        msgBox.exec();
        return false;
    }
#endif

    qDebug() << "Starting ArenaMP client" << info.absoluteFilePath() << arguments;

    // Start the executable
    if (detached) {
        qint64 detachedPid = 0;
        if (!QProcess::startDetached(path, arguments, workingDirectory, &detachedPid)) {
            // A few Windows runners reject the detached overload for a
            // wrapper/compatibility executable even though a normal QProcess
            // can start it.  Retry once in the attached object and verify the
            // process reached the Started state before reporting failure.
            mProcess->setWorkingDirectory(workingDirectory);
            mProcess->start(path, arguments);
            if (mProcess->waitForStarted(3000))
                return true;
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Error starting executable"));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setText(tr("<html><head/><body><p><b>Could not start %1</b></p> \
                              <p>An error occurred while starting %1.</p> \
                              <p>Press \"Show Details...\" for more information.</p></body></html>").arg(info.fileName()));
            msgBox.setDetailedText(mProcess->errorString());
            msgBox.exec();
            return false;
        }
    } else {
        mProcess->setWorkingDirectory(workingDirectory);
        mProcess->start(path, arguments);

        /*
        if (!mProcess->waitForFinished()) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Error starting executable"));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setText(tr("<html><head/><body><p><b>Could not start %1</b></p> \
                              <p>An error occurred while starting %1.</p> \
                              <p>Press \"Show Details...\" for more information.</p></body></html>").arg(info.fileName()));
            msgBox.setDetailedText(mProcess->errorString());
            msgBox.exec();

            return false;
        }

        if (mProcess->exitCode() != 0 || mProcess->exitStatus() == QProcess::CrashExit) {
            QString error(mProcess->readAllStandardError());
            error.append(tr("\nArguments:\n"));
            error.append(arguments.join(" "));

            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Error running executable"));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setText(tr("<html><head/><body><p><b>Executable %1 returned an error</b></p> \
                              <p>An error occurred while running %1.</p> \
                              <p>Press \"Show Details...\" for more information.</p></body></html>").arg(info.fileName()));
            msgBox.setDetailedText(error);
            msgBox.exec();

            return false;
        }
        */
    }

    return true;

}

void Process::ProcessInvoker::processError(QProcess::ProcessError error)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("Error running executable"));
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setText(tr("<html><head/><body><p><b>Executable %1 returned an error</b></p> \
                      <p>An error occurred while running %1.</p> \
                      <p>Press \"Show Details...\" for more information.</p></body></html>").arg(mName));
    msgBox.setDetailedText(mProcess->errorString());
    msgBox.exec();

}

void Process::ProcessInvoker::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0 || exitStatus == QProcess::CrashExit) {
        QString error(mProcess->readAllStandardError());
        error.append(tr("\nArguments:\n"));
        error.append(mArguments.join(" "));

        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Error running executable"));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setText(tr("<html><head/><body><p><b>Executable %1 returned an error</b></p> \
                          <p>An error occurred while running %1.</p> \
                          <p>Press \"Show Details...\" for more information.</p></body></html>").arg(mName));
        msgBox.setDetailedText(error);
        msgBox.exec();
    }
}
