#include <algorithm>
#include <sstream>
#include <iostream>
#include <string>

#include <QDebug>
#include <QCoreApplication>
#include <QtBluetooth>
#include <QtMultimedia>

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

struct app_context {
  std::vector<QBluetoothSocket *> client_sockets;

  QMediaPlaylist playlist;
  QMediaPlayer player;
};

void client_disconnected(app_context &ctx,
                         QBluetoothSocket *socket) {
  std::cerr << "client disconnected: " << socket->peerName().toStdString() << std::endl;

  ctx.client_sockets.erase(
      std::remove(ctx.client_sockets.begin(),
                  ctx.client_sockets.end(),
                  socket),
      ctx.client_sockets.end());

  socket->deleteLater();
}

std::vector<std::string> parse_cmd(const std::string &cmd) {
  std::string delim = ",";
  std::vector<std::string> cmdv;

  unsigned long start = 0U;
  unsigned long end = cmd.find(delim);

  while (end != std::string::npos) {
    cmdv.emplace_back(cmd.substr(start, end - start));
    start = end + delim.length();
    end = cmd.find(delim, start);
  }

  cmdv.emplace_back(cmd.substr(start, end));

  return std::move(cmdv);
}

void play(app_context &ctx) {
  std::cerr << "playing sound" << std::endl;

  ctx.playlist.clear();
  ctx.playlist.addMedia(QUrl::fromLocalFile("/usr/lib/pink.wav"));
  ctx.playlist.setPlaybackMode(QMediaPlaylist::Loop);

  ctx.player.setPlaylist(&ctx.playlist);
  ctx.player.setVolume(50);
  ctx.player.play();
}

void stop(app_context &ctx) {
  std::cerr << "stopping sound" << std::endl;
  ctx.player.stop();
}

void vol_up(app_context &ctx) {
}

void vol_down(app_context &ctx) {
}

void bt_discover(app_context &ctx) {
}

void bt_connect(app_context &ctx) {
}

void read_socket(app_context &ctx,
                 QBluetoothSocket *socket) {
  while (socket->canReadLine()) {
    QByteArray line = socket->readLine().trimmed();
    std::string recv_cmd(line.constData(), static_cast<unsigned long>(line.length()));
    std::cerr << "read message from "
              << socket->peerName().toStdString()
              << ": "
              << recv_cmd
              << std::endl;

    auto cmdv = parse_cmd(recv_cmd);

    std::cerr << "cmdv[0]: " << cmdv[0].c_str() << std::endl;

    std::map<std::string, std::function<void()>> cmd_dispatch;

    cmd_dispatch.emplace("PLAY", [&ctx]() {
      play(ctx);
    });

    cmd_dispatch.emplace("STOP", [&ctx]() {
      stop(ctx);
    });

    cmd_dispatch.emplace("SCAN", []() {
      std::cerr << "handling SCAN command" << std::endl;
    });

    cmd_dispatch.emplace("CONNECT", []() {
      std::cerr << "handling CONNECT command" << std::endl;
    });

    auto cmd_it = cmd_dispatch.find(cmdv[0]);

    if (cmd_it != cmd_dispatch.end()) {
      cmd_dispatch[cmd_it->first]();
      socket->write("OK\n");
    } else {
      socket->write("ERR\n");
    }
  }
}

void client_connected(QBluetoothServer &rfcomm_server,
                      app_context &ctx) {
  QBluetoothSocket *socket = rfcomm_server.nextPendingConnection();

  if (!socket) {
    return;
  }

  QObject::connect(socket, &QBluetoothSocket::readyRead, [&ctx, socket]() {
    read_socket(ctx, socket);
  });
  QObject::connect(socket, &QBluetoothSocket::disconnected, [&ctx, socket]() {
    client_disconnected(ctx, socket);
  });
  ctx.client_sockets.push_back(socket);

  std::cerr << "client connected: " << socket->peerName().toStdString() << std::endl;
}

int main(int argc, char *argv[]) {
  std::cerr << "starting up whitenoise-bt-controller" << std::endl;
  QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true\nqt.multimedia = true\n*.debug = true"));
  QCoreApplication a(argc, argv);

  app_context ctx;

  QBluetoothServer rfcomm_server(QBluetoothServiceInfo::RfcommProtocol, &a);
  QBluetoothServiceInfo service_info;

  QBluetoothLocalDevice local_device;

  if (!local_device.isValid()) {
    std::cerr << "no valid BT device" << std::endl;
    return 1;
  }

  std::cerr << "powering on device" << std::endl;
  local_device.powerOn();
  std::cerr << "becoming discoverable" << std::endl;
  local_device.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

  if (!rfcomm_server.listen(local_device.address())) {
    std::cerr << "cannot bind server to: "
              << local_device.address().toString().toStdString()
              << std::endl;
    return 1;
  }

  QObject::connect(&rfcomm_server, &QBluetoothServer::newConnection, [&rfcomm_server, &ctx]() {
    client_connected(rfcomm_server, ctx);
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

  for (QBluetoothSocket *socket : ctx.client_sockets) {
    delete socket;
  }

  return result;
}
