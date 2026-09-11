#ifndef OPENMW_COMPONENTS_MISC_ARENATHEME_H
#define OPENMW_COMPONENTS_MISC_ARENATHEME_H

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

namespace ArenaUi
{
    // Compiled-in theme shared by Launcher, Wizard, Updater and Qt utilities.
    inline void applyMorrowindGlassPalette(QApplication& app)
    {
        if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            app.setStyle(fusion);

        const QColor obsidian(24, 25, 28);
        const QColor surface(32, 33, 36);
        const QColor surfaceRaised(44, 44, 46);
        const QColor brass(190, 156, 94);
        const QColor gold(224, 192, 128);
        const QColor text(232, 225, 212);
        const QColor muted(163, 151, 132);
        const QColor disabled(102, 94, 82);

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
        palette.setColor(QPalette::BrightText, QColor(255, 238, 202));
        palette.setColor(QPalette::Link, gold);
        palette.setColor(QPalette::Highlight, brass);
        palette.setColor(QPalette::HighlightedText, QColor(25, 20, 14));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        app.setPalette(palette);

        app.setStyleSheet(QStringLiteral(R"ARENA(
            QMainWindow, QDialog, QWizard, QWidget#centralWidget, QWidget#centralwidget {
                background-color: #18191c;
                color: #e8e1d4;
            }
            QLabel { color: #e8e1d4; background: transparent; }
            QLabel:disabled { color: #665e52; }
            QGroupBox {
                color: #d7b978;
                border: 1px solid #514c43;
                border-radius: 14px;
                margin-top: 12px;
                padding-top: 9px;
                background-color: rgba(36, 30, 24, 185);
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QPushButton {
                color: #eadfca;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(92, 84, 69, 180), stop:1 rgba(47, 45, 41, 225));
                border: 1px solid #7a633e;
                border-radius: 11px;
                padding: 6px 12px;
                min-height: 20px;
            }
            QToolButton {
                color: #eadfca;
                background-color: rgba(53, 44, 34, 210);
                border: 1px solid #6b5738;
                border-radius: 9px;
                padding: 4px 6px;
            }
            QPushButton:hover, QToolButton:hover {
                background-color: rgba(79, 64, 45, 235);
                border-color: #c29b59;
                color: #f2dfb5;
            }
            QPushButton:pressed, QToolButton:pressed {
                background-color: #2b241c;
                border-color: #8e7247;
            }
            QPushButton:default {
                border: 1px solid #d0aa63;
                color: #f2dfb5;
            }
            QPushButton:disabled, QToolButton:disabled {
                color: #6f675b;
                background-color: #202124;
                border-color: #3b332a;
            }
            QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox,
            QListView, QTreeView, QTableView {
                color: #e8e1d4;
                background-color: rgba(31, 26, 21, 235);
                border: 1px solid #514c43;
                border-radius: 9px;
                selection-background-color: #b99558;
                selection-color: #18130e;
            }
            QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { padding: 4px 7px; }
            QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,
            QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
            QListView:focus, QTreeView:focus, QTableView:focus {
                border-color: #c5a15f;
            }
            QComboBox QAbstractItemView {
                background-color: #202124;
                border: 1px solid #6b5738;
                color: #e8e1d4;
                selection-background-color: #b99558;
                selection-color: #18130e;
            }
            QAbstractItemView::item:hover { background-color: rgba(104, 82, 50, 145); }
            QAbstractItemView::item:selected { background-color: #b99558; color: #18130e; }
            QTabWidget::pane {
                border: 1px solid #4b4031;
                border-radius: 8px;
                background-color: rgba(29, 24, 19, 210);
            }
            QTabBar::tab {
                color: #b9aa91;
                background-color: #202124;
                border: 1px solid #42382c;
                padding: 7px 13px;
            }
            QTabBar::tab:selected {
                color: #ebd29f;
                background-color: #392f24;
                border-color: #9e7f4c;
            }
            QHeaderView::section {
                color: #d9c49a;
                background-color: #2a231c;
                border: 0;
                border-right: 1px solid #4d4030;
                border-bottom: 1px solid #4d4030;
                padding: 5px;
            }
            QProgressBar {
                color: #e8e1d4;
                background-color: #202124;
                border: 1px solid #514c43;
                border-radius: 9px;
                text-align: center;
            }
            QProgressBar::chunk {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #9e7949, stop:1 #e4c88d);
                border-radius: 5px;
            }
            QMenu {
                color: #e8e1d4;
                background-color: #202124;
                border: 1px solid #5b4a34;
                padding: 4px;
            }
            QMenu::item { padding: 6px 20px 6px 10px; border-radius: 4px; }
            QMenu::item:selected { background-color: #6e5736; color: #f1dfb9; }
            QToolTip {
                color: #f0e4ce;
                background-color: #2b241d;
                border: 1px solid #8a6f45;
                padding: 5px;
            }
            QCheckBox, QRadioButton { color: #ded5c6; spacing: 6px; }
            QScrollBar:vertical, QScrollBar:horizontal {
                background: #191a1d;
                border: none;
                margin: 0;
            }
            QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
                background: #62513a;
                border-radius: 5px;
                min-height: 24px;
                min-width: 24px;
            }
            QScrollBar::handle:hover { background: #8e7247; }
            QStatusBar { color: #aa9a82; background: #191a1d; }
            QWidget[arenaGlassWindow="true"] { background: transparent; }
            QWidget#centralwidget, QWidget#centralWidget, QWizardPage { background: transparent; }
            QLabel#arenaWindowTitle { color: #d9c9ac; font-weight: 600; }
            QToolButton#arenaClose, QToolButton#arenaMinimize, QToolButton#arenaMaximize {
                padding: 0; border-radius: 12px; border: 1px solid #74675a;
                color: #eadfca; background: #423e38; font-size: 16px;
            }
            QToolButton#arenaClose:hover { background: #96574c; border-color: #d29480; }
            QToolButton#arenaMinimize:hover { background: #917548; border-color: #d4b573; }
            QToolButton#arenaMaximize:hover { background: #4e7463; border-color: #88ac93; }
            QPushButton[arenaPrimary="true"] {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e4c78c, stop:1 #b18a50);
                color: #211b13; border-color: #ead4a6; font-weight: 600;
            }
            QPushButton[arenaPrimary="true"]:hover { background: #e9cf98; }
            QPushButton[arenaPrimary="true"]:pressed { background: #ac844b; }
            QPushButton[arenaPrimary="true"]:disabled { background: #3c3831; color: #8f8675; border-color: #514c43; }
            QListWidget#iconWidget { background: rgba(28, 29, 32, 160); border-radius: 14px; padding: 6px; }
            QListWidget#iconWidget::item { border-radius: 10px; padding: 4px; }
            QProgressBar { min-height: 18px; }
            QCheckBox:focus, QRadioButton:focus { color: #f2d79e; }
        )ARENA"));
    }
}

#endif
