#include "Settings.h"

#include <QCoreApplication>
#include <QTemporaryFile>
#include <QDir>

bool Settings::portableModeAvailable()
{
    static const bool ok = [] {
        const QString dir = QCoreApplication::applicationDirPath();
        QTemporaryFile probe(dir + "/.write-test-XXXXXX");
        return probe.open();
    }();
    return ok;
}

Settings::Settings()
{
    if (portableModeAvailable()) {
        const QString iniPath = QCoreApplication::applicationDirPath() + "/FreeMusicPlayer.ini";
        m_settings = std::make_unique<QSettings>(iniPath, QSettings::IniFormat);
    } else {
        m_settings = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                                   "FreeMusicPlayer", "FreeMusicPlayer");
    }
}

Settings::~Settings()
{
    sync();
}

void Settings::sync()
{
    if (m_settings)
        m_settings->sync();
}

int Settings::volume() const
{
    return m_settings->value("playback/volume", 70).toInt();
}
void Settings::setVolume(int v)
{
    m_settings->setValue("playback/volume", v);
}

bool Settings::muted() const
{
    return m_settings->value("playback/muted", false).toBool();
}
void Settings::setMuted(bool m)
{
    m_settings->setValue("playback/muted", m);
}

int Settings::shuffleMode() const
{
    return m_settings->value("playback/shuffle", 0).toInt();
}
void Settings::setShuffleMode(int m)
{
    m_settings->setValue("playback/shuffle", m);
}

int Settings::repeatMode() const
{
    return m_settings->value("playback/repeat", 0).toInt();
}
void Settings::setRepeatMode(int m)
{
    m_settings->setValue("playback/repeat", m);
}

QString Settings::lastPlayedFile() const
{
    return m_settings->value("playback/lastFile").toString();
}
void Settings::setLastPlayedFile(const QString &path)
{
    m_settings->setValue("playback/lastFile", path);
}

qint64 Settings::lastPlaybackPositionMs() const
{
    return m_settings->value("playback/lastPositionMs", 0).toLongLong();
}
void Settings::setLastPlaybackPositionMs(qint64 ms)
{
    m_settings->setValue("playback/lastPositionMs", ms);
}

QStringList Settings::playlistFiles() const
{
    return m_settings->value("playlist/files").toStringList();
}
void Settings::setPlaylistFiles(const QStringList &files)
{
    m_settings->setValue("playlist/files", files);
}

int Settings::lastPlaylistIndex() const
{
    return m_settings->value("playlist/lastIndex", -1).toInt();
}
void Settings::setLastPlaylistIndex(int index)
{
    m_settings->setValue("playlist/lastIndex", index);
}

bool Settings::eqEnabled() const
{
    return m_settings->value("eq/enabled", true).toBool();
}
void Settings::setEqEnabled(bool on)
{
    m_settings->setValue("eq/enabled", on);
}

QString Settings::eqPresetName() const
{
    return m_settings->value("eq/preset", "Flat").toString();
}
void Settings::setEqPresetName(const QString &name)
{
    m_settings->setValue("eq/preset", name);
}

QVector<double> Settings::eqCustomGains() const
{
    const QStringList raw = m_settings->value("eq/customGains").toStringList();
    QVector<double> gains;
    gains.reserve(raw.size());
    for (const QString &s : raw)
        gains.append(s.toDouble());
    return gains;
}
void Settings::setEqCustomGains(const QVector<double> &gains)
{
    QStringList raw;
    raw.reserve(gains.size());
    for (double g : gains)
        raw.append(QString::number(g, 'f', 3));
    m_settings->setValue("eq/customGains", raw);
}

QString Settings::visualizerStyle() const
{
    // "Brick Box"/"Neon Green" (below) are only the FIRST-RUN defaults - once
    // any value has actually been saved (including by the user just picking
    // something in the UI), that saved value always wins over these.
    return m_settings->value("visualizer/style", "Brick Box").toString();
}
void Settings::setVisualizerStyle(const QString &style)
{
    m_settings->setValue("visualizer/style", style);
}

QString Settings::visualizerColorScheme() const
{
    return m_settings->value("visualizer/colorScheme", "Neon Green").toString();
}
void Settings::setVisualizerColorScheme(const QString &scheme)
{
    m_settings->setValue("visualizer/colorScheme", scheme);
}

bool Settings::visualizerEnabled() const
{
    return m_settings->value("visualizer/enabled", true).toBool();
}
void Settings::setVisualizerEnabled(bool on)
{
    m_settings->setValue("visualizer/enabled", on);
}

QString Settings::themeBackgroundColor() const
{
    return m_settings->value("theme/backgroundColor").toString();
}
void Settings::setThemeBackgroundColor(const QString &hexColor)
{
    m_settings->setValue("theme/backgroundColor", hexColor);
}

QString Settings::themeAccentColor() const
{
    return m_settings->value("theme/accentColor").toString();
}
void Settings::setThemeAccentColor(const QString &hexColor)
{
    m_settings->setValue("theme/accentColor", hexColor);
}

QStringList Settings::tabOrder() const
{
    return m_settings->value("ui/tabOrder").toStringList();
}
void Settings::setTabOrder(const QStringList &order)
{
    m_settings->setValue("ui/tabOrder", order);
}

QByteArray Settings::windowGeometry() const
{
    return m_settings->value("window/geometry").toByteArray();
}
void Settings::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue("window/geometry", geometry);
}
