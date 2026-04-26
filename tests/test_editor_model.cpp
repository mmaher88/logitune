#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "EditorModel.h"
#include "DeviceRegistry.h"

namespace {
QString writeMinimalDescriptor(const QString &dir) {
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/descriptor.json"));
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(R"({
  "name": "Tester",
  "status": "beta",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": [
    {"xPct": 0.10, "yPct": 0.20},
    {"xPct": 0.30, "yPct": 0.40}
  ]
})");
    f.close();
    return QFileInfo(dir).canonicalFilePath();
}
}

TEST(EditorModel, EditingFlagAndInitialState) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, /*editing=*/true);
    EXPECT_TRUE(m.editing());
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_TRUE(m.activeDevicePath().isEmpty());
}

TEST(EditorModel, ActiveDevicePathSetterEmitsSignals) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    QSignalSpy pathSpy(&m, &logitune::EditorModel::activeDevicePathChanged);
    QSignalSpy dirtySpy(&m, &logitune::EditorModel::dirtyChanged);
    m.setActiveDevicePath(QStringLiteral("/tmp/foo"));
    EXPECT_EQ(pathSpy.count(), 1);
    EXPECT_EQ(dirtySpy.count(), 1);
    EXPECT_EQ(m.activeDevicePath(), QStringLiteral("/tmp/foo"));
}

TEST(EditorModel, UpdateSlotPositionMutatesPendingAndPushesUndo) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    QSignalSpy dirtySpy(&m, &logitune::EditorModel::dirtyChanged);
    QSignalSpy undoSpy(&m, &logitune::EditorModel::undoStateChanged);

    m.updateSlotPosition(1, 0.50, 0.60);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_GE(dirtySpy.count(), 1);
    EXPECT_GE(undoSpy.count(), 1);
}

TEST(EditorModel, UpdateHotspotMutatesPendingAndPushesUndo) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QDir().mkpath(tmp.path() + QStringLiteral("/dev"));
    QFile f(tmp.path() + QStringLiteral("/dev/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({
  "name": "Tester", "status": "beta", "productIds": ["0xffff"],
  "features": {}, "controls": [],
  "hotspots": {
    "buttons": [
      {"buttonIndex": 0, "xPct": 0.10, "yPct": 0.20, "side": "left", "labelOffsetYPct": 0.0}
    ],
    "scroll": []
  },
  "images": {}, "easySwitchSlots": []
})");
    f.close();
    const QString path = QFileInfo(tmp.path() + QStringLiteral("/dev")).canonicalFilePath();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.updateHotspot(0, 0.55, 0.66, QStringLiteral("right"), 0.10);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
}

TEST(EditorModel, UpdateScrollHotspotMutatesScrollArray) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QDir().mkpath(tmp.path() + QStringLiteral("/dev"));
    QFile f(tmp.path() + QStringLiteral("/dev/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({
  "name": "Tester", "status": "beta", "productIds": ["0xffff"],
  "features": {}, "controls": [],
  "hotspots": {
    "buttons": [],
    "scroll": [
      {"buttonIndex": -1, "xPct": 0.10, "yPct": 0.20, "side": "left", "labelOffsetYPct": 0.0, "kind": "scrollwheel"}
    ]
  },
  "images": {}, "easySwitchSlots": []
})");
    f.close();
    const QString path = QFileInfo(tmp.path() + QStringLiteral("/dev")).canonicalFilePath();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.updateScrollHotspot(0, 0.55, 0.66, QStringLiteral("right"), 0.10);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());

    m.undo();
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
}

TEST(EditorModel, UndoRestoresSlotPosition) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.updateSlotPosition(0, 0.99, 0.99);
    EXPECT_TRUE(m.canUndo());
    m.undo();

    EXPECT_FALSE(m.canUndo());
    EXPECT_TRUE(m.canRedo());
    EXPECT_FALSE(m.hasUnsavedChanges());

    m.redo();
    EXPECT_TRUE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_TRUE(m.hasUnsavedChanges());
}

