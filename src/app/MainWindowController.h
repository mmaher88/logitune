#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace logitune {

class MainWindowController : public QObject
{
public:
    explicit MainWindowController(QQmlApplicationEngine &engine, bool dark = false);

    bool showMainWindow();

private:
    QQmlApplicationEngine &m_engine;
    QPointer<QQuickWindow> m_window;
    bool m_dark;
    bool m_themeInitialized = false;
};

} // namespace logitune
