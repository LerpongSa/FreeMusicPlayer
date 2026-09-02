#include "TagEditor.h"
#include "TextDecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QVector>
#include <QPair>

#include <algorithm>

namespace {

// ---- small big/little-endian readers/writers (mirrors CoverArtExtractor.cpp;
// duplicated rather than shared across translation units, matching this
// project's existing style of keeping each parser file self-contained) -----

quint32 beUint(const uchar *p, int n)
{
    quint32 v = 0;
    for (int i = 0; i < n; ++i)
        v = (v << 8) | p[i];
    return v;
}

// Big-endian encode of the low `n` bytes of `v` (n <= 4).
QByteArray beBytes(quint32 v, int n)
{
    QByteArray out(n, '\0');
    for (int i = 0; i < n; ++i)
        out[n - 1 - i] = char((v >> (8 * i)) & 0xFF);
    return out;
}

quint32 leUint32(const uchar *p)
{
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

QByteArray leUint32Bytes(quint32 v)
{
    QByteArray out(4, '\0');
    out[0] = char(v & 0xFF);
    out[1] = char((v >> 8) & 0xFF);
    out[2] = char((v >> 16) & 0xFF);
    out[3] = char((v >> 24) & 0xFF);
    return out;
}

// ID3v2 "syncsafe" integer: 7 significant bits per byte. Only the ID3v2 TAG
// header's size field is syncsafe in v2.3 - frame sizes are plain big-endian
// in v2.3 (they only became syncsafe too in v2.4).
quint32 unsyncsafe(const uchar *p)
{
    return (quint32(p[0] & 0x7F) << 21) | (quint32(p[1] & 0x7F) << 14) |
           (quint32(p[2] & 0x7F) << 7) | quint32(p[3] & 0x7F);
}

QByteArray syncsafe(quint32 v)
{
    QByteArray out(4, '\0');
    out[0] = char((v >> 21) & 0x7F);
    out[1] = char((v >> 14) & 0x7F);
    out[2] = char((v >> 7) & 0x7F);
    out[3] = char(v & 0x7F);
    return out;
}

bool atomicWrite(const QString &filePath, const QByteArray &data, QString *errorMessage)
{
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = out.errorString();
        return false;
    }
    if (out.write(data) != data.size()) {
        if (errorMessage)
            *errorMessage = out.errorString();
        out.cancelWriting();
        return false;
    }
    if (!out.commit()) {
        if (errorMessage)
            *errorMessage = out.errorString();
        return false;
    }
    return true;
}

// ---- ID3v2 text-frame codec ---------------------------------------------

// Encoding byte 0 nominally means ISO-8859-1, but real-world Thai taggers
// routinely wrote raw TIS-620/CP874 bytes under that same encoding byte
// (ID3v2 has no dedicated Thai encoding), which renders as garbage like
// "µÑã¯AO" if blindly decoded as Latin-1 - this is exactly what
// TextDecoder::decodeId3String()'s sniffing (ASCII passthrough, else
// strict UTF-8, else a Thai-byte-run heuristic, else genuine Latin-1) is
// for; see TextDecoder.h for the detection rationale. Delegating here
// instead of decoding encoding-0 as plain Latin-1 directly is the fix for
// mojibake track titles on legacy Thai-tagged MP3/WAV files.
QString decodeId3Text(const QByteArray &body)
{
    if (body.isEmpty())
        return QString();
    const quint8 encoding = static_cast<quint8>(body.at(0));
    const QByteArray text = body.mid(1);

    QString s = TextDecoder::decodeId3String(text, encoding);

    // Strip a trailing terminator some writers include.
    while (!s.isEmpty() && s.back() == QChar(u'\0'))
        s.chop(1);
    return s;
}

// Prefer plain Latin-1 (encoding 0, no BOM) when the text round-trips
// losslessly - smaller, and read correctly by every ID3v2 parser ever
// written. Fall back to UTF-16LE with BOM (encoding 1, valid since v2.3)
// for anything else, Thai text included.
QByteArray encodeId3Text(const QString &text)
{
    if (QString::fromLatin1(text.toLatin1()) == text) {
        QByteArray out;
        out.append(char(0));
        out.append(text.toLatin1());
        return out;
    }

    QByteArray out;
    out.append(char(1));
    out.append(char(0xFF));
    out.append(char(0xFE));
    for (const QChar &c : text) {
        const ushort u = c.unicode();
        out.append(char(u & 0xFF));
        out.append(char((u >> 8) & 0xFF));
    }
    return out;
}

// Builds an APIC (attached picture) frame body: encoding byte (0 =
// ISO-8859-1, used here since the description is always left empty),
// null-terminated ASCII MIME type, picture-type byte (3 = "Cover (front)",
// the conventional choice every player looks for first), an empty
// description (its terminator width depends on the encoding byte - 1 byte
// for encoding 0, which is what's used here), then the raw image bytes.
// Mirrors exactly what CoverArtExtractor::parseApicBody() expects to read
// back (see CoverArtExtractor.cpp) - the two are kept in lockstep.
QByteArray encodeApicFrame(const QString &mimeType, const QByteArray &imageData)
{
    QByteArray body;
    body.append(char(0));            // text encoding: ISO-8859-1
    body.append(mimeType.toLatin1());
    body.append(char(0));            // MIME type null terminator
    body.append(char(3));            // picture type: Cover (front)
    body.append(char(0));            // empty description + its 1-byte terminator
    body.append(imageData);
    return body;
}

// ---- reading Title/Artist/Album out of a full ID3v2 tag (header+frames) --
// Shared by MP3 (tag at file offset 0) and WAV (tag is the payload of a
// RIFF "id3 " chunk) - same reasoning as CoverArtExtractor's parseId3Tag.
// Understands both v2.2 (3-byte frame ids: TT2/TP1/TAL) and v2.3/2.4
// (4-byte ids: TIT2/TPE1/TALB) for reading.

TrackTags readTagsFromId3(const QByteArray &tag)
{
    TrackTags t;
    if (tag.size() < 10 || tag.mid(0, 3) != "ID3")
        return t;

    const auto *d = reinterpret_cast<const uchar *>(tag.constData());
    const quint8 major = d[3];
    const quint8 flags = d[5];
    const quint32 tagSize = unsyncsafe(d + 6);
    const qint64 tagEnd = std::min<qint64>(10 + qint64(tagSize), tag.size());
    qint64 pos = 10;

    if (flags & 0x40) {
        if (pos + 4 > tag.size()) {
            t.valid = true;
            return t;
        }
        const quint32 extSize = (major == 4) ? unsyncsafe(d + pos) : beUint(d + pos, 4);
        pos += (major == 4) ? extSize : (extSize + 4);
    }

    while (pos < tagEnd) {
        QByteArray frameId;
        quint32 size;
        qint64 frameStart;

        if (major == 2) {
            if (pos + 6 > tagEnd || d[pos] == 0)
                break;
            frameId = tag.mid(static_cast<int>(pos), 3);
            size = beUint(d + pos + 3, 3);
            frameStart = pos + 6;
        } else {
            if (pos + 10 > tagEnd || d[pos] == 0)
                break;
            frameId = tag.mid(static_cast<int>(pos), 4);
            size = (major == 4) ? unsyncsafe(d + pos + 4) : beUint(d + pos + 4, 4);
            frameStart = pos + 10;
        }

        if (size > 0) {
            if (frameStart + qint64(size) > tag.size())
                break; // truncated/corrupt tag
            const QByteArray body = tag.mid(static_cast<int>(frameStart), static_cast<int>(size));
            if (frameId == "TIT2" || frameId == "TT2")
                t.title = decodeId3Text(body);
            else if (frameId == "TPE1" || frameId == "TP1")
                t.artist = decodeId3Text(body);
            else if (frameId == "TALB" || frameId == "TAL")
                t.album = decodeId3Text(body);
        }

        pos = frameStart + size;
    }

    t.valid = true;
    return t;
}

// ---- rebuilding an ID3v2.3 tag, preserving every frame this editor doesn't
// touch (other text frames, APIC cover art, everything) -------------------

struct RawFrame
{
    QByteArray id; // always 4 bytes
    QByteArray body;
};

// Parses an existing tag's frames for preservation. Only understands
// v2.3/2.4 (4-byte ids/sizes) - a v2.2 tag (3-byte ids) can't be
// byte-compatibly re-emitted into the v2.3 container this always writes, so
// for those `frames` comes back empty (the rare old-tagged file loses any
// non text frames, e.g. a legacy "PIC" cover, when its tags are edited here;
// ID3v2.2 predates 2000 and is essentially extinct in practice).
void parseId3FramesForRewrite(const QByteArray &tag, QVector<RawFrame> &frames)
{
    frames.clear();
    if (tag.size() < 10 || tag.mid(0, 3) != "ID3")
        return; // no existing tag - nothing to preserve, not an error

    const auto *d = reinterpret_cast<const uchar *>(tag.constData());
    const quint8 major = d[3];
    const quint8 flags = d[5];
    if (major < 3)
        return; // v2.2 - see comment above

    const quint32 tagSize = unsyncsafe(d + 6);
    const qint64 tagEnd = std::min<qint64>(10 + qint64(tagSize), tag.size());
    qint64 pos = 10;

    if (flags & 0x40) {
        if (pos + 4 > tag.size())
            return;
        const quint32 extSize = (major == 4) ? unsyncsafe(d + pos) : beUint(d + pos, 4);
        pos += (major == 4) ? extSize : (extSize + 4);
    }

    while (pos + 10 <= tagEnd) {
        if (d[pos] == 0)
            break; // padding reached
        RawFrame f;
        f.id = tag.mid(static_cast<int>(pos), 4);
        const quint32 size = (major == 4) ? unsyncsafe(d + pos + 4) : beUint(d + pos + 4, 4);
        const qint64 frameStart = pos + 10;
        if (frameStart + qint64(size) > tag.size())
            break;
        f.body = tag.mid(static_cast<int>(frameStart), static_cast<int>(size));
        frames.append(f);
        pos = frameStart + size;
    }
}

QByteArray buildId3Tag(const QVector<RawFrame> &frames)
{
    QByteArray body;
    for (const RawFrame &f : frames) {
        if (f.id.size() != 4 || f.body.isEmpty())
            continue; // an empty body here means the field was cleared
        body.append(f.id);
        body.append(beBytes(quint32(f.body.size()), 4)); // frame size: plain, not syncsafe
        body.append(char(0));
        body.append(char(0)); // frame flags
        body.append(f.body);
    }

    QByteArray tag;
    tag.append("ID3", 3);
    tag.append(char(3)); // major version 2.3
    tag.append(char(0)); // revision
    tag.append(char(0)); // flags
    tag.append(syncsafe(quint32(body.size()))); // tag header size IS syncsafe
    tag.append(body);
    return tag;
}

QByteArray rebuildId3Tag(const QByteArray &existingTag, const TrackTags &tags)
{
    QVector<RawFrame> kept;
    parseId3FramesForRewrite(existingTag, kept);
    const bool touchCover = tags.coverArtAction != TrackTags::CoverArtAction::Keep;
    for (int i = kept.size() - 1; i >= 0; --i) {
        const QByteArray &id = kept.at(i).id;
        if (id == "TIT2" || id == "TPE1" || id == "TALB")
            kept.removeAt(i);
        else if (touchCover && id == "APIC")
            kept.removeAt(i); // Keep leaves any existing APIC(s) exactly as they were
    }

    auto addIfNonEmpty = [&](const char *id, const QString &value) {
        if (value.isEmpty())
            return;
        RawFrame f;
        f.id = QByteArray(id);
        f.body = encodeId3Text(value);
        kept.append(f);
    };
    addIfNonEmpty("TIT2", tags.title);
    addIfNonEmpty("TPE1", tags.artist);
    addIfNonEmpty("TALB", tags.album);

    if (tags.coverArtAction == TrackTags::CoverArtAction::Replace) {
        RawFrame f;
        f.id = "APIC";
        f.body = encodeApicFrame(tags.newCoverMimeType, tags.newCoverData);
        kept.append(f);
    }

    return buildId3Tag(kept);
}

// ---- MP3 -----------------------------------------------------------------

TrackTags readMp3Tags(QFile &file)
{
    file.seek(0);
    const QByteArray header = file.read(10);
    if (header.size() < 10 || header.mid(0, 3) != "ID3")
        return TrackTags();
    const auto *d = reinterpret_cast<const uchar *>(header.constData());
    const quint32 tagSize = unsyncsafe(d + 6);
    const qint64 total = std::min<qint64>(10 + qint64(tagSize), file.size());
    file.seek(0);
    return readTagsFromId3(file.read(total));
}

bool writeMp3Tags(const QString &filePath, const TrackTags &tags, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray whole = file.readAll();
    file.close();

    qint64 audioStart = 0;
    QByteArray existingTag;
    if (whole.size() >= 10 && whole.mid(0, 3) == "ID3") {
        const auto *d = reinterpret_cast<const uchar *>(whole.constData());
        const quint32 tagSize = unsyncsafe(d + 6);
        audioStart = std::min<qint64>(10 + qint64(tagSize), whole.size());
        existingTag = whole.left(static_cast<int>(audioStart));
    }

    const QByteArray newTag = rebuildId3Tag(existingTag, tags);
    const QByteArray out = newTag + whole.mid(static_cast<int>(audioStart));
    return atomicWrite(filePath, out, errorMessage);
}

// ---- WAV: the same ID3v2 tag, carried inside a RIFF "id3 " chunk ---------

TrackTags readWavTags(QFile &file)
{
    file.seek(0);
    const QByteArray riffHeader = file.read(12);
    if (riffHeader.size() < 12 || riffHeader.mid(0, 4) != "RIFF" || riffHeader.mid(8, 4) != "WAVE")
        return TrackTags();

    const quint32 riffSize = leUint32(reinterpret_cast<const uchar *>(riffHeader.constData() + 4));
    const qint64 end = std::min<qint64>(8 + qint64(riffSize), file.size());
    qint64 pos = 12;

    while (pos + 8 <= end) {
        file.seek(pos);
        const QByteArray chunkHeader = file.read(8);
        if (chunkHeader.size() < 8)
            break;
        const QByteArray chunkId = chunkHeader.mid(0, 4);
        const quint32 size = leUint32(reinterpret_cast<const uchar *>(chunkHeader.constData() + 4));
        const qint64 payloadStart = pos + 8;
        if (payloadStart + qint64(size) > file.size())
            break;

        if (chunkId == "id3 " || chunkId == "ID3 ") {
            file.seek(payloadStart);
            TrackTags t = readTagsFromId3(file.read(size));
            t.valid = true;
            return t;
        }

        pos = payloadStart + size;
        if (size % 2 == 1)
            pos += 1;
    }

    TrackTags t;
    t.valid = true; // recognized as WAV, just has no tag chunk yet
    return t;
}

bool writeWavTags(const QString &filePath, const TrackTags &tags, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray whole = file.readAll();
    file.close();

    if (whole.size() < 12 || whole.mid(0, 4) != "RIFF" || whole.mid(8, 4) != "WAVE") {
        if (errorMessage)
            *errorMessage = QStringLiteral("Not a WAV file.");
        return false;
    }

    struct Chunk { QByteArray id; QByteArray data; };
    QVector<Chunk> chunks;
    QByteArray existingId3;

    const quint32 riffSize = leUint32(reinterpret_cast<const uchar *>(whole.constData() + 4));
    const qint64 end = std::min<qint64>(8 + qint64(riffSize), whole.size());
    qint64 pos = 12;

    while (pos + 8 <= end) {
        const auto *d = reinterpret_cast<const uchar *>(whole.constData());
        const QByteArray chunkId = whole.mid(static_cast<int>(pos), 4);
        const quint32 size = leUint32(d + pos + 4);
        const qint64 payloadStart = pos + 8;
        if (payloadStart + qint64(size) > whole.size())
            break; // corrupt/truncated - stop, keep whatever was parsed so far

        const QByteArray payload = whole.mid(static_cast<int>(payloadStart), static_cast<int>(size));
        if (chunkId == "id3 " || chunkId == "ID3 ")
            existingId3 = payload;
        else
            chunks.append({chunkId, payload});

        pos = payloadStart + size;
        if (size % 2 == 1)
            pos += 1;
    }

    const QByteArray newTag = rebuildId3Tag(existingId3, tags);

    QByteArray body; // everything after "WAVE"
    for (const Chunk &c : chunks) {
        body.append(c.id);
        body.append(leUint32Bytes(quint32(c.data.size())));
        body.append(c.data);
        if (c.data.size() % 2 == 1)
            body.append(char(0));
    }
    body.append("id3 ", 4);
    body.append(leUint32Bytes(quint32(newTag.size())));
    body.append(newTag);
    if (newTag.size() % 2 == 1)
        body.append(char(0));

    QByteArray out;
    out.append("RIFF", 4);
    out.append(leUint32Bytes(quint32(4 + body.size()))); // "WAVE" + body
    out.append("WAVE", 4);
    out.append(body);

    return atomicWrite(filePath, out, errorMessage);
}

// ---- FLAC: VORBIS_COMMENT metadata block ---------------------------------

void extractVorbisFields(const QByteArray &body, QByteArray &vendorOut, QVector<QPair<QString, QString>> &others)
{
    if (body.size() < 8)
        return;
    const auto *d = reinterpret_cast<const uchar *>(body.constData());
    const quint32 vendorLen = leUint32(d);
    qint64 i = 4;
    if (i + qint64(vendorLen) > body.size())
        return;
    vendorOut = body.mid(static_cast<int>(i), static_cast<int>(vendorLen));
    i += vendorLen;

    if (i + 4 > body.size())
        return;
    const quint32 count = leUint32(reinterpret_cast<const uchar *>(body.constData() + i));
    i += 4;

    for (quint32 c = 0; c < count && i + 4 <= body.size(); ++c) {
        const quint32 len = leUint32(reinterpret_cast<const uchar *>(body.constData() + i));
        i += 4;
        if (i + qint64(len) > body.size())
            break;
        const QString entry = QString::fromUtf8(body.mid(static_cast<int>(i), static_cast<int>(len)));
        i += len;

        const int eq = entry.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = entry.left(eq).toUpper();
        if (key == QLatin1String("TITLE") || key == QLatin1String("ARTIST") || key == QLatin1String("ALBUM"))
            continue; // dropped here - the caller appends fresh values for these
        others.append({entry.left(eq), entry.mid(eq + 1)});
    }
}

void parseVorbisCommentIntoTags(const QByteArray &body, TrackTags &t)
{
    if (body.size() < 8)
        return;
    const auto *d = reinterpret_cast<const uchar *>(body.constData());
    const quint32 vendorLen = leUint32(d);
    qint64 i = 4 + qint64(vendorLen);
    if (i + 4 > body.size())
        return;
    const quint32 count = leUint32(reinterpret_cast<const uchar *>(body.constData() + i));
    i += 4;

    for (quint32 c = 0; c < count && i + 4 <= body.size(); ++c) {
        const quint32 len = leUint32(reinterpret_cast<const uchar *>(body.constData() + i));
        i += 4;
        if (i + qint64(len) > body.size())
            break;
        const QString entry = QString::fromUtf8(body.mid(static_cast<int>(i), static_cast<int>(len)));
        i += len;

        const int eq = entry.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = entry.left(eq).toUpper();
        const QString value = entry.mid(eq + 1);
        if (key == QLatin1String("TITLE"))
            t.title = value;
        else if (key == QLatin1String("ARTIST"))
            t.artist = value;
        else if (key == QLatin1String("ALBUM"))
            t.album = value;
    }
}

QByteArray buildVorbisComment(const QByteArray &vendor, const QVector<QPair<QString, QString>> &others,
                               const TrackTags &tags)
{
    QVector<QByteArray> comments;
    for (const auto &kv : others)
        comments.append((kv.first + QLatin1Char('=') + kv.second).toUtf8());
    if (!tags.title.isEmpty())
        comments.append(("TITLE=" + tags.title).toUtf8());
    if (!tags.artist.isEmpty())
        comments.append(("ARTIST=" + tags.artist).toUtf8());
    if (!tags.album.isEmpty())
        comments.append(("ALBUM=" + tags.album).toUtf8());

    QByteArray out;
    out.append(leUint32Bytes(quint32(vendor.size())));
    out.append(vendor);
    out.append(leUint32Bytes(quint32(comments.size())));
    for (const QByteArray &c : comments) {
        out.append(leUint32Bytes(quint32(c.size())));
        out.append(c);
    }
    return out;
}

// Builds a METADATA_BLOCK_PICTURE body (FLAC block type 6): 4-byte
// big-endian picture type (3 = "Cover (front)"), 4-byte mime length + mime
// bytes, 4-byte description length + description bytes (left empty here),
// then width/height/color-depth/indexed-colors (4 bytes each - decoded from
// the image itself when possible, since the spec allows but doesn't require
// 0/unknown), and finally a 4-byte image length + the raw image bytes.
// Mirrors exactly what CoverArtExtractor::parseFlacPicture() expects to
// read back (see CoverArtExtractor.cpp) - the two are kept in lockstep.
QByteArray buildFlacPictureBlock(const QByteArray &imageData, const QString &mimeType)
{
    QImage img;
    img.loadFromData(imageData);
    const quint32 width = img.isNull() ? 0 : quint32(img.width());
    const quint32 height = img.isNull() ? 0 : quint32(img.height());
    const quint32 depth = img.isNull() ? 0 : quint32(img.depth());
    const quint32 indexedColors = (!img.isNull() && img.colorCount() > 0) ? quint32(img.colorCount()) : 0;
    const QByteArray mimeBytes = mimeType.toLatin1();

    QByteArray out;
    out.append(beBytes(3, 4)); // picture type: Cover (front)
    out.append(beBytes(quint32(mimeBytes.size()), 4));
    out.append(mimeBytes);
    out.append(beBytes(0, 4)); // description length: empty
    out.append(beBytes(width, 4));
    out.append(beBytes(height, 4));
    out.append(beBytes(depth, 4));
    out.append(beBytes(indexedColors, 4));
    out.append(beBytes(quint32(imageData.size()), 4));
    out.append(imageData);
    return out;
}

TrackTags readFlacTags(QFile &file)
{
    TrackTags t;
    file.seek(0);
    if (file.read(4) != "fLaC")
        return t;

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

        if (blockType == 4) {
            file.seek(blockStart);
            parseVorbisCommentIntoTags(file.read(size), t);
            t.valid = true;
            return t;
        }

        pos = blockStart + size;
        if (isLast || pos > file.size())
            break;
    }

