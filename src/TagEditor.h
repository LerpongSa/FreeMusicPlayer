#pragma once
//
// Reads and writes the Title/Artist/Album text tags - and, as of the cover
// art editing feature, the embedded cover picture too - across the
// containers this player supports, content-sniffed (falling back to file
// extension) the same way CoverArtExtractor sniffs cover art:
//   - MP3:  ID3v2.3/2.4 TIT2/TPE1/TALB text frames + an APIC picture frame
//           (ID3v2.2 is read-only - its 3-byte frame ids aren't byte-
//           compatible with the 4-byte v2.3 container this always writes
//           back, see TagEditor.cpp).
//   - WAV:  the same ID3v2 tag format (text frames + APIC), carried inside
//           a RIFF "id3 " chunk (mirrors CoverArtExtractor's WAV cover-art
//           path).
//   - FLAC: the VORBIS_COMMENT metadata block (TITLE/ARTIST/ALBUM fields)
//           for text, and a METADATA_BLOCK_PICTURE block (type 6) for the
//           cover.
//   - MP4/M4A: read-only, cover art included. Safely rewriting nested atom
//           sizes - and the sample-offset tables that reference into
//           'mdat' once 'moov' changes size - needs more validation than
//           this bring-up covers, so writeSupported() reports false rather
//           than risking a corrupted media file. Use writeSupported() to
//           decide whether to offer Save for a given file.
//
// Every write preserves everything else already in the file - other ID3
// frames, other Vorbis comment fields (TRACKNUMBER, GENRE, DATE, ...), and
// any other RIFF chunks. The embedded picture is left completely untouched
// unless TrackTags::coverArtAction says otherwise (see below) - a plain
// Title/Artist/Album edit never touches it. Writes go to a temp file via
// QSaveFile and only replace the original on success, so a failure (disk
// full, permissions, ...) never leaves a half-written file.
//
#include <QByteArray>
#include <QString>

struct TrackTags
{
    bool valid = false; // true once the file's container was at least recognized
    QString title;
    QString artist;
    QString album;

    // What to do with the embedded cover picture on write. Keep (the
    // default) leaves whatever picture frame/block is already in the file
    // completely alone - readTags() never populates newCoverData itself,
    // cover art is read separately via CoverArtExtractor, so a caller that
    // only wants to edit text tags can leave this at its default and the
    // write path won't touch the picture at all.
    enum class CoverArtAction { Keep, Replace, Remove };
    CoverArtAction coverArtAction = CoverArtAction::Keep;
    QByteArray newCoverData;   // raw image bytes - only read when Replace
    QString newCoverMimeType; // e.g. "image/jpeg" or "image/png" - only read when Replace
};

namespace TagEditor {

// Reads the current tags. Fields come back empty if the tag/frame isn't
// present; `valid` is false only if the container itself couldn't be
// identified at all (so the caller can tell "no title tag" from "couldn't
// even open/parse this file").
TrackTags readTags(const QString &filePath);

// True if writeTags() can actually save changes for this file (see the
// MP4/M4A note above).
bool writeSupported(const QString &filePath);

// Writes Title/Artist/Album back to the file. An empty field clears that
// tag (rather than leaving the old value in place). Returns false and
// fills *errorMessage on failure.
bool writeTags(const QString &filePath, const TrackTags &tags, QString *errorMessage);

} // namespace TagEditor
