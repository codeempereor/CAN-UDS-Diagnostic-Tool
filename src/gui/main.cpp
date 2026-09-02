#include <QApplication>
#include <QMainWindow>
#include <QStyleFactory>
#include <QPalette>
#include "main_window.h"

int main(int argc, char *argv[])
{
    // 高DPI支持（Qt6默认启用，这里显式设置确保兼容）
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);
    app.setApplicationName("CAN/UDS Tool");
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("CAN Tool");

    // 强制使用Fusion风格（跨平台一致，不跟随系统深色模式）
    app.setStyle(QStyleFactory::create("Fusion"));

    // 强制浅色调色板（白色背景，确保在任何系统主题下都是浅色界面）
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    lightPalette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    lightPalette.setColor(QPalette::Link, QColor(0, 0, 255));
    lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    lightPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    app.setPalette(lightPalette);

    // 全局浅色样式表（双重保险，确保所有控件背景都是浅色）
    app.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #f0f0f0;
            color: #000000;
        }
        QDockWidget {
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background-color: #e0e0e0;
            color: #000000;
            padding: 4px;
            border: 1px solid #c0c0c0;
        }
        QMenuBar {
            background-color: #f0f0f0;
            color: #000000;
        }
        QMenuBar::item:selected {
            background-color: #0078d7;
            color: #ffffff;
        }
        QMenu {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #c0c0c0;
        }
        QMenu::item:selected {
            background-color: #0078d7;
            color: #ffffff;
        }
        QToolBar {
            background-color: #f0f0f0;
            color: #000000;
            border: 1px solid #c0c0c0;
        }
        QStatusBar {
            background-color: #e0e0e0;
            color: #000000;
        }
        QPushButton {
            background-color: #f0f0f0;
            color: #000000;
            border: 1px solid #c0c0c0;
            padding: 4px 12px;
            border-radius: 2px;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QComboBox {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #c0c0c0;
            padding: 2px;
        }
        QTableWidget, QTreeWidget, QListWidget {
            background-color: #ffffff;
            color: #000000;
            alternate-background-color: #f5f5f5;
            gridline-color: #e0e0e0;
        }
        QHeaderView::section {
            background-color: #e0e0e0;
            color: #000000;
            border: 1px solid #c0c0c0;
            padding: 4px;
        }
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background-color: #f0f0f0;
        }
        QTabBar::tab {
            background-color: #e0e0e0;
            color: #000000;
            border: 1px solid #c0c0c0;
            padding: 4px 12px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #000000;
        }
        QScrollBar:vertical {
            background-color: #f0f0f0;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background-color: #c0c0c0;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #a0a0a0;
        }
        QScrollBar:horizontal {
            background-color: #f0f0f0;
            height: 12px;
        }
        QScrollBar::handle:horizontal {
            background-color: #c0c0c0;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #a0a0a0;
        }
        QGroupBox {
            background-color: #f0f0f0;
            color: #000000;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #000000;
        }
        QLabel {
            background-color: transparent;
            color: #000000;
        }
    )");

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
