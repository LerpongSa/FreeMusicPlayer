#pragma once
//
// Text decoding for legacy tag fields (ID3v2 "encoding 0" strings and
// similar). ID3 encoding byte 0 nominally means ISO-8859-1, but real-world
// taggers frequently wrote whatever the local code page was - Thai files
// commonly carry TIS-620/CP874 bytes, which render as garbage like
// "ÅÐÁÕ¹" if blindly decoded as latin1. This sniffs instead of trusting
// the declared encoding.
//
// Verified against a Python reference before porting: correctly detects
// genuine Thai text AND does not mis-detect accented Latin-1 names like
// "Björk", "Sigur Rós", "Motörhead" as Thai (those are false positives a
// naive "any byte >= 0xA0" check produces).
//
#include <QString>
#include <QByteArray>

namespace TextDecoder {

// Encoding byte as used by ID3v2 APIC/text frames: 0=latin1/legacy sniffed,
// 1=UTF-16 with BOM, 2=UTF-16BE no BOM (v2.4 only), 3=UTF-8 (v2.4 only).
QString decodeId3String(const QByteArray &raw, quint8 encodingByte);

// The sniffing fallback used for encoding byte 0: ASCII passthrough, else
// strict UTF-8 validation, else legacy single-byte code page (TIS-620 when
// the heuristic below fires), else plain Latin-1.
QString decodeLegacyOrUtf8(const QByteArray &raw);

// True only when there are at least two CONSECUTIVE bytes >= 0xA0 - this is
// what tells genuine Thai byte runs apart from isolated accented Latin-1
// letters, which also live in the high byte range but never run in pairs.
bool looksLikeThaiLegacy(const QByteArray &raw);

QString decodeTis620(const QByteArray &raw);

} // namespace TextDecoder
