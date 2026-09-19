/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onPlayPauseClicked",
        "",
        "onStopClicked",
        "onPreviousClicked",
        "onNextClicked",
        "onShuffleToggled",
        "on",
        "onRepeatClicked",
        "onMuteToggled",
        "onVolumeSliderMoved",
        "value",
        "onSeekSliderPressed",
        "onSeekSliderReleased",
        "onSeekSliderMoved",
        "onAddFilesClicked",
        "onAddFolderClicked",
        "onAddIsoClicked",
        "onLoadPlaylistClicked",
        "onSavePlaylistClicked",
        "onClearPlaylistClicked",
        "onPlaylistItemActivated",
        "row",
        "onPlaylistContextMenuRequested",
        "QPoint",
        "pos",
        "onPlaylistItemsChanged",
        "onPlaylistCurrentIndexChanged",
        "index",
        "onEqEnabledToggled",
        "onEqPresetChanged",
        "name",
        "onEqBandSliderChanged",
        "band",
        "onVizStyleChanged",
        "onVizColorSchemeChanged",
        "onVizEnabledToggled",
        "onEditTagClicked",
        "onShutdownStartClicked",
        "onShutdownCancelClicked",
        "onShutdownTimerTick",
        "onThemeBackgroundColorClicked",
        "onThemeAccentColorClicked",
        "onThemeResetClicked",
        "onIsoOpenFailed",
        "message",
        "onIsoTrackCountKnown",
        "count",
        "formatLabel",
        "onIsoTrackStarted",
        "title",
        "onIsoTrackProgress",
        "phase",
        "percent",
        "onIsoTrackFailed",
        "onIsoTrackFinished",
        "outFlacPath",
        "onIsoImportFinished",
        "flacPaths",
        "onEngineStateChanged",
        "AudioEngine::State",
        "state",
        "onEngineTrackLoaded",
        "durationMs",
        "onEngineDurationChanged",
        "onEnginePositionChanged",
        "positionMs",
        "onEnginePlaybackFinished",
        "onEngineError",
        "onEngineFormatDescriptionChanged",
        "text",
        "onEngineAudioActive"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onPlayPauseClicked'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onStopClicked'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPreviousClicked'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onNextClicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShuffleToggled'
        QtMocHelpers::SlotData<void(bool)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Slot 'onRepeatClicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMuteToggled'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onVolumeSliderMoved'
        QtMocHelpers::SlotData<void(int)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Slot 'onSeekSliderPressed'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSeekSliderReleased'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSeekSliderMoved'
        QtMocHelpers::SlotData<void(int)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Slot 'onAddFilesClicked'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAddFolderClicked'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAddIsoClicked'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoadPlaylistClicked'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSavePlaylistClicked'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearPlaylistClicked'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPlaylistItemActivated'
        QtMocHelpers::SlotData<void(int)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 22 },
        }}),
        // Slot 'onPlaylistContextMenuRequested'
        QtMocHelpers::SlotData<void(const QPoint &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 24, 25 },
        }}),
        // Slot 'onPlaylistItemsChanged'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPlaylistCurrentIndexChanged'
        QtMocHelpers::SlotData<void(int)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 28 },
        }}),
        // Slot 'onEqEnabledToggled'
        QtMocHelpers::SlotData<void(bool)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Slot 'onEqPresetChanged'
        QtMocHelpers::SlotData<void(const QString &)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 },
        }}),
        // Slot 'onEqBandSliderChanged'
        QtMocHelpers::SlotData<void(int, int)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'onVizStyleChanged'
        QtMocHelpers::SlotData<void(const QString &)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 },
        }}),
        // Slot 'onVizColorSchemeChanged'
        QtMocHelpers::SlotData<void(const QString &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 },
        }}),
        // Slot 'onVizEnabledToggled'
        QtMocHelpers::SlotData<void(bool)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Slot 'onEditTagClicked'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShutdownStartClicked'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShutdownCancelClicked'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShutdownTimerTick'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onThemeBackgroundColorClicked'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onThemeAccentColorClicked'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onThemeResetClicked'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onIsoOpenFailed'
        QtMocHelpers::SlotData<void(const QString &)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
        // Slot 'onIsoTrackCountKnown'
        QtMocHelpers::SlotData<void(int, const QString &)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 47 }, { QMetaType::QString, 48 },
        }}),
        // Slot 'onIsoTrackStarted'
        QtMocHelpers::SlotData<void(int, int, const QString &)>(49, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::Int, 47 }, { QMetaType::QString, 50 },
        }}),
        // Slot 'onIsoTrackProgress'
        QtMocHelpers::SlotData<void(int, const QString &, int)>(51, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::QString, 52 }, { QMetaType::Int, 53 },
        }}),
        // Slot 'onIsoTrackFailed'
        QtMocHelpers::SlotData<void(int, const QString &, const QString &)>(54, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::QString, 50 }, { QMetaType::QString, 45 },
        }}),
        // Slot 'onIsoTrackFinished'
        QtMocHelpers::SlotData<void(int, const QString &, const QString &)>(55, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::QString, 50 }, { QMetaType::QString, 56 },
        }}),
        // Slot 'onIsoImportFinished'
        QtMocHelpers::SlotData<void(const QStringList &)>(57, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QStringList, 58 },
        }}),
        // Slot 'onEngineStateChanged'
        QtMocHelpers::SlotData<void(AudioEngine::State)>(59, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 60, 61 },
        }}),
        // Slot 'onEngineTrackLoaded'
        QtMocHelpers::SlotData<void(qint64)>(62, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 63 },
        }}),
        // Slot 'onEngineDurationChanged'
        QtMocHelpers::SlotData<void(qint64)>(64, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 63 },
        }}),
        // Slot 'onEnginePositionChanged'
        QtMocHelpers::SlotData<void(qint64)>(65, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 66 },
        }}),
        // Slot 'onEnginePlaybackFinished'
        QtMocHelpers::SlotData<void()>(67, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEngineError'
        QtMocHelpers::SlotData<void(const QString &)>(68, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
        // Slot 'onEngineFormatDescriptionChanged'
        QtMocHelpers::SlotData<void(const QString &)>(69, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Slot 'onEngineAudioActive'
        QtMocHelpers::SlotData<void()>(71, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onPlayPauseClicked(); break;
        case 1: _t->onStopClicked(); break;
        case 2: _t->onPreviousClicked(); break;
        case 3: _t->onNextClicked(); break;
        case 4: _t->onShuffleToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->onRepeatClicked(); break;
        case 6: _t->onMuteToggled(); break;
        case 7: _t->onVolumeSliderMoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onSeekSliderPressed(); break;
        case 9: _t->onSeekSliderReleased(); break;
        case 10: _t->onSeekSliderMoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->onAddFilesClicked(); break;
        case 12: _t->onAddFolderClicked(); break;
        case 13: _t->onAddIsoClicked(); break;
        case 14: _t->onLoadPlaylistClicked(); break;
        case 15: _t->onSavePlaylistClicked(); break;
        case 16: _t->onClearPlaylistClicked(); break;
        case 17: _t->onPlaylistItemActivated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->onPlaylistContextMenuRequested((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 19: _t->onPlaylistItemsChanged(); break;
        case 20: _t->onPlaylistCurrentIndexChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->onEqEnabledToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 22: _t->onEqPresetChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->onEqBandSliderChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 24: _t->onVizStyleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->onVizColorSchemeChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->onVizEnabledToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 27: _t->onEditTagClicked(); break;
        case 28: _t->onShutdownStartClicked(); break;
        case 29: _t->onShutdownCancelClicked(); break;
        case 30: _t->onShutdownTimerTick(); break;
        case 31: _t->onThemeBackgroundColorClicked(); break;
        case 32: _t->onThemeAccentColorClicked(); break;
        case 33: _t->onThemeResetClicked(); break;
        case 34: _t->onIsoOpenFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->onIsoTrackCountKnown((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 36: _t->onIsoTrackStarted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 37: _t->onIsoTrackProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 38: _t->onIsoTrackFailed((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 39: _t->onIsoTrackFinished((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 40: _t->onIsoImportFinished((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 41: _t->onEngineStateChanged((*reinterpret_cast<std::add_pointer_t<AudioEngine::State>>(_a[1]))); break;
        case 42: _t->onEngineTrackLoaded((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 43: _t->onEngineDurationChanged((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 44: _t->onEnginePositionChanged((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 45: _t->onEnginePlaybackFinished(); break;
        case 46: _t->onEngineError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 47: _t->onEngineFormatDescriptionChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 48: _t->onEngineAudioActive(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 49)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 49;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 49)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 49;
    }
    return _id;
}
QT_WARNING_POP
