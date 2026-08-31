#include "MainWindow.h"
#include "CoverArtExtractor.h"
#include "TagEditor.h"
#include "TagEditDialog.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QTabWidget>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPainter>
#include <QProcess>
#include <QDesktopServices>
#include <QApplication>
#include <QStyleOptionSlider>
#include <QStyle>

#include <algorithm>

namespace {

const QStringList kAudioExtensions = {"mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus", "aiff", "wv", "dsf"};

QString audioFileFilter()
{
    QStringList patterns;
    for (const QString &ext : kAudioExtensions)
        patterns << "*." + ext;
    return QStringLiteral("Audio Files (%1);;All Files (*)").arg(patterns.join(' '));
}

// Always HH:MM:SS (unlike MainWindow::formatTime, which drops the hours
// field when zero) so the shutdown countdown display keeps a fixed width
// instead of visibly reflowing whenever it ticks across an hour boundary.
QString formatCountdown(qint64 totalSeconds)
{
    if (totalSeconds < 0)
        totalSeconds = 0;
    const qint64 h = totalSeconds / 3600;
    const qint64 m = (totalSeconds % 3600) / 60;
    const qint64 s = totalSeconds % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

// Bipolar EQ band slider. A plain QSlider's QSS sub-page/add-page can only
// color the track from one END to the handle, which looks wrong for a
// value that swings above AND below zero: at 0 dB (handle dead center) it
// still shows half the track "lit" from the top edge down to the handle,
// as if something were boosted. Real EQ hardware/software highlights from
// the CENTER (0 dB) out towards the handle instead, so a centered handle
// shows no highlight at all and the highlight direction flips with cut vs.
// boost. QSS can't express a highlight anchored at an arbitrary midpoint,
// so this repaints the slider itself; mouse/keyboard handling is left
// entirely to the base QSlider (only paintEvent is overridden), so
// dragging/clicking/arrow-keys all still work exactly as before.
class EqSlider : public QSlider
{
public:
    explicit EqSlider(QWidget *parent = nullptr) : QSlider(Qt::Vertical, parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

        // Ask the style where the handle WOULD be for the center (0 dB)
        // value, without touching the slider's actual value - gives the
        // exact same pixel math the real handle uses, so it stays correct
        // under any style/DPI instead of hand-deriving the geometry.
        QStyleOptionSlider centerOpt = opt;
        const int centerVal = (minimum() + maximum()) / 2;
        centerOpt.sliderPosition = centerVal;
        centerOpt.sliderValue = centerVal;
        const QRect centerRect = style()->subControlRect(QStyle::CC_Slider, &centerOpt, QStyle::SC_SliderHandle, this);

        const int trackW = 5;
        const int cx = grooveRect.center().x();
        const QRect track(cx - trackW / 2, grooveRect.top(), trackW, grooveRect.height());

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Theme::kBorder));
        p.drawRoundedRect(track, trackW / 2.0, trackW / 2.0);

        const int centerY = centerRect.center().y();
        const int handleY = handleRect.center().y();
        const int top = std::min(centerY, handleY);
        const int bottom = std::max(centerY, handleY);
        if (bottom > top) {
            const QRect highlight(cx - trackW / 2, top, trackW, bottom - top);
            p.setBrush(QColor(Theme::kAccent));
            p.drawRoundedRect(highlight, trackW / 2.0, trackW / 2.0);
        }

        // Thin reference tick at 0 dB, so "centered" reads clearly even
        // on the rare frame where the highlight itself is zero-height.
        p.setPen(QPen(QColor(255, 255, 255, 70), 1));
        p.drawLine(track.left() - 3, centerY, track.right() + 3, centerY);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Theme::kText));
        p.drawEllipse(handleRect);
    }
};

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_engine = new AudioEngine(this);
    m_playlist = new Playlist(this);

    setupUi();
    setupConnections();
    setAcceptDrops(true);

    setWindowTitle(tr("FreeMusicPlayer"));
    resize(980, 640);

    restoreSettings();
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(14, 14, 14, 10);
    rootLayout->setSpacing(10);

    // ---- Left panel: cover art + track details, pinned top-left ----
    auto *leftPanel = new QWidget(central);
    leftPanel->setMaximumWidth(240);
    auto *leftPanelLayout = new QVBoxLayout(leftPanel);
    leftPanelLayout->setContentsMargins(0, 0, 0, 0);
    leftPanelLayout->setSpacing(8);

    auto *coverFrame = new QFrame(leftPanel);
    coverFrame->setObjectName("CoverFrame");
    coverFrame->setFixedSize(222, 222); // 50% bigger than the original 148x148
    auto *coverLayout = new QVBoxLayout(coverFrame);
    coverLayout->setContentsMargins(4, 4, 4, 4);
    m_coverLabel = new QLabel(coverFrame);
    m_coverLabel->setFixedSize(214, 214); // 50% bigger than the original 140x140
    m_coverLabel->setScaledContents(false);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    coverLayout->addWidget(m_coverLabel);
    leftPanelLayout->addWidget(coverFrame);
    // Extra breathing room above the title specifically (on top of the
    // panel's normal 8px inter-widget spacing): Thai titles carry tone
    // marks/vowels stacked above the base character, and QLabel's sizeHint
    // is computed from the declared font's own metrics, not the Thai
    // fallback font actually used to render those glyphs on Windows - so a
    // tightly-spaced label can visually crowd/clip against the cover art
    // above it. This gap plus #TrackTitle's padding-top (Theme.h) give that
    // extra headroom.
    leftPanelLayout->addSpacing(6);

    m_titleLabel = new QLabel(tr("No track loaded"), leftPanel);
    m_titleLabel->setObjectName("TrackTitle");
    m_titleLabel->setWordWrap(true);
    m_artistLabel = new QLabel(tr("—"), leftPanel);
    m_artistLabel->setObjectName("TrackArtist");
    m_artistLabel->setWordWrap(true);
    m_formatLabel = new QLabel(QString(), leftPanel);
    m_formatLabel->setObjectName("TrackArtist");
    m_formatLabel->setWordWrap(true);
    leftPanelLayout->addWidget(m_titleLabel);
    leftPanelLayout->addWidget(m_artistLabel);
    leftPanelLayout->addWidget(m_formatLabel);

    m_editTagBtn = new QPushButton(tr("Edit Tag..."), leftPanel);
    connect(m_editTagBtn, &QPushButton::clicked, this, &MainWindow::onEditTagClicked);
    leftPanelLayout->addWidget(m_editTagBtn);

    leftPanelLayout->addStretch(1);

    // ---- Visualizer: its own full-width row, below the top split ----
    auto *vizHeaderRow = new QHBoxLayout();
    auto *vizLabel = new QLabel(tr("VISUALIZER"), central);
    vizLabel->setObjectName("SectionHeader");
    m_vizStyleCombo = new QComboBox(central);
    m_vizStyleCombo->addItems(Visualizer::styleNames());
    m_vizColorSchemeCombo = new QComboBox(central);
    m_vizColorSchemeCombo->addItems(Visualizer::colorSchemeNames());
    m_vizEnableCheck = new QCheckBox(tr("Enabled"), central);
    m_vizEnableCheck->setChecked(true);
    vizHeaderRow->addWidget(vizLabel);
    vizHeaderRow->addStretch(1);
    vizHeaderRow->addWidget(m_vizEnableCheck);
    vizHeaderRow->addWidget(m_vizStyleCombo);
    vizHeaderRow->addWidget(m_vizColorSchemeCombo);

    m_visualizer = new Visualizer(m_engine, central);
    m_visualizer->setMinimumHeight(180); // doubled from 90 - more room for a prettier display
    // leftPanel, vizHeaderRow, and m_visualizer are all added to
    // rootLayout at the end of this function, once `tabs` exists too -
    // see the assembly block right after the Equalizer tab is built.

    // ---- Seek row ----
    auto *seekLayout = new QHBoxLayout();
    m_positionLabel = new QLabel("0:00", central);
    m_durationLabel = new QLabel("0:00", central);
    m_seekSlider = new QSlider(Qt::Horizontal, central);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setEnabled(false);
    seekLayout->addWidget(m_positionLabel);
    seekLayout->addWidget(m_seekSlider, 1);
    seekLayout->addWidget(m_durationLabel);

    m_muteBtn = new QToolButton(central);
    m_muteBtn->setIcon(IconFactory::make(IconFactory::Glyph::VolumeHigh, Theme::kText));
    m_volumeSlider = new QSlider(Qt::Horizontal, central);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(120);
    seekLayout->addSpacing(10);
    seekLayout->addWidget(m_muteBtn);
    seekLayout->addWidget(m_volumeSlider);

    // ---- Transport row ----
    auto *transportLayout = new QHBoxLayout();
    transportLayout->setSpacing(8);

    const QColor iconColor(Theme::kText);

    m_shuffleBtn = new QPushButton(central);
    m_shuffleBtn->setObjectName("TransportButton");
    m_shuffleBtn->setCheckable(true);
    m_shuffleBtn->setToolTip(tr("Shuffle"));

    m_prevBtn = new QPushButton(central);
    m_prevBtn->setObjectName("TransportButton");
    m_prevBtn->setIcon(IconFactory::make(IconFactory::Glyph::Previous, iconColor));
    m_prevBtn->setToolTip(tr("Previous"));

    m_playBtn = new QPushButton(central);
    m_playBtn->setObjectName("PlayButton");
    m_playBtn->setIcon(IconFactory::make(IconFactory::Glyph::Play, Qt::white, 26));
    m_playBtn->setToolTip(tr("Play/Pause"));

    m_stopBtn = new QPushButton(central);
    m_stopBtn->setObjectName("TransportButton");
    m_stopBtn->setIcon(IconFactory::make(IconFactory::Glyph::Stop, iconColor));
    m_stopBtn->setToolTip(tr("Stop"));

    m_nextBtn = new QPushButton(central);
    m_nextBtn->setObjectName("TransportButton");
    m_nextBtn->setIcon(IconFactory::make(IconFactory::Glyph::Next, iconColor));
    m_nextBtn->setToolTip(tr("Next"));

    m_repeatBtn = new QPushButton(central);
    m_repeatBtn->setObjectName("TransportButton");
    m_repeatBtn->setCheckable(true);
    m_repeatBtn->setToolTip(tr("Repeat"));

    transportLayout->addStretch(1);
    transportLayout->addWidget(m_shuffleBtn);
    transportLayout->addWidget(m_prevBtn);
    transportLayout->addWidget(m_playBtn);
    transportLayout->addWidget(m_stopBtn);
    transportLayout->addWidget(m_nextBtn);
    transportLayout->addWidget(m_repeatBtn);
    transportLayout->addStretch(1);
    // Volume (mute button + slider) now lives in the seek row instead of
    // here, so this row is purely [stretch][6 transport buttons][stretch]
    // and the button group renders genuinely centered regardless of how
    // wide the volume control is.

    // ---- Tabs: Playlist / Equalizer ----
    auto *tabs = new QTabWidget(central);

    // Playlist tab
    auto *playlistTab = new QWidget(tabs);
    auto *playlistLayout = new QVBoxLayout(playlistTab);
    auto *playlistToolbar = new QHBoxLayout();
    auto *addFilesBtn = new QPushButton(tr("Add Files"), playlistTab);
    addFilesBtn->setIcon(IconFactory::make(IconFactory::Glyph::FolderOpen, iconColor, 16));
    auto *addFolderBtn = new QPushButton(tr("Add Folder"), playlistTab);
    addFolderBtn->setIcon(IconFactory::make(IconFactory::Glyph::FolderOpen, iconColor, 16));
    auto *loadBtn = new QPushButton(tr("Load Playlist"), playlistTab);
    loadBtn->setIcon(IconFactory::make(IconFactory::Glyph::ListMusic, iconColor, 16));
    auto *saveBtn = new QPushButton(tr("Save Playlist"), playlistTab);
    saveBtn->setIcon(IconFactory::make(IconFactory::Glyph::Save, iconColor, 16));
    auto *clearBtn = new QPushButton(tr("Clear"), playlistTab);
    clearBtn->setIcon(IconFactory::make(IconFactory::Glyph::Clear, iconColor, 16));

    playlistToolbar->addWidget(addFilesBtn);
    playlistToolbar->addWidget(addFolderBtn);
    playlistToolbar->addWidget(loadBtn);
    playlistToolbar->addWidget(saveBtn);
    playlistToolbar->addStretch(1);
    playlistToolbar->addWidget(clearBtn);
    playlistLayout->addLayout(playlistToolbar);

    m_playlistView = new QListWidget(playlistTab);
    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    playlistLayout->addWidget(m_playlistView, 1);

    tabs->addTab(playlistTab, tr("Playlist"));

    connect(addFilesBtn, &QPushButton::clicked, this, &MainWindow::onAddFilesClicked);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::onAddFolderClicked);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadPlaylistClicked);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSavePlaylistClicked);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearPlaylistClicked);

    // Equalizer tab
    auto *eqTab = new QWidget(tabs);
    auto *eqLayout = new QVBoxLayout(eqTab);
    auto *eqHeaderRow = new QHBoxLayout();
    m_eqEnableCheck = new QCheckBox(tr("Enable Equalizer"), eqTab);
    m_eqEnableCheck->setChecked(true);
    m_eqPresetCombo = new QComboBox(eqTab);
    for (const Equalizer::Preset &preset : Equalizer::builtinPresets())
        m_eqPresetCombo->addItem(preset.name);
    m_eqPresetCombo->addItem(tr("Custom"));
    eqHeaderRow->addWidget(m_eqEnableCheck);
    eqHeaderRow->addStretch(1);
    eqHeaderRow->addWidget(new QLabel(tr("Preset:"), eqTab));
    eqHeaderRow->addWidget(m_eqPresetCombo);
    eqLayout->addLayout(eqHeaderRow);

    auto *bandsLayout = new QHBoxLayout();
    bandsLayout->setSpacing(10);
    const auto &freqs = Equalizer::bandFrequencies();
    for (int b = 0; b < Equalizer::kBandCount; ++b) {
        auto *col = new QVBoxLayout();
        auto *valueLabel = new QLabel("0.0 dB", eqTab);
        valueLabel->setAlignment(Qt::AlignCenter);
        auto *slider = new EqSlider(eqTab);
        slider->setRange(-120, 120); // 0.1 dB resolution, -12..+12 dB
        slider->setValue(0);
        slider->setMinimumHeight(140);
        const double freq = freqs[static_cast<size_t>(b)];
        const QString freqLabel = freq >= 1000.0 ? QString::number(freq / 1000.0, 'g', 3) + " kHz"
                                                   : QString::number(freq, 'f', 0) + " Hz";
        auto *freqLabelWidget = new QLabel(freqLabel, eqTab);
        freqLabelWidget->setAlignment(Qt::AlignCenter);

        col->addWidget(valueLabel);
        col->addWidget(slider, 0, Qt::AlignHCenter);
        col->addWidget(freqLabelWidget);
        bandsLayout->addLayout(col);

        m_eqSliders[static_cast<size_t>(b)] = slider;
        m_eqValueLabels[static_cast<size_t>(b)] = valueLabel;
    }
    eqLayout->addLayout(bandsLayout, 1);

    tabs->addTab(eqTab, tr("Equalizer"));

    // Shutdown tab: a sleep-timer that either just closes the app, or
    // closes the app and shuts the whole PC down, after a countdown.
    auto *shutdownTab = new QWidget(tabs);
    auto *shutdownLayout = new QVBoxLayout(shutdownTab);
    shutdownLayout->setSpacing(14);

    auto *shutdownActionLabel = new QLabel(tr("ACTION"), shutdownTab);
    shutdownActionLabel->setObjectName("SectionHeader");
    shutdownLayout->addWidget(shutdownActionLabel);

    // QRadioButton auto-exclusivity groups by shared parent, so these two
    // are mutually exclusive without needing an explicit QButtonGroup.
    m_shutdownCloseOnlyRadio = new QRadioButton(tr("1. Close the program"), shutdownTab);
    m_shutdownCloseOnlyRadio->setChecked(true);
    m_shutdownCloseAndComputerRadio =
        new QRadioButton(tr("2. Close the program and Shutdown Computer"), shutdownTab);
    shutdownLayout->addWidget(m_shutdownCloseOnlyRadio);
    shutdownLayout->addWidget(m_shutdownCloseAndComputerRadio);

    auto *shutdownTimerLabel = new QLabel(tr("TIMER"), shutdownTab);
    shutdownTimerLabel->setObjectName("SectionHeader");
    shutdownLayout->addWidget(shutdownTimerLabel);

    auto *shutdownTimeRow = new QHBoxLayout();
    // QSpinBox formats its displayed number using the widget's locale by
    // default, which inherits the system locale - on a Windows machine set
    // to Thai with "use native digits" turned on, that renders the value
    // in Thai numerals (๐-๙) instead of 0-9. Pinning each spin box to the
    // "C" locale forces plain Arabic-numeral (0-9) digits regardless of
    // the user's Windows regional settings.
    const QLocale arabicDigitsLocale = QLocale::c();
    m_shutdownHoursSpin = new QSpinBox(shutdownTab);
    m_shutdownHoursSpin->setRange(0, 23);
    m_shutdownHoursSpin->setSuffix(tr(" h"));
    m_shutdownHoursSpin->setLocale(arabicDigitsLocale);
    m_shutdownMinutesSpin = new QSpinBox(shutdownTab);
    m_shutdownMinutesSpin->setRange(0, 59);
    m_shutdownMinutesSpin->setValue(30);
    m_shutdownMinutesSpin->setSuffix(tr(" m"));
    m_shutdownMinutesSpin->setLocale(arabicDigitsLocale);
    m_shutdownSecondsSpin = new QSpinBox(shutdownTab);
    m_shutdownSecondsSpin->setRange(0, 59);
    m_shutdownSecondsSpin->setSuffix(tr(" s"));
    m_shutdownSecondsSpin->setLocale(arabicDigitsLocale);
    shutdownTimeRow->addWidget(m_shutdownHoursSpin);
    shutdownTimeRow->addWidget(m_shutdownMinutesSpin);
    shutdownTimeRow->addWidget(m_shutdownSecondsSpin);
    shutdownTimeRow->addStretch(1);
    shutdownLayout->addLayout(shutdownTimeRow);

    auto *shutdownBtnRow = new QHBoxLayout();
    m_shutdownStartBtn = new QPushButton(tr("Start Countdown"), shutdownTab);
    m_shutdownCancelBtn = new QPushButton(tr("Cancel"), shutdownTab);
    m_shutdownCancelBtn->setEnabled(false);
    shutdownBtnRow->addWidget(m_shutdownStartBtn);
    shutdownBtnRow->addWidget(m_shutdownCancelBtn);
    shutdownBtnRow->addStretch(1);
    shutdownLayout->addLayout(shutdownBtnRow);

    auto *shutdownCountdownFrame = new QFrame(shutdownTab);
    shutdownCountdownFrame->setObjectName("Panel");
    auto *shutdownCountdownLayout = new QVBoxLayout(shutdownCountdownFrame);
    m_shutdownCountdownLabel = new QLabel(tr("--:--:--"), shutdownCountdownFrame);
    m_shutdownCountdownLabel->setObjectName("ShutdownCountdown");
    m_shutdownCountdownLabel->setAlignment(Qt::AlignCenter);
    m_shutdownStatusLabel = new QLabel(tr("No timer set."), shutdownCountdownFrame);
    m_shutdownStatusLabel->setObjectName("TrackArtist");
    m_shutdownStatusLabel->setAlignment(Qt::AlignCenter);
    shutdownCountdownLayout->addWidget(m_shutdownCountdownLabel);
    shutdownCountdownLayout->addWidget(m_shutdownStatusLabel);
    shutdownLayout->addWidget(shutdownCountdownFrame);

    shutdownLayout->addStretch(1);

    tabs->addTab(shutdownTab, tr("Shutdown"));

    connect(m_shutdownStartBtn, &QPushButton::clicked, this, &MainWindow::onShutdownStartClicked);
    connect(m_shutdownCancelBtn, &QPushButton::clicked, this, &MainWindow::onShutdownCancelClicked);

    // Top row: cover + track details pinned top-left, Playlist/Equalizer
    // filling the rest of the width as the main content area. Visualizer
    // gets its own full-width row below that, then seek, then transport.
    auto *topRowLayout = new QHBoxLayout();
    topRowLayout->setSpacing(14);
    topRowLayout->addWidget(leftPanel);
    topRowLayout->addWidget(tabs, 1);

    rootLayout->addLayout(topRowLayout, 1);
    rootLayout->addLayout(vizHeaderRow);
    rootLayout->addWidget(m_visualizer);
    rootLayout->addLayout(seekLayout);
    rootLayout->addLayout(transportLayout);

    statusBar();

    setStyleSheet(Theme::appStyleSheet());
}

