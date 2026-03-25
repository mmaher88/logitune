#include "ActionExecutor.h"

#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

// ButtonAction is defined in ProfileEngine.h once that task is complete.
// We include a minimal definition here so ActionExecutor can compile standalone.
#ifndef LOGITUNE_BUTTON_ACTION_DEFINED
#define LOGITUNE_BUTTON_ACTION_DEFINED
namespace logitune {
struct ButtonAction {
    enum class Type {
        None,
        Keystroke,
        DBusCall,
        LaunchApp,
    };
    Type type = Type::None;
    QString payload;  // keystroke combo, D-Bus spec, or command
};
} // namespace logitune
#endif

namespace logitune {

// ---------------------------------------------------------------------------
// GestureDetector
// ---------------------------------------------------------------------------

void GestureDetector::reset()
{
    m_totalDx = 0;
    m_totalDy = 0;
}

void GestureDetector::addDelta(int dx, int dy)
{
    m_totalDx += dx;
    m_totalDy += dy;
}

GestureDirection GestureDetector::resolve() const
{
    const int absDx = m_totalDx < 0 ? -m_totalDx : m_totalDx;
    const int absDy = m_totalDy < 0 ? -m_totalDy : m_totalDy;

    if (absDx < kThreshold && absDy < kThreshold)
        return GestureDirection::Click;

    // Dominant axis wins
    if (absDx >= absDy) {
        return m_totalDx > 0 ? GestureDirection::Right : GestureDirection::Left;
    } else {
        return m_totalDy > 0 ? GestureDirection::Down : GestureDirection::Up;
    }
}

// ---------------------------------------------------------------------------
// ActionExecutor — construction / destruction
// ---------------------------------------------------------------------------

ActionExecutor::ActionExecutor(QObject *parent)
    : QObject(parent)
{
}

ActionExecutor::~ActionExecutor()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// uinput device lifecycle
// ---------------------------------------------------------------------------

bool ActionExecutor::initUinput()
{
    m_uinputFd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_uinputFd < 0)
        return false;

    // Enable key events
    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY);
    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_SYN);

    // Register all keycodes we might emit
    const int keys[] = {
        KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_LEFTALT, KEY_LEFTMETA,
        KEY_TAB, KEY_BACK, KEY_FORWARD,
        KEY_VOLUMEUP, KEY_VOLUMEDOWN,
        KEY_SPACE, KEY_ENTER, KEY_ESC, KEY_DELETE,
        KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
        KEY_F1,  KEY_F2,  KEY_F3,  KEY_F4,
        KEY_F5,  KEY_F6,  KEY_F7,  KEY_F8,
        KEY_F9,  KEY_F10, KEY_F11, KEY_F12,
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
        KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
        KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
        KEY_Y, KEY_Z,
        KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
        KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    };
    for (int k : keys)
        ::ioctl(m_uinputFd, UI_SET_KEYBIT, k);

    struct uinput_setup usetup{};
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x046d;   // Logitech
    usetup.id.product = 0x0001;
    std::strncpy(usetup.name, "logitune-virtual-kbd", UINPUT_MAX_NAME_SIZE);

    if (::ioctl(m_uinputFd, UI_DEV_SETUP, &usetup) < 0) {
        shutdown();
        return false;
    }
    if (::ioctl(m_uinputFd, UI_DEV_CREATE) < 0) {
        shutdown();
        return false;
    }
    return true;
}

void ActionExecutor::shutdown()
{
    if (m_uinputFd >= 0) {
        ::ioctl(m_uinputFd, UI_DEV_DESTROY);
        ::close(m_uinputFd);
        m_uinputFd = -1;
    }
}

// ---------------------------------------------------------------------------
// Low-level uinput helpers
// ---------------------------------------------------------------------------

void ActionExecutor::sendKey(int keycode, bool press)
{
    if (m_uinputFd < 0)
        return;

    struct input_event ev{};
    ev.type  = EV_KEY;
    ev.code  = static_cast<unsigned short>(keycode);
    ev.value = press ? 1 : 0;
    ::write(m_uinputFd, &ev, sizeof(ev));
}

void ActionExecutor::syncUinput()
{
    if (m_uinputFd < 0)
        return;

    struct input_event ev{};
    ev.type  = EV_SYN;
    ev.code  = SYN_REPORT;
    ev.value = 0;
    ::write(m_uinputFd, &ev, sizeof(ev));
}

// ---------------------------------------------------------------------------
// Action dispatch
// ---------------------------------------------------------------------------

void ActionExecutor::executeAction(const ButtonAction &action)
{
    switch (action.type) {
    case ButtonAction::Type::Keystroke:
        injectKeystroke(action.payload);
        break;
    case ButtonAction::Type::DBusCall:
        executeDBusCall(action.payload);
        break;
    case ButtonAction::Type::LaunchApp:
        launchApp(action.payload);
        break;
    case ButtonAction::Type::None:
    default:
        break;
    }
}

