#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <memory>
#include "services/DeviceSelection.h"
#include "models/DeviceModel.h"
#include "helpers/TestFixtures.h"

namespace logitune::test {

class DeviceSelectionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        m_deviceModel = std::make_unique<DeviceModel>();
        m_selection   = std::make_unique<DeviceSelection>(m_deviceModel.get());
    }

    std::unique_ptr<DeviceModel>     m_deviceModel;
    std::unique_ptr<DeviceSelection> m_selection;
};

} // namespace logitune::test