void MainWindow::setupConnections()
{
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &MainWindow::onPreviousClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::onNextClicked);
    connect(m_shuffleBtn, &QPushButton::toggled, this, &MainWindow::onShuffleToggled);
    connect(m_repeatBtn, &QPushButton::clicked, this, &MainWindow::onRepeatClicked);
    connect(m_muteBtn, &QToolButton::clicked, this, &MainWindow::onMuteToggled);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeSliderMoved);

    connect(m_seekSlider, &QSlider::sliderPressed, this, &MainWindow::onSeekSliderPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &MainWindow::onSeekSliderReleased);
    connect(m_seekSlider, &QSlider::sliderMoved, this, &MainWindow::onSeekSliderMoved);

    connect(m_playlistView, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        onPlaylistItemActivated(m_playlistView->row(item));
    });
    connect(m_playlistView, &QListWidget::customContextMenuRequested, this,
            &MainWindow::onPlaylistContextMenuRequested);

    connect(m_playlist, &Playlist::itemsChanged, this, &MainWindow::onPlaylistItemsChanged);
    connect(m_playlist, &Playlist::currentIndexChanged, this, &MainWindow::onPlaylistCurrentIndexChanged);

    connect(m_eqEnableCheck, &QCheckBox::toggled, this, &MainWindow::onEqEnabledToggled);
    connect(m_eqPresetCombo, &QComboBox::currentTextChanged, this, &MainWindow::onEqPresetChanged);
    for (int b = 0; b < Equalizer::kBandCount; ++b) {
        connect(m_eqSliders[static_cast<size_t>(b)], &QSlider::valueChanged, this, [this, b](int value) {
            onEqBandSliderChanged(b, value);
        });
    }

    connect(m_vizStyleCombo, &QComboBox::currentTextChanged, this, &MainWindow::onVizStyleChanged);
    connect(m_vizColorSchemeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onVizColorSchemeChanged);
    connect(m_vizEnableCheck, &QCheckBox::toggled, this, &MainWindow::onVizEnabledToggled);

    connect(m_engine, &AudioEngine::stateChanged, this, &MainWindow::onEngineStateChanged);
    connect(m_engine, &AudioEngine::trackLoaded, this, &MainWindow::onEngineTrackLoaded);
    connect(m_engine, &AudioEngine::durationChanged, this, &MainWindow::onEngineDurationChanged);
    connect(m_engine, &AudioEngine::positionChanged, this, &MainWindow::onEnginePositionChanged);
    connect(m_engine, &AudioEngine::playbackFinished, this, &MainWindow::onEnginePlaybackFinished);
    connect(m_engine, &AudioEngine::errorOccurred, this, &MainWindow::onEngineError);
    connect(m_engine, &AudioEngine::formatDescriptionChanged, this, &MainWindow::onEngineFormatDescriptionChanged);
}

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------