void ActionExecutor::injectKeystroke(const QString &combo)
{
    const auto keys = parseKeystroke(combo);
    if (keys.empty())
        return;

    // Press all keys in order
    for (int k : keys)
        sendKey(k, true);
    syncUinput();

    // Release all keys in reverse order
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        sendKey(*it, false);
    syncUinput();
}

void ActionExecutor::executeDBusCall(const QString &spec)
{
    const auto call = parseDBusAction(spec);
    if (call.method.isEmpty())
        return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        call.service, call.path, call.interface, call.method);
    QDBusConnection::sessionBus().send(msg);
}

void ActionExecutor::launchApp(const QString &command)
{
    QProcess::startDetached(command);
}

GestureDetector &ActionExecutor::gestureDetector()
{
    return m_gestureDetector;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::vector<int> ActionExecutor::parseKeystroke(const QString &combo)
{
    const QStringList parts = combo.split(QLatin1Char('+'));
    std::vector<int> keys;
    keys.reserve(static_cast<size_t>(parts.size()));

    for (const QString &part : parts) {
        const QString tok = part.trimmed();

        // Modifiers
        if (tok == QLatin1String("Ctrl"))    { keys.push_back(KEY_LEFTCTRL);  continue; }
        if (tok == QLatin1String("Shift"))   { keys.push_back(KEY_LEFTSHIFT); continue; }
        if (tok == QLatin1String("Alt"))     { keys.push_back(KEY_LEFTALT);   continue; }
        if (tok == QLatin1String("Super"))   { keys.push_back(KEY_LEFTMETA);  continue; }

        // Special keys
        if (tok == QLatin1String("Tab"))         { keys.push_back(KEY_TAB);        continue; }
        if (tok == QLatin1String("Back"))        { keys.push_back(KEY_BACK);        continue; }
        if (tok == QLatin1String("Forward"))     { keys.push_back(KEY_FORWARD);     continue; }
        if (tok == QLatin1String("VolumeUp"))    { keys.push_back(KEY_VOLUMEUP);    continue; }
        if (tok == QLatin1String("VolumeDown"))  { keys.push_back(KEY_VOLUMEDOWN);  continue; }
        if (tok == QLatin1String("Space"))       { keys.push_back(KEY_SPACE);       continue; }
        if (tok == QLatin1String("Enter"))       { keys.push_back(KEY_ENTER);       continue; }
        if (tok == QLatin1String("Escape"))      { keys.push_back(KEY_ESC);         continue; }
        if (tok == QLatin1String("Delete"))      { keys.push_back(KEY_DELETE);      continue; }
        if (tok == QLatin1String("Up"))          { keys.push_back(KEY_UP);          continue; }
        if (tok == QLatin1String("Down"))        { keys.push_back(KEY_DOWN);        continue; }
        if (tok == QLatin1String("Left"))        { keys.push_back(KEY_LEFT);        continue; }
        if (tok == QLatin1String("Right"))       { keys.push_back(KEY_RIGHT);       continue; }

        // F1–F12
        if (tok.startsWith(QLatin1Char('F')) && tok.length() >= 2) {
            bool ok = false;
            int n = tok.mid(1).toInt(&ok);
            if (ok && n >= 1 && n <= 12) {
                keys.push_back(KEY_F1 + (n - 1));
                continue;
            }
        }

        // Single letter A–Z (case-insensitive)
        if (tok.length() == 1) {
            const QChar ch = tok.at(0).toUpper();
            if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
                keys.push_back(KEY_A + (ch.toLatin1() - 'A'));
                continue;
            }
            // Digits 0–9
            if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
                // KEY_0=11, KEY_1=2, ..., KEY_9=10
                // Layout: KEY_1=2, KEY_2=3, ..., KEY_9=10, KEY_0=11
                int digit = ch.toLatin1() - '0';
                int keycode = (digit == 0) ? KEY_0 : (KEY_1 + digit - 1);
                keys.push_back(keycode);
                continue;
            }
        }
    }

    return keys;
}

DBusCall ActionExecutor::parseDBusAction(const QString &spec)
{
    const QStringList parts = spec.split(QLatin1Char(','));
    if (parts.size() != 4)
        return {};   // method will be empty, caller treats as invalid

    return DBusCall{
        parts[0].trimmed(),
        parts[1].trimmed(),
        parts[2].trimmed(),
        parts[3].trimmed(),
    };
}

QString ActionExecutor::gestureDirectionName(GestureDirection dir)
{
    switch (dir) {
    case GestureDirection::None:  return QStringLiteral("None");
    case GestureDirection::Up:    return QStringLiteral("Up");
    case GestureDirection::Down:  return QStringLiteral("Down");
    case GestureDirection::Left:  return QStringLiteral("Left");
    case GestureDirection::Right: return QStringLiteral("Right");
    case GestureDirection::Click: return QStringLiteral("Click");
    }
    return QStringLiteral("None");
}

} // namespace logitune
