#ifndef BT_SERVER_H
#define BT_SERVER_H

#include <memory>

#include <QObject>
#include <QtBluetooth>

QT_USE_NAMESPACE

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

class bt_server : public QObject {
 Q_OBJECT

 public:
  explicit bt_server(QObject *parent = nullptr);
 ~bt_server();

 private slots:
  void client_connected();
  void client_disconnected();
  void read_socket();

 private:
  QBluetoothServer rfcomm_server;
  QBluetoothServiceInfo service_info;
  std::vector<QBluetoothSocket *> client_sockets;
};

#endif // BT_SERVER_H