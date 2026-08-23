#pragma once
//
// Central dark-theme color tokens + the QSS strings for every top-level
// window/dialog in the app. Kept together in one header because the skill
// guidance is explicit: a QWidget-level stylesheet propagates *color* to
// child popups/dialogs but not *background*, so each separate window needs
// its own complete stylesheet rather than relying on inheritance.
//
#include <QString>

namespace Theme {

constexpr const char *kBg0        = "#14151f"; // window background
constexpr const char *kBg1        = "#1b1c28"; // panels
constexpr const char *kBg2        = "#232435"; // controls / list items
constexpr const char *kBg3        = "#2c2e42"; // hovered controls
constexpr const char *kBorder     = "#33344a";
constexpr const char *kText       = "#e8e8f0";
constexpr const char *kTextDim    = "#9698b0";
constexpr const char *kAccent     = "#6c5ce7";
constexpr const char *kAccentHi   = "#8578f0";
constexpr const char *kDanger     = "#e55b6c";

// Applied once on QApplication - covers MainWindow and every plain QWidget
// child (buttons, sliders, list widgets, etc).
inline QString appStyleSheet()
{
    return QString(R"(
        QWidget {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI", sans-serif;
            font-size: 13px;
        }
        QMainWindow, QDialog {
            background-color: %1;
        }
        QLabel { background: transparent; }
        QLabel#TrackTitle {
            font-size: 16px;
            font-weight: 600;
            padding-top: 4px;
            padding-bottom: 2px;
        }
        QLabel#TrackArtist { color: %3; font-size: 12px; }
        QLabel#ShutdownCountdown {
            font-size: 40px;
            font-weight: 700;
            padding: 18px 0px;
            letter-spacing: 2px;
        }
        QLabel#SectionHeader {
            color: %3;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1px;
            text-transform: uppercase;
        }
        QFrame#CoverFrame, QFrame#Panel {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 10px;
        }
        QPushButton {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QPushButton:hover  { background-color: %6; }
        QPushButton:pressed{ background-color: %5; }
        QPushButton:checked{ background-color: %7; border-color: %7; }
        QPushButton#TransportButton {
            border-radius: 20px;
            min-width: 40px; min-height: 40px;
            max-width: 40px; max-height: 40px;
            padding: 0px;
            /* Qt's CSS box model adds padding/border ON TOP of min/max
               width|height (content-box sizing) - the base QPushButton
               rule's "padding: 6px 10px" would otherwise stretch this box
               wider than tall (40+20 horiz vs 40+12 vert) and turn the
               circle into an oval. Zeroing padding here keeps the box a
               true 40x40 square so border-radius: 20px (exactly half) is
               a perfect circle. */
        }
        QPushButton#PlayButton {
            border-radius: 26px;
            min-width: 52px; min-height: 52px;
            max-width: 52px; max-height: 52px;
            padding: 0px;
            background-color: %7;
            border-color: %7;
        }
        QPushButton#PlayButton:hover { background-color: %8; }
        QToolButton {
            background: transparent;
            border: none;
            border-radius: 4px;
            padding: 4px;
        }
        QToolButton:hover { background-color: %6; }
        QToolButton:checked { background-color: %7; }

        QSlider::groove:horizontal {
            height: 5px;
            background: %5;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: %7;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 13px; height: 13px;
            margin: -5px 0;
            border-radius: 6px;
            background: %2;
        }
        QSlider::groove:vertical {
            width: 5px;
            background: %5;
            border-radius: 2px;
        }
        QSlider::add-page:vertical {
            background: %5;
            border-radius: 2px;
        }
        QSlider::sub-page:vertical {
            background: %7;
            border-radius: 2px;
        }
        QSlider::handle:vertical {
            width: 13px; height: 13px;
            margin: 0 -5px;
            border-radius: 6px;
            background: %2;
        }

        QListWidget {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 8px;
            outline: none;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-radius: 4px;
        }
        QListWidget::item:selected {
            background-color: %7;
            color: #ffffff;
        }
        QListWidget::item:hover:!selected {
            background-color: %6;
        }

        QComboBox {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 4px 8px;
        }
        QComboBox:hover { background-color: %6; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background-color: %1;
            color: %2;
            border: 1px solid %5;
            selection-background-color: %7;
            selection-color: #ffffff;
            outline: none;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %5;
            border-radius: 5px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

        QMenu {
            background-color: %1;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px;
            border-radius: 4px;
        }
        QMenu::item:selected { background-color: %7; color: #ffffff; }
        QMenu::separator { height: 1px; background: %5; margin: 4px 6px; }

        QMessageBox, QInputDialog {
            background-color: %1;
        }
        QInputDialog QLineEdit {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 4px;
            color: %2;
        }
        QCheckBox::indicator {
            width: 15px; height: 15px;
            border: 1px solid %5;
            border-radius: 3px;
            background: %4;
        }
        QCheckBox::indicator:checked {
            background: %7;
            border-color: %7;
        }

        QStatusBar { background-color: %1; color: %3; }
        QToolTip {
            background-color: %1;
            color: %2;
            border: 1px solid %5;
            padding: 4px;
        }
    )")
        .arg(kBg0, kText, kTextDim, kBg2, kBorder, kBg3, kAccent, kAccentHi);
}

} // namespace Theme
