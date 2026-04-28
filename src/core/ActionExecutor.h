#pragma once
#include "ButtonAction.h"
#include "interfaces/IInputInjector.h"
#include <QObject>
#include <QString>
#include <vector>

namespace logitune {

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
    QString arg;      // optional; empty means "call with no args"
};

class ActionExecutor : public QObject {
    Q_OBJECT
public:
    explicit ActionExecutor(IInputInjector *injector = nullptr, QObject *parent = nullptr);
    void setInjector(IInputInjector *injector);

    void executeAction(const ButtonAction &action);
    void injectKeystroke(const QString &combo);
    void injectCtrlScroll(int direction);
    void injectHorizontalScroll(int direction);
    void executeDBusCall(const QString &spec);
    void launchApp(const QString &command);

    // Static helpers (testable) — parseKeystroke forwards to UinputInjector
    static std::vector<int> parseKeystroke(const QString &combo);
    static DBusCall parseDBusAction(const QString &spec);
    static QString gestureDirectionName(GestureDirection dir);

    GestureDetector &gestureDetector();

private:
    IInputInjector *m_injector;
    GestureDetector m_gestureDetector;
};

} // namespace logitune
