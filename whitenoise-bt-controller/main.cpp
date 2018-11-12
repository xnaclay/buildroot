#include <QDebug>
#include <QCoreApplication>
#include <QtBluetooth>
#include "bt_server.h"

int main(int argc, char *argv[]) {
  qInfo() << "starting up whitenoise-bt-controller";
  QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
  QCoreApplication a(argc, argv);

  bt_server server(&a);

  return a.exec();
}
