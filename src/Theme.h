#pragma once
//
// Central dark-theme color tokens + the QSS strings for every top-level
// window/dialog in the app. Kept together in one header because the skill
// guidance is explicit: a QWidget-level stylesheet propagates *color* to
// child popups/dialogs but not *background*, so each separate window needs
// its own complete stylesheet rather than relying on inheritance.
//
// The kXxx constants below are the DEFAULT palette, kept as literal hex
// strings so "Reset to Default" (see resetToDefaultPalette()) always lands
// on exactly these values, not a re-derived approximation. Everything that
// actually paints the UI - appStyleSheet() and the accessor functions below
// it - reads from current(), the live/possibly-user-customized palette, not
// from these constants directly.
//
#include <QColor>
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

struct Palette {
    QColor bg0, bg1, bg2, bg3, border, text, textDim, accent, accentHi, danger;
};

// Nudges a color's HSL lightness by deltaL (in -1.0..1.0), keeping hue and
// saturation. Used to derive the panel/border/hover shades and the "hi"
// accent variant from just the two colors the Theme tab actually exposes
// (background + accent), the same way the hand-picked default palette's
// shades relate to kBg0/kAccent.
inline QColor adjustLightness(const QColor &c, double deltaL)
{
    // QColor::getHslF() takes float* (not qreal*/double*) in this Qt build -
    // float* and double* don't implicitly convert to each other (unlike a
    // plain float/double value, which does), so these MUST be float here
    // even though the rest of this function does its math in double.
    float h = 0.0f, s = 0.0f, l = 0.0f, a = 1.0f;
    c.getHslF(&h, &s, &l, &a);
    if (h < 0.0f)
        h = 0.0f; // achromatic (gray) colors report h=-1; setHslF wants 0..1
    l = static_cast<float>(qBound(0.0, static_cast<double>(l) + deltaL, 1.0));
    QColor out;
    out.setHslF(h, s, l, a);
    return out;
}

inline Palette defaultPalette()
{
    Palette p;
    p.bg0 = QColor(kBg0);
    p.bg1 = QColor(kBg1);
    p.bg2 = QColor(kBg2);
    p.bg3 = QColor(kBg3);
    p.border = QColor(kBorder);
    p.text = QColor(kText);
    p.textDim = QColor(kTextDim);
    p.accent = QColor(kAccent);
    p.accentHi = QColor(kAccentHi);
    p.danger = QColor(kDanger);
    return p;
}

// Derives a full palette from just a background and an accent color - the
// two things the Theme tab lets the user pick. The lightness deltas below
// were reverse-engineered from the default palette itself (e.g. kBg3 is
// ~0.115 lighter than kBg0 in HSL), so a custom theme keeps the same "steps"
// between window/panel/control/hover shades that the hand-tuned default has.
inline Palette derivePalette(const QColor &backgroundColor, const QColor &accentColor)
{
    Palette p;
    p.bg0 = backgroundColor;
    p.bg1 = adjustLightness(backgroundColor, 0.035);
    p.bg2 = adjustLightness(backgroundColor, 0.075);
    p.bg3 = adjustLightness(backgroundColor, 0.115);
    p.border = adjustLightness(backgroundColor, 0.14);

    float h = 0.0f, s = 0.0f, l = 0.0f, a = 1.0f; // see adjustLightness() above re: float* here
    backgroundColor.getHslF(&h, &s, &l, &a);
    const bool darkBg = l < 0.5f;
    p.text = darkBg ? QColor("#f0f0f5") : QColor("#1a1a22");
    p.textDim = adjustLightness(p.text, darkBg ? -0.35 : 0.35);

    p.accent = accentColor;
    p.accentHi = adjustLightness(accentColor, 0.075);
    p.danger = QColor(kDanger);
    return p;
}

// The palette actually in effect - defaultPalette() until setCustomPalette()
// is called, or resetToDefaultPalette() afterwards. A function-local static
// in an inline function is guaranteed to be the single shared instance
// across every translation unit that includes this header, so this works as
// a plain global without needing a Theme.cpp.
inline Palette &currentPaletteRef()
{
    static Palette p = defaultPalette();
    return p;
}

inline const Palette &current()
{
    return currentPaletteRef();
}

inline void setCustomPalette(const QColor &backgroundColor, const QColor &accentColor)
{
    currentPaletteRef() = derivePalette(backgroundColor, accentColor);
}

inline void resetToDefaultPalette()
{
    currentPaletteRef() = defaultPalette();
}

// Whether the live palette differs from the hand-picked default - lets
// callers (e.g. the Theme tab's own swatch buttons on startup) tell a
// restored custom theme apart from "nothing was ever customized".
inline bool isCustomPalette()
{
    const Palette &p = current();
    return p.bg0 != QColor(kBg0) || p.accent != QColor(kAccent);
}

// Convenience QColor accessors for code that paints its own widgets
// (QPainter-drawn icons/handles/placeholders) instead of going through the
// QSS stylesheet below, so that custom-painted UI stays in sync with the
// live/user-chosen palette too, not just stylesheet-driven widgets.
inline QColor bg3Color()      { return current().bg3; }
inline QColor borderColor()   { return current().border; }
inline QColor textColor()     { return current().text; }
inline QColor textDimColor()  { return current().textDim; }
inline QColor accentColor()   { return current().accent; }
inline QColor accentHiColor() { return current().accentHi; }

// Applied once on QApplication - covers MainWindow and every plain QWidget
// child (buttons, sliders, list widgets, etc).
inline QString appStyleSheet()
{
    const Palette &p = current();
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
        QTabWidget::pane {
            border: 1px solid %5;
            border-radius: 8px;
            top: -1px;
            background-color: %1;
        }
        QTabWidget {
            background-color: %1;
        }
        QTabBar {
            background-color: %1;
            border: none;
        }
        QTabBar::tab {
            background-color: %6;
            color: %2;
            padding: 8px 18px;
            border: 1px solid %5;
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: %7;
            color: #ffffff;
            border-color: %7;
        }
        QTabBar::tab:!selected:hover {
            background-color: %8;
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
        .arg(p.bg0.name(), p.text.name(), p.textDim.name(), p.bg2.name(), p.border.name(),
             p.bg3.name(), p.accent.name(), p.accentHi.name());
}

} // namespace Theme