    t.valid = true; // recognized as FLAC, just has no comment block yet
    return t;
}

bool writeFlacTags(const QString &filePath, const TrackTags &tags, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray whole = file.readAll();
    file.close();

    if (!whole.startsWith("fLaC")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Not a FLAC file.");
        return false;
    }

    struct Block { int type; QByteArray body; };
    QVector<Block> blocks;
    qint64 pos = 4;
    qint64 audioStart = -1;

    while (pos + 4 <= whole.size()) {
        const auto *d = reinterpret_cast<const uchar *>(whole.constData());
        const uchar h0 = d[pos];
        const bool isLast = (h0 & 0x80) != 0;
        const int blockType = h0 & 0x7F;
        const quint32 size = beUint(d + pos + 1, 3);
        const qint64 blockStart = pos + 4;
        if (blockStart + qint64(size) > whole.size()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Corrupt or truncated FLAC metadata.");
            return false;
        }
        blocks.append({blockType, whole.mid(static_cast<int>(blockStart), static_cast<int>(size))});
        pos = blockStart + size;
        if (isLast) {
            audioStart = pos;
            break;
        }
    }

    if (audioStart < 0 || blocks.isEmpty() || blocks.first().type != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Couldn't find FLAC metadata in this file.");
        return false;
    }

    QByteArray vendor = "FreeMusicPlayer 1.0.0";
    QVector<QPair<QString, QString>> otherComments;
    const bool touchCover = tags.coverArtAction != TrackTags::CoverArtAction::Keep;
    for (int idx = 1; idx < blocks.size();) {
        if (blocks.at(idx).type == 4) {
            extractVorbisFields(blocks.at(idx).body, vendor, otherComments);
            blocks.removeAt(idx);
        } else if (touchCover && blocks.at(idx).type == 6) {
            blocks.removeAt(idx); // Keep leaves any existing PICTURE block(s) exactly as they were
        } else {
            ++idx;
        }
    }

    QVector<Block> finalBlocks;
    finalBlocks.append(blocks.first()); // STREAMINFO must stay first
    finalBlocks.append({4, buildVorbisComment(vendor, otherComments, tags)});
    if (tags.coverArtAction == TrackTags::CoverArtAction::Replace)
        finalBlocks.append({6, buildFlacPictureBlock(tags.newCoverData, tags.newCoverMimeType)});
    for (int idx = 1; idx < blocks.size(); ++idx)
        finalBlocks.append(blocks.at(idx));

    QByteArray out;
    out.append("fLaC", 4);
    for (int idx = 0; idx < finalBlocks.size(); ++idx) {
        const bool isLast = (idx == finalBlocks.size() - 1);
        const Block &b = finalBlocks.at(idx);
        const quint8 h0 = quint8(b.type & 0x7F) | (isLast ? 0x80 : 0x00);
        out.append(char(h0));
        out.append(beBytes(quint32(b.body.size()), 3));
        out.append(b.body);
    }
    out.append(whole.mid(static_cast<int>(audioStart)));

    return atomicWrite(filePath, out, errorMessage);
}

