#include <algorithm>
#include <sstream>
#include "bt_server.h"

bt_server::bt_server(QObject *parent) :
    QObject(parent),
    rfcomm_server(QBluetoothServiceInfo::RfcommProtocol, this) {

  QBluetoothLocalDevice local_device;

  if (!local_device.isValid()) {
    std::stringstream err_msg;
    err_msg << "no valid BT device";
    throw std::runtime_error(err_msg.str());
  }

  qInfo() << "powering on device";
  local_device.powerOn();
  qInfo() << "becoming discoverable";
  local_device.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

  connect(&rfcomm_server, SIGNAL(newConnection()), this, SLOT(client_connected()));
  if (!rfcomm_server.listen(local_device.address())) {
    std::stringstream err_msg;
    err_msg << "cannot bind server to"
            << local_device.address().toString().toStdString();
    throw std::runtime_error(err_msg.str());
  }

  QBluetoothServiceInfo::Sequence class_id;
  class_id << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));

  service_info.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList, class_id);
  class_id.prepend(QVariant::fromValue(QBluetoothUuid(BT_SERVER_UUID)));

  service_info.setAttribute(QBluetoothServiceInfo::ServiceClassIds, class_id);

  service_info.setAttribute(QBluetoothServiceInfo::ServiceName, tr("White Noise Controller"));
  service_info.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                            tr("Controls white noise played over a Bluetooth speaker."));
  service_info.setAttribute(QBluetoothServiceInfo::ServiceProvider, tr("whitenoise-bt-controller"));

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
}

bt_server::~bt_server() {
  service_info.unregisterService();

  for (QBluetoothSocket *socket : client_sockets) {
    delete socket;
  }
}

void bt_server::client_connected() {
  QBluetoothSocket *socket = rfcomm_server.nextPendingConnection();

  if (!socket) {
    return;
  }

  connect(socket, SIGNAL(readyRead()), this, SLOT(read_socket()));
  connect(socket, SIGNAL(disconnected()), this, SLOT(client_disconnected()));
  client_sockets.push_back(socket);

  qInfo() << "client connected: " << socket->peerName();
}

void bt_server::client_disconnected() {
  auto *socket = qobject_cast<QBluetoothSocket *>(sender());

  if (!socket) {
    return;
  }

  qInfo() << "client disconnected: " << socket->peerName();

  client_sockets.erase(
      std::remove(client_sockets.begin(),
                  client_sockets.end(),
                  socket),
      client_sockets.end());

  socket->deleteLater();
}

void bt_server::read_socket() {
  auto *socket = qobject_cast<QBluetoothSocket *>(sender());

  if (!socket) {
    return;
  }

  while (socket->canReadLine()) {
    QByteArray line = socket->readLine().trimmed();
    qInfo() << "read message from "
            << socket->peerName()
            << ": "
            << QString::fromUtf8(line.constData(), line.length());

    socket->write("OK\n");
  }
}
