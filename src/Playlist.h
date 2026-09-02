#pragma once
//
// Playlist model: ordered track list, current-track tracking, M3U8
// load/save/clear, and the shuffle/repeat state machine.
//
// The shuffle/repeat logic and the multi-select-delete index bookkeeping
// were both simulated hundreds of times in Python before being ported here
// (see project notes) - that caught two real bugs during design: (1) a
// naive "check bounds, then reshuffle" ordering lets repeat-off run
// forever because the end-of-playlist check happened AFTER a call that had
// already reshuffled and wrapped; the fix is to check bounds strictly
// before ever touching the shuffle order. (2) reshuffling naively can put
// the same track back-to-back across a cycle boundary; the fix swaps the
// new cycle's first pick away from the previous cycle's last pick.
//
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class Playlist : public QObject
{
    Q_OBJECT

public:
    enum class RepeatMode { Off, All, One };

    struct Track
    {
        QString filePath;
        QString displayName; // file base name, shown until/unless real tag title is read
    };

    explicit Playlist(QObject *parent = nullptr);

    int count() const { return m_tracks.size(); }
    bool isEmpty() const { return m_tracks.isEmpty(); }
    const Track &at(int index) const { return m_tracks.at(index); }
    QStringList filePaths() const;

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index); // clamps, emits currentIndexChanged
    QString currentFilePath() const;

    void addFiles(const QStringList &paths);   // appends, skipping duplicates already present
    void removeIndices(const QVector<int> &indices); // multi-select delete; keeps currentIndex on the same track when possible
    void clear();

    void moveTrack(int from, int to);

    // Reorders tracks in one shot: reordered[i] = the OLD track that was at
    // index newOrder[i]. Used to sync a drag-and-drop reorder performed in
    // the playlist view back into this model - the view just hands back its
    // final item order (see MainWindow::refreshPlaylistWidget /
    // setupConnections) rather than us trying to replay Qt's internal-move
    // row math one step at a time. No-op if newOrder isn't a valid
    // permutation of the current indices.
    void reorder(const QVector<int> &newOrder);

    bool setShuffle(bool on);
    bool shuffle() const { return m_shuffle; }

    void setRepeatMode(RepeatMode mode);
    RepeatMode repeatMode() const { return m_repeatMode; }

    // Advances according to shuffle/repeat rules. Returns false when
    // playback should stop (end of list reached with repeat off) - caller
    // should NOT change currentIndex in that case, it's left at the last track.
    bool advanceToNext();
    bool advanceToPrevious();

    // Load appends to the current list (call clear() first for a fresh
    // load). Returns the number of entries that were skipped because the
    // referenced file no longer exists - the caller should report this so
    // the user knows why the list is shorter than the file on disk.
    int loadM3u(const QString &path, int *skippedCount = nullptr);
    bool saveM3u(const QString &path) const;

signals:
    void itemsChanged();
    void currentIndexChanged(int index);
    void shuffleChanged(bool on);
    void repeatModeChanged(RepeatMode mode);

private:
    void reshuffle(int avoidFirstIndex = -1);
    void ensureShuffleValid();

    QVector<Track> m_tracks;
    int m_currentIndex = -1;

    bool m_shuffle = false;
    RepeatMode m_repeatMode = RepeatMode::Off;

    QVector<int> m_shuffleOrder; // permutation of track indices
    int m_shufflePos = -1;
};
