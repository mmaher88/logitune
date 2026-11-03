#include "HyprlandCursor.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QRectF>

namespace logitune {

static QJsonDocument askHyprland(const QByteArray &request)
{
    const auto signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (signature.isEmpty()) return {};
    const auto path = qEnvironmentVariable("XDG_RUNTIME_DIR") + "/hypr/" + signature + "/.socket.sock";
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(200)) return {};
    socket.write(request);
    if (!socket.waitForBytesWritten(200)) return {};
    QByteArray reply;
    while (socket.waitForReadyRead(200)) reply += socket.readAll();
    return QJsonDocument::fromJson(reply);
}

std::optional<CursorSpot> hyprlandCursor()
{
    const auto cursor = askHyprland("j/cursorpos").object();
    if (!cursor.contains("x") || !cursor.contains("y")) return std::nullopt;
    const QPointF position(cursor.value("x").toDouble(), cursor.value("y").toDouble());
    for (const auto &value : askHyprland("j/monitors").array()) {
        const auto monitor = value.toObject();
        const double scale = monitor.value("scale").toDouble(1);
        if (scale <= 0) continue;
        double width = monitor.value("width").toDouble() / scale;
        double height = monitor.value("height").toDouble() / scale;
        if (monitor.value("transform").toInt() % 2) std::swap(width, height);
        const QRectF bounds(monitor.value("x").toDouble(), monitor.value("y").toDouble(), width, height);
        if (bounds.contains(position))
            return CursorSpot{monitor.value("name").toString(), position - bounds.topLeft()};
    }
    return std::nullopt;
}
}
