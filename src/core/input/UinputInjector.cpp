#include "input/UinputInjector.h"
#include "logging/LogManager.h"

#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

namespace logitune {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

UinputInjector::UinputInjector(QObject *parent)
    : IInputInjector(parent)
{
}

UinputInjector::~UinputInjector()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// uinput device lifecycle
// ---------------------------------------------------------------------------

bool UinputInjector::init()
{
    m_uinputFd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_uinputFd < 0)
        return false;

    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY);
    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_SYN);
    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_REL);
    ::ioctl(m_uinputFd, UI_SET_RELBIT, REL_WHEEL);
    ::ioctl(m_uinputFd, UI_SET_RELBIT, REL_HWHEEL);

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
        KEY_MUTE, KEY_PLAYPAUSE, KEY_NEXTSONG, KEY_PREVIOUSSONG, KEY_STOPCD, KEY_SYSRQ,
        KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN,
        KEY_BRIGHTNESSUP, KEY_BRIGHTNESSDOWN,
        KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE,
        KEY_SEMICOLON, KEY_COMMA, KEY_DOT, KEY_SLASH,
        KEY_BACKSLASH, KEY_GRAVE, KEY_APOSTROPHE, KEY_KPPLUS,
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

void UinputInjector::shutdown()
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

void UinputInjector::emitKey(int keycode, bool press)
{
    if (m_uinputFd < 0)
        return;

    struct input_event ev{};
    ev.type  = EV_KEY;
    ev.code  = static_cast<unsigned short>(keycode);
    ev.value = press ? 1 : 0;
    ::write(m_uinputFd, &ev, sizeof(ev));
}

void UinputInjector::emitSync()
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
// Input injection
// ---------------------------------------------------------------------------

void UinputInjector::injectKeystroke(const QString &combo)
{
    const auto keys = parseKeystroke(combo);
    if (keys.empty())
        return;

    // Press all keys in order
    for (int k : keys)
        emitKey(k, true);
    emitSync();

    // Release all keys in reverse order
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        emitKey(*it, false);
    emitSync();
}

void UinputInjector::injectCtrlScroll(int direction)
{
    if (m_uinputFd < 0 || direction == 0)
        return;

    // Press Ctrl
    emitKey(KEY_LEFTCTRL, true);
    emitSync();

    // Emit scroll wheel event
    struct input_event ev{};
    ev.type = EV_REL;
    ev.code = REL_WHEEL;
    ev.value = direction; // +1 = scroll up (zoom in), -1 = scroll down (zoom out)
    ssize_t n = ::write(m_uinputFd, &ev, sizeof(ev));
    qCDebug(lcInput) << "CtrlScroll write:" << n << "bytes, direction=" << direction
                      << "fd=" << m_uinputFd;
    emitSync();

    // Release Ctrl
    emitKey(KEY_LEFTCTRL, false);
    emitSync();
}

void UinputInjector::injectHorizontalScroll(int direction)
{
    if (m_uinputFd < 0 || direction == 0)
        return;

    struct input_event ev{};
    ev.type = EV_REL;
    ev.code = REL_HWHEEL;
    ev.value = direction; // +1 = scroll right, -1 = scroll left
    ::write(m_uinputFd, &ev, sizeof(ev));
    emitSync();
}

void UinputInjector::sendDBusCall(const QString &spec)
{
    const QStringList parts = spec.split(QLatin1Char(','));
    if (parts.size() != 4)
        return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        parts[0].trimmed(), parts[1].trimmed(),
        parts[2].trimmed(), parts[3].trimmed());
    QDBusConnection::sessionBus().send(msg);
}

