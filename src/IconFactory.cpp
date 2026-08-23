#include "IconFactory.h"

#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QFont>

namespace IconFactory {

namespace {

// QImage with an explicit alpha-capable format, rather than QPixmap - some
// platform pixmap formats don't support CompositionMode_Clear (used by
// drawClear() to cut holes), while ARGB32_Premultiplied always does.
QImage newCanvas(int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

void drawPlay(QPainter &p, double s)
{
    QPolygonF tri;
    tri << QPointF(s * 0.30, s * 0.20) << QPointF(s * 0.30, s * 0.80) << QPointF(s * 0.82, s * 0.5);
    p.drawPolygon(tri);
}

void drawPause(QPainter &p, double s)
{
    p.drawRect(QRectF(s * 0.26, s * 0.20, s * 0.18, s * 0.60));
    p.drawRect(QRectF(s * 0.56, s * 0.20, s * 0.18, s * 0.60));
}

void drawStop(QPainter &p, double s)
{
    p.drawRoundedRect(QRectF(s * 0.26, s * 0.26, s * 0.48, s * 0.48), s * 0.04, s * 0.04);
}

void drawSkip(QPainter &p, double s, bool forward)
{
    const double dir = forward ? 1.0 : -1.0;
    const double cx = s * 0.5;
    QPolygonF tri;
    if (forward) {
        tri << QPointF(s * 0.24, s * 0.22) << QPointF(s * 0.24, s * 0.78) << QPointF(s * 0.62, s * 0.5);
    } else {
        tri << QPointF(s * 0.76, s * 0.22) << QPointF(s * 0.76, s * 0.78) << QPointF(s * 0.38, s * 0.5);
    }
    p.drawPolygon(tri);
    p.drawRect(QRectF(cx + dir * s * 0.14, s * 0.22, s * 0.08, s * 0.56));
}

void drawShuffle(QPainter &p, double s)
{
    QPen pen = p.pen();
    pen.setWidthF(s * 0.07);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QPainterPath path1;
    path1.moveTo(s * 0.18, s * 0.28);
    path1.lineTo(s * 0.40, s * 0.28);
    path1.cubicTo(s * 0.55, s * 0.28, s * 0.55, s * 0.72, s * 0.72, s * 0.72);
    p.drawPath(path1);

    QPainterPath path2;
    path2.moveTo(s * 0.18, s * 0.72);
    path2.lineTo(s * 0.40, s * 0.72);
    path2.cubicTo(s * 0.50, s * 0.72, s * 0.52, s * 0.55, s * 0.60, s * 0.44);
    p.drawPath(path2);

    // arrowheads
    p.setBrush(pen.color());
    p.setPen(Qt::NoPen);
    QPolygonF a1;
    a1 << QPointF(s * 0.68, s * 0.62) << QPointF(s * 0.84, s * 0.72) << QPointF(s * 0.68, s * 0.82);
    p.drawPolygon(a1);
    QPolygonF a2;
    a2 << QPointF(s * 0.68, s * 0.18) << QPointF(s * 0.84, s * 0.28) << QPointF(s * 0.68, s * 0.38);
    p.drawPolygon(a2);
}

void drawRepeat(QPainter &p, double s, bool one)
{
    QPen pen = p.pen();
    pen.setWidthF(s * 0.08);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QRectF r(s * 0.18, s * 0.22, s * 0.64, s * 0.56);
    p.drawArc(r, 20 * 16, 320 * 16);

    p.setBrush(pen.color());
    p.setPen(Qt::NoPen);
    QPolygonF arrow;
    arrow << QPointF(s * 0.74, s * 0.16) << QPointF(s * 0.90, s * 0.30) << QPointF(s * 0.72, s * 0.40);
    p.drawPolygon(arrow);

    if (one) {
        QFont f = QFont();
        f.setPixelSize(static_cast<int>(s * 0.42));
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(0, 0, s, s), Qt::AlignCenter, "1");
    }
}

void drawVolume(QPainter &p, double s, int level) // 0=mute,1=low,2=high
{
    QPolygonF speaker;
    speaker << QPointF(s * 0.18, s * 0.40) << QPointF(s * 0.34, s * 0.40) << QPointF(s * 0.52, s * 0.24)
            << QPointF(s * 0.52, s * 0.76) << QPointF(s * 0.34, s * 0.60) << QPointF(s * 0.18, s * 0.60);
    p.drawPolygon(speaker);

    QPen pen = p.pen();
    pen.setWidthF(s * 0.06);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    if (level == 0) {
        p.drawLine(QPointF(s * 0.62, s * 0.36), QPointF(s * 0.86, s * 0.64));
        p.drawLine(QPointF(s * 0.86, s * 0.36), QPointF(s * 0.62, s * 0.64));
    } else {
        p.drawArc(QRectF(s * 0.56, s * 0.30, s * 0.24, s * 0.40), -50 * 16, 100 * 16);
        if (level == 2)
            p.drawArc(QRectF(s * 0.62, s * 0.18, s * 0.30, s * 0.64), -50 * 16, 100 * 16);
    }
}

void drawFolderOpen(QPainter &p, double s)
{
    QPainterPath back;
    back.addRoundedRect(QRectF(s * 0.14, s * 0.28, s * 0.34, s * 0.44), s * 0.03, s * 0.03);
    p.drawPath(back);

    QPolygonF front;
    front << QPointF(s * 0.16, s * 0.40) << QPointF(s * 0.86, s * 0.40) << QPointF(s * 0.78, s * 0.80)
          << QPointF(s * 0.08, s * 0.80);
    p.drawPolygon(front);
}

void drawSave(QPainter &p, double s)
{
    p.drawRoundedRect(QRectF(s * 0.18, s * 0.16, s * 0.64, s * 0.68), s * 0.05, s * 0.05);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(s * 0.30, s * 0.16, s * 0.40, s * 0.22));
    p.setBrush(p.pen().color());
    p.drawRect(QRectF(s * 0.30, s * 0.56, s * 0.40, s * 0.20));
}

void drawClear(QPainter &p, double s)
{
    p.drawRoundedRect(QRectF(s * 0.26, s * 0.30, s * 0.48, s * 0.52), s * 0.04, s * 0.04);
    p.drawRect(QRectF(s * 0.18, s * 0.22, s * 0.64, s * 0.08));
    p.drawRect(QRectF(s * 0.40, s * 0.14, s * 0.20, s * 0.08));

    // Cut two vertical "strike" slots out of the bin body using the clear
    // composition mode, rather than drawing them in a second color.
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.drawRect(QRectF(s * 0.38, s * 0.40, s * 0.06, s * 0.34));
    p.drawRect(QRectF(s * 0.56, s * 0.40, s * 0.06, s * 0.34));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void drawListMusic(QPainter &p, double s)
{
    QPen pen = p.pen();
    pen.setWidthF(s * 0.08);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    for (int i = 0; i < 3; ++i) {
        const double y = s * (0.26 + i * 0.24);
        p.drawLine(QPointF(s * 0.16, y), QPointF(s * 0.62, y));
    }
    p.setBrush(pen.color());
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(s * 0.78, s * 0.72), s * 0.10, s * 0.10);
    p.drawRect(QRectF(s * 0.84, s * 0.30, s * 0.05, s * 0.42));
}

void drawAppIcon(QPainter &p, double s)
{
    QRadialGradient grad(QPointF(s * 0.5, s * 0.42), s * 0.6);
    grad.setColorAt(0.0, QColor(0x85, 0x78, 0xf0));
    grad.setColorAt(1.0, QColor(0x4a, 0x3f, 0xb8));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(s * 0.06, s * 0.06, s * 0.88, s * 0.88));

