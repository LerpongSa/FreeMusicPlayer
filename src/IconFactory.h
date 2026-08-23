#pragma once
//
// Hand-drawn transport/control icons. Qt's style()->standardIcon(...) draws
// dark artwork meant for light backgrounds and effectively disappears
// against this app's dark theme, so every icon here is painted from
// scratch in whatever color the theme needs - which also lets toggle
// buttons (shuffle/repeat/mute) recolor themselves to signal on/off state.
//
#include <QIcon>
#include <QColor>

namespace IconFactory {

enum class Glyph {
    Play,
    Pause,
    Previous,
    Next,
    Stop,
    Shuffle,
    RepeatAll,
    RepeatOne,
    VolumeMute,
    VolumeLow,
    VolumeHigh,
    FolderOpen,
    Save,
    Clear,
    ListMusic,
    AppIcon, // used for both the window icon and rendered into app.ico
};

QIcon make(Glyph glyph, const QColor &color, int size = 22);

} // namespace IconFactory
