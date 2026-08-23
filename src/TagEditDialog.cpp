#include "TagEditDialog.h"

#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>

TagEditDialog::TagEditDialog(const QString &title, const QString &artist, const QString &album,
                              bool writeSupported, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Tag"));
    setModal(true);

    auto *rootLayout = new QVBoxLayout(this);

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
        auto *note = new QLabel(tr("Saving tags for this file type isn't supported yet "
                                    "(MP3, FLAC, and WAV only) - showing the current tag "
                                    "for reference only."),
                                 this);
        note->setWordWrap(true);
        rootLayout->addWidget(note);
        if (QPushButton *saveBtn = buttons->button(QDialogButtonBox::Save))
            saveBtn->setEnabled(false);
    }

    rootLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(360, sizeHint().height());
}

QString TagEditDialog::title() const { return m_titleEdit->text().trimmed(); }
QString TagEditDialog::artist() const { return m_artistEdit->text().trimmed(); }
QString TagEditDialog::album() const { return m_albumEdit->text().trimmed(); }