void UinputInjector::launchApp(const QString &command)
{
    QProcess::startDetached(command);
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::vector<int> UinputInjector::parseKeystroke(const QString &combo)
{
    // Handle bare "+" before splitting (since "+" is the delimiter)
    if (combo == QLatin1String("+")) return { KEY_KPPLUS };

    const QStringList parts = combo.split(QLatin1Char('+'));
    std::vector<int> keys;
    keys.reserve(static_cast<size_t>(parts.size()));

    for (const QString &part : parts) {
        const QString tok = part.trimmed();

        // Modifiers
        if (tok == QLatin1String("Ctrl"))    { keys.push_back(KEY_LEFTCTRL);  continue; }
        if (tok == QLatin1String("Shift"))   { keys.push_back(KEY_LEFTSHIFT); continue; }
        if (tok == QLatin1String("Alt"))     { keys.push_back(KEY_LEFTALT);   continue; }
        if (tok == QLatin1String("Super") || tok == QLatin1String("Meta"))
            { keys.push_back(KEY_LEFTMETA);  continue; }

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
        if (tok == QLatin1String("Left"))        { keys.push_back(KEY_LEFT);          continue; }
        if (tok == QLatin1String("Right"))       { keys.push_back(KEY_RIGHT);        continue; }
        if (tok == QLatin1String("Mute"))        { keys.push_back(KEY_MUTE);         continue; }
        if (tok == QLatin1String("Play"))        { keys.push_back(KEY_PLAYPAUSE);    continue; }
        if (tok == QLatin1String("Next"))        { keys.push_back(KEY_NEXTSONG);     continue; }
        if (tok == QLatin1String("Previous"))    { keys.push_back(KEY_PREVIOUSSONG); continue; }
        if (tok == QLatin1String("Stop"))        { keys.push_back(KEY_STOPCD);       continue; }
        if (tok == QLatin1String("Print"))       { keys.push_back(KEY_SYSRQ);        continue; }
        if (tok == QLatin1String("Home"))        { keys.push_back(KEY_HOME);         continue; }
        if (tok == QLatin1String("End"))         { keys.push_back(KEY_END);          continue; }
        if (tok == QLatin1String("PageUp"))      { keys.push_back(KEY_PAGEUP);       continue; }
        if (tok == QLatin1String("PageDown"))    { keys.push_back(KEY_PAGEDOWN);     continue; }
        if (tok == QLatin1String("BrightnessUp"))   { keys.push_back(KEY_BRIGHTNESSUP);   continue; }
        if (tok == QLatin1String("BrightnessDown")) { keys.push_back(KEY_BRIGHTNESSDOWN); continue; }

        // F1-F12 (F11/F12 are not sequential with F1-F10)
        if (tok.startsWith(QLatin1Char('F')) && tok.length() >= 2) {
            bool ok = false;
            int n = tok.mid(1).toInt(&ok);
            if (ok && n >= 1 && n <= 12) {
                static constexpr int fKeys[12] = {
                    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
                    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
                };
                keys.push_back(fKeys[n - 1]);
                continue;
            }
        }

        // Single letter A-Z (case-insensitive) -- KEY codes follow QWERTY scan order, not alphabetical
        if (tok.length() == 1) {
            const QChar ch = tok.at(0).toUpper();
            if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
                static constexpr int letterKeys[26] = {
                    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
                    KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
                    KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
                    KEY_Y, KEY_Z,
                };
                keys.push_back(letterKeys[ch.toLatin1() - 'A']);
                continue;
            }
            // Digits 0-9
            if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
                // KEY_0=11, KEY_1=2, ..., KEY_9=10
                // Layout: KEY_1=2, KEY_2=3, ..., KEY_9=10, KEY_0=11
                int digit = ch.toLatin1() - '0';
                int keycode = (digit == 0) ? KEY_0 : (KEY_1 + digit - 1);
                keys.push_back(keycode);
                continue;
            }
            // Punctuation / symbol keys.
            //
            // The unshifted set (row 1) maps 1:1 to its scancode. The shifted
            // set (row 2) also maps to the base scancode; callers are
            // expected to have "Shift" as another token in the chord so the
            // press-stack becomes Shift + base = the shifted char.
            //
            // Rationale: the UI captures chords as the characters produced,
            // not the physical scancodes. A user pressing Shift+= saves as
            // "Shift++" (both the modifier name and the shifted output). The
            // split-on-'+' yields ["Shift", "", "+"] and the lone "+" must
            // resolve to KEY_EQUAL so Shift+Equal reconstructs "+". Without
            // these cases the shifted symbols are silently dropped, so e.g.
            // remapping a mouse button to "+" only taps Shift with no key.
            switch (ch.toLatin1()) {
            case '-':  keys.push_back(KEY_MINUS);      continue;
            case '=':  keys.push_back(KEY_EQUAL);      continue;
            case '[':  keys.push_back(KEY_LEFTBRACE);  continue;
            case ']':  keys.push_back(KEY_RIGHTBRACE); continue;
            case ';':  keys.push_back(KEY_SEMICOLON);  continue;
            case ',':  keys.push_back(KEY_COMMA);      continue;
            case '.':  keys.push_back(KEY_DOT);        continue;
            case '/':  keys.push_back(KEY_SLASH);      continue;
            case '\\': keys.push_back(KEY_BACKSLASH);  continue;
            case '`':  keys.push_back(KEY_GRAVE);      continue;
            case '\'': keys.push_back(KEY_APOSTROPHE); continue;
            // Shifted symbols on US QWERTY. Valid only when the chord also
            // contains "Shift" (which the UI captures separately).
            case '_':  keys.push_back(KEY_MINUS);      continue;
            case '+':  keys.push_back(KEY_EQUAL);      continue;
            case '{':  keys.push_back(KEY_LEFTBRACE);  continue;
            case '}':  keys.push_back(KEY_RIGHTBRACE); continue;
            case ':':  keys.push_back(KEY_SEMICOLON);  continue;
            case '<':  keys.push_back(KEY_COMMA);      continue;
            case '>':  keys.push_back(KEY_DOT);        continue;
            case '?':  keys.push_back(KEY_SLASH);      continue;
            case '|':  keys.push_back(KEY_BACKSLASH);  continue;
            case '~':  keys.push_back(KEY_GRAVE);      continue;
            case '"':  keys.push_back(KEY_APOSTROPHE); continue;
            case '!':  keys.push_back(KEY_1);          continue;
            case '@':  keys.push_back(KEY_2);          continue;
            case '#':  keys.push_back(KEY_3);          continue;
            case '$':  keys.push_back(KEY_4);          continue;
            case '%':  keys.push_back(KEY_5);          continue;
            case '^':  keys.push_back(KEY_6);          continue;
            case '&':  keys.push_back(KEY_7);          continue;
            case '*':  keys.push_back(KEY_8);          continue;
            case '(':  keys.push_back(KEY_9);          continue;
            case ')':  keys.push_back(KEY_0);          continue;
            }
        }

    }

    return keys;
}

} // namespace logitune
