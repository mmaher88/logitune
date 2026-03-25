#pragma once
#include <QObject>
#include <QString>
#include <vector>

namespace logitune {

// Forward declaration — ButtonAction is defined in ProfileEngine.h (Task 8)
struct ButtonAction;

enum class GestureDirection { None, Up, Down, Left, Right, Click };

class GestureDetector {
public:
    void reset();
    void addDelta(int dx, int dy);
    GestureDirection resolve() const;

private:
    int m_totalDx = 0;
    int m_totalDy = 0;
    static constexpr int kThreshold = 50;  // HID++ delta units
};

struct DBusCall {
    QString service;
    QString path;
    QString interface;
    QString method;
};

class ActionExecutor : public QObject {
    Q_OBJECT
public:
    explicit ActionExecutor(QObject *parent = nullptr);
    ~ActionExecutor();

    bool initUinput();   // Create /dev/uinput virtual keyboard
    void shutdown();     // Destroy uinput device

    void executeAction(const ButtonAction &action);
    void injectKeystroke(const QString &combo);
    void executeDBusCall(const QString &spec);
    void launchApp(const QString &command);

    // Static helpers (testable)
    static std::vector<int> parseKeystroke(const QString &combo);
    static DBusCall parseDBusAction(const QString &spec);
    static QString gestureDirectionName(GestureDirection dir);

    GestureDetector &gestureDetector();

private:
    int m_uinputFd = -1;
    GestureDetector m_gestureDetector;
    void sendKey(int keycode, bool press);
    void syncUinput();
};

} // namespace logitune
