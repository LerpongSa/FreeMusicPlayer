#pragma once
//
// Portable-mode aware QSettings factory + typed accessors for everything
// this app persists as "defaults for next time": playlist, last track,
// volume, shuffle/repeat, EQ gains + preset, visualizer style, window
// geometry.
//
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>
#include <memory>

class Settings
{
public:
    Settings();
    ~Settings();

    // True if the app folder itself is writable (portable distribution).
    // Cached after first check.
    static bool portableModeAvailable();

    // --- Playback ---------------------------------------------------
    int volume() const;                 // 0-100
    void setVolume(int v);

    bool muted() const;
    void setMuted(bool m);

    int shuffleMode() const;            // 0 = off, 1 = on
    void setShuffleMode(int m);

    int repeatMode() const;             // 0 = off, 1 = repeat-all, 2 = repeat-one
    void setRepeatMode(int m);

    QString lastPlayedFile() const;
    void setLastPlayedFile(const QString &path);

    qint64 lastPlaybackPositionMs() const;
    void setLastPlaybackPositionMs(qint64 ms);

    // --- Playlist -----------------------------------------------------
    QStringList playlistFiles() const;
    void setPlaylistFiles(const QStringList &files);

    int lastPlaylistIndex() const;
    void setLastPlaylistIndex(int index);

    // --- Equalizer ------------------------------------------------------
    bool eqEnabled() const;
    void setEqEnabled(bool on);

    QString eqPresetName() const;
    void setEqPresetName(const QString &name);

    // 10 gains in dB, one per band, in band order.
    QVector<double> eqCustomGains() const;
    void setEqCustomGains(const QVector<double> &gains);

    // --- Visualizer -----------------------------------------------------
    QString visualizerStyle() const;
    void setVisualizerStyle(const QString &style);

    QString visualizerColorScheme() const;
    void setVisualizerColorScheme(const QString &scheme);

    bool visualizerEnabled() const;
    void setVisualizerEnabled(bool on);

    // --- Window -----------------------------------------------------
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    void sync();

private:
    std::unique_ptr<QSettings> m_settings;
};
