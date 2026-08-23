#pragma once
//
// Small modal dialog for editing a track's Title/Artist/Album text tags.
// Knows nothing about file I/O - MainWindow reads the current values via
// TagEditor::readTags() before constructing this, and writes them back via
// TagEditor::writeTags() if the user accepts.
//
#include <QDialog>

class QLineEdit;

class TagEditDialog : public QDialog
{
    Q_OBJECT

public:
    // `writeSupported` disables the Save button and shows an explanatory
    // note when true saving isn't possible for this file's type (currently
    // MP4/M4A - see TagEditor.h) - the fields are still shown for reference.
    explicit TagEditDialog(const QString &title, const QString &artist, const QString &album,
                            bool writeSupported, QWidget *parent = nullptr);

    QString title() const;
    QString artist() const;
    QString album() const;

private:
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_artistEdit = nullptr;
    QLineEdit *m_albumEdit = nullptr;
};
