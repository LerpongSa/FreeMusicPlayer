#pragma once
//
// Small modal dialog for editing a track's Title/Artist/Album text tags,
// plus the embedded cover picture. Knows nothing about actual file I/O -
// MainWindow reads the current values via TagEditor::readTags() and the
// current cover via CoverArtExtractor::extract() before constructing this,
// and writes everything back via TagEditor::writeTags() if the user
// accepts, using the coverArtAction()/newCoverData()/newCoverMimeType()
// accessors below to fill in TrackTags' cover-art fields (see TagEditor.h).
//
#include "TagEditor.h" // for TrackTags::CoverArtAction

#include <QByteArray>
#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class TagEditDialog : public QDialog
{
    Q_OBJECT

public:
    // `writeSupported` disables Save AND the cover-editing buttons, and
    // shows an explanatory note, when true saving isn't possible for this
    // file's type (currently MP4/M4A - see TagEditor.h) - the fields/cover
    // are still shown for reference. `currentCoverData`/`currentCoverMime`
    // are what's currently shown as the track's cover (embedded art, or
    // CoverArtExtractor's folder-image fallback) - pass an empty
    // `currentCoverData` if there's nothing to show.
    explicit TagEditDialog(const QString &title, const QString &artist, const QString &album,
                            const QByteArray &currentCoverData, const QString &currentCoverMime,
                            bool writeSupported, QWidget *parent = nullptr);

    QString title() const;
    QString artist() const;
    QString album() const;

    // Keep unless the user picked a new image (-> Replace, with
    // newCoverData()/newCoverMimeType() filled in) or clicked Remove Cover
    // (-> Remove). Picking a new image after clicking Remove overrides back
    // to Replace - the last action taken wins.
    TrackTags::CoverArtAction coverArtAction() const { return m_coverAction; }
    QByteArray newCoverData() const { return m_newCoverData; }
    QString newCoverMimeType() const { return m_newCoverMimeType; }

private slots:
    void onChangeCoverClicked();
    void onRemoveCoverClicked();

private:
    void updateCoverPreview(const QByteArray &data);
    void updateCoverButtonsEnabled();

    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_artistEdit = nullptr;
    QLineEdit *m_albumEdit = nullptr;

    QLabel *m_coverPreview = nullptr;
    QPushButton *m_changeCoverBtn = nullptr;
    QPushButton *m_removeCoverBtn = nullptr;

    bool m_writeSupported = true;
    bool m_hasCover = false; // true while there's a cover to show/remove (current or newly picked)
    TrackTags::CoverArtAction m_coverAction = TrackTags::CoverArtAction::Keep;
    QByteArray m_newCoverData;
    QString m_newCoverMimeType;
};
