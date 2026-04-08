#include "desktop/GenericDesktop.h"

namespace logitune {

GenericDesktop::GenericDesktop(QObject *parent)
    : LinuxDesktopBase(parent)
{
}

void GenericDesktop::start()
{
    // No-op: generic fallback has no window tracking
}

bool GenericDesktop::available() const
{
    return true;
}

QString GenericDesktop::desktopName() const
{
    return QStringLiteral("Generic");
}

QStringList GenericDesktop::detectedCompositors() const
{
    return {};
}

void GenericDesktop::blockGlobalShortcuts(bool)
{
    // No-op: generic fallback cannot block global shortcuts
}

} // namespace logitune
