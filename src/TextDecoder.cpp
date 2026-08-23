#include "TextDecoder.h"

#include <QStringDecoder>
#include <QStringConverter>

namespace TextDecoder {

bool looksLikeThaiLegacy(const QByteArray &raw)
{
    int run = 0;
    for (unsigned char b : raw) {
        if (b >= 0xA0) {
            if (++run >= 2)
                return true;
        } else {
            run = 0;
        }
    }
    return false;
}

QString decodeTis620(const QByteArray &raw)
{
    QString out;
    out.reserve(raw.size());
    for (unsigned char b : raw) {
        if (b < 0x80) {
            out.append(QChar(b));
        } else if ((b >= 0xA1 && b <= 0xDA) || (b >= 0xDF && b <= 0xFB)) {
            out.append(QChar(static_cast<char16_t>(0x0E00 + (b - 0xA0))));
        } else {
            out.append(QChar('?'));
        }
    }
    return out;
}

QString decodeLegacyOrUtf8(const QByteArray &raw)
{
    if (raw.isEmpty())
        return QString();

    bool isAscii = true;
    for (unsigned char b : raw) {
        if (b >= 0x80) {
            isAscii = false;
            break;
        }
    }
    if (isAscii)
        return QString::fromLatin1(raw);

    QStringDecoder utf8Decoder(QStringDecoder::Utf8);
    const QString asUtf8 = utf8Decoder(raw);
    if (!utf8Decoder.hasError())
        return asUtf8;

    if (looksLikeThaiLegacy(raw))
        return decodeTis620(raw);

    return QString::fromLatin1(raw);
}

QString decodeId3String(const QByteArray &raw, quint8 encodingByte)
{
    switch (encodingByte) {
    case 1: { // UTF-16 with BOM
        QStringDecoder decoder(QStringDecoder::Utf16);
        return decoder(raw);
    }
    case 2: { // UTF-16BE, no BOM (ID3v2.4 only)
        QStringDecoder decoder(QStringDecoder::Utf16BE);
        return decoder(raw);
    }
    case 3: { // UTF-8 (ID3v2.4 only)
        QStringDecoder decoder(QStringDecoder::Utf8);
        QString s = decoder(raw);
        return decoder.hasError() ? QString::fromLatin1(raw) : s;
    }
    case 0:
    default:
        return decodeLegacyOrUtf8(raw);
    }
}

} // namespace TextDecoder