void MainWindow::onPlayPauseClicked()
{
    if (m_engine->state() == AudioEngine::State::Playing) {
        m_engine->pause();
        return;
    }
    if (m_engine->currentFilePath().isEmpty()) {
        if (m_playlist->isEmpty())
            return;
        if (m_playlist->currentIndex() < 0)
            m_playlist->setCurrentIndex(0);
        playIndex(m_playlist->currentIndex(), true);
        return;
    }
    m_engine->play();
}

void MainWindow::onStopClicked()
{
    m_engine->stop();
}

void MainWindow::onPreviousClicked()
{
    if (m_engine->positionMs() > 3000) {
        m_engine->seek(0); // standard UX: "previous" restarts the current track once you're a few seconds in
        return;
    }
    if (m_playlist->advanceToPrevious())
        playIndex(m_playlist->currentIndex(), true);
    else
        m_engine->seek(0);
}

void MainWindow::onNextClicked()
{
    if (m_playlist->advanceToNext())
        playIndex(m_playlist->currentIndex(), true);
    else
        m_engine->stop();
}

void MainWindow::onShuffleToggled(bool on)
{
    m_playlist->setShuffle(on);
    updateShuffleIcon();
}

void MainWindow::onRepeatClicked()
{
    using RM = Playlist::RepeatMode;
    const RM mode = m_playlist->repeatMode();
    const RM next = (mode == RM::Off) ? RM::All : (mode == RM::All ? RM::One : RM::Off);
    m_playlist->setRepeatMode(next);
    updateRepeatIcon();
}

