#ifndef OPENMW_COMPONENTS_MISC_ARENATHEME_H
#define OPENMW_COMPONENTS_MISC_ARENATHEME_H

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>
#include <QString>

namespace ArenaUi
{
    // Compiled-in visual language shared by Launcher, Wizard, Updater and Qt
    // utilities. The controls deliberately stay native Qt widgets so the UI
    // remains cheap to draw on integrated GPUs and Android-derived systems.
    inline void applyMorrowindGlassPalette(QApplication& app)
    {
        if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            app.setStyle(fusion);

        const QColor obsidian(25, 26, 29);
        const QColor surface(31, 32, 35);
        const QColor surfaceRaised(43, 44, 48);
        const QColor brass(194, 157, 91);
        const QColor gold(226, 193, 126);
        const QColor text(235, 230, 220);
        const QColor disabled(105, 102, 96);

        QPalette palette;
        palette.setColor(QPalette::Window, obsidian);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, surface);
        palette.setColor(QPalette::AlternateBase, surfaceRaised);
        palette.setColor(QPalette::ToolTipBase, surfaceRaised);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, surfaceRaised);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, QColor(255, 241, 211));
        palette.setColor(QPalette::Link, gold);
        palette.setColor(QPalette::Highlight, brass);
        palette.setColor(QPalette::HighlightedText, QColor(28, 24, 18));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        app.setPalette(palette);

        // MSVC has a relatively small limit for a single string literal.
        // Keep the shared QSS in several chunks so the launcher, wizard and
        // updater can all include this header without C2026.
        QString arenaStyleSheet;
        arenaStyleSheet.reserve(25201);
        arenaStyleSheet += QStringLiteral(R"ARENA(
            QMainWindow, QDialog, QWizard, QWidget#centralWidget, QWidget#centralwidget {
                background-color: #191a1d;
                color: #ebe6dc;
            }
            QWidget { selection-background-color: #c19b5b; selection-color: #1d1812; }
            QLabel { color: #ebe6dc; background: transparent; }
            QLabel:disabled { color: #706c64; }

            QStackedWidget#pagesWidget {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 rgba(36,37,40,205), stop:1 rgba(19,20,23,215));
                border: 1px solid rgba(233, 216, 182, 34);
                border-radius: 15px;
            }
            QFrame[arenaCard="true"] {
                background-color: rgba(255, 255, 255, 12);
                border: 1px solid rgba(255, 255, 255, 24);
                border-radius: 12px;
            }
            QFrame[arenaFooter="true"] {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(255,255,255,10), stop:1 rgba(255,255,255,6));
                border: 1px solid rgba(255,255,255,20);
                border-radius: 12px;
            }
            QLabel#versionLabel {
                padding: 5px 9px;
                border-radius: 8px;
                font-size: 11px;
                font-weight: 600;
            }
            QLabel#versionLabel[arenaStatus="online"] {
                color: #62d987;
                background-color: rgba(56,180,92,18);
                border: 1px solid rgba(83,202,113,36);
            }
            QLabel#versionLabel[arenaStatus="offline"] {
                color: #aaa69e;
                background-color: rgba(255,255,255,6);
                border: 1px solid rgba(255,255,255,15);
            }

            QGroupBox {
                color: #d7bd89;
                background-color: rgba(255, 255, 255, 7);
                border: 1px solid rgba(230, 206, 158, 33);
                border-radius: 12px;
                margin-top: 12px;
                padding-top: 8px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }

            QPushButton {
                color: #eee8dd;
                background-color: rgba(255, 255, 255, 18);
                border: 1px solid rgba(226, 193, 126, 72);
                border-radius: 9px;
                padding: 5px 11px;
                min-height: 20px;
            }
            QPushButton:hover {
                background-color: rgba(226, 193, 126, 28);
                border-color: rgba(226, 193, 126, 145);
            }
            QPushButton:pressed {
                background-color: rgba(173, 132, 74, 70);
                border-color: #9f7e4b;
            }
            QPushButton:default { border-color: #d6ae68; }
            QPushButton:disabled {
                color: #716e68;
                background-color: rgba(255, 255, 255, 6);
                border-color: rgba(255, 255, 255, 14);
            }
            QPushButton[arenaPrimary="true"] {
                color: #241d13;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #e7c887, stop:1 #b98e50);
                border: 1px solid #f0d79e;
                font-weight: 600;
                padding-left: 15px;
                padding-right: 15px;
            }
            QPushButton[arenaPrimary="true"]:hover { background: #ecd093; }
            QPushButton[arenaPrimary="true"]:pressed { background: #ae8248; }
            QPushButton[arenaPrimary="true"]:disabled {
                background: #3c3933; color: #8d887e; border-color: #555048;
            }
            QPushButton[arenaQuiet="true"] {
                background-color: rgba(255, 255, 255, 8);
                border-color: rgba(255, 255, 255, 22);
            }
            QPushButton[arenaQuiet="true"]:hover {
                background-color: rgba(255, 255, 255, 16);
                border-color: rgba(226, 193, 126, 82);
            }
            QDialogButtonBox QPushButton { min-width: 82px; }

            QToolButton {
                color: #ece5d8;
                background-color: rgba(255, 255, 255, 12);
                border: 1px solid rgba(226, 193, 126, 55);
                border-radius: 8px;
                padding: 4px 6px;
            }
            QToolButton:hover {
                background-color: rgba(226, 193, 126, 24);
                border-color: rgba(226, 193, 126, 125);
            }
            QToolButton:pressed { background-color: rgba(151, 112, 61, 65); }
            QToolButton:disabled { color: #716e68; border-color: rgba(255,255,255,12); }

            QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox,
            QListView, QTreeView, QTableView {
                color: #ebe6dc;
                background-color: rgba(18, 19, 21, 205);
                border: 1px solid rgba(231, 218, 194, 46);
                border-radius: 8px;
            }
            QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
                min-height: 20px;
                padding: 4px 8px;
            }
            QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
                border-color: rgba(226, 193, 126, 92);
            }
            QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,
            QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
            QListView:focus, QTreeView:focus, QTableView:focus {
                border-color: #c9a562;
            }
            QComboBox { padding-right: 30px; }
            QComboBox::drop-down {
                subcontrol-origin: padding;
                subcontrol-position: top right;
                width: 26px;
                border: 0;
                border-left: 1px solid rgba(255,255,255,18);
                background-color: rgba(255,255,255,6);
                border-top-right-radius: 8px;
                border-bottom-right-radius: 8px;
            }
            QComboBox::drop-down:hover { background-color: rgba(226,193,126,18); }
            QComboBox::down-arrow {
                image: url(:/arena/arenaicons/chevron-down.png);
                width: 10px;
                height: 10px;
            }
            QComboBox::down-arrow:on { top: 1px; }
            QComboBox QAbstractItemView {
                background-color: #25262a;
                border: 1px solid #5b5245;
                color: #ebe6dc;
                padding: 4px;
                outline: 0;
            }
            QAbstractItemView::item { border-radius: 5px; padding: 4px; }
            QAbstractItemView::item:hover { background-color: rgba(226, 193, 126, 30); }
            QAbstractItemView::item:selected { background-color: #b99254; color: #201a12; }

            QCheckBox, QRadioButton { color: #e1dcd2; spacing: 8px; }
            QCheckBox::indicator {
)ARENA");
        arenaStyleSheet += QStringLiteral(R"ARENA(                width: 18px; height: 18px;
                border: 1px solid #6b685f;
                border-radius: 5px;
                background-color: rgba(24, 25, 28, 235);
            }
            QCheckBox::indicator:hover {
                border-color: #c9a562;
                background-color: rgba(226, 193, 126, 18);
            }
            QCheckBox::indicator:checked {
                image: url(:/arena/arenaicons/check.png);
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #efd18c, stop:1 #bd8c4a);
                border: 1px solid #f1d79d;
            }
            QCheckBox::indicator:checked:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #f5d99a, stop:1 #ca9855);
                border-color: #ffe5af;
            }
            QCheckBox::indicator:disabled {
                background-color: #222326;
                border-color: #44443f;
            }
            QCheckBox::indicator:checked:disabled {
                image: url(:/arena/arenaicons/check.png);
                background-color: #6f624d;
                border-color: #81735b;
            }
            QRadioButton::indicator {
                width: 18px; height: 18px;
                border: 1px solid #6b685f;
                border-radius: 10px;
                background-color: rgba(24, 25, 28, 235);
            }
            QRadioButton::indicator:hover { border-color: #c9a562; }
            QRadioButton::indicator:checked {
                image: url(:/arena/arenaicons/radio-dot.png);
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #efd18c, stop:1 #bd8c4a);
                border: 1px solid #f1d79d;
            }
            QCheckBox:focus, QRadioButton:focus { color: #f0d7a4; }

            QTabWidget::pane {
                top: -1px;
                background-color: rgba(255, 255, 255, 5);
                border: 1px solid rgba(232, 215, 181, 30);
                border-radius: 11px;
            }
            QTabBar { background: transparent; }
            QTabBar::tab {
                color: #aaa69e;
                background-color: transparent;
                border: 1px solid transparent;
                border-radius: 8px;
                padding: 6px 12px;
                margin: 2px 2px 3px 0;
                min-width: 72px;
            }
            QTabBar::tab:hover {
                color: #e6dfd3;
                background-color: rgba(255,255,255,9);
            }
            QTabBar::tab:selected {
                color: #f0d9ac;
                background-color: rgba(198, 158, 91, 22);
                border-color: rgba(216, 178, 111, 70);
            }
            QTabWidget[arenaSegmented="true"] QTabBar::tab { padding: 5px 11px; }

            /* U016 horizontal controls: equal-width, no scrolling arrows, readable RU labels. */
            QTabWidget#pageTabs::pane {
                top: -1px;
                background-color: rgba(15,16,18,105);
                border: 1px solid rgba(233,216,182,25);
                border-radius: 13px;
            }
            QTabWidget#pageTabs QTabBar::tab {
                min-width: 0px;
                min-height: 22px;
                padding: 5px 8px;
                margin: 2px 2px 4px 0;
                color: #b7b2aa;
                background: transparent;
                border: 1px solid transparent;
                border-radius: 9px;
                font-size: 12px;
                font-weight: 600;
            }
            QTabWidget#pageTabs QTabBar::tab:hover {
                color: #eee7da;
                background-color: rgba(255,255,255,9);
            }
            QTabWidget#pageTabs QTabBar::tab:selected {
                color: #271f14;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #e7c887, stop:1 #ad8047);
                border-color: #efd498;
            }
            QTabWidget#playModeTabs::pane, QTabWidget#serverSettingsModeTabs::pane {
                top: -1px;
                background-color: rgba(255,255,255,5);
                border: 1px solid rgba(233,216,182,22);
                border-radius: 12px;
            }
            QTabWidget#playModeTabs QTabBar::tab,
            QTabWidget#serverSettingsModeTabs QTabBar::tab {
                min-width: 0px;
                min-height: 20px;
                padding: 4px 8px;
                margin: 2px 2px 4px 0;
                border: 1px solid transparent;
                border-radius: 8px;
                color: #b0aca4;
                font-size: 12px;
                background: transparent;
            }
            QTabWidget#playModeTabs QTabBar::tab:hover,
            QTabWidget#serverSettingsModeTabs QTabBar::tab:hover {
                color: #eee7da;
                background-color: rgba(255,255,255,9);
            }
            QTabWidget#playModeTabs QTabBar::tab:selected,
            QTabWidget#serverSettingsModeTabs QTabBar::tab:selected {
                color: #f0d7a4;
                background-color: rgba(198,158,91,27);
                border-color: rgba(226,193,126,84);
            }
            QFrame[arenaHeroCard="true"] {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(201,159,88,20), stop:0.45 rgba(255,255,255,9), stop:1 rgba(91,113,130,9));
                border: 1px solid rgba(226,193,126,37);
                border-radius: 12px;
            }
            QLineEdit[arenaBuildName="true"] {
                font-weight: 600;
                color: #f1e7d5;
                background-color: rgba(13,14,16,176);
                border-color: rgba(226,193,126,42);
            }
            QFrame[arenaSectionHeader="true"] {
                background: transparent;
                border: 0;
                padding: 1px 2px 4px 2px;
            }
            QLabel[arenaSectionTitle="true"] {
                color: #f0e9dc;
                font-size: 14px;
                font-weight: 650;
            }

            /* Advanced follows the same compact horizontal switch language. */
            QTabWidget#AdvancedTabWidget::pane {
                top: -1px;
                background-color: rgba(16,17,19,120);
                border: 1px solid rgba(233,216,182,24);
                border-radius: 12px;
            }
            QTabWidget#AdvancedTabWidget QTabBar::tab {
                min-width: 0px;
                padding: 6px 8px;
                margin: 2px 2px 4px 0;
                border-radius: 8px;
                border: 1px solid transparent;
                color: #aaa69e;
                background: transparent;
            }
            QTabWidget#AdvancedTabWidget QTabBar::tab:hover {
                color: #eee7da;
                background-color: rgba(255,255,255,9);
            }
            QTabWidget#AdvancedTabWidget QTabBar::tab:selected {
                color: #f0d7a4;
)ARENA");
        arenaStyleSheet += QStringLiteral(R"ARENA(                background-color: rgba(198,158,91,25);
                border-color: rgba(226,193,126,78);
            }

            /* U014 compact graphics page: segmented macOS-like inner navigation. */
            QTabWidget#DisplayTabWidget::pane {
                top: -1px;
                background-color: rgba(16, 17, 19, 130);
                border: 1px solid rgba(233, 216, 182, 27);
                border-radius: 13px;
            }
            QTabWidget#DisplayTabWidget QTabBar::tab {
                min-width: 92px;
                padding: 6px 15px;
                margin: 2px 3px 4px 0;
                color: #aaa69e;
                border: 1px solid transparent;
                border-radius: 9px;
                background: transparent;
            }
            QTabWidget#DisplayTabWidget QTabBar::tab:hover {
                color: #eee7da;
                background-color: rgba(255,255,255,9);
            }
            QTabWidget#DisplayTabWidget QTabBar::tab:selected {
                color: #f0d7a4;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 rgba(205, 164, 94, 46), stop:1 rgba(144, 104, 55, 28));
                border-color: rgba(226, 193, 126, 93);
            }

            QFrame[arenaCard="true"] {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 rgba(255,255,255,13), stop:1 rgba(255,255,255,7));
                border: 1px solid rgba(237, 220, 187, 34);
                border-radius: 12px;
            }
            QLabel[arenaTitle="true"] {
                color: #f0e9dc;
                font-size: 14px;
                font-weight: 600;
            }
            QLabel[arenaMuted="true"] {
                color: #b3afa8;
                font-size: 11px;
            }
            QLabel[arenaAccent="true"] {
                color: #e3bd70;
                font-weight: 600;
            }
            QLabel[arenaSettingLabel="true"] { color: #d8d2c8; }
            QFrame[arenaDivider="true"] {
                border: 0;
                background-color: rgba(255,255,255,20);
                min-height: 1px;
                max-height: 1px;
            }
            QPushButton[arenaPresetButton="true"] {
                min-height: 25px;
                padding: 3px 7px;
                border-radius: 8px;
                color: #d9d4ca;
                background-color: rgba(255,255,255,9);
                border: 1px solid rgba(255,255,255,20);
                font-size: 11px;
                font-weight: 500;
            }
            QPushButton[arenaPresetButton="true"]:hover {
                color: #f1e9da;
                background-color: rgba(226,193,126,18);
                border-color: rgba(226,193,126,75);
            }
            QPushButton[arenaPresetButton="true"]:checked {
                color: #261d12;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #efd18c, stop:1 #b98949);
                border: 1px solid #f2d79b;
                font-weight: 650;
            }
            QWidget#qualityPresetButtonsHost { background: transparent; }

            QListWidget#iconWidget {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(255,255,255,9), stop:1 rgba(255,255,255,5));
                border: 1px solid rgba(255,255,255,20);
                border-radius: 14px;
                padding: 5px;
                outline: 0;
            }
            QListWidget#iconWidget::item {
                color: #d8d2c7;
                background: transparent;
                border: 1px solid transparent;
                border-radius: 10px;
                padding: 3px 7px;
            }
            QListWidget#iconWidget::item:hover {
                color: #f2eadc;
                background-color: rgba(255,255,255,10);
                border-color: rgba(255,255,255,18);
            }
            QListWidget#iconWidget::item:selected {
                color: #f6dda8;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 rgba(205,164,94,52), stop:1 rgba(128,91,46,34));
                border-color: rgba(233,194,120,105);
            }

            QHeaderView::section {
                color: #d7c8aa;
                background-color: #28292d;
                border: 0;
                border-right: 1px solid #3d3e42;
                border-bottom: 1px solid #3d3e42;
                padding: 5px;
            }
            QProgressBar {
                min-height: 17px;
                color: #ebe6dc;
                background-color: #222327;
                border: 1px solid #4b4842;
                border-radius: 8px;
                text-align: center;
            }
            QProgressBar::chunk {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #a67e48, stop:1 #e2c27e);
                border-radius: 6px;
            }

            QMenu {
                color: #ebe6dc;
                background-color: #25262a;
                border: 1px solid #504a40;
                border-radius: 8px;
                padding: 4px;
            }
            QMenu::item { padding: 6px 20px 6px 10px; border-radius: 5px; }
            QMenu::item:selected { background-color: rgba(193,154,88,65); color: #f2e4c9; }
            QToolTip {
                color: #f0e8dc;
                background-color: #2a2928;
                border: 1px solid #765f40;
                border-radius: 6px;
                padding: 5px;
            }

            /* U015 server console and server-settings polish. */
            QWidget#serverConsolePanel { background: transparent; }
            QFrame[arenaToolbar="true"] {
                background-color: rgba(255,255,255,7);
                border: 1px solid rgba(255,255,255,19);
                border-radius: 11px;
            }
            QFrame[arenaConsoleCard="true"] {
                background-color: rgba(11,12,14,225);
                border: 1px solid rgba(226,193,126,38);
                border-radius: 12px;
            }
            QLabel[arenaConsoleAddress="true"] {
                color: #77cf92;
                font-weight: 600;
                padding: 4px 8px;
                background-color: rgba(62,167,91,17);
                border: 1px solid rgba(91,190,116,38);
                border-radius: 7px;
            }
            QLabel[arenaConsolePort="true"] {
                color: #dfc187;
                padding: 4px 8px;
                background-color: rgba(226,193,126,10);
                border: 1px solid rgba(226,193,126,30);
                border-radius: 7px;
            }
            QPlainTextEdit[arenaConsole="true"] {
                color: #cec9bf;
                background-color: #101114;
                border: 0;
                border-radius: 11px;
                padding: 8px;
)ARENA");
        arenaStyleSheet += QStringLiteral(R"ARENA(                selection-background-color: #725b36;
                selection-color: #fff4df;
            }
            QPlainTextEdit[arenaCodeEditor="true"] {
                color: #d8d2c7;
                background-color: #111216;
                border: 1px solid rgba(226,193,126,38);
                border-radius: 10px;
                padding: 8px;
                selection-background-color: #725b36;
            }
            QPushButton[arenaDanger="true"] {
                color: #ffd7d2;
                background-color: rgba(180,65,58,34);
                border-color: rgba(238,111,100,90);
                font-weight: 600;
            }
            QPushButton[arenaDanger="true"]:hover {
                background-color: rgba(210,75,66,55);
                border-color: rgba(255,130,118,145);
            }
            QPushButton[arenaDanger="true"]:disabled {
                color: #776967;
                background-color: rgba(255,255,255,5);
                border-color: rgba(255,255,255,13);
            }
            QTabWidget#serverSettingsCategoryTabs::pane {
                top: -1px;
                background-color: rgba(14,15,17,112);
                border: 1px solid rgba(233,216,182,24);
                border-radius: 11px;
            }
            QTabWidget#serverSettingsCategoryTabs QTabBar::tab {
                min-width: 0px;
                padding: 5px 11px;
                margin: 2px 2px 4px 0;
                color: #aaa69e;
                border: 1px solid transparent;
                border-radius: 8px;
                background: transparent;
            }
            QTabWidget#serverSettingsCategoryTabs QTabBar::tab:hover {
                color: #ebe4d8;
                background-color: rgba(255,255,255,9);
            }
            QTabWidget#serverSettingsCategoryTabs QTabBar::tab:selected {
                color: #f0d7a4;
                background-color: rgba(198,158,91,24);
                border-color: rgba(216,178,111,72);
            }
            QGroupBox[arenaSettingsGroup="true"] {
                background-color: rgba(255,255,255,6);
                border-color: rgba(230,206,158,27);
                margin-top: 13px;
                padding-top: 9px;
            }
            QScrollArea[arenaSettingsScroll="true"] { border: 0; background: transparent; }

            QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
            QScrollBar:vertical { background: transparent; border: none; width: 9px; margin: 2px; }
            QScrollBar:horizontal { background: transparent; border: none; height: 9px; margin: 2px; }
            QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
                background: #57534c;
                border-radius: 4px;
                min-height: 24px;
                min-width: 24px;
            }
            QScrollBar::handle:hover { background: #777066; }
            QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
            QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
            QStatusBar { color: #aaa49a; background: transparent; }

            QWidget[arenaGlassWindow="true"] { background: transparent; }
            QWidget#centralwidget, QWidget#centralWidget, QWizardPage { background: transparent; }
            QLabel#arenaWindowTitle { color: #d8d2c8; font-weight: 600; }
            QToolButton#arenaClose, QToolButton#arenaMinimize, QToolButton#arenaMaximize {
                padding: 0;
                border-radius: 7px;
                border: 1px solid rgba(0,0,0,70);
                color: transparent;
            }
            QToolButton#arenaClose { background-color: #ff5f57; }
            QToolButton#arenaMinimize { background-color: #febc2e; }
            QToolButton#arenaMaximize { background-color: #28c840; }
            QToolButton#arenaMaximize:disabled { background-color: #1f9f37; border-color: rgba(0,0,0,50); }
            QToolButton#arenaClose:hover { background-color: #ff756e; }
            QToolButton#arenaMinimize:hover { background-color: #ffca55; }
            QToolButton#arenaMaximize:hover { background-color: #4bd361; }
        )ARENA");
        app.setStyleSheet(arenaStyleSheet);
    }
}

#endif
