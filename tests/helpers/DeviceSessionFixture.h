#pragma once
#include <gtest/gtest.h>
#include <memory>

#include "DeviceRegistry.h"
#include "DeviceSession.h"
#include "helpers/TestFixtures.h"
#include "hidpp/FeatureDispatcher.h"
#include "hidpp/HidrawDevice.h"

namespace logitune::test {

/// Reaches into the transport-owned internals of a DeviceSession.
///
/// enumerateAndSetup() cannot run without hardware — FeatureDispatcher::
/// enumerate() fails against /dev/null and the function bails before it
/// reaches anything worth asserting on. Tests therefore drive the one step
/// that a re-enumeration performs on already-built state: swap the
/// dispatcher, then rebuild what points at it.
class DeviceSessionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_session = std::make_unique<DeviceSession>(
            std::move(hidraw), 0xFF, QStringLiteral("Bluetooth"), &m_registry);
    }

    /// The half of enumerateAndSetup() that survives without a real device:
    /// a fresh FeatureDispatcher replaces the previous one, and everything
    /// holding a pointer into it has to be rebuilt.
    void reenumerate() {
        m_session->m_features = std::make_unique<hidpp::FeatureDispatcher>();
        m_session->rebuildCommandProcessor();
    }

    hidpp::FeatureDispatcher *dispatcher() const {
        return m_session->m_features.get();
    }
    hidpp::CommandProcessor *processor() const {
        return m_session->m_commandProcessor.get();
    }

    DeviceRegistry                 m_registry;
    std::unique_ptr<DeviceSession> m_session;
};

} // namespace logitune::test
