#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");

    QQmlApplicationEngine engine;
    engine.loadFromModule("Logitune", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
