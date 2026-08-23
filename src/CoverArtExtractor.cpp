#include "CoverArtExtractor.h"
#include "TextDecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>

#include <algorithm>

namespace {

// ---- small big/little-endian readers (manual, to avoid any ambiguity
// around QtEndian template argument deduction) --------------------------

quint32 beUint32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}
quint32 beUint(const uchar *p, int n)
{
    quint32 v = 0;
    for (int i = 0; i < n; ++i)
        v = (v << 8) | p[i];
    return v;
}
quint64 beUint64(const uchar *p)
{
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | quint64(p[i]);
    return v;
}
quint32 leUint32(const uchar *p)
{
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

// ID3v2 "syncsafe" integer: 7 significant bits per byte.
quint32 unsyncsafe(const uchar *p)
{
    return (quint32(p[0] & 0x7F) << 21) | (quint32(p[1] & 0x7F) << 14) |
           (quint32(p[2] & 0x7F) << 7) | quint32(p[3] & 0x7F);
}

// ---- ID3v2 APIC/PIC frame body -> image bytes --------------------------

CoverArt parseApicBody(const QByteArray &body, bool isPic)
{
    CoverArt art;
    if (body.size() < 2)
        return art;

    const auto *d = reinterpret_cast<const uchar *>(body.constData());
    int i = 0;
    const quint8 encoding = d[i++];

    QString mime;
    if (isPic) { // ID3v2.2 "PIC": 3-character format code instead of a MIME string
        if (body.size() < i + 3)
            return art;
        const QString fmt = QString::fromLatin1(body.mid(i, 3)).toUpper();
        i += 3;
        mime = (fmt == QLatin1String("PNG")) ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    } else {
        const int end = body.indexOf('\0', i);
        if (end < 0)
            return art;
        mime = QString::fromLatin1(body.mid(i, end - i));
        i = end + 1;
    }

    if (i >= body.size())
        return art;
    ++i; // picture type byte, not needed here

    int imgStart;
    if (encoding == 1 || encoding == 2) {
        // UTF-16 description: terminator is 2 null bytes. Description
        // terminator width depends on encoding, per the skill notes - 1
        // byte for latin1/UTF-8, 2 for UTF-16.
        int j = i;
        bool found = false;
        while (j + 1 < body.size()) {
            if (d[j] == 0 && d[j + 1] == 0) {
                found = true;
                break;
            }
            j += 2;
        }
        if (!found)
            return art;
        imgStart = j + 2;
    } else {
        const int end = body.indexOf('\0', i);
        if (end < 0)
            return art;
        imgStart = end + 1;
    }

    if (imgStart >= body.size())
        return art;

    art.imageData = body.mid(imgStart);
    art.mimeType = mime.isEmpty() ? QStringLiteral("image/jpeg") : mime;
    art.valid = !art.imageData.isEmpty();
    return art;
}

// ---- Full ID3v2 tag (header + frames) -> cover art ----------------------
// Shared by MP3 (tag at file offset 0) and WAV (tag is the payload of a
// RIFF "id3 " chunk) - a WAV file starts with "RIFF", so checking for "ID3"
// at offset 0 fails immediately on WAV, which is why the two containers
// need to share this parser rather than one assuming MP3-only layout.

CoverArt parseId3Tag(const QByteArray &tag)
{
    CoverArt art;
    if (tag.size() < 10 || tag.mid(0, 3) != "ID3")
        return art;

    const auto *d = reinterpret_cast<const uchar *>(tag.constData());
    const quint8 major = d[3];
    const quint8 flags = d[5];
    const quint32 tagSize = unsyncsafe(d + 6);
    const qint64 tagEnd = std::min<qint64>(10 + static_cast<qint64>(tagSize), tag.size());

    qint64 pos = 10;

    if (flags & 0x40) { // extended header present - skip it
        if (pos + 4 > tag.size())
            return art;
        const quint32 extSize = (major == 4) ? unsyncsafe(d + pos) : beUint(d + pos, 4);
        // v2.4: extSize is the total extended-header size (incl. its own
        // 4-byte size field). v2.3: extSize excludes that field, so add it back.
        pos += (major == 4) ? extSize : (extSize + 4);
    }

    while (pos < tagEnd) {
        QString frameId;
        quint32 size;
        qint64 frameStart;

        if (major == 2) {
            if (pos + 6 > tagEnd || d[pos] == 0) // padding reached
                break;
            frameId = QString::fromLatin1(tag.mid(pos, 3));
            size = beUint(d + pos + 3, 3);
            frameStart = pos + 6;
        } else {
            if (pos + 10 > tagEnd || d[pos] == 0) // padding reached
                break;
            frameId = QString::fromLatin1(tag.mid(pos, 4));
            size = (major == 4) ? unsyncsafe(d + pos + 4) : beUint(d + pos + 4, 4);
            frameStart = pos + 10;
        }

        if (size == 0) {
            pos = frameStart;
            continue;
        }
        if (frameStart + static_cast<qint64>(size) > tag.size())
            break; // truncated/corrupt tag

        if (frameId == QLatin1String("APIC") || frameId == QLatin1String("PIC")) {
            const QByteArray body = tag.mid(static_cast<int>(frameStart), static_cast<int>(size));
            CoverArt found = parseApicBody(body, frameId == QLatin1String("PIC"));
            if (found.valid)
                return found;
        }

        pos = frameStart + size;
    }

    return art;
}

CoverArt extractFromMp3(QFile &file)
{
    file.seek(0);
    const QByteArray header = file.read(10);
    if (header.size() < 10 || header.mid(0, 3) != "ID3")
        return {};

    const auto *d = reinterpret_cast<const uchar *>(header.constData());
    const quint32 tagSize = unsyncsafe(d + 6);
    const qint64 total = std::min<qint64>(10 + static_cast<qint64>(tagSize), file.size());
    file.seek(0);
    const QByteArray fullTag = file.read(total);
    return parseId3Tag(fullTag);
}

CoverArt extractFromWav(QFile &file)
{
    file.seek(0);
    const QByteArray riffHeader = file.read(12);
    if (riffHeader.size() < 12 || riffHeader.mid(0, 4) != "RIFF" || riffHeader.mid(8, 4) != "WAVE")
        return {};

    const quint32 riffSize = leUint32(reinterpret_cast<const uchar *>(riffHeader.constData() + 4));
    const qint64 end = std::min<qint64>(8 + static_cast<qint64>(riffSize), file.size());

    qint64 pos = 12;
    while (pos + 8 <= end) {
        file.seek(pos);
        const QByteArray chunkHeader = file.read(8);
        if (chunkHeader.size() < 8)
            break;

        const QByteArray chunkId = chunkHeader.mid(0, 4);
        const quint32 size = leUint32(reinterpret_cast<const uchar *>(chunkHeader.constData() + 4));
        const qint64 payloadStart = pos + 8;

        if (chunkId == "id3 " || chunkId == "ID3 ") {
            file.seek(payloadStart);
            const QByteArray payload = file.read(size);
            CoverArt art = parseId3Tag(payload);
            if (art.valid)
                return art;
        }

        pos = payloadStart + size;
        if (size % 2 == 1)
            pos += 1; // RIFF chunks start at even offsets; skip the pad byte
    }

    return {};
}

// ---- FLAC METADATA_BLOCK_PICTURE ---------------------------------------

CoverArt parseFlacPicture(const QByteArray &body)
{
    CoverArt art;
    if (body.size() < 4)
        return art;
    const auto *d = reinterpret_cast<const uchar *>(body.constData());

    qint64 i = 4; // skip picture type
    if (i + 4 > body.size())
        return art;
    const quint32 mimeLen = beUint32(d + i);
    i += 4;
    if (i + mimeLen > static_cast<quint32>(body.size()))
        return art;
    const QString mime = QString::fromLatin1(body.mid(static_cast<int>(i), static_cast<int>(mimeLen)));
    i += mimeLen;

    if (i + 4 > body.size())
        return art;
    const quint32 descLen = beUint32(d + i);
    i += 4 + descLen;

    i += 16; // width, height, color depth, indexed colors: 4 bytes each, unused here
    if (i + 4 > body.size())
        return art;
    const quint32 imgLen = beUint32(d + i);
    i += 4;
    if (i + imgLen > static_cast<quint32>(body.size()))
        return art;

    art.imageData = body.mid(static_cast<int>(i), static_cast<int>(imgLen));
    art.mimeType = mime.isEmpty() ? QStringLiteral("image/jpeg") : mime;
    art.valid = !art.imageData.isEmpty();
    return art;
}

CoverArt extractFromFlac(QFile &file)
{
    file.seek(0);
    if (file.read(4) != "fLaC")
        return {};

    qint64 pos = 4;
    while (pos + 4 <= file.size()) {
        file.seek(pos);
        const QByteArray header = file.read(4);
        if (header.size() < 4)
            break;

        const uchar h0 = static_cast<uchar>(header[0]);
        const bool isLast = (h0 & 0x80) != 0;
        const int blockType = h0 & 0x7F;
        const quint32 size = beUint(reinterpret_cast<const uchar *>(header.constData()) + 1, 3);
        const qint64 blockStart = pos + 4;

        if (blockType == 6) { // PICTURE
            file.seek(blockStart);
            const QByteArray body = file.read(size);
            CoverArt art = parseFlacPicture(body);
            if (art.valid)
                return art;
        }

        pos = blockStart + size;
        if (isLast || pos > file.size())
            break;
    }

    return {};
}

// ---- MP4/M4A moov.udta.meta.ilst.covr.data ------------------------------

// Finds the first direct child atom named `name` within [start, end),
// returning its payload range. Handles the rare 64-bit extended-size atom
// header (size field == 1) so a huge 'mdat' before 'moov' doesn't desync
// the walk; we still never read mdat's payload, only its header, since
// we're not interested in it.
bool findChildAtom(QFile &file, qint64 start, qint64 end, const char *name, qint64 &outStart, qint64 &outEnd)
{
    qint64 pos = start;
    while (pos + 8 <= end) {
        file.seek(pos);
        const QByteArray header = file.read(8);
        if (header.size() < 8)
            break;

        quint64 size = beUint32(reinterpret_cast<const uchar *>(header.constData()));
        const QByteArray atomName = header.mid(4, 4);
        qint64 bodyStart = pos + 8;

        if (size == 1) { // 64-bit extended size follows immediately
            const QByteArray ext = file.read(8);
            if (ext.size() < 8)
                break;
            size = beUint64(reinterpret_cast<const uchar *>(ext.constData()));
            bodyStart = pos + 16;
        } else if (size == 0) {
            size = static_cast<quint64>(end - pos); // extends to end of parent
        }

        const qint64 bodyEnd = pos + static_cast<qint64>(size);
        if (bodyEnd <= pos || bodyEnd > end + 16) // corrupt/overflowing size guard
            break;

        if (atomName == name) {
            outStart = bodyStart;
            outEnd = bodyEnd;
            return true;
        }

        pos = bodyEnd;
    }
    return false;
}

CoverArt extractFromMp4(QFile &file)
{
    const qint64 fileSize = file.size();

    qint64 moovStart, moovEnd;
    if (!findChildAtom(file, 0, fileSize, "moov", moovStart, moovEnd))
        return {};
    qint64 udtaStart, udtaEnd;
    if (!findChildAtom(file, moovStart, moovEnd, "udta", udtaStart, udtaEnd))
        return {};
    qint64 metaStart, metaEnd;
    if (!findChildAtom(file, udtaStart, udtaEnd, "meta", metaStart, metaEnd))
        return {};
    metaStart += 4; // 'meta' has a 4-byte version/flags header before its children
    qint64 ilstStart, ilstEnd;
    if (!findChildAtom(file, metaStart, metaEnd, "ilst", ilstStart, ilstEnd))
        return {};
    qint64 covrStart, covrEnd;
    if (!findChildAtom(file, ilstStart, ilstEnd, "covr", covrStart, covrEnd))
        return {};
    qint64 dataStart, dataEnd;
    if (!findChildAtom(file, covrStart, covrEnd, "data", dataStart, dataEnd))
        return {};

    const qint64 imgStart = dataStart + 8; // 'data' atom: 4-byte type + 4-byte locale, then raw bytes
    const qint64 imgLen = dataEnd - imgStart;
    if (imgLen <= 0)
        return {};

    file.seek(imgStart);
    const QByteArray img = file.read(imgLen);

    CoverArt art;
    art.imageData = img;
    if (img.startsWith("\x89PNG"))
        art.mimeType = QStringLiteral("image/png");
    else
        art.mimeType = QStringLiteral("image/jpeg"); // covers JFIF/EXIF JPEG magic variants
    art.valid = !img.isEmpty();
    return art;
}

// ---- folder image fallback ----------------------------------------------

QString findFolderImage(const QString &audioFilePath)
{
    const QFileInfo fi(audioFilePath);
    const QDir dir = fi.dir();
    static const QStringList kNames = {"cover", "folder", "front", "album", "artwork"};
    static const QStringList kExts = {"jpg", "jpeg", "png", "webp", "bmp"};

    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &name : kNames) {
        for (const QString &ext : kExts) {
            const QString target = name + "." + ext;
            for (const QString &entry : entries) {
                if (entry.compare(target, Qt::CaseInsensitive) == 0)
                    return dir.filePath(entry);
            }
        }
    }
    return QString();
}

} // namespace

