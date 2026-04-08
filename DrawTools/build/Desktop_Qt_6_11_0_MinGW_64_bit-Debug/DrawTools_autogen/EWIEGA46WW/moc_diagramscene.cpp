/****************************************************************************
** Meta object code from reading C++ file 'diagramscene.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../diagramscene.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'diagramscene.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
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
struct qt_meta_tag_ZN12DiagramSceneE_t {};
} // unnamed namespace

template <> constexpr inline auto DiagramScene::qt_create_metaobjectdata<qt_meta_tag_ZN12DiagramSceneE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DiagramScene",
        "itemInserted",
        "",
        "QGraphicsItem*",
        "item",
        "modeChanged",
        "Mode",
        "mode",
        "SelectMode",
        "InsertRectMode",
        "InsertEllipseMode",
        "InsertLineMode",
        "InsertArrowMode",
        "InsertJunctionDotMode",
        "InsertTextMode",
        "InsertSymbolMode"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'itemInserted'
        QtMocHelpers::SignalData<void(QGraphicsItem *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'modeChanged'
        QtMocHelpers::SignalData<void(enum Mode)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Mode'
        QtMocHelpers::EnumData<enum Mode>(6, 6, QMC::EnumFlags{}).add({
            {    8, Mode::SelectMode },
            {    9, Mode::InsertRectMode },
            {   10, Mode::InsertEllipseMode },
            {   11, Mode::InsertLineMode },
            {   12, Mode::InsertArrowMode },
            {   13, Mode::InsertJunctionDotMode },
            {   14, Mode::InsertTextMode },
            {   15, Mode::InsertSymbolMode },
        }),
    };
    return QtMocHelpers::metaObjectData<DiagramScene, qt_meta_tag_ZN12DiagramSceneE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DiagramScene::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsScene::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12DiagramSceneE_t>.metaTypes,
    nullptr
} };

void DiagramScene::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DiagramScene *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->itemInserted((*reinterpret_cast<std::add_pointer_t<QGraphicsItem*>>(_a[1]))); break;
        case 1: _t->modeChanged((*reinterpret_cast<std::add_pointer_t<enum Mode>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QGraphicsItem* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(QGraphicsItem * )>(_a, &DiagramScene::itemInserted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(Mode )>(_a, &DiagramScene::modeChanged, 1))
            return;
    }
}

const QMetaObject *DiagramScene::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiagramScene::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsScene::qt_metacast(_clname);
}

int DiagramScene::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsScene::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void DiagramScene::itemInserted(QGraphicsItem * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void DiagramScene::modeChanged(Mode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
