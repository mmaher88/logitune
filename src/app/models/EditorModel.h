#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QStack>
#include <QJsonObject>
#include "EditCommand.h"

namespace logitune {

class DeviceRegistry;

class EditorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing CONSTANT)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString activeDevicePath READ activeDevicePath WRITE setActiveDevicePath NOTIFY activeDevicePathChanged)
public:
    explicit EditorModel(DeviceRegistry *registry, bool editing, QObject *parent = nullptr);

    bool editing() const { return m_editing; }
    bool hasUnsavedChanges() const { return m_dirty.contains(m_activeDevicePath); }
    bool canUndo() const;
    bool canRedo() const;
    QString activeDevicePath() const { return m_activeDevicePath; }

public slots:
    void setActiveDevicePath(const QString &path);

signals:
    void dirtyChanged();
    void undoStateChanged();
    void activeDevicePathChanged();

private:
    DeviceRegistry *m_registry;
    bool m_editing;
    QString m_activeDevicePath;
    QHash<QString, QJsonObject> m_pendingEdits;
    QHash<QString, QStack<EditCommand>> m_undoStacks;
    QHash<QString, QStack<EditCommand>> m_redoStacks;
    QSet<QString> m_dirty;
};

} // namespace logitune
