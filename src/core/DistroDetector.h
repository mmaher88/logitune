#pragma once

#include <QString>

namespace logitune::util {

enum class DistroFamily {
    Unknown,
    Arch,
    Debian,
    Fedora,
};

// Parse /etc/os-release once per process and classify the distro.
// Result is cached in a function-local static for subsequent calls.
DistroFamily detectDistroFamily();

// Test hook: classify from a specific file path. Does NOT cache.
// Intended for unit tests that feed synthetic content.
DistroFamily detectDistroFamilyFromFile(const QString &path);

} // namespace logitune::util