    p.setBrush(Qt::white);
    QPolygonF note;
    note << QPointF(s * 0.40, s * 0.28) << QPointF(s * 0.40, s * 0.66) << QPointF(s * 0.64, s * 0.60)
         << QPointF(s * 0.64, s * 0.24);
    p.drawPolygon(note);
    p.drawEllipse(QPointF(s * 0.36, s * 0.70), s * 0.09, s * 0.075);
    p.drawEllipse(QPointF(s * 0.60, s * 0.64), s * 0.09, s * 0.075);
}

} // namespace

QIcon make(Glyph glyph, const QColor &color, int size)
{
    QImage img = newCanvas(size);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const double s = size;

    switch (glyph) {
    case Glyph::Play: drawPlay(p, s); break;
    case Glyph::Pause: drawPause(p, s); break;
    case Glyph::Previous: drawSkip(p, s, false); break;
    case Glyph::Next: drawSkip(p, s, true); break;
    case Glyph::Stop: drawStop(p, s); break;
    case Glyph::Shuffle: p.setPen(QPen(color)); drawShuffle(p, s); break;
    case Glyph::RepeatAll: p.setPen(QPen(color)); drawRepeat(p, s, false); break;
    case Glyph::RepeatOne: p.setPen(QPen(color)); drawRepeat(p, s, true); break;
    case Glyph::VolumeMute: p.setPen(QPen(color)); drawVolume(p, s, 0); break;
    case Glyph::VolumeLow: p.setPen(QPen(color)); drawVolume(p, s, 1); break;
    case Glyph::VolumeHigh: p.setPen(QPen(color)); drawVolume(p, s, 2); break;
    case Glyph::FolderOpen: drawFolderOpen(p, s); break;
    case Glyph::Save: drawSave(p, s); break;
    case Glyph::Clear: drawClear(p, s); break;
    case Glyph::ListMusic: drawListMusic(p, s); break;
    case Glyph::AppIcon: drawAppIcon(p, s); break;
    }

    p.end();
    return QIcon(QPixmap::fromImage(img));
}

} // namespace IconFactory
