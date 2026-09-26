#pragma once
#include <QQuickImageProvider>
#include <QIcon>
#include <QDir>
#include <QUrl>

namespace logitune {
class AppIconProvider : public QQuickImageProvider {
public:
    AppIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const auto name = QUrl::fromPercentEncoding(id.toUtf8());
        QIcon icon = QDir::isAbsolutePath(name) ? QIcon(name) : QIcon::fromTheme(name);
        if (icon.isNull()) icon = QIcon::fromTheme("application-x-executable");
        const QSize target = requestedSize.isValid() ? requestedSize.expandedTo(QSize(1, 1)).boundedTo(QSize(256, 256)) : QSize(24, 24);
        auto pixmap = icon.pixmap(target);
        if (pixmap.isNull()) {
            pixmap = QPixmap(target);
            pixmap.fill(Qt::transparent);
        }
        if (size) *size = pixmap.size();
        return pixmap;
    }
};
}
