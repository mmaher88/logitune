#pragma once
#include <QString>
#include <span>
#include <vector>
#include <cstdint>

namespace logitune::hidpp {

class HidrawDevice {
public:
    struct DeviceInfo {
        uint16_t vendorId{};
        uint16_t productId{};
        QString path;
    };

    explicit HidrawDevice(const QString &path);
    ~HidrawDevice();

    // Non-copyable
    HidrawDevice(const HidrawDevice &) = delete;
    HidrawDevice &operator=(const HidrawDevice &) = delete;

    bool open();     // open fd with O_RDWR | O_NONBLOCK, read HIDIOCGRAWINFO
    void close();
    bool isOpen() const;
    int fd() const;
    DeviceInfo info() const;

    int writeReport(std::span<const uint8_t> data);
    std::vector<uint8_t> readReport(int timeoutMs = 2000); // uses poll() with timeout

private:
    QString m_path;
    int m_fd = -1;
    DeviceInfo m_info{};
};

} // namespace logitune::hidpp
