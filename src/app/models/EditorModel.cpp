#include "EditorModel.h"
#include "DeviceModel.h"
#include "DeviceRegistry.h"
#include "devices/JsonDevice.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>

namespace logitune {

EditorModel::EditorModel(DeviceRegistry *registry, DeviceModel *deviceModel,
                         bool editing, QObject *parent)
    : QObject(parent), m_registry(registry), m_deviceModel(deviceModel), m_editing(editing) {
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &EditorModel::onExternalFileChanged);

    if (m_registry) {
        for (const auto &dev : m_registry->devices()) {
            if (auto *jd = dynamic_cast<const JsonDevice*>(dev.get())) {
                m_watcher->addPath(jd->sourcePath() + QStringLiteral("/descriptor.json"));
            }
        }
    }
}

void EditorModel::onExternalFileChanged(const QString &filePath) {
    const QString devicePath = QFileInfo(filePath).absolutePath();
    const QString canonical = QFileInfo(devicePath).canonicalFilePath();

    if (m_selfWrittenPaths.contains(canonical)) {
        m_selfWrittenPaths.remove(canonical);
        return;
    }

    if (m_dirty.contains(canonical)) {
        emit externalChangeDetected(canonical);
        return;
    }

    if (m_registry)
        m_registry->reload(canonical);
}

QVariantMap EditorModel::pendingFor(const QString &path) const {
    return m_pendingEdits.value(path).toVariantMap();
}

bool EditorModel::canUndo() const {
    auto it = m_undoStacks.find(m_activeDevicePath);
    return it != m_undoStacks.end() && !it->isEmpty();
}

bool EditorModel::canRedo() const {
    auto it = m_redoStacks.find(m_activeDevicePath);
    return it != m_redoStacks.end() && !it->isEmpty();
}

void EditorModel::setActiveDevicePath(const QString &path) {
    if (path == m_activeDevicePath) return;
    m_activeDevicePath = path;
    emit activeDevicePathChanged();
    emit dirtyChanged();
    emit undoStateChanged();
}

void EditorModel::ensurePending(const QString &path) {
    if (m_pendingEdits.contains(path)) return;
    QFile f(path + QStringLiteral("/descriptor.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    m_pendingEdits[path] = QJsonDocument::fromJson(f.readAll()).object();
}

void EditorModel::pushCommand(EditCommand cmd) {
    m_undoStacks[m_activeDevicePath].push(std::move(cmd));
    m_redoStacks[m_activeDevicePath].clear();
    m_dirty.insert(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
}

void EditorModel::pushStateToActiveDevice() {
    if (!m_registry) return;
    if (m_activeDevicePath.isEmpty()) return;
    if (!m_pendingEdits.contains(m_activeDevicePath)) return;

    // findBySourcePath returns const IDevice* because the public registry
    // contract is read-only, but editor mode owns the mutation lifecycle and
    // we know the concrete type is a JsonDevice that can be refreshed in
    // place. Const-cast is safe here.
    const IDevice *constDev = m_registry->findBySourcePath(m_activeDevicePath);
    if (!constDev) return;
    auto *jd = const_cast<JsonDevice*>(dynamic_cast<const JsonDevice*>(constDev));
    if (!jd) return;

    jd->refreshFromObject(m_pendingEdits.value(m_activeDevicePath));

    if (m_deviceModel)
        m_deviceModel->refreshFromActiveDevice();
}

void EditorModel::updateSlotPosition(int idx, double xPct, double yPct) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonArray slotsArr = obj.value(QStringLiteral("easySwitchSlots")).toArray();
    if (idx < 0 || idx >= slotsArr.size()) return;

    EditCommand cmd;
    cmd.kind = EditCommand::SlotMove;
    cmd.index = idx;
    cmd.before = slotsArr[idx];

    QJsonObject slot = slotsArr[idx].toObject();
    slot[QStringLiteral("xPct")] = xPct;
    slot[QStringLiteral("yPct")] = yPct;
    slotsArr[idx] = slot;
    obj[QStringLiteral("easySwitchSlots")] = slotsArr;
    cmd.after = slotsArr[idx];

    pushCommand(std::move(cmd));
    pushStateToActiveDevice();
}

void EditorModel::updateHotspot(int idx, double xPct, double yPct,
                                 const QString &side, double labelOffsetYPct) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonObject hotspots = obj.value(QStringLiteral("hotspots")).toObject();
    QJsonArray buttons = hotspots.value(QStringLiteral("buttons")).toArray();
    if (idx < 0 || idx >= buttons.size()) return;

    EditCommand cmd;
    cmd.kind = EditCommand::HotspotMove;
    cmd.role = QStringLiteral("buttons");
    cmd.index = idx;
    cmd.before = buttons[idx];

    QJsonObject hs = buttons[idx].toObject();
    hs[QStringLiteral("xPct")] = xPct;
    hs[QStringLiteral("yPct")] = yPct;
    hs[QStringLiteral("side")] = side;
    hs[QStringLiteral("labelOffsetYPct")] = labelOffsetYPct;
    buttons[idx] = hs;
    hotspots[QStringLiteral("buttons")] = buttons;
    obj[QStringLiteral("hotspots")] = hotspots;
    cmd.after = buttons[idx];

    pushCommand(std::move(cmd));
    pushStateToActiveDevice();
}

void EditorModel::updateScrollHotspot(int idx, double xPct, double yPct,
                                       const QString &side, double labelOffsetYPct) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonObject hotspots = obj.value(QStringLiteral("hotspots")).toObject();
    QJsonArray scroll = hotspots.value(QStringLiteral("scroll")).toArray();
    if (idx < 0 || idx >= scroll.size()) return;

    EditCommand cmd;
    cmd.kind = EditCommand::HotspotMove;
    cmd.role = QStringLiteral("scroll");
    cmd.index = idx;
    cmd.before = scroll[idx];

    QJsonObject hs = scroll[idx].toObject();
    hs[QStringLiteral("xPct")] = xPct;
    hs[QStringLiteral("yPct")] = yPct;
    hs[QStringLiteral("side")] = side;
    hs[QStringLiteral("labelOffsetYPct")] = labelOffsetYPct;
    scroll[idx] = hs;
    hotspots[QStringLiteral("scroll")] = scroll;
    obj[QStringLiteral("hotspots")] = hotspots;
    cmd.after = scroll[idx];

    pushCommand(std::move(cmd));
    pushStateToActiveDevice();
}

