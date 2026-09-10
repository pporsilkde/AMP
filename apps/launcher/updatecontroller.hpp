#ifndef ARENA_UPDATECONTROLLER_HPP
#define ARENA_UPDATECONTROLLER_HPP
#include <QString>
class QWidget;
namespace Launcher
{
    namespace UpdateController
    {
        enum class Result { Continue, Restarting, Stop };
        Result beforeLaunch(QWidget* parent, const QString& manifestPath, const QString& dataPath);
        void showResult(QWidget* parent, const QString& manifestPath);
    }
}
#endif
