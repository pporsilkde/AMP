// ArenaMP U025 — скелет панели управления голосом.
//
// Список устройств берём из уже существующего utils/openalutil.cpp
// (он же используется страницей настроек звука), новых зависимостей нет.

#include "voicepanel.hpp"
#include "voicecommutator.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <components/config/launchersettings.hpp>

#include "../utils/openalutil.hpp"

using namespace Launcher;

VoicePanel::VoicePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("voicePanel"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);

    mEnabled = new QCheckBox(tr("Voice chat"), this);
    mStateLabel = new QLabel(tr("Disabled"), this);
    mStateLabel->setProperty("arenaMuted", true);
    mSpeakersLabel = new QLabel(this);
    mSpeakersLabel->setWordWrap(true);
    mSpeakersLabel->setProperty("arenaMuted", true);

    mMode = new QComboBox(this);
    mMode->addItem(tr("Push to talk"), QStringLiteral("ptt"));
    mMode->addItem(tr("Voice activity"), QStringLiteral("vad"));

    mPushToTalkKey = new QKeySequenceEdit(this);
    mInputDevice = new QComboBox(this);
    mOutputDevice = new QComboBox(this);

    mMicGain = new QSlider(Qt::Horizontal, this);
    mMicGain->setRange(0, 200);
    mVolume = new QSlider(Qt::Horizontal, this);
    mVolume->setRange(0, 100);

    mMuteButton = new QPushButton(tr("Mute microphone"), this);
    mMuteButton->setCheckable(true);

    QFormLayout* form = new QFormLayout();
    form->addRow(tr("Mode"), mMode);
    form->addRow(tr("Key"), mPushToTalkKey);
    form->addRow(tr("Microphone"), mInputDevice);
    form->addRow(tr("Output"), mOutputDevice);
    form->addRow(tr("Sensitivity"), mMicGain);
    form->addRow(tr("Volume"), mVolume);

    layout->addWidget(mEnabled);
    layout->addWidget(mStateLabel);
    layout->addLayout(form);
    layout->addWidget(mMuteButton);
    layout->addWidget(mSpeakersLabel);

    refreshDevices();

    connect(mEnabled, &QCheckBox::toggled, this, [this](bool on)
    {
        updateEnabledState();
        emit enabledChanged(on);
    });
    connect(mMuteButton, &QPushButton::clicked, this, &VoicePanel::slotToggleMute);
    connect(mMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        const bool pushToTalk = index == 0;
        mPushToTalkKey->setEnabled(pushToTalk);
        emit pushToTalkChanged(pushToTalk);
    });
    connect(mMicGain, &QSlider::valueChanged, this, &VoicePanel::micGainChanged);
    connect(mVolume, &QSlider::valueChanged, this, &VoicePanel::volumeChanged);
    connect(mInputDevice, &QComboBox::currentTextChanged, this, &VoicePanel::inputDeviceChanged);
    connect(mOutputDevice, &QComboBox::currentTextChanged, this, &VoicePanel::outputDeviceChanged);

    updateEnabledState();
}

void VoicePanel::refreshDevices()
{
    mInputDevice->clear();
    mOutputDevice->clear();
    mInputDevice->addItem(tr("Default"), QString());
    mOutputDevice->addItem(tr("Default"), QString());

    // TODO: enumerateOpenALDevices() / enumerateOpenALDevicesCapture()
    // из utils/openalutil.hpp — те же списки, что на странице звука.
    // Если устройств захвата нет вообще, показать «Микрофон не найден»
    // и оставить голос доступным только на приём.
}

void VoicePanel::loadSettings(Config::LauncherSettings& settings)
{
    mEnabled->setChecked(settings.value(QStringLiteral("Voice/enabled"),
        QStringLiteral("false")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
    const QString mode = settings.value(QStringLiteral("Voice/mode"), QStringLiteral("ptt"));
    mMode->setCurrentIndex(mode == QLatin1String("vad") ? 1 : 0);
    mPushToTalkKey->setKeySequence(QKeySequence(
        settings.value(QStringLiteral("Voice/pushToTalkKey"), QStringLiteral("V"))));
    mMicGain->setValue(settings.value(QStringLiteral("Voice/micGain"), QStringLiteral("100")).toInt());
    mVolume->setValue(settings.value(QStringLiteral("Voice/volume"), QStringLiteral("80")).toInt());
    updateEnabledState();
}

void VoicePanel::saveSettings(Config::LauncherSettings& settings)
{
    settings.setValue(QStringLiteral("Voice/enabled"),
        mEnabled->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    settings.setValue(QStringLiteral("Voice/mode"), mMode->currentData().toString());
    settings.setValue(QStringLiteral("Voice/pushToTalkKey"),
        mPushToTalkKey->keySequence().toString());
    settings.setValue(QStringLiteral("Voice/micGain"), QString::number(mMicGain->value()));
    settings.setValue(QStringLiteral("Voice/volume"), QString::number(mVolume->value()));
    settings.setValue(QStringLiteral("Voice/inputDevice"), mInputDevice->currentData().toString());
    settings.setValue(QStringLiteral("Voice/outputDevice"), mOutputDevice->currentData().toString());
}

void VoicePanel::setVoiceAvailable(bool available)
{
    mAvailable = available;
    if (!available)
        setStateText(tr("Voice is disabled on this server"));
    updateEnabledState();
}

void VoicePanel::setGameRunning(bool running)
{
    // Клавиша рации в игре обрабатывается движком и передаётся по мосту,
    // поэтому редактировать её на ходу бессмысленно.
    mPushToTalkKey->setEnabled(!running && mMode->currentIndex() == 0);
    setStateText(running ? tr("In game: nearby players are audible") : tr("Lobby"));
}

void VoicePanel::setStateText(const QString& text)
{
    mStateLabel->setText(text);
}

void VoicePanel::setSpeakers(const QStringList& names)
{
    mSpeakersLabel->setText(names.isEmpty()
        ? QString() : tr("Speaking: %1").arg(names.join(QStringLiteral(", "))));
}

void VoicePanel::attachCommutator(VoiceCommutator* commutator)
{
    mCommutator = commutator;
    // TODO: связать сигналы панели с VoiceCommutator::applySettings и
    // stateChanged/speakersChanged обратно в setStateText/setSpeakers.
}

void VoicePanel::slotToggleMute()
{
    mMuted = mMuteButton->isChecked();
    mMuteButton->setText(mMuted ? tr("Unmute microphone") : tr("Mute microphone"));
    // Локальный mute — только прекращение передачи: слышать других игрок
    // продолжает, иначе непонятно, почему «оглох».
    emit muteChanged(mMuted);
}

void VoicePanel::updateEnabledState()
{
    const bool on = mEnabled->isChecked() && mAvailable;
    for (QWidget* widget : { static_cast<QWidget*>(mMode), static_cast<QWidget*>(mInputDevice),
        static_cast<QWidget*>(mOutputDevice), static_cast<QWidget*>(mMicGain),
        static_cast<QWidget*>(mVolume), static_cast<QWidget*>(mMuteButton) })
    {
        widget->setEnabled(on);
    }
    mPushToTalkKey->setEnabled(on && mMode->currentIndex() == 0);
    mEnabled->setEnabled(mAvailable);
}
