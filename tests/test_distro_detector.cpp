#include <gtest/gtest.h>
#include <QTemporaryFile>
#include <QTextStream>
#include "DistroDetector.h"

using logitune::util::DistroFamily;

namespace {

// Writes the given body to a temp file and returns its path. The temp
// file stays alive for the duration of the test via the caller's
// std::unique_ptr so QTemporaryFile's destructor does not delete it
// before the detector reads it.
std::unique_ptr<QTemporaryFile> writeOsRelease(const QString &body) {
    auto f = std::make_unique<QTemporaryFile>();
    f->setAutoRemove(true);
    EXPECT_TRUE(f->open());
    QTextStream ts(f.get());
    ts << body;
    ts.flush();
    f->close();
    return f;
}

} // namespace

TEST(DistroDetector, ArchLinux) {
    auto f = writeOsRelease(R"(NAME="Arch Linux"
ID=arch
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Arch);
}

TEST(DistroDetector, ArchLinuxSingleQuoted) {
    auto f = writeOsRelease(R"(NAME='Arch Linux'
ID='arch'
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Arch);
}

TEST(DistroDetector, CachyOSViaIdLike) {
    auto f = writeOsRelease(R"(NAME="CachyOS Linux"
ID=cachyos
ID_LIKE=arch
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Arch);
}

TEST(DistroDetector, Ubuntu) {
    auto f = writeOsRelease(R"(NAME="Ubuntu"
ID=ubuntu
ID_LIKE=debian
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, PopOsViaIdLike) {
    auto f = writeOsRelease(R"(NAME="Pop!_OS"
ID=pop
ID_LIKE="ubuntu debian"
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, PlainDebian) {
    auto f = writeOsRelease(R"(NAME="Debian GNU/Linux"
ID=debian
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, Fedora) {
    auto f = writeOsRelease(R"(NAME="Fedora Linux"
ID=fedora
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Fedora);
}

TEST(DistroDetector, RockyLinuxViaIdLike) {
    auto f = writeOsRelease(R"(NAME="Rocky Linux"
ID=rocky
ID_LIKE="centos rhel fedora"
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Fedora);
}

TEST(DistroDetector, UnknownDistro) {
    auto f = writeOsRelease(R"(NAME="Frankendistro"
ID=frankendistro
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Unknown);
}

TEST(DistroDetector, MissingFileIsUnknown) {
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(
                  QStringLiteral("/nonexistent/path/to/os-release")),
              DistroFamily::Unknown);
}
