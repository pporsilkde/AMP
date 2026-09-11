// Include the actual updater window and transaction helpers in the smoke test.
#define main arenaUpdaterMain
#include "../../../apps/launcher/updater/arena_updater.cpp"
#undef main
#include "../arenaglassicons.hpp"
#include <QMainWindow>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QWizard>
#include <QWizardPage>
#include <QToolButton>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    ArenaUi::applyMorrowindGlassPalette(app);
    if (selfTest() != 0) return 1;
    ProgressWindow updater;
    ArenaUi::installGlassWindow(updater);
    ArenaUi::installGlassWindow(updater); // must not reserve a second title bar
    if (updater.contentsMargins().top() != 44) return 2;
    updater.present();
    updater.itemProgress(63, 100, QStringLiteral("63 / 100 MiB — update.zip"));
    auto* close = updater.findChild<QToolButton*>(QStringLiteral("arenaClose"));
    if (!close || !updater.isVisible()) return 3;
    close->click();
    if (!gCancelRequested || !updater.isVisible()) return 4;
    gCancelRequested = false;
    // The window must remain visible while cancellation is locked for apply.
    updater.setCancelable(false);
    updater.close();
    if (gCancelRequested || !updater.isVisible()) return 5;
    updater.itemProgress(64, 100, QStringLiteral("64 / 100 MiB — update.zip"));
    if (!updater.isVisible()) return 6;
    updater.grab().save(QStringLiteral("updater-preview.png"));
    updater.hide();

    QMainWindow launcher;
    launcher.setWindowTitle(QStringLiteral("ArenaMP • Launcher"));
    launcher.setFixedSize(960, 660);
    auto* central = new QWidget(&launcher);
    central->setObjectName(QStringLiteral("centralwidget"));
    launcher.setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setSpacing(8);
    auto* tabs = new QListWidget(central);
    tabs->setObjectName(QStringLiteral("iconWidget"));
    tabs->setViewMode(QListView::IconMode);
    tabs->setFlow(QListView::LeftToRight);
    tabs->setIconSize(QSize(25, 25));
    tabs->setWrapping(false);
    tabs->setGridSize(QSize(172, 52));
    tabs->setWordWrap(false);
    tabs->setTextElideMode(Qt::ElideNone);
    tabs->setFixedHeight(64);
    const QStringList names{"play", "browse", "graphics", "settings", "advanced"};
    const QStringList labels{"Play", "Data Files", "Graphics", "Settings", "Advanced"};
    for (int i = 0; i < names.size(); ++i)
    {
        const auto icon = ArenaUi::glassIcon(names[i]);
        if (icon.pixmap(32, 32).isNull()) return 7;
        (new QListWidgetItem(icon, labels[i], tabs))->setSizeHint(QSize(170, 50));
    }
    tabs->setCurrentRow(0);
    layout->addWidget(tabs);
    auto* group = new QGroupBox(QStringLiteral("Connection"), central);
    auto* fields = new QVBoxLayout(group);
    fields->addWidget(new QLabel(QStringLiteral("ArenaMP — Morrowind multiplayer"), group));
    fields->addWidget(new QLineEdit(QStringLiteral("play.example.org"), group));
    fields->addWidget(new QLineEdit(QStringLiteral("25565"), group));
    layout->addWidget(group);
    layout->addStretch();
    auto* bar = new QProgressBar(central);
    bar->setValue(63);
    layout->addWidget(bar);
    auto* buttons = new QHBoxLayout;
    for (const QString& name : {QStringLiteral("changelog"), QStringLiteral("server"), QStringLiteral("play")})
    {
        auto* button = new QPushButton(ArenaUi::glassIcon(name), name, central);
        if (name == QLatin1String("play")) button->setProperty("arenaPrimary", true);
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    ArenaUi::installGlassWindow(launcher);
    launcher.show();
    app.processEvents();
    if (!launcher.findChild<QToolButton*>(QStringLiteral("arenaMaximize"))->isHidden()) return 8;
    if (central->geometry().top() < 44) return 9;
    launcher.grab().save(QStringLiteral("launcher-component-preview.png"));
    launcher.hide();
    QWizard wizard;
    wizard.setWindowTitle(QStringLiteral("ArenaMP — Setup"));
    auto* page = new QWizardPage;
    page->setTitle(QStringLiteral("Choose your Morrowind installation"));
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(new QLineEdit(QStringLiteral("C:/Games/Morrowind"), page));
    wizard.addPage(page);
    wizard.resize(700, 430);
    ArenaUi::installGlassWindow(wizard);
    wizard.show();
    app.processEvents();
    if (page->mapTo(&wizard, QPoint()).y() < 44) return 10;
    wizard.grab().save(QStringLiteral("wizard-component-preview.png"));
    return 0;
}
