#include <QDebug>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    qInfo() << "starting up whitenoise-bt-controller";
    QCoreApplication a(argc, argv);

    return a.exec();
}
