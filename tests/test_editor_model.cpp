#include <gtest/gtest.h>
#include <QSignalSpy>
#include "EditorModel.h"
#include "DeviceRegistry.h"

TEST(EditorModel, EditingFlagAndInitialState) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, /*editing=*/true);
    EXPECT_TRUE(m.editing());
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_TRUE(m.activeDevicePath().isEmpty());
}

TEST(EditorModel, ActiveDevicePathSetterEmitsSignals) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    QSignalSpy pathSpy(&m, &logitune::EditorModel::activeDevicePathChanged);
    QSignalSpy dirtySpy(&m, &logitune::EditorModel::dirtyChanged);
    m.setActiveDevicePath(QStringLiteral("/tmp/foo"));
    EXPECT_EQ(pathSpy.count(), 1);
    EXPECT_EQ(dirtySpy.count(), 1);
    EXPECT_EQ(m.activeDevicePath(), QStringLiteral("/tmp/foo"));
}
