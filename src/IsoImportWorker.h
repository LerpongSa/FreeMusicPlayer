#pragma once
//
// QObject worker driving one whole "Add ISO..." import (see
// IsoAudioExtractor.h) off the UI thread. Meant to be moved to a QThread by
// the caller (MainWindow) via moveToThread(); every slot here then runs on
// that thread's own event loop, which is also what lets the synchronous
// QAudioDecoder use inside IsoAudioExtractor::extractTrackToFlac() work
// (QAudioDecoder needs a running event loop on whatever thread it's used
// from - the worker thread has one via QThread::exec(), the same pattern
// already verified in this feature's throwaway round-trip test tools).
//
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

class IsoImportWorker : public QObject
{
    Q_OBJECT

public:
    explicit IsoImportWorker(QObject *parent = nullptr);

public slots:
    // Opens isoPath, extracts every track it finds straight to .flac files
    // under outDir (created if missing), and emits progress along the way.
    // Intended to be invoked via a queued connection onto the worker
    // thread (e.g. QMetaObject::invokeMethod(worker, "run", Qt::QueuedConnection, ...)),
    // never called directly from the UI thread.
    void run(const QString &isoPath, const QString &outDir);

    // Thread-safe: sets a flag checked between tracks (not mid-track - a
    // track already being extracted always finishes or fails on its own
    // before the next one is skipped). Safe to call from the UI thread.
    void cancel();

signals:
    void openFailed(QString message);
    void trackCountKnown(int count, QString formatLabel);
    void trackStarted(int index, int count, QString title);
    // Fine-grained progress within the track currently being extracted -
    // see IsoAudioExtractor::ProgressPhase for what each phase means and
    // when percent is meaningful vs. just a start/end marker. `phase` is
    // already translated (via this object's own tr(), a legitimate QObject
    // context) so MainWindow can drop it straight into a label.
    void trackProgress(int index, QString phase, int percent);
    void trackFailed(int index, QString title, QString message);
    void trackFinished(int index, QString title, QString outFlacPath);
    // Always emitted last, exactly once, whether the import ran to
    // completion, was cancelled partway, or every track failed -
    // flacPaths lists only the tracks that actually succeeded.
    void importFinished(QStringList flacPaths);

private:
    std::atomic_bool m_cancelRequested{false};
};
