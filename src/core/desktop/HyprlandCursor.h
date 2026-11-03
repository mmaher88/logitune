#pragma once
#include <QPointF>
#include <QString>
#include <optional>

namespace logitune {
struct CursorSpot {
    QString screenName;
    QPointF positionOnScreen;
};
std::optional<CursorSpot> hyprlandCursor();
}
