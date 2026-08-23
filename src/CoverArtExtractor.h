#pragma once
//
// Embedded cover-art extraction across the containers this player supports,
// plus a folder-image fallback. Every container is probed by content
// (magic bytes), never by file extension, because files get misnamed.
// Parsing logic (ID3v2.2/.3/.4 APIC/PIC, the WAV "id3 " RIFF chunk, FLAC
// METADATA_BLOCK_PICTURE, and the MP4 covr atom) was validated against
// synthetic fixtures in Python before being ported here - see the project
// notes for the reference script and the specific edge cases it covers
// (syncsafe vs plain frame sizes, the ID3v2.3 extended header, UTF-16
// description terminators, RIFF odd-chunk padding, MP4 64-bit atom sizes).
//
#include <QString>
#include <QByteArray>

struct CoverArt
{
    bool valid = false;
    QByteArray imageData;
    QString mimeType; // e.g. "image/jpeg", "image/png"
};

namespace CoverArtExtractor {

// Tries the file's own embedded artwork first (format sniffed from
// content), then a cover/folder/front/album/artwork.{jpg,png,webp,bmp}
// image beside the track. Returns an invalid CoverArt if nothing is found.
CoverArt extract(const QString &audioFilePath);

// True if decoding an image of this mime type would actually succeed with
// the Qt image plugins available at runtime - lets the UI tell "no artwork
// in this file" apart from "artwork found, but qjpeg.dll/qwebp.dll is
// missing", which otherwise look identical to the user.
bool imageFormatSupported(const QString &mimeType);

} // namespace CoverArtExtractor
