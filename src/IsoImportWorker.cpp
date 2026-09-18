#include "IsoImportWorker.h"
#include "IsoAudioExtractor.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace {

// IsoAudioExtractor.cpp has no Qt UI context of its own (see its header
// comment), so ProgressPhase is translated to display text here instead,
// through IsoImportWorker's own tr() - the first point in this feature's
// call chain that's actually a QObject.
QString phaseLabel(IsoAudioExtractor::ProgressPhase phase)
{
    switch (phase) {
    case IsoAudioExtractor::ProgressPhase::ReadingSacdAudio:
        return IsoImportWorker::tr("Reading SACD audio");
    case IsoAudioExtractor::ProgressPhase::WritingDsf:
        return IsoImportWorker::tr("Preparing audio");
    case IsoAudioExtractor::ProgressPhase::DecodingDsd:
        return IsoImportWorker::tr("Decoding DSD");
    case IsoAudioExtractor::ProgressPhase::ReadingCdda:
        return IsoImportWorker::tr("Reading audio");
    case IsoAudioExtractor::ProgressPhase::EncodingFlac:
        return IsoImportWorker::tr("Encoding FLAC");
    }
    return QString();
}

} // namespace

IsoImportWorker::IsoImportWorker(QObject *parent)
    : QObject(parent)
{
}

void IsoImportWorker::cancel()
{
    m_cancelRequested = true;
}

void IsoImportWorker::run(const QString &isoPath, const QString &outDir)
{
    m_cancelRequested = false;

    const IsoAudioExtractor::OpenResult opened = IsoAudioExtractor::open(isoPath);
    if (!opened.ok) {
        emit openFailed(opened.errorMessage);
        emit importFinished(QStringList());
        return;
    }

    QDir().mkpath(outDir);

    // Intermediate .dsf files (SACD path only - see IsoAudioExtractor.cpp)
    // live here and are deleted again per track as soon as each is
    // decoded; the whole directory (and anything left behind by a
    // cancelled/failed run) goes away when this QTemporaryDir is
    // destroyed at the end of run().
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        emit openFailed(QStringLiteral("Could not create a temporary working directory."));
        emit importFinished(QStringList());
        return;
    }

    const int count = static_cast<int>(opened.tracks.size());
    emit trackCountKnown(count, opened.formatLabel);

    static const QRegularExpression kUnsafeFilenameChars(QStringLiteral("[\\\\/:*?\"<>|]"));

    QStringList outPaths;
    for (int i = 0; i < count; ++i) {
        if (m_cancelRequested)
            break;

        const IsoAudioExtractor::TrackInfo &track = opened.tracks[static_cast<size_t>(i)];
        emit trackStarted(i, count, track.title);

        const QString safeTitle = QString(track.title).replace(kUnsafeFilenameChars, QStringLiteral("_"));
        const QString outPath = QDir(outDir).filePath(
            QStringLiteral("%1 - %2.flac").arg(track.number, 2, 10, QChar('0')).arg(safeTitle));

        const IsoAudioExtractor::ExtractResult r = IsoAudioExtractor::extractTrackToFlac(
            isoPath, track, outPath, tempDir.path(),
            [this, i](IsoAudioExtractor::ProgressPhase phase, int percent) {
                emit trackProgress(i, phaseLabel(phase), percent);
            });

        if (!r.ok) {
            emit trackFailed(i, track.title, r.errorMessage);
            continue;
        }
        outPaths << outPath;
        emit trackFinished(i, track.title, outPath);
    }

    emit importFinished(outPaths);
}
