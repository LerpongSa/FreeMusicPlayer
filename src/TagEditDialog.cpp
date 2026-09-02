#include "TagEditDialog.h"

#include <QBuffer>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kCoverPreviewSize = 96;
}

TagEditDialog::TagEditDialog(const QString &title, const QString &artist, const QString &album,
                              const QByteArray &currentCoverData, const QString &currentCoverMime,
                              bool writeSupported, QWidget *parent)
    : QDialog(parent), m_writeSupported(writeSupported), m_hasCover(!currentCoverData.isEmpty())
{
    Q_UNUSED(currentCoverMime); // only needed if/when we re-encode; the preview just decodes the bytes

    setWindowTitle(tr("Edit Tag"));
    setModal(true);

    auto *rootLayout = new QVBoxLayout(this);

    // ---- Cover art row: preview + Change/Remove buttons ----
    auto *coverRow = new QHBoxLayout();
    m_coverPreview = new QLabel(this);
    m_coverPreview->setFixedSize(kCoverPreviewSize, kCoverPreviewSize);
    m_coverPreview->setAlignment(Qt::AlignCenter);
    m_coverPreview->setStyleSheet(QStringLiteral("QLabel { border: 1px solid palette(mid); }"));
    updateCoverPreview(currentCoverData);
    coverRow->addWidget(m_coverPreview);

    auto *coverButtonsCol = new QVBoxLayout();
    m_changeCoverBtn = new QPushButton(tr("Change Cover..."), this);
    m_removeCoverBtn = new QPushButton(tr("Remove Cover"), this);
    connect(m_changeCoverBtn, &QPushButton::clicked, this, &TagEditDialog::onChangeCoverClicked);
    connect(m_removeCoverBtn, &QPushButton::clicked, this, &TagEditDialog::onRemoveCoverClicked);
    coverButtonsCol->addWidget(m_changeCoverBtn);
    coverButtonsCol->addWidget(m_removeCoverBtn);
    coverButtonsCol->addStretch(1);
    coverRow->addLayout(coverButtonsCol);
    coverRow->addStretch(1);
    rootLayout->addLayout(coverRow);

    // ---- Title/Artist/Album fields ----
    auto *form = new QFormLayout();
    m_titleEdit = new QLineEdit(title, this);
    m_artistEdit = new QLineEdit(artist, this);
    m_albumEdit = new QLineEdit(album, this);
    form->addRow(tr("Title:"), m_titleEdit);
    form->addRow(tr("Artist:"), m_artistEdit);
    form->addRow(tr("Album:"), m_albumEdit);
    rootLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    if (!writeSupported) {
        auto *note = new QLabel(tr("Saving tags (including cover art) for this file type isn't "
                                    "supported yet (MP3, FLAC, and WAV only) - showing the current "
                                    "tag for reference only."),
                                 this);
        note->setWordWrap(true);
        rootLayout->addWidget(note);
        if (QPushButton *saveBtn = buttons->button(QDialogButtonBox::Save))
            saveBtn->setEnabled(false);
    }

    rootLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateCoverButtonsEnabled();

    resize(400, sizeHint().height());
}

QString TagEditDialog::title() const { return m_titleEdit->text().trimmed(); }
QString TagEditDialog::artist() const { return m_artistEdit->text().trimmed(); }
QString TagEditDialog::album() const { return m_albumEdit->text().trimmed(); }

void TagEditDialog::updateCoverPreview(const QByteArray &data)
{
    QPixmap pm;
    if (!data.isEmpty() && pm.loadFromData(data)) {
        m_coverPreview->setText(QString());
        m_coverPreview->setPixmap(
            pm.scaled(m_coverPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_coverPreview->setPixmap(QPixmap());
        m_coverPreview->setText(tr("No\nCover"));
    }
}

void TagEditDialog::updateCoverButtonsEnabled()
{
    m_changeCoverBtn->setEnabled(m_writeSupported);
    m_removeCoverBtn->setEnabled(m_writeSupported && m_hasCover);
}

void TagEditDialog::onChangeCoverClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose Cover Image"), QString(),
                                                        tr("Images (*.jpg *.jpeg *.png *.bmp *.gif *.webp)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Choose Cover Image"), tr("Couldn't open the selected image."));
        return;
    }
    const QByteArray raw = file.readAll();
    file.close();

    const QString ext = QFileInfo(path).suffix().toLower();
    QByteArray data;
    QString mime;
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")) {
        data = raw;
        mime = QStringLiteral("image/jpeg");
    } else if (ext == QLatin1String("png")) {
        data = raw;
        mime = QStringLiteral("image/png");
    } else {
        // Less universally-supported embedded picture formats (BMP/GIF/
        // WEBP/...) get re-encoded to PNG here, so the file this ends up
        // written into is readable by the widest range of players/taggers
        // rather than trusting every one of them to understand e.g. an
        // embedded WEBP picture frame.
        QImage img;
        if (!img.loadFromData(raw)) {
            QMessageBox::warning(this, tr("Choose Cover Image"), tr("Couldn't read the selected image."));
            return;
        }
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        mime = QStringLiteral("image/png");
    }

    if (data.isEmpty()) {
        QMessageBox::warning(this, tr("Choose Cover Image"), tr("Couldn't read the selected image."));
        return;
    }

    m_coverAction = TrackTags::CoverArtAction::Replace;
    m_newCoverData = data;
    m_newCoverMimeType = mime;
    m_hasCover = true;
    updateCoverPreview(data);
    updateCoverButtonsEnabled();
}

void TagEditDialog::onRemoveCoverClicked()
{
    m_coverAction = TrackTags::CoverArtAction::Remove;
    m_newCoverData.clear();
    m_newCoverMimeType.clear();
    m_hasCover = false;
    updateCoverPreview(QByteArray());
    updateCoverButtonsEnabled();
}
