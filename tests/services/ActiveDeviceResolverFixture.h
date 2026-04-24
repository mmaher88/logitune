#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <memory>
#include "services/ActiveDeviceResolver.h"
#include "models/DeviceModel.h"
#include "helpers/TestFixtures.h"

namespace logitune::test {

class ActiveDeviceResolverFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        m_deviceModel = std::make_unique<DeviceModel>();
        m_selection   = std::make_unique<ActiveDeviceResolver>(m_deviceModel.get());
    }

    std::unique_ptr<DeviceModel>     m_deviceModel;
    std::unique_ptr<ActiveDeviceResolver> m_selection;
};

} // namespace logitune::test