void EditorModel::updateText(const QString &field, int index, const QString &value) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];

    EditCommand cmd;
    cmd.kind = EditCommand::TextEdit;
    cmd.role = field;
    cmd.index = index;

    if (field == QStringLiteral("deviceName")) {
        cmd.before = obj.value(QStringLiteral("name"));
        cmd.after = value;
        obj[QStringLiteral("name")] = value;
    } else if (field == QStringLiteral("controlDisplayName")) {
        QJsonArray controls = obj.value(QStringLiteral("controls")).toArray();
        if (index < 0 || index >= controls.size()) return;
        QJsonObject c = controls[index].toObject();
        cmd.before = c.value(QStringLiteral("displayName"));
        c[QStringLiteral("displayName")] = value;
        controls[index] = c;
        obj[QStringLiteral("controls")] = controls;
        cmd.after = value;
    } else if (field == QStringLiteral("slotLabel")) {
        QJsonArray slotsArr = obj.value(QStringLiteral("easySwitchSlots")).toArray();
        if (index < 0 || index >= slotsArr.size()) return;
        QJsonObject s = slotsArr[index].toObject();
        cmd.before = s.value(QStringLiteral("label"));
        s[QStringLiteral("label")] = value;
        slotsArr[index] = s;
        obj[QStringLiteral("easySwitchSlots")] = slotsArr;
        cmd.after = value;
    } else {
        return;
    }
    pushCommand(std::move(cmd));
    pushStateToActiveDevice();
}

void EditorModel::applyCommand(const EditCommand &cmd, bool reverse) {
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    const QJsonValue &target = reverse ? cmd.before : cmd.after;
    if (cmd.kind == EditCommand::SlotMove) {
        QJsonArray slotsArr = obj.value(QStringLiteral("easySwitchSlots")).toArray();
        slotsArr[cmd.index] = target;
        obj[QStringLiteral("easySwitchSlots")] = slotsArr;
    } else if (cmd.kind == EditCommand::HotspotMove) {
        QJsonObject hotspots = obj.value(QStringLiteral("hotspots")).toObject();
        const QString arrayKey = cmd.role.isEmpty()
            ? QStringLiteral("buttons")
            : cmd.role;
        QJsonArray arr = hotspots.value(arrayKey).toArray();
        arr[cmd.index] = target;
        hotspots[arrayKey] = arr;
        obj[QStringLiteral("hotspots")] = hotspots;
    } else if (cmd.kind == EditCommand::ImageReplace) {
        QJsonObject images = obj.value(QStringLiteral("images")).toObject();
        images[cmd.role] = target.toString();
        obj[QStringLiteral("images")] = images;
    } else if (cmd.kind == EditCommand::TextEdit) {
        if (cmd.role == QStringLiteral("deviceName")) {
            obj[QStringLiteral("name")] = target;
        } else if (cmd.role == QStringLiteral("controlDisplayName")) {
            QJsonArray controls = obj.value(QStringLiteral("controls")).toArray();
            QJsonObject c = controls[cmd.index].toObject();
            c[QStringLiteral("displayName")] = target;
            controls[cmd.index] = c;
            obj[QStringLiteral("controls")] = controls;
        } else if (cmd.role == QStringLiteral("slotLabel")) {
            QJsonArray slotsArr = obj.value(QStringLiteral("easySwitchSlots")).toArray();
            QJsonObject s = slotsArr[cmd.index].toObject();
            s[QStringLiteral("label")] = target;
            slotsArr[cmd.index] = s;
            obj[QStringLiteral("easySwitchSlots")] = slotsArr;
        }
    }
}

