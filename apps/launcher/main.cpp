#include <iostream>

#include <QTranslator>
#include <QTextCodec>
#include <QDir>
#include <QFile>
#include <QStyleFactory>

#include <components/misc/arenarussiantranslator.hpp>

#ifdef MAC_OS_X_VERSION_MIN_REQUIRED
#undef MAC_OS_X_VERSION_MIN_REQUIRED
// We need to do this because of Qt: https://bugreports.qt-project.org/browse/QTBUG-22154
#define MAC_OS_X_VERSION_MIN_REQUIRED __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif // MAC_OS_X_VERSION_MIN_REQUIRED

#include "maindialog.hpp"

int main(int argc, char *argv[])
{
    try
    {
        QApplication app(argc, argv);

        // ArenaMP X058: make the project-supplied Morrowind texture theme the
        // launcher-wide base style instead of letting each platform/native
        // widget style win independently. Fusion gives the stylesheet one
        // deterministic widget model on Windows/Linux, while the QSS and its
        // texture fragments are embedded in launcher.qrc.
        if (QStyle* arenaStyle = QStyleFactory::create(QStringLiteral("Fusion")))
            app.setStyle(arenaStyle);

        QFile arenaTheme(QStringLiteral(":/theme/arenamp-launcher.qss"));
        if (arenaTheme.open(QIODevice::ReadOnly | QIODevice::Text))
            app.setStyleSheet(QString::fromUtf8(arenaTheme.readAll()));

        // ArenaMW UI language follows the Windows system language. Russian
        // Windows gets the built-in Russian UI; every other locale uses English.
        ArenaUi::RussianTranslator arenaTranslator;
        if (ArenaUi::useRussianSystemUi())
            app.installTranslator(&arenaTranslator);

        // Now we make sure the current dir is set to application path
        QDir dir(QCoreApplication::applicationDirPath());

        QDir::setCurrent(dir.absolutePath());

        Launcher::MainDialog mainWin;

        Launcher::FirstRunDialogResult result = mainWin.showFirstRunDialog();
        if (result == Launcher::FirstRunDialogResultFailure)
            return 0;

        if (result == Launcher::FirstRunDialogResultContinue)
            mainWin.show();

        int exitCode = app.exec();

        return exitCode;
    }
    catch (std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 0;
    }
}