// ---- MP4/M4A: read-only ---------------------------------------------------
// Mirrors CoverArtExtractor's atom walk. findChildAtom() is duplicated here
// (rather than shared) since the original is private to that translation
// unit - see CoverArtExtractor.cpp for fuller comments on the 64-bit
// extended atom size edge case.

bool findChildAtom(QFile &file, qint64 start, qint64 end, const char *name, qint64 &outStart, qint64 &outEnd)
{
    qint64 pos = start;
    while (pos + 8 <= end) {
        file.seek(pos);
        const QByteArray header = file.read(8);
        if (header.size() < 8)
            break;

        quint64 size = beUint(reinterpret_cast<const uchar *>(header.constData()), 4);
        const QByteArray atomName = header.mid(4, 4);
        qint64 bodyStart = pos + 8;

        if (size == 1) {
            const QByteArray ext = file.read(8);
            if (ext.size() < 8)
                break;
            const auto *e = reinterpret_cast<const uchar *>(ext.constData());
            size = (quint64(beUint(e, 4)) << 32) | quint64(beUint(e + 4, 4));
            bodyStart = pos + 16;
        } else if (size == 0) {
            size = quint64(end - pos);
        }

        const qint64 bodyEnd = pos + qint64(size);
        if (bodyEnd <= pos || bodyEnd > end + 16)
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

QString readMp4TextAtom(QFile &file, qint64 ilstStart, qint64 ilstEnd, const char *atomName)
{
    qint64 aStart, aEnd;
    if (!findChildAtom(file, ilstStart, ilstEnd, atomName, aStart, aEnd))
        return QString();
    qint64 dataStart, dataEnd;
    if (!findChildAtom(file, aStart, aEnd, "data", dataStart, dataEnd))
        return QString();

    const qint64 textStart = dataStart + 8; // 4-byte type indicator + 4-byte locale
    const qint64 textLen = dataEnd - textStart;
    if (textLen <= 0)
        return QString();

    file.seek(textStart);
    return QString::fromUtf8(file.read(textLen));
}

TrackTags readMp4Tags(QFile &file)
{
    TrackTags t;
    const qint64 fileSize = file.size();

    qint64 moovStart, moovEnd;
    if (!findChildAtom(file, 0, fileSize, "moov", moovStart, moovEnd))
        return t;
    t.valid = true;

    qint64 udtaStart, udtaEnd;
    if (!findChildAtom(file, moovStart, moovEnd, "udta", udtaStart, udtaEnd))
        return t;
    qint64 metaStart, metaEnd;
    if (!findChildAtom(file, udtaStart, udtaEnd, "meta", metaStart, metaEnd))
        return t;
    metaStart += 4; // 'meta' has a 4-byte version/flags header before its children
    qint64 ilstStart, ilstEnd;
    if (!findChildAtom(file, metaStart, metaEnd, "ilst", ilstStart, ilstEnd))
        return t;

    // "\xA9nam"/"\xA9ART"/"\xA9alb": the leading byte is the raw 0xA9 (copyright
    // sign) byte iTunes uses for these atom names, not a printable character.
    t.title = readMp4TextAtom(file, ilstStart, ilstEnd, "\xA9nam");
    t.artist = readMp4TextAtom(file, ilstStart, ilstEnd, "\xA9ART");
    t.album = readMp4TextAtom(file, ilstStart, ilstEnd, "\xA9alb");
    return t;
}

// ---- container detection (content first, extension as fallback) ---------

enum class Container { Unknown, Mp3, Wav, Flac, Mp4 };

Container detectContainer(const QString &filePath)
{
    QByteArray magic;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly))
        magic = file.peek(12);

    if (magic.startsWith("ID3"))
        return Container::Mp3;
    if (magic.size() >= 12 && magic.mid(0, 4) == "RIFF" && magic.mid(8, 4) == "WAVE")
        return Container::Wav;
    if (magic.startsWith("fLaC"))
        return Container::Flac;

    // No recognizable tag/container magic at the very start - e.g. an MP3
    // with no ID3v2 tag yet, or any MP4/M4A (which has no fixed magic at
    // offset 0 at all, just an 'ftyp' atom a few bytes in). Fall back to the
    // file extension, same as how the player already decides what to try
    // decoding for playback.
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QLatin1String("mp3"))
        return Container::Mp3;
    if (ext == QLatin1String("wav"))
        return Container::Wav;
    if (ext == QLatin1String("flac"))
        return Container::Flac;
    if (ext == QLatin1String("m4a") || ext == QLatin1String("mp4") || ext == QLatin1String("aac"))
        return Container::Mp4;
    return Container::Unknown;
}

} // namespace

namespace TagEditor {

TrackTags readTags(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return TrackTags();

    switch (detectContainer(filePath)) {
    case Container::Mp3:
        return readMp3Tags(file);
    case Container::Wav:
        return readWavTags(file);
    case Container::Flac:
        return readFlacTags(file);
    case Container::Mp4:
        return readMp4Tags(file);
    default:
        return TrackTags();
    }
}

bool writeSupported(const QString &filePath)
{
    switch (detectContainer(filePath)) {
    case Container::Mp3:
    case Container::Wav:
    case Container::Flac:
        return true;
    default:
        return false;
    }
}

bool writeTags(const QString &filePath, const TrackTags &tags, QString *errorMessage)
{
    switch (detectContainer(filePath)) {
    case Container::Mp3:
        return writeMp3Tags(filePath, tags, errorMessage);
    case Container::Wav:
        return writeWavTags(filePath, tags, errorMessage);
    case Container::Flac:
        return writeFlacTags(filePath, tags, errorMessage);
    default:
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Editing tags in this file type isn't supported yet "
                "(only MP3, FLAC, and WAV can be saved).");
        return false;
    }
}

} // namespace TagEditor
