#ifndef ARENAMP_VOICEPANEL_HPP
#define ARENAMP_VOICEPANEL_HPP

// ArenaMP U026 — launcher controls for the existing native in-game VoiceChat.
// The launcher intentionally does not open its own microphone/audio pipeline.

#include <QWidget>
#include <QString>

class QCheckBox;
class QKeySequenceEdit;
class QLabel;

namespace Config { class LauncherSettings; }

namespace Launcher
{
    class VoicePanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit VoicePanel(QWidget* parent = nullptr);

        void loadSettings(Config::LauncherSettings& settings);
        void saveSettings(Config::LauncherSettings& settings);
        void setVoiceAvailable(bool available);
        void setGameRunning(bool running);
        void setStateText(const QString& text);

        bool voiceEnabled() const;
        QString pushToTalkKey() const;

    signals:
        void enabledChanged(bool enabled);

    private:
        void updateEnabledState();

        QCheckBox* mEnabled = nullptr;
        QKeySequenceEdit* mPushToTalkKey = nullptr;
        QLabel* mStateLabel = nullptr;
        bool mAvailable = true;
        bool mGameRunning = false;
    };
}
#endif
