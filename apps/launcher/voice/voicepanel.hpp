#ifndef ARENAMP_VOICEPANEL_HPP
#define ARENAMP_VOICEPANEL_HPP

// ArenaMP U025 — управление голосом в лаунчере.
//
// Живёт на странице «Чат», под списком каналов, и остаётся доступным
// после запуска игры: лаунчер не закрывается и работает коммутатором,
// поэтому громкость, устройство и mute меняются на ходу, не выходя из мира.

#include <QWidget>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QSlider;

namespace Config { class LauncherSettings; }

namespace Launcher
{
    class VoiceCommutator;

    class VoicePanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit VoicePanel(QWidget* parent = nullptr);

        void loadSettings(Config::LauncherSettings& settings);
        void saveSettings(Config::LauncherSettings& settings);

        /// Сервер сообщил, что голос включён (voicePort != 0).
        void setVoiceAvailable(bool available);
        void setGameRunning(bool running);
        /// Строка состояния: лобби / подключение / в игре.
        void setStateText(const QString& text);
        /// Кто сейчас говорит — приходит от коммутатора.
        void setSpeakers(const QStringList& names);

        void attachCommutator(VoiceCommutator* commutator);

    signals:
        void enabledChanged(bool enabled);
        void muteChanged(bool muted);
        void pushToTalkChanged(bool pushToTalk);
        void inputDeviceChanged(const QString& device);
        void outputDeviceChanged(const QString& device);
        void micGainChanged(int percent);
        void volumeChanged(int percent);

    private slots:
        void slotToggleMute();

    private:
        void refreshDevices();
        void updateEnabledState();

        QCheckBox* mEnabled = nullptr;
        QComboBox* mMode = nullptr;           // рация / по голосу
        QComboBox* mInputDevice = nullptr;
        QComboBox* mOutputDevice = nullptr;
        QKeySequenceEdit* mPushToTalkKey = nullptr;
        QSlider* mMicGain = nullptr;
        QSlider* mVolume = nullptr;
        QPushButton* mMuteButton = nullptr;
        QLabel* mStateLabel = nullptr;
        QLabel* mSpeakersLabel = nullptr;

        VoiceCommutator* mCommutator = nullptr;
        bool mMuted = false;
        bool mAvailable = false;
    };
}
#endif
