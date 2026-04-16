#pragma once
#include <QString>
#include <QJsonValue>

namespace logitune {

struct EditCommand {
    enum Kind { SlotMove, HotspotMove, ImageReplace, TextEdit };
    Kind kind;
    int index = -1;
    QString role;
    QJsonValue before;
    QJsonValue after;
};

} // namespace logitune