void MainWindow::onMuteToggled()
{
    m_engine->setMuted(!m_engine->isMuted());
    updateVolumeIcon();
}

void MainWindow::onVolumeSliderMoved(int value)
{
    m_engine->setVolume(value);
    if (value > 0 && m_engine->isMuted())
        m_engine->setMuted(false);
    updateVolumeIcon();
}

// ---------------------------------------------------------------------------
// Seek
// ---------------------------------------------------------------------------

void MainWindow::onSeekSliderPressed()
{
    m_seekSliderDragging = true;
}

void MainWindow::onSeekSliderReleased()
{
    m_seekSliderDragging = false;
    m_engine->seek(static_cast<qint64>(m_seekSlider->value()) * 1000);
}

void MainWindow::onSeekSliderMoved(int value)
{
    m_positionLabel->setText(formatTime(static_cast<qint64>(value) * 1000));
}

// ---------------------------------------------------------------------------
// Playlist
// ---------------------------------------------------------------------------

void MainWindow::onAddFilesClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Add Files"), QString(), audioFileFilter());
    addFilesToPlaylist(paths);
}

void MainWindow::onAddFolderClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add Folder"));
    if (dir.isEmpty())
        return;

    QStringList paths;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString ext = QFileInfo(path).suffix().toLower();
        if (kAudioExtensions.contains(ext))
            paths << path;
    }
    paths.sort(Qt::CaseInsensitive);
    addFilesToPlaylist(paths);
}

