#include "voicepanel.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QVBoxLayout>

#include <components/config/launchersettings.hpp>

using namespace Launcher;

VoicePanel::VoicePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("voicePanel"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);

    mEnabled = new QCheckBox(tr("Voice chat"), this);
    mStateLabel = new QLabel(tr("Voice settings apply when the game starts"), this);
    mStateLabel->setWordWrap(true);
    mStateLabel->setProperty("arenaMuted", true);
    mPushToTalkKey = new QKeySequenceEdit(this);

    QFormLayout* form = new QFormLayout();
    form->addRow(tr("Key"), mPushToTalkKey);

    layout->addWidget(mEnabled);
    layout->addWidget(mStateLabel);
    layout->addLayout(form);

    connect(mEnabled, &QCheckBox::toggled, this, [this](bool on)
    {
        updateEnabledState();
        emit enabledChanged(on);
    });

    updateEnabledState();
}

void VoicePanel::loadSettings(Config::LauncherSettings& settings)
{
    mEnabled->setChecked(settings.value(QStringLiteral("Voice/enabled"),
        QStringLiteral("false")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
    mPushToTalkKey->setKeySequence(QKeySequence(
        settings.value(QStringLiteral("Voice/pushToTalkKey"), QStringLiteral("V"))));
    updateEnabledState();
}

void VoicePanel::saveSettings(Config::LauncherSettings& settings)
{
    settings.setValue(QStringLiteral("Voice/enabled"),
        mEnabled->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    settings.setValue(QStringLiteral("Voice/pushToTalkKey"), pushToTalkKey());
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
    mGameRunning = running;
    setStateText(running ? tr("In-game voice uses the server settings") : tr("Voice settings apply when the game starts"));
    updateEnabledState();
}

void VoicePanel::setStateText(const QString& text)
{
    mStateLabel->setText(text);
}

bool VoicePanel::voiceEnabled() const
{
    return mEnabled != nullptr && mEnabled->isChecked();
}

QString VoicePanel::pushToTalkKey() const
{
    if (mPushToTalkKey == nullptr)
        return QStringLiteral("V");
    const QString value = mPushToTalkKey->keySequence().toString().trimmed();
    return value.isEmpty() ? QStringLiteral("V") : value;
}

void VoicePanel::updateEnabledState()
{
    mEnabled->setEnabled(mAvailable);
    mPushToTalkKey->setEnabled(mAvailable && mEnabled->isChecked() && !mGameRunning);
}