void EditorModel::undo() {
    auto &stack = m_undoStacks[m_activeDevicePath];
    if (stack.isEmpty()) return;
    EditCommand cmd = stack.pop();
    applyCommand(cmd, true);
    m_redoStacks[m_activeDevicePath].push(cmd);

    if (stack.isEmpty()) m_dirty.remove(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
    pushStateToActiveDevice();
}

void EditorModel::redo() {
    auto &stack = m_redoStacks[m_activeDevicePath];
    if (stack.isEmpty()) return;
    EditCommand cmd = stack.pop();
    applyCommand(cmd, false);
    m_undoStacks[m_activeDevicePath].push(cmd);
    m_dirty.insert(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
    pushStateToActiveDevice();
}

void EditorModel::save() {
    if (m_activeDevicePath.isEmpty()) return;
    if (!m_pendingEdits.contains(m_activeDevicePath)) return;

    m_selfWrittenPaths.insert(m_activeDevicePath);

    QString err;
    auto result = m_writer.write(m_activeDevicePath,
                                 m_pendingEdits.value(m_activeDevicePath), &err);
    if (result != DescriptorWriter::Ok) {
        m_selfWrittenPaths.remove(m_activeDevicePath);
        emit saveFailed(m_activeDevicePath, err);
        return;
    }

    if (m_registry)
        m_registry->reload(m_activeDevicePath);

    m_pendingEdits.remove(m_activeDevicePath);
    m_undoStacks.remove(m_activeDevicePath);
    m_redoStacks.remove(m_activeDevicePath);
    m_dirty.remove(m_activeDevicePath);

    // The JsonDevice now reflects the on-disk state; bump QML bindings.
    if (m_deviceModel)
        m_deviceModel->refreshFromActiveDevice();

    emit dirtyChanged();
    emit undoStateChanged();
    emit saved(m_activeDevicePath);
}

void EditorModel::replaceImage(const QString &role, const QString &sourcePath) {
    if (m_activeDevicePath.isEmpty()) return;
    if (role != QStringLiteral("front") && role != QStringLiteral("side") && role != QStringLiteral("back")) return;
    if (!QFile::exists(sourcePath)) return;

    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonObject images = obj.value(QStringLiteral("images")).toObject();
    const QString prevName = images.value(role).toString();
    const QString newName = role + QStringLiteral(".png");
    const QString destPath = m_activeDevicePath + QStringLiteral("/") + newName;

    const QString tmpPath = destPath + QStringLiteral(".tmp");
    QFile::remove(tmpPath);
    if (!QFile::copy(sourcePath, tmpPath)) return;
    QFile::remove(destPath);
    if (!QFile::rename(tmpPath, destPath)) {
        QFile::remove(tmpPath);
        return;
    }

    EditCommand cmd;
    cmd.kind = EditCommand::ImageReplace;
    cmd.role = role;
    cmd.before = prevName;
    cmd.after = newName;

    images[role] = newName;
    obj[QStringLiteral("images")] = images;
    pushCommand(std::move(cmd));
    pushStateToActiveDevice();
}

void EditorModel::reset() {
    if (m_activeDevicePath.isEmpty()) return;
    m_pendingEdits.remove(m_activeDevicePath);
    m_undoStacks.remove(m_activeDevicePath);
    m_redoStacks.remove(m_activeDevicePath);
    m_dirty.remove(m_activeDevicePath);
    // Any mutations we pushed into the live JsonDevice now need to be
    // reverted to match the on-disk state.
    if (m_registry)
        m_registry->reload(m_activeDevicePath);
    if (m_deviceModel)
        m_deviceModel->refreshFromActiveDevice();
    emit dirtyChanged();
    emit undoStateChanged();
}

} // namespace logitune
