#include "Playlist.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>

Playlist::Playlist(QObject *parent) : QObject(parent) {}

QStringList Playlist::filePaths() const
{
    QStringList out;
    out.reserve(m_tracks.size());
    for (const Track &t : m_tracks)
        out << t.filePath;
    return out;
}

void Playlist::setCurrentIndex(int index)
{
    if (m_tracks.isEmpty()) {
        if (m_currentIndex != -1) {
            m_currentIndex = -1;
            emit currentIndexChanged(-1);
        }
        return;
    }
    index = std::clamp(index, 0, static_cast<int>(m_tracks.size()) - 1);
    if (index == m_currentIndex)
        return;
    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);
}

QString Playlist::currentFilePath() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_tracks.size())
        return QString();
    return m_tracks[m_currentIndex].filePath;
}

void Playlist::addFiles(const QStringList &paths)
{
    bool changed = false;
    for (const QString &p : paths) {
        bool exists = false;
        for (const Track &t : m_tracks) {
            if (t.filePath == p) {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;

        Track t;
        t.filePath = p;
        t.displayName = QFileInfo(p).completeBaseName();
        m_tracks.push_back(t);
        changed = true;
    }

    if (changed) {
        m_shuffleOrder.clear();
        m_shufflePos = -1;
        if (m_currentIndex < 0 && !m_tracks.isEmpty())
            setCurrentIndex(0);
        emit itemsChanged();
    }
}

void Playlist::removeIndices(const QVector<int> &indicesIn)
{
    if (indicesIn.isEmpty() || m_tracks.isEmpty())
        return;

    QVector<int> indices = indicesIn;
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    const QSet<int> removeSet(indices.begin(), indices.end());
    const bool currentWasRemoved = removeSet.contains(m_currentIndex);

    // Verified against a Python reference: descending pass, decrementing
    // for every removed index strictly before the (old) current position.
    int newCurrent = m_currentIndex;
    for (auto it = indices.crbegin(); it != indices.crend(); ++it) {
        if (*it < newCurrent)
            --newCurrent;
    }

    QVector<Track> survivors;
    survivors.reserve(m_tracks.size() - indices.size());
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (!removeSet.contains(i))
            survivors.push_back(m_tracks[i]);
    }
    m_tracks = survivors;

    if (m_tracks.isEmpty()) {
        newCurrent = -1;
    } else if (currentWasRemoved) {
        // The exact track that was playing is gone; land on the next
        // remaining track at/after that position rather than guessing.
        newCurrent = std::min(newCurrent, static_cast<int>(m_tracks.size()) - 1);
    }

    m_currentIndex = newCurrent;
    m_shuffleOrder.clear();
    m_shufflePos = -1;

    emit itemsChanged();
    emit currentIndexChanged(m_currentIndex);
}

void Playlist::clear()
{
    if (m_tracks.isEmpty() && m_currentIndex == -1)
        return;
    m_tracks.clear();
    m_currentIndex = -1;
    m_shuffleOrder.clear();
    m_shufflePos = -1;
    emit itemsChanged();
    emit currentIndexChanged(-1);
}

void Playlist::moveTrack(int from, int to)
{
    if (from == to || from < 0 || from >= m_tracks.size() || to < 0 || to >= m_tracks.size())
        return;

    m_tracks.move(from, to);

    if (m_currentIndex == from)
        m_currentIndex = to;
    else if (from < m_currentIndex && to >= m_currentIndex)
        --m_currentIndex;
    else if (from > m_currentIndex && to <= m_currentIndex)
        ++m_currentIndex;

    m_shuffleOrder.clear();
    m_shufflePos = -1;
    emit itemsChanged();
}

void Playlist::reshuffle(int avoidFirstIndex)
{
    m_shuffleOrder.resize(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i)
        m_shuffleOrder[i] = i;

    for (int i = m_shuffleOrder.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(QRandomGenerator::global()->bounded(i + 1));
        std::swap(m_shuffleOrder[i], m_shuffleOrder[j]);
    }

    // Avoid replaying the same track back-to-back across a shuffle-cycle
    // boundary (the naive reshuffle can otherwise pick the same track that
    // just finished as the very next one).
    if (avoidFirstIndex >= 0 && m_shuffleOrder.size() > 1 && m_shuffleOrder[0] == avoidFirstIndex)
        std::swap(m_shuffleOrder[0], m_shuffleOrder[1]);

    m_shufflePos = 0;
}

void Playlist::ensureShuffleValid()
{
    if (m_shuffleOrder.size() != m_tracks.size()) {
        reshuffle();
        const int pos = m_shuffleOrder.indexOf(m_currentIndex);
        if (pos >= 0) {
            m_shufflePos = pos;
        } else if (!m_shuffleOrder.isEmpty()) {
            m_shufflePos = 0;
            m_currentIndex = m_shuffleOrder[0];
        }
    }
}

bool Playlist::setShuffle(bool on)
{
    if (on == m_shuffle)
        return true;
    m_shuffle = on;
    if (on) {
        reshuffle();
        const int pos = m_shuffleOrder.indexOf(m_currentIndex);
        m_shufflePos = pos >= 0 ? pos : 0;
    } else {
        m_shuffleOrder.clear();
        m_shufflePos = -1;
    }
    emit shuffleChanged(m_shuffle);
    return true;
}

void Playlist::setRepeatMode(RepeatMode mode)
{
    if (mode == m_repeatMode)
        return;
    m_repeatMode = mode;
    emit repeatModeChanged(mode);
}

bool Playlist::advanceToNext()
{
    if (m_tracks.isEmpty())
        return false;

    if (m_shuffle) {
        ensureShuffleValid();
        const int nextPos = m_shufflePos + 1;

        // The end-of-cycle bounds check happens strictly before any
        // reshuffle call - reversing this order is exactly the bug that
        // let repeat-off run forever during design (see Playlist.h).
        if (nextPos >= m_shuffleOrder.size()) {
            if (m_repeatMode == RepeatMode::All) {
                const int last = m_shuffleOrder[m_shufflePos];
                reshuffle(last);
                setCurrentIndex(m_shuffleOrder[m_shufflePos]);
                return true;
            }
            if (m_repeatMode == RepeatMode::One)
                return true; // replay current, no reshuffle, no advance
            return false;    // repeat off: stop, leave currentIndex as-is
        }

        m_shufflePos = nextPos;
        setCurrentIndex(m_shuffleOrder[m_shufflePos]);
        return true;
    }

    if (m_repeatMode == RepeatMode::One)
        return true;

    int next = m_currentIndex + 1;
    if (next >= m_tracks.size()) {
        if (m_repeatMode == RepeatMode::All)
            next = 0;
        else
            return false;
    }
    setCurrentIndex(next);
    return true;
}

bool Playlist::advanceToPrevious()
{
    if (m_tracks.isEmpty())
        return false;

    if (m_shuffle) {
        ensureShuffleValid();
        const int prevPos = m_shufflePos - 1;

        if (prevPos < 0) {
            if (m_repeatMode == RepeatMode::All) {
                m_shufflePos = m_shuffleOrder.size() - 1;
                setCurrentIndex(m_shuffleOrder[m_shufflePos]);
                return true;
            }
            if (m_repeatMode == RepeatMode::One)
                return true;
            return false;
        }

        m_shufflePos = prevPos;
        setCurrentIndex(m_shuffleOrder[m_shufflePos]);
        return true;
    }

    if (m_repeatMode == RepeatMode::One)
        return true;

    int prev = m_currentIndex - 1;
    if (prev < 0) {
        if (m_repeatMode == RepeatMode::All)
            prev = m_tracks.size() - 1;
        else
            return false;
    }
    setCurrentIndex(prev);
    return true;
}

int Playlist::loadM3u(const QString &path, int *skippedCount)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (skippedCount)
            *skippedCount = 0;
        return 0;
    }

    const QDir baseDir = QFileInfo(path).dir();
    QStringList validPaths;
    int skipped = 0;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        QString resolved = line;
        if (QFileInfo(resolved).isRelative())
            resolved = baseDir.filePath(resolved);

        if (QFileInfo::exists(resolved))
            validPaths << resolved;
        else
            ++skipped;
    }

    addFiles(validPaths);

    if (skippedCount)
        *skippedCount = skipped;
    return validPaths.size();
}

bool Playlist::saveM3u(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    stream << "#EXTM3U\n";
    for (const Track &t : m_tracks) {
        stream << "#EXTINF:-1," << t.displayName << "\n";
        stream << t.filePath << "\n";
    }
    return true;
}
