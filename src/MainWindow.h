#pragma once

#include "AudioEngine.h"
#include "Playlist.h"
#include "Settings.h"
#include "Visualizer.h"

#include <QMainWindow>
#include <QVector>
#include <QDateTime>

#include <array>

class QLabel;
class QSlider;
class QPushButton;
class QToolButton;
class QListWidget;
class QComboBox;
class QCheckBox;
class QRadioButton;
class QSpinBox;
class QTimer;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // Transport
    void onPlayPauseClicked();
    void onStopClicked();
    void onPreviousClicked();
    void onNextClicked();
    void onShuffleToggled(bool on);
    void onRepeatClicked();
    void onMuteToggled();
    void onVolumeSliderMoved(int value);

    // Seek
    void onSeekSliderPressed();
    void onSeekSliderReleased();
    void onSeekSliderMoved(int value);

    // Playlist
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onLoadPlaylistClicked();
    void onSavePlaylistClicked();
    void onClearPlaylistClicked();
    void onPlaylistItemActivated(int row);
    void onPlaylistContextMenuRequested(const QPoint &pos);
    void onPlaylistItemsChanged();
    void onPlaylistCurrentIndexChanged(int index);

    // Equalizer
    void onEqEnabledToggled(bool on);
    void onEqPresetChanged(const QString &name);
    void onEqBandSliderChanged(int band, int value);

    // Visualizer
    void onVizStyleChanged(const QString &name);
    void onVizColorSchemeChanged(const QString &name);
    void onVizEnabledToggled(bool on);

    // Tag editing
    void onEditTagClicked();

    // Shutdown timer
    void onShutdownStartClicked();
    void onShutdownCancelClicked();
    void onShutdownTimerTick();

    // Theme
    void onThemeBackgroundColorClicked();
    void onThemeAccentColorClicked();
    void onThemeResetClicked();

    // AudioEngine
    void onEngineStateChanged(AudioEngine::State state);
    void onEngineTrackLoaded(qint64 durationMs);
    void onEngineDurationChanged(qint64 durationMs);
    void onEnginePositionChanged(qint64 positionMs);
    void onEnginePlaybackFinished();
    void onEngineError(const QString &message);
    void onEngineFormatDescriptionChanged(const QString &text);

private:
    void setupUi();
    void setupConnections();
    void restoreSettings();
    void saveSettings();

    void playIndex(int index, bool autoPlay);
    void updatePlayPauseIcon(bool playing);
    void updateShuffleIcon();
    void updateRepeatIcon();
    void updateVolumeIcon();
    void updateCoverArt(const QString &filePath);
    void refreshTrackInfoLabels(const QString &filePath); // reads real tags via TagEditor, falls back to filename/folder
    void refreshPlaylistWidget();
    void addFilesToPlaylist(const QStringList &paths);
    void performScheduledShutdown(bool alsoShutdownComputer);
    static QString formatTime(qint64 ms);

    void applyThemePalette();     // pushes Theme::current() into the stylesheet + every hand-painted widget, then saves it
    void updateThemeTabSwatches();// repaints the two Theme-tab color buttons to match Theme::current()
    void refreshStaticIcons();    // re-icons buttons with no update*Icon() of their own (see call site for the list)
    void restoreTabOrder();       // reorders the tab bar to match Settings::tabOrder(), if a custom order was saved

    AudioEngine *m_engine = nullptr;
    Playlist *m_playlist = nullptr;
    Settings m_settings;
    Visualizer *m_visualizer = nullptr;

    // Tabs
    QTabWidget *m_tabs = nullptr; // actually a TabWidgetWithSwappableBar (MainWindow.cpp anonymous namespace) - stored as the QTabWidget base since the subclass is private to that .cpp file

    // Header
    QLabel *m_coverLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_artistLabel = nullptr;
    QLabel *m_formatLabel = nullptr;
    QPushButton *m_editTagBtn = nullptr;

    // Seek
    QSlider *m_seekSlider = nullptr;
    QLabel *m_positionLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    bool m_seekSliderDragging = false;

    // Set for exactly as long as AudioEngine::State::Loading lasts (decode
    // of the whole track into memory - see AudioEngine.h), so the app-wide
    // cursor shows a busy spinner instead of leaving the pointer looking
    // idle/clickable while nothing is actually playable yet. Tracked with
    // our own flag rather than trusting stateChanged's Loading->Loading
    // no-op (setState() only emits on an actual change) to be paired 1:1,
    // so onEngineStateChanged() can't push QApplication's override-cursor
    // stack twice for one Loading span or restore one that was never set.
    bool m_loadingCursorActive = false;

    // Transport
    QPushButton *m_shuffleBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QToolButton *m_muteBtn = nullptr;
    QSlider *m_volumeSlider = nullptr;

    // Playlist
    QListWidget *m_playlistView = nullptr;
    QPushButton *m_addFilesBtn = nullptr;
    QPushButton *m_addFolderBtn = nullptr;
    QPushButton *m_loadPlaylistBtn = nullptr;
    QPushButton *m_savePlaylistBtn = nullptr;
    QPushButton *m_clearPlaylistBtn = nullptr;

    // Equalizer
    QCheckBox *m_eqEnableCheck = nullptr;
    QComboBox *m_eqPresetCombo = nullptr;
    std::array<QSlider *, Equalizer::kBandCount> m_eqSliders{};
    std::array<QLabel *, Equalizer::kBandCount> m_eqValueLabels{};

    // Visualizer controls
    QComboBox *m_vizStyleCombo = nullptr;
    QComboBox *m_vizColorSchemeCombo = nullptr;
    QCheckBox *m_vizEnableCheck = nullptr;

    // Shutdown timer
    QRadioButton *m_shutdownCloseOnlyRadio = nullptr;
    QRadioButton *m_shutdownCloseAndComputerRadio = nullptr;
    QSpinBox *m_shutdownHoursSpin = nullptr;
    QSpinBox *m_shutdownMinutesSpin = nullptr;
    QSpinBox *m_shutdownSecondsSpin = nullptr;
    QPushButton *m_shutdownStartBtn = nullptr;
    QPushButton *m_shutdownCancelBtn = nullptr;
    QLabel *m_shutdownCountdownLabel = nullptr;
    QLabel *m_shutdownStatusLabel = nullptr;
    QTimer *m_shutdownTimer = nullptr;
    QDateTime m_shutdownTargetTime;
    bool m_shutdownExitInProgress = false; // set right before an auto-close triggered by the shutdown timer, so closeEvent() skips the "Are you sure?" prompt

    // Theme
    QPushButton *m_themeBackgroundColorBtn = nullptr;
    QPushButton *m_themeAccentColorBtn = nullptr;
    QPushButton *m_themeResetBtn = nullptr;

    bool m_restoringState = false;
};
