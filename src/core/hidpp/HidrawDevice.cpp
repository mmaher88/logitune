#include "HidrawDevice.h"
#include "HidppTypes.h"

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#include <cerrno>
#include <cstring>

namespace logitune::hidpp {

HidrawDevice::HidrawDevice(const QString &path)
    : m_path(path)
{
    m_info.path = path;
}

HidrawDevice::~HidrawDevice()
{
    close();
}

bool HidrawDevice::open()
{
    if (m_fd >= 0)
        return true; // already open

    const QByteArray pathBytes = m_path.toLocal8Bit();
    m_fd = ::open(pathBytes.constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0)
        return false;

    struct hidraw_devinfo rawInfo{};
    if (ioctl(m_fd, HIDIOCGRAWINFO, &rawInfo) < 0) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    m_info.vendorId  = static_cast<uint16_t>(rawInfo.vendor);
    m_info.productId = static_cast<uint16_t>(rawInfo.product);
    return true;
}

void HidrawDevice::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool HidrawDevice::isOpen() const
{
    return m_fd >= 0;
}

int HidrawDevice::fd() const
{
    return m_fd;
}

HidrawDevice::DeviceInfo HidrawDevice::info() const
{
    return m_info;
}

int HidrawDevice::writeReport(std::span<const uint8_t> data)
{
    if (m_fd < 0)
        return -1;
    return static_cast<int>(::write(m_fd, data.data(), data.size()));
}

std::vector<uint8_t> HidrawDevice::readReport(int timeoutMs)
{
    if (m_fd < 0)
        return {};

    struct pollfd pfd{};
    pfd.fd     = m_fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeoutMs);
    if (ret <= 0)
        return {}; // timeout or error

    if (!(pfd.revents & POLLIN))
        return {};

    std::vector<uint8_t> buf(kLongReportSize);
    ssize_t n = ::read(m_fd, buf.data(), buf.size());
    if (n <= 0)
        return {};

    buf.resize(static_cast<size_t>(n));
    return buf;
}

} // namespace logitune::hidpp
