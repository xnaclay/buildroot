#include <algorithm>
#include <sstream>

#include <QDebug>
#include <QCoreApplication>
#include <QtBluetooth>

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

void client_disconnected(std::vector<QBluetoothSocket *> &client_sockets,
                         QBluetoothSocket *socket) {
  qInfo() << "client disconnected: " << socket->peerName();

  client_sockets.erase(
      std::remove(client_sockets.begin(),
                  client_sockets.end(),
                  socket),
      client_sockets.end());

  socket->deleteLater();
}

void read_socket(QBluetoothSocket *socket) {
  while (socket->canReadLine()) {
    QByteArray line = socket->readLine().trimmed();
    qInfo() << "read message from "
            << socket->peerName()
            << ": "
            << QString::fromUtf8(line.constData(), line.length());

    socket->write("OK\n");
  }
}

void client_connected(QBluetoothServer &rfcomm_server,
                      std::vector<QBluetoothSocket *> &client_sockets) {
  QBluetoothSocket *socket = rfcomm_server.nextPendingConnection();

  if (!socket) {
    return;
  }

  QObject::connect(socket, &QBluetoothSocket::readyRead, [socket]() {
    read_socket(socket);
  });
  QObject::connect(socket, &QBluetoothSocket::disconnected, [&client_sockets, socket]() {
    client_disconnected(client_sockets, socket);
  });
  client_sockets.push_back(socket);

  qInfo() << "client connected: " << socket->peerName();
}

int main(int argc, char *argv[]) {
  qInfo() << "starting up whitenoise-bt-controller";
  QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
  QCoreApplication a(argc, argv);

  QBluetoothServer rfcomm_server(QBluetoothServiceInfo::RfcommProtocol, &a);
  QBluetoothServiceInfo service_info;
  std::vector<QBluetoothSocket *> client_sockets;

  QBluetoothLocalDevice local_device;

  if (!local_device.isValid()) {
    qCritical() << "no valid BT device";
    return 1;
  }

  qInfo() << "powering on device";
  local_device.powerOn();
  qInfo() << "becoming discoverable";
  local_device.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

  if (!rfcomm_server.listen(local_device.address())) {
    qCritical() << "cannot bind server to: "
                << local_device.address();
    return 1;
  }

  QObject::connect(&rfcomm_server, &QBluetoothServer::newConnection, [&rfcomm_server, &client_sockets]() {
    client_connected(rfcomm_server, client_sockets);
  });

  QBluetoothServiceInfo::Sequence class_id;
  class_id << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));

  service_info.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList, class_id);
  class_id.prepend(QVariant::fromValue(QBluetoothUuid(BT_SERVER_UUID)));

  service_info.setAttribute(QBluetoothServiceInfo::ServiceClassIds, class_id);

  service_info.setAttribute(QBluetoothServiceInfo::ServiceName, "White Noise Controller");
  service_info.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                            "Controls white noise played over a Bluetooth speaker.");
  service_info.setAttribute(QBluetoothServiceInfo::ServiceProvider, "whitenoise-bt-controller");

  service_info.setServiceUuid(QBluetoothUuid(BT_SERVER_UUID));

  QBluetoothServiceInfo::Sequence public_browse;
  public_browse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
  service_info.setAttribute(QBluetoothServiceInfo::BrowseGroupList, public_browse);

  QBluetoothServiceInfo::Sequence protocol_descriptor_list;
  QBluetoothServiceInfo::Sequence protocol;

  protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
  protocol_descriptor_list.append(QVariant::fromValue(protocol));
  protocol.clear();
  protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
           << QVariant::fromValue(quint8(rfcomm_server.serverPort()));
  protocol_descriptor_list.append(QVariant::fromValue(protocol));
  service_info.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocol_descriptor_list);

  service_info.registerService(local_device.address());

  auto result = QCoreApplication::exec();

  service_info.unregisterService();

  for (QBluetoothSocket *socket : client_sockets) {
    delete socket;
  }

  return result;
}
