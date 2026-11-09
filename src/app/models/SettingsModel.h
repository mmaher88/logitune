#pragma once
#include <QObject>
#include <QSettings>
#include <QTimer>

namespace logitune {

/// SettingsModel — ViewModel for app-level settings (logging, theme, bug reports).
/// Separates application concerns from DeviceModel's device-specific state.
class SettingsModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool loggingEnabled READ loggingEnabled WRITE setLoggingEnabled NOTIFY loggingEnabledChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath NOTIFY loggingEnabledChanged)
    /// True only in debug builds. Gates developer-only affordances in QML —
    /// notably the Test Exception button, which deliberately crashes the app
    /// and must never be reachable in a shipped build.
    Q_PROPERTY(bool debugBuild READ debugBuild CONSTANT)

public:
    explicit SettingsModel(QObject *parent = nullptr);

    bool loggingEnabled() const;
    void setLoggingEnabled(bool enabled);
    QString logFilePath() const;

    /// Qt defines QT_NO_DEBUG for release configurations; packaging builds all
    /// use CMAKE_BUILD_TYPE=Release, so this is false in every shipped package.
    bool debugBuild() const {
#ifdef QT_NO_DEBUG
        return false;
#else
        return true;
#endif
    }

    Q_INVOKABLE void saveThemeDark(bool dark);
    Q_INVOKABLE void openBugReport();
    Q_INVOKABLE void testCrash() {
        QTimer::singleShot(0, [] { throw std::runtime_error("Test crash from UI"); });
    }

signals:
    void loggingEnabledChanged();
};

} // namespace logitune
