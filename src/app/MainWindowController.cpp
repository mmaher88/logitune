#include "MainWindowController.h"

#include <QQmlExpression>
#include <QTimer>

#ifdef __GLIBC__
#include <malloc.h>
#endif

namespace logitune {

MainWindowController::MainWindowController(QQmlApplicationEngine &engine, bool dark)
    : m_engine(engine), m_dark(dark)
{
}

bool MainWindowController::showMainWindow()
{
    if (!m_window) {
        m_engine.load(QUrl(QStringLiteral("qrc:/Logitune/qml/Main.qml")));
        if (m_engine.rootObjects().isEmpty())
            return false;
        m_window = qobject_cast<QQuickWindow*>(m_engine.rootObjects().last());
        if (!m_window)
            return false;

        auto *window = m_window.data();
        connect(window, &QQuickWindow::closing, this, [this, window]() {
            m_window = nullptr;
            // Defer deletion until Qt has finished delivering the close event.
            window->deleteLater();
        });
        connect(window, &QObject::destroyed, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                m_engine.collectGarbage();
                // Keep live singleton types valid while releasing unused UI components.
                m_engine.trimComponentCache();
#ifdef __GLIBC__
                // Return freed UI heap pages after deferred render cleanup has run.
                QTimer::singleShot(100, this, [this]() {
                    if (!m_window)
                        malloc_trim(0);
                });
#endif
            });
        });

        if (!m_themeInitialized) {
            QQmlExpression theme(QQmlEngine::contextForObject(window), window,
                                 m_dark ? QStringLiteral("Theme.dark = true")
                                        : QStringLiteral("Theme.dark = false"));
            theme.evaluate();
            if (theme.hasError())
                qWarning() << "Theme expression error:" << theme.error().toString();
            m_themeInitialized = true;
        }
    }

    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    return true;
}

} // namespace logitune