void MainWindow::onLoadPlaylistClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Playlist"), QString(),
                                                        tr("Playlists (*.m3u *.m3u8);;All Files (*)"));
    if (path.isEmpty())
        return;

    m_playlist->clear();
    int skipped = 0;
    const int loaded = m_playlist->loadM3u(path, &skipped);

    if (skipped > 0)
        statusBar()->showMessage(tr("Loaded %1 track(s); %2 file(s) could not be found and were skipped.")
                                      .arg(loaded)
                                      .arg(skipped),
                                  6000);
    else
        statusBar()->showMessage(tr("Loaded %1 track(s).").arg(loaded), 3000);
}

void MainWindow::onSavePlaylistClicked()
{
    if (m_playlist->isEmpty()) {
        QMessageBox::information(this, tr("Save Playlist"), tr("The playlist is empty."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Playlist"), QString(),
                                                        tr("Playlist (*.m3u8)"));
    if (path.isEmpty())
        return;

    if (m_playlist->saveM3u(path))
        statusBar()->showMessage(tr("Playlist saved."), 3000);
    else
        QMessageBox::warning(this, tr("Save Playlist"), tr("Could not write the playlist file."));
}

void MainWindow::onClearPlaylistClicked()
{
    if (m_playlist->isEmpty())
        return;
    const auto reply = QMessageBox::question(this, tr("Clear Playlist"),
                                              tr("Remove all tracks from the playlist?"));
    if (reply != QMessageBox::Yes)
        return;
    m_engine->stop();
    m_playlist->clear();
}

void MainWindow::onPlaylistItemActivated(int row)
{
    playIndex(row, true);
}

void MainWindow::onPlaylistContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = m_playlistView->itemAt(pos);
    if (item && !item->isSelected())
        m_playlistView->setCurrentItem(item); // right-clicking an unselected row selects just that row first

    QMenu menu(this);
    QAction *playAction = menu.addAction(tr("Play"));
    QAction *removeAction = menu.addAction(tr("Remove from Playlist"));
    menu.addSeparator();
    QAction *showAction = menu.addAction(tr("Show in Folder"));

    playAction->setEnabled(item != nullptr);
    removeAction->setEnabled(!m_playlistView->selectedItems().isEmpty());
    showAction->setEnabled(item != nullptr);

    QAction *chosen = menu.exec(m_playlistView->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == playAction && item) {
        playIndex(m_playlistView->row(item), true);
    } else if (chosen == removeAction) {
        QVector<int> rows;
        for (QListWidgetItem *it : m_playlistView->selectedItems())
            rows << m_playlistView->row(it);
        m_playlist->removeIndices(rows);
    } else if (chosen == showAction && item) {
        const QString path = m_playlist->at(m_playlistView->row(item)).filePath;
#ifdef Q_OS_WIN
        QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
    }
}

void MainWindow::onPlaylistItemsChanged()
{
    refreshPlaylistWidget();
}

void MainWindow::onPlaylistCurrentIndexChanged(int index)
{
    if (index >= 0 && index < m_playlistView->count())
        m_playlistView->setCurrentRow(index);
}

// ---------------------------------------------------------------------------
// Equalizer
// ---------------------------------------------------------------------------

void MainWindow::onEqEnabledToggled(bool on)
{
    m_engine->equalizer().setEnabled(on);
}

void MainWindow::onEqPresetChanged(const QString &name)
{
    if (m_restoringState)
        return;
    if (!m_engine->equalizer().applyPreset(name))
        return; // "Custom" (or an unrecognized name): keep whatever the sliders already show

    const auto gains = m_engine->equalizer().currentGains();
    for (int b = 0; b < Equalizer::kBandCount; ++b) {
        QSlider *slider = m_eqSliders[static_cast<size_t>(b)];
        slider->blockSignals(true);
        slider->setValue(static_cast<int>(gains[static_cast<size_t>(b)] * 10));
        slider->blockSignals(false);
        m_eqValueLabels[static_cast<size_t>(b)]->setText(
            QString::number(gains[static_cast<size_t>(b)], 'f', 1) + " dB");
    }
}

void MainWindow::onEqBandSliderChanged(int band, int value)
{
    const double db = value / 10.0;
    m_engine->equalizer().setBandGain(band, db);
    m_eqValueLabels[static_cast<size_t>(band)]->setText(QString::number(db, 'f', 1) + " dB");

    if (!m_restoringState) {
        m_eqPresetCombo->blockSignals(true);
        m_eqPresetCombo->setCurrentIndex(m_eqPresetCombo->count() - 1); // "Custom" is always last
        m_eqPresetCombo->blockSignals(false);
    }
}

// ---------------------------------------------------------------------------
// Visualizer
// ---------------------------------------------------------------------------

void MainWindow::onVizStyleChanged(const QString &name)
{
    m_visualizer->setStyle(Visualizer::styleFromName(name));
}

void MainWindow::onVizColorSchemeChanged(const QString &name)
{
    m_visualizer->setColorScheme(Visualizer::colorSchemeFromName(name));
}

void MainWindow::onVizEnabledToggled(bool on)
{
    m_visualizer->setVisible(on);
    m_visualizer->setActive(on && m_engine->state() == AudioEngine::State::Playing);
}

// ---------------------------------------------------------------------------
// Shutdown timer
// ---------------------------------------------------------------------------

void MainWindow::onShutdownStartClicked()
{
    const int hours = m_shutdownHoursSpin->value();
    const int minutes = m_shutdownMinutesSpin->value();
    const int seconds = m_shutdownSecondsSpin->value();
    const qint64 totalSeconds = static_cast<qint64>(hours) * 3600 + static_cast<qint64>(minutes) * 60 + seconds;

    if (totalSeconds <= 0) {
        QMessageBox::information(this, tr("Shutdown Timer"), tr("Please set a time greater than zero."));
        return;
    }

    const bool alsoShutdownComputer = m_shutdownCloseAndComputerRadio->isChecked();
    if (alsoShutdownComputer) {
        // Extra confirmation only for the destructive option - shutting
        // down the whole PC, not just this app - so a misclick doesn't
        // silently arm it. The countdown display plus the Cancel button
        // are the ongoing safety net after that.
        const auto reply =
            QMessageBox::question(this, tr("Shutdown Timer"),
                                   tr("This will close FreeMusicPlayer and shut down your computer in %1. Continue?")
                                       .arg(formatCountdown(totalSeconds)),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    m_shutdownTargetTime = QDateTime::currentDateTime().addSecs(totalSeconds);

    m_shutdownHoursSpin->setEnabled(false);
    m_shutdownMinutesSpin->setEnabled(false);
    m_shutdownSecondsSpin->setEnabled(false);
    m_shutdownCloseOnlyRadio->setEnabled(false);
    m_shutdownCloseAndComputerRadio->setEnabled(false);
    m_shutdownStartBtn->setEnabled(false);
    m_shutdownCancelBtn->setEnabled(true);

    m_shutdownStatusLabel->setText(alsoShutdownComputer
                                        ? tr("The program will close and the computer will shut down when this reaches zero.")
                                        : tr("The program will close when this reaches zero."));
    m_shutdownCountdownLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::kAccentHi));

    if (!m_shutdownTimer) {
        m_shutdownTimer = new QTimer(this);
        m_shutdownTimer->setInterval(1000);
        connect(m_shutdownTimer, &QTimer::timeout, this, &MainWindow::onShutdownTimerTick);
    }
    onShutdownTimerTick(); // paint the starting value immediately instead of waiting a full second for the first tick
    m_shutdownTimer->start();
}

void MainWindow::onShutdownCancelClicked()
{
    if (m_shutdownTimer)
        m_shutdownTimer->stop();

    m_shutdownHoursSpin->setEnabled(true);
    m_shutdownMinutesSpin->setEnabled(true);
    m_shutdownSecondsSpin->setEnabled(true);
    m_shutdownCloseOnlyRadio->setEnabled(true);
    m_shutdownCloseAndComputerRadio->setEnabled(true);
    m_shutdownStartBtn->setEnabled(true);
    m_shutdownCancelBtn->setEnabled(false);

    m_shutdownCountdownLabel->setStyleSheet(QString()); // back to the default theme color from Theme.h
    m_shutdownCountdownLabel->setText(tr("--:--:--"));
    m_shutdownStatusLabel->setText(tr("Timer canceled."));
}

void MainWindow::onShutdownTimerTick()
{
    // Recomputed from the wall clock each tick (rather than just
    // decrementing a counter) so the countdown stays accurate even if a
    // 1-second QTimer tick occasionally lands late.
    const qint64 remainingSecs = QDateTime::currentDateTime().secsTo(m_shutdownTargetTime);
    if (remainingSecs <= 0) {
        m_shutdownTimer->stop();
        m_shutdownCountdownLabel->setText(formatCountdown(0));
        performScheduledShutdown(m_shutdownCloseAndComputerRadio->isChecked());
        return;
    }
    m_shutdownCountdownLabel->setText(formatCountdown(remainingSecs));
}

// ---------------------------------------------------------------------------
// AudioEngine callbacks
// ---------------------------------------------------------------------------

void MainWindow::onEngineStateChanged(AudioEngine::State state)
{
    const bool playing = (state == AudioEngine::State::Playing);
    updatePlayPauseIcon(playing);
    m_visualizer->setActive(playing && m_vizEnableCheck->isChecked());
    m_seekSlider->setEnabled(state != AudioEngine::State::Loading);
}

void MainWindow::onEngineTrackLoaded(qint64 durationMs)
{
    m_seekSlider->setEnabled(true);
    m_seekSlider->setRange(0, static_cast<int>(durationMs / 1000));
    m_durationLabel->setText(formatTime(durationMs));
}

void MainWindow::onEngineDurationChanged(qint64 durationMs)
{
    m_seekSlider->setRange(0, static_cast<int>(durationMs / 1000));
    m_durationLabel->setText(formatTime(durationMs));
}

void MainWindow::onEnginePositionChanged(qint64 positionMs)
{
    m_positionLabel->setText(formatTime(positionMs));
    if (!m_seekSliderDragging)
        m_seekSlider->setValue(static_cast<int>(positionMs / 1000));
}

void MainWindow::onEnginePlaybackFinished()
{
    if (!m_playlist->advanceToNext()) {
        updatePlayPauseIcon(false);
        m_visualizer->setActive(false);
        return;
    }

    // Always go through the full reload (AudioEngine::loadFile), even when
    // the "next" track is the same index we were just on (repeat-one, or
    // repeat-all with only one track in the playlist). This used to take a
    // seek(0)+play() shortcut instead to skip the redecode, but by the time
    // this runs the engine already went through handlePlaybackEnded() ->
    // setState(Stopped), so seek()'s "was it playing before?" check saw
    // Stopped and skipped restarting the QAudioSink itself; play() calling
    // m_sink->start() right after should have covered that, but in practice
    // the sink did not reliably resume producing audible output after
    // reaching a natural end-of-stream this way (reported: track just goes
    // silent, as if Stop had been pressed, instead of looping). Recreating
    // the QAudioSink via a full reload sidesteps whatever state the sink was
    // left in - same short decode pause already accepted for every other
    // track change, just also paid here now.
    playIndex(m_playlist->currentIndex(), true);
}

void MainWindow::onEngineError(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::onEngineFormatDescriptionChanged(const QString &text)
{
    m_formatLabel->setText(text);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MainWindow::playIndex(int index, bool autoPlay)
{
    if (index < 0 || index >= m_playlist->count())
        return;

    m_playlist->setCurrentIndex(index);
    const QString path = m_playlist->currentFilePath();

    m_engine->loadFile(path, autoPlay);
    updateCoverArt(path);
    refreshTrackInfoLabels(path);
}

void MainWindow::updatePlayPauseIcon(bool playing)
{
    m_playBtn->setIcon(IconFactory::make(playing ? IconFactory::Glyph::Pause : IconFactory::Glyph::Play,
                                          Qt::white, 26));
}

void MainWindow::updateShuffleIcon()
{
    const QColor color = m_playlist->shuffle() ? QColor(Qt::white) : QColor(Theme::kText);
    m_shuffleBtn->setIcon(IconFactory::make(IconFactory::Glyph::Shuffle, color));
    m_shuffleBtn->setChecked(m_playlist->shuffle());
}

void MainWindow::updateRepeatIcon()
{
    using RM = Playlist::RepeatMode;
    const RM mode = m_playlist->repeatMode();
    m_repeatBtn->setChecked(mode != RM::Off);

    // The button background already turns accent-purple whenever checked
    // (the shared #TransportButton:checked QSS rule in Theme.h), which used
    // to make "All" and "One" look identical at a glance - both purple,
    // differing only by a small "1" badge on the icon (see IconFactory::
    // drawRepeat). That made it easy to think you're on Repeat All while
    // actually on Repeat One - which by design never advances to the next
    // track, replaying the same one forever - and read that as "repeat all
    // is broken". Giving each mode its own icon color removes the ambiguity.
    QColor color;
    if (mode == RM::Off)
        color = QColor(Theme::kText);
    else if (mode == RM::All)
        color = QColor(Qt::white);
    else // One
        color = QColor("#ffd166"); // warm amber - distinct from All's white and Off's dim text

    m_repeatBtn->setIcon(IconFactory::make(mode == RM::One ? IconFactory::Glyph::RepeatOne
                                                             : IconFactory::Glyph::RepeatAll,
                                            color));
    m_repeatBtn->setToolTip(mode == RM::Off ? tr("Repeat: Off") : mode == RM::All ? tr("Repeat: All") : tr("Repeat: One"));
}

void MainWindow::updateVolumeIcon()
{
    const QColor color(Theme::kText);
    IconFactory::Glyph glyph;
    if (m_engine->isMuted() || m_volumeSlider->value() == 0)
        glyph = IconFactory::Glyph::VolumeMute;
    else if (m_volumeSlider->value() < 50)
        glyph = IconFactory::Glyph::VolumeLow;
    else
        glyph = IconFactory::Glyph::VolumeHigh;
    m_muteBtn->setIcon(IconFactory::make(glyph, color));
}

void MainWindow::refreshTrackInfoLabels(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const TrackTags tags = TagEditor::readTags(filePath);
    m_titleLabel->setText(!tags.title.isEmpty() ? tags.title : fi.completeBaseName());
    m_artistLabel->setText(!tags.artist.isEmpty() ? tags.artist : fi.dir().dirName());
}

void MainWindow::onEditTagClicked()
{
    const QString path = m_playlist->currentFilePath();
    if (path.isEmpty()) {
        statusBar()->showMessage(tr("Load a track first to edit its tags."), 3000);
        return;
    }

    TrackTags current = TagEditor::readTags(path);
    if (current.title.isEmpty())
        current.title = m_titleLabel->text();
    if (current.artist.isEmpty())
        current.artist = m_artistLabel->text();

    const bool canWrite = TagEditor::writeSupported(path);
    TagEditDialog dialog(current.title, current.artist, current.album, canWrite, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    TrackTags updated;
    updated.title = dialog.title();
    updated.artist = dialog.artist();
    updated.album = dialog.album();

    QString errorMessage;
    if (!TagEditor::writeTags(path, updated, &errorMessage)) {
        QMessageBox::warning(this, tr("Couldn't Save Tag"), errorMessage);
        return;
    }

    refreshTrackInfoLabels(path);
    statusBar()->showMessage(tr("Tag saved."), 3000);
}

void MainWindow::updateCoverArt(const QString &filePath)
{
    const CoverArt art = CoverArtExtractor::extract(filePath);
    if (art.valid && CoverArtExtractor::imageFormatSupported(art.mimeType)) {
        QPixmap pm;
        if (pm.loadFromData(art.imageData)) {
            m_coverLabel->setPixmap(pm.scaled(m_coverLabel->size(), Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation));
            return;
        }
    }

    // No embedded/folder artwork found (or the image plugin needed to
    // decode it isn't available) - draw a placeholder rather than leaving
    // a blank frame.
    QPixmap placeholder(m_coverLabel->size());
    placeholder.fill(Qt::transparent);
    QPainter p(&placeholder);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::kBg3));
    p.drawRoundedRect(placeholder.rect(), 10, 10);
    p.setPen(QColor(Theme::kTextDim));
    QFont f = p.font();
    f.setPointSize(28);
    p.setFont(f);
    p.drawText(placeholder.rect(), Qt::AlignCenter, QString::fromUtf8("\xe2\x99\xab")); // ♫
    p.end();
    m_coverLabel->setPixmap(placeholder);
}

void MainWindow::refreshPlaylistWidget()
{
    m_playlistView->blockSignals(true);
    m_playlistView->clear();
    for (int i = 0; i < m_playlist->count(); ++i)
        m_playlistView->addItem(m_playlist->at(i).displayName);
    if (m_playlist->currentIndex() >= 0 && m_playlist->currentIndex() < m_playlistView->count())
        m_playlistView->setCurrentRow(m_playlist->currentIndex());
    m_playlistView->blockSignals(false);
}

void MainWindow::addFilesToPlaylist(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const bool wasEmpty = m_playlist->isEmpty();
    m_playlist->addFiles(paths);
    if (wasEmpty && !m_playlist->isEmpty() && m_engine->currentFilePath().isEmpty()) {
        // Show the first track's info without starting playback.
        const QString path = m_playlist->currentFilePath();
        refreshTrackInfoLabels(path);
        updateCoverArt(path);
    }
}

void MainWindow::performScheduledShutdown(bool alsoShutdownComputer)
{
    // Skip closeEvent()'s "Are you sure you want to exit?" prompt - this is
    // an automated action the user already confirmed when starting the
    // timer (and confirmed a second time for the shutdown-computer option
    // specifically), not an accidental click on the window's close button.
    m_shutdownExitInProgress = true;

#ifdef Q_OS_WIN
    if (alsoShutdownComputer) {
        // /f forces other running applications to close without their own
        // prompts, /t 0 shuts down immediately. Launched detached so the
        // scheduled shutdown proceeds independently of - and isn't lost
        // when - our own process exits right after this.
        QProcess::startDetached("shutdown", {"/s", "/f", "/t", "0"});
    }
#else
    if (alsoShutdownComputer)
        QProcess::startDetached("shutdown", {"-h", "now"});
#endif

    close();
}

QString MainWindow::formatTime(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 totalSeconds = ms / 1000;
    const qint64 h = totalSeconds / 3600;
    const qint64 m = (totalSeconds % 3600) / 60;
    const qint64 s = totalSeconds % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void MainWindow::restoreSettings()
{
    m_restoringState = true;

    restoreGeometry(m_settings.windowGeometry());

    const QStringList saved = m_settings.playlistFiles();
    QStringList existing;
    int missing = 0;
    for (const QString &p : saved) {
        if (QFileInfo::exists(p))
            existing << p;
        else
            ++missing;
    }
    if (!existing.isEmpty())
        m_playlist->addFiles(existing);
    if (missing > 0) {
        statusBar()->showMessage(
            tr("%1 track(s) from your last session could no longer be found and were skipped.").arg(missing),
            6000);
    }

    const int lastIndex = m_settings.lastPlaylistIndex();
    // Restore the selection, but only when the list wasn't shortened by
    // missing files - otherwise the saved index would point at the wrong
    // song. Never auto-play on startup either way.
    if (missing == 0 && lastIndex >= 0 && lastIndex < m_playlist->count())
        m_playlist->setCurrentIndex(lastIndex);
    else if (!m_playlist->isEmpty())
        m_playlist->setCurrentIndex(0);

    const int vol = m_settings.volume();
    m_volumeSlider->setValue(vol);
    m_engine->setVolume(vol);
    m_engine->setMuted(m_settings.muted());

    m_playlist->setShuffle(m_settings.shuffleMode() != 0);
    m_shuffleBtn->setChecked(m_playlist->shuffle());
    updateShuffleIcon();

    m_playlist->setRepeatMode(static_cast<Playlist::RepeatMode>(m_settings.repeatMode()));
    updateRepeatIcon();

    m_engine->equalizer().setEnabled(m_settings.eqEnabled());
    m_eqEnableCheck->setChecked(m_settings.eqEnabled());

    const QString presetName = m_settings.eqPresetName();
    if (presetName == QLatin1String("Custom")) {
        const QVector<double> gains = m_settings.eqCustomGains();
        if (gains.size() == Equalizer::kBandCount) {
            std::array<double, Equalizer::kBandCount> arr{};
            for (int i = 0; i < Equalizer::kBandCount; ++i)
                arr[static_cast<size_t>(i)] = gains[i];
            m_engine->equalizer().setCustomGains(arr);
        }
        m_eqPresetCombo->setCurrentIndex(m_eqPresetCombo->count() - 1);
    } else {
        m_engine->equalizer().applyPreset(presetName);
        const int idx = m_eqPresetCombo->findText(presetName);
        if (idx >= 0)
            m_eqPresetCombo->setCurrentIndex(idx);
    }
    {
        // Reflect the already-applied gains on the sliders without routing
        // back through onEqBandSliderChanged() - that slot unconditionally
        // marks the equalizer's current preset as "Custom", which would
        // silently corrupt a restored built-in preset name even though the
        // values it writes back are numerically identical.
        const auto gains = m_engine->equalizer().currentGains();
        for (int b = 0; b < Equalizer::kBandCount; ++b) {
            QSlider *slider = m_eqSliders[static_cast<size_t>(b)];
            slider->blockSignals(true);
            slider->setValue(static_cast<int>(gains[static_cast<size_t>(b)] * 10));
            slider->blockSignals(false);
            m_eqValueLabels[static_cast<size_t>(b)]->setText(
                QString::number(gains[static_cast<size_t>(b)], 'f', 1) + " dB");
        }
    }

    m_vizEnableCheck->setChecked(m_settings.visualizerEnabled());
    m_visualizer->setVisible(m_settings.visualizerEnabled());
    const QString vizStyle = m_settings.visualizerStyle();
    const int vidx = m_vizStyleCombo->findText(vizStyle);
    if (vidx >= 0)
        m_vizStyleCombo->setCurrentIndex(vidx);
    m_visualizer->setStyle(Visualizer::styleFromName(vizStyle));

    const QString vizColorScheme = m_settings.visualizerColorScheme();
    const int vcidx = m_vizColorSchemeCombo->findText(vizColorScheme);
    if (vcidx >= 0)
        m_vizColorSchemeCombo->setCurrentIndex(vcidx);
    m_visualizer->setColorScheme(Visualizer::colorSchemeFromName(vizColorScheme));

    updateVolumeIcon();
    refreshPlaylistWidget();

    if (!m_playlist->isEmpty()) {
        const QString path = m_playlist->currentFilePath();
        refreshTrackInfoLabels(path);
        updateCoverArt(path);
        m_engine->loadFile(path, false); // restore selection, but never auto-play on startup
    }

    m_restoringState = false;
}

void MainWindow::saveSettings()
{
    m_settings.setWindowGeometry(saveGeometry());

    m_settings.setPlaylistFiles(m_playlist->filePaths());
    m_settings.setLastPlaylistIndex(m_playlist->currentIndex());
    m_settings.setLastPlayedFile(m_engine->currentFilePath());
    m_settings.setLastPlaybackPositionMs(m_engine->positionMs());

    m_settings.setVolume(m_volumeSlider->value());
    m_settings.setMuted(m_engine->isMuted());
    m_settings.setShuffleMode(m_playlist->shuffle() ? 1 : 0);
    m_settings.setRepeatMode(static_cast<int>(m_playlist->repeatMode()));

    m_settings.setEqEnabled(m_engine->equalizer().isEnabled());
    const QString presetName = m_engine->equalizer().currentPresetName();
    m_settings.setEqPresetName(presetName);
    if (presetName == QLatin1String("Custom")) {
        const auto gains = m_engine->equalizer().currentGains();
        QVector<double> v(gains.begin(), gains.end());
        m_settings.setEqCustomGains(v);
    }

    m_settings.setVisualizerEnabled(m_vizEnableCheck->isChecked());
    m_settings.setVisualizerStyle(Visualizer::styleToName(m_visualizer->style()));
    m_settings.setVisualizerColorScheme(Visualizer::colorSchemeToName(m_visualizer->colorScheme()));

    m_settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_shutdownExitInProgress) {
        const auto answer = QMessageBox::question(this, tr("Exit"), tr("Are you sure you want to exit?"),
                                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }

    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    addFilesToPlaylist(paths);
}