TEST(EditorModel, UpdateTextEditsAllThreeKindsAndUndoes) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QDir().mkpath(tmp.path() + QStringLiteral("/dev"));
    QFile f(tmp.path() + QStringLiteral("/dev/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({
  "name": "Original Name", "status": "beta", "productIds": ["0xffff"],
  "features": {},
  "controls": [
    {"controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left", "defaultActionType": "default", "configurable": false}
  ],
  "hotspots": {"buttons": [], "scroll": []}, "images": {},
  "easySwitchSlots": [{"xPct": 0.1, "yPct": 0.2}]
})");
    f.close();
    const QString path = QFileInfo(tmp.path() + QStringLiteral("/dev")).canonicalFilePath();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.updateText(QStringLiteral("deviceName"), -1, QStringLiteral("New Name"));
    m.updateText(QStringLiteral("controlDisplayName"), 0, QStringLiteral("Primary"));
    m.updateText(QStringLiteral("slotLabel"), 0, QStringLiteral("Mac"));

    EXPECT_TRUE(m.canUndo());
    m.undo(); m.undo(); m.undo();
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
}

TEST(EditorModel, PerDeviceStacksAreIsolated) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString pathA = writeMinimalDescriptor(tmp.path() + QStringLiteral("/devA"));
    const QString pathB = writeMinimalDescriptor(tmp.path() + QStringLiteral("/devB"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);

    m.setActiveDevicePath(pathA);
    m.updateSlotPosition(0, 0.5, 0.5);
    EXPECT_TRUE(m.canUndo());
    EXPECT_TRUE(m.hasUnsavedChanges());

    m.setActiveDevicePath(pathB);
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.hasUnsavedChanges());

    m.updateSlotPosition(0, 0.7, 0.7);
    EXPECT_TRUE(m.canUndo());

    m.setActiveDevicePath(pathA);
    EXPECT_TRUE(m.canUndo());
    EXPECT_TRUE(m.hasUnsavedChanges());
}

TEST(EditorModel, SaveWritesPendingAndClearsState) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    QSignalSpy savedSpy(&m, &logitune::EditorModel::saved);
    m.updateSlotPosition(0, 0.55, 0.66);
    m.save();

    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_EQ(savedSpy.count(), 1);

    QFile f(path + QStringLiteral("/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    EXPECT_DOUBLE_EQ(obj[QStringLiteral("easySwitchSlots")].toArray()[0].toObject()[QStringLiteral("xPct")].toDouble(), 0.55);
}

TEST(EditorModel, SaveFailurePreservesState) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.55, 0.66);

    // Remove the descriptor so QSaveFile can't write (works even as root)
    QFile::remove(path + QStringLiteral("/descriptor.json"));
    QDir(path).removeRecursively();

    QSignalSpy failSpy(&m, &logitune::EditorModel::saveFailed);
    m.save();

    EXPECT_EQ(failSpy.count(), 1);
    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
}

TEST(EditorModel, ResetDiscardsPendingAndClearsStacks) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.updateSlotPosition(0, 0.99, 0.99);
    EXPECT_TRUE(m.hasUnsavedChanges());

    m.reset();

    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());

    QFile f(path + QStringLiteral("/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    EXPECT_DOUBLE_EQ(obj[QStringLiteral("easySwitchSlots")].toArray()[0].toObject()[QStringLiteral("xPct")].toDouble(), 0.10);
}

TEST(EditorModel, ReplaceImageCopiesFileAndUpdatesPending) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    const QString src = tmp.path() + QStringLiteral("/source-image.png");
    QFile sf(src);
    ASSERT_TRUE(sf.open(QIODevice::WriteOnly));
    sf.write("\x89PNG\r\n");
    sf.close();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    m.replaceImage(QStringLiteral("back"), src);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
    EXPECT_TRUE(QFile::exists(path + QStringLiteral("/back.png")));
}

TEST(EditorModel, ExternalChangeSilentReloadWhenNotDirty) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);
    m.onExternalFileChanged(path + QStringLiteral("/descriptor.json"));

    EXPECT_EQ(externalSpy.count(), 0);
}

TEST(EditorModel, ExternalChangeWhileDirtyEmitsConflictSignal) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.99, 0.99);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);
    m.onExternalFileChanged(path + QStringLiteral("/descriptor.json"));

    EXPECT_EQ(externalSpy.count(), 1);
    EXPECT_EQ(externalSpy.first().first().toString(), path);
    EXPECT_TRUE(m.hasUnsavedChanges());
}

TEST(EditorModel, SaveSuppressesOwnWatcherFire) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + QStringLiteral("/dev"));

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, nullptr, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.55, 0.66);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);
    m.save();

    m.onExternalFileChanged(path + QStringLiteral("/descriptor.json"));

    EXPECT_EQ(externalSpy.count(), 0);
}
