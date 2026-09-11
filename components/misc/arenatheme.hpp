#ifndef OPENMW_COMPONENTS_MISC_ARENATHEME_H
#define OPENMW_COMPONENTS_MISC_ARENATHEME_H

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

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

        app.setStyleSheet(QStringLiteral(R"ARENA(
            QMainWindow, QDialog, QWizard, QWidget#centralWidget, QWidget#centralwidget {
                background-color: #191a1d;
                color: #ebe6dc;
            }
            QWidget { selection-background-color: #c19b5b; selection-color: #1d1812; }
            QLabel { color: #ebe6dc; background: transparent; }
            QLabel:disabled { color: #706c64; }

            QStackedWidget#pagesWidget {
                background-color: rgba(24, 24, 26, 172);
                border: 1px solid rgba(233, 216, 182, 38);
                border-radius: 15px;
            }
            QFrame[arenaCard="true"] {
                background-color: rgba(255, 255, 255, 12);
                border: 1px solid rgba(255, 255, 255, 24);
                border-radius: 12px;
            }
            QFrame[arenaFooter="true"] {
                background-color: rgba(255, 255, 255, 8);
                border: 1px solid rgba(255, 255, 255, 18);
                border-radius: 12px;
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

            QCheckBox, QRadioButton { color: #e1dcd2; spacing: 7px; }
            QCheckBox::indicator {
                width: 16px; height: 16px;
                border: 1px solid #69665f;
                border-radius: 5px;
                background-color: #25262a;
            }
            QCheckBox::indicator:hover { border-color: #bd9b61; background-color: #2d2c2b; }
            QCheckBox::indicator:checked {
                background-color: #d1aa63;
                border: 4px solid #d1aa63;
            }
            QCheckBox::indicator:disabled { background-color: #222326; border-color: #44443f; }
            QRadioButton::indicator {
                width: 16px; height: 16px;
                border: 1px solid #69665f;
                border-radius: 9px;
                background-color: #25262a;
            }
            QRadioButton::indicator:hover { border-color: #bd9b61; }
            QRadioButton::indicator:checked {
                background-color: #d1aa63;
                border: 4px solid #474038;
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

            QListWidget#iconWidget {
                background-color: rgba(255,255,255,7);
                border: 1px solid rgba(255,255,255,18);
                border-radius: 14px;
                padding: 5px;
                outline: 0;
            }
            QListWidget#iconWidget::item {
                color: #d8d2c7;
                background: transparent;
                border: 1px solid transparent;
                border-radius: 10px;
                padding: 2px 8px;
            }
            QListWidget#iconWidget::item:hover {
                color: #efe7d8;
                background-color: rgba(255,255,255,9);
            }
            QListWidget#iconWidget::item:selected {
                color: #f1d69f;
                background-color: rgba(190, 148, 80, 26);
                border-color: rgba(223, 184, 111, 82);
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
            QToolButton#arenaClose:hover { background-color: #ff756e; }
            QToolButton#arenaMinimize:hover { background-color: #ffca55; }
            QToolButton#arenaMaximize:hover { background-color: #4bd361; }
        )ARENA"));
    }
}

#endif
