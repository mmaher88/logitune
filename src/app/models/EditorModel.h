#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QStack>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include "EditCommand.h"
#include "devices/DescriptorWriter.h"

namespace logitune {

class DeviceRegistry;
class DeviceModel;

class EditorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing CONSTANT)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString activeDevicePath READ activeDevicePath WRITE setActiveDevicePath NOTIFY activeDevicePathChanged)
public:
    explicit EditorModel(DeviceRegistry *registry, DeviceModel *deviceModel,
                         bool editing, QObject *parent = nullptr);

    bool editing() const { return m_editing; }
    bool hasUnsavedChanges() const { return m_dirty.contains(m_activeDevicePath); }
    bool canUndo() const;
    bool canRedo() const;
    QString activeDevicePath() const { return m_activeDevicePath; }

    Q_INVOKABLE QVariantMap pendingFor(const QString &path) const;

public slots:
    void setActiveDevicePath(const QString &path);
    Q_INVOKABLE void updateSlotPosition(int idx, double xPct, double yPct);
    Q_INVOKABLE void updateHotspot(int hotspotIndex, double xPct, double yPct,
                                   const QString &side, double labelOffsetYPct);
    Q_INVOKABLE void updateScrollHotspot(int hotspotIndex, double xPct, double yPct,
                                         const QString &side, double labelOffsetYPct);
    Q_INVOKABLE void updateText(const QString &field, int index, const QString &value);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void save();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void replaceImage(const QString &role, const QString &sourcePath);
    void onExternalFileChanged(const QString &filePath);

signals:
    void dirtyChanged();
    void undoStateChanged();
    void activeDevicePathChanged();
    void saved(const QString &path);
    void saveFailed(const QString &path, const QString &error);
    void externalChangeDetected(const QString &path);

private:
    void ensurePending(const QString &path);
    void pushCommand(EditCommand cmd);
    void applyCommand(const EditCommand &cmd, bool reverse);
    // Push the current pending JSON into the live JsonDevice owned by the
    // registry and ask DeviceModel to re-emit its property signals so QML
    // bindings re-fetch. No-op if DeviceModel is null (tests).
    void pushStateToActiveDevice();

    DeviceRegistry *m_registry;
    DeviceModel *m_deviceModel;
    bool m_editing;
    QString m_activeDevicePath;
    QHash<QString, QJsonObject> m_pendingEdits;
    QHash<QString, QStack<EditCommand>> m_undoStacks;
    QHash<QString, QStack<EditCommand>> m_redoStacks;
    QSet<QString> m_dirty;
    DescriptorWriter m_writer;
    QSet<QString> m_selfWrittenPaths;
    QFileSystemWatcher *m_watcher = nullptr;
};

} // namespace logitune