namespace CoverArtExtractor {

CoverArt extract(const QString &audioFilePath)
{
    QFile file(audioFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray magic = file.peek(12);
    CoverArt art;

    // Try every relevant container by content, not by extension - files
    // get misnamed, and each check here is a cheap magic-byte test before
    // any real parsing happens.
    if (magic.startsWith("ID3")) {
        art = extractFromMp3(file);
        if (art.valid)
            return art;
    }
    if (magic.size() >= 12 && magic.mid(0, 4) == "RIFF" && magic.mid(8, 4) == "WAVE") {
        art = extractFromWav(file);
        if (art.valid)
            return art;
    }
    if (magic.startsWith("fLaC")) {
        art = extractFromFlac(file);
        if (art.valid)
            return art;
    }
    // MP4/M4A has no single fixed magic at offset 0 - just attempt the atom
    // walk; it fails fast (no 'moov' atom) on files that aren't MP4-family.
    art = extractFromMp4(file);
    if (art.valid)
        return art;

    file.close();

    const QString folderImage = findFolderImage(audioFilePath);
    if (!folderImage.isEmpty()) {
        QFile img(folderImage);
        if (img.open(QIODevice::ReadOnly)) {
            CoverArt fallback;
            fallback.imageData = img.readAll();
            fallback.mimeType = folderImage.endsWith(".png", Qt::CaseInsensitive)
                                     ? QStringLiteral("image/png")
                                     : QStringLiteral("image/jpeg");
            fallback.valid = !fallback.imageData.isEmpty();
            return fallback;
        }
    }

    return {};
}

bool imageFormatSupported(const QString &mimeType)
{
    QByteArray fmt;
    if (mimeType.contains("png"))
        fmt = "png";
    else if (mimeType.contains("jpeg") || mimeType.contains("jpg"))
        fmt = "jpg";
    else if (mimeType.contains("webp"))
        fmt = "webp";
    else if (mimeType.contains("bmp"))
        fmt = "bmp";
    else
        return true; // unrecognized mime: don't block on our own uncertainty

    const auto formats = QImageReader::supportedImageFormats();
    if (formats.contains(fmt))
        return true;
    if (fmt == "jpg" && formats.contains("jpeg"))
        return true;
    return false;
}

} // namespace CoverArtExtractor
