#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include <unistd.h>

#include <QDebug>
#include <QCoreApplication>
#include <QtBluetooth>
#include <QtMultimedia>

#include <BluezQt/InitManagerJob>
#include <BluezQt/Manager>
#include <BluezQt/Device>
#include <QtBluetooth/QBluetoothLocalDevice>

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

struct app_context {
  std::vector<QBluetoothSocket *> client_sockets;

  BluezQt::Manager manager;

  QSettings settings;
  QBluetoothLocalDevice local_device;
  QBluetoothDeviceDiscoveryAgent disco_agent;

  bool connected_speaker = false;
  QBluetoothAddress speaker_device;

  QTimer scan_timer;
  QTimer advertise_timer;

  QTimer player_timer;
  bool playing = false;
  bool player_switch = false;
  QMediaPlayer player_a;
  QMediaPlayer player_b;
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

  return cmdv;
}

void save_state(app_context &ctx) {

  std::ofstream play_state_of;
  play_state_of.open(".whitenoise-bt-controller.state.play");
  if (ctx.playing) {
    play_state_of << "true";
  } else {
    play_state_of << "false";
  }
  play_state_of.close();

  std::ofstream vol_state_of;
  vol_state_of.open(".whitenoise-bt-controller.state.vol");
  vol_state_of << ctx.player_a.volume();
  vol_state_of.close();

  sync();
}

void report_status(app_context &ctx) {
  int vol = ctx.player_a.volume();

  for (const auto &socket : ctx.client_sockets) {
    socket->write("VOL,");
    socket->write(std::to_string(vol).c_str());
    socket->write("\n");

    if (ctx.connected_speaker) {
      socket->write("CONNECTED_SPEAKER,");
      socket->write(ctx.speaker_device.toString().toStdString().c_str());
      socket->write("\n");
    } else {
      socket->write("DISCONNECTED_SPEAKER");
      socket->write("\n");
    }

    if (ctx.playing) {
      socket->write("PLAYING");
      socket->write("\n");
    } else {
      socket->write("STOPPED");
      socket->write("\n");
    }
  }
}

void play(app_context &ctx) {
  std::cerr << "playing sound" << std::endl;

  ctx.playing = true;

  ctx.player_a.setPosition(0);
  ctx.player_a.play();

  ctx.player_timer.setInterval(30000);
  ctx.player_timer.start();

  report_status(ctx);
  save_state(ctx);
}

void restore_state(app_context &ctx) {
  int vol;
  std::ifstream vol_state_if;
  vol_state_if.open(".whitenoise-bt-controller.state.vol");

  if (!vol_state_if.fail()) {
    vol_state_if >> vol;
    std::cerr << "restoring volume: " << vol << std::endl;
    ctx.player_a.setVolume(vol);
    ctx.player_b.setVolume(vol);
  }

  vol_state_if.close();

  std::string play_state_txt;
  std::ifstream play_state_if;
  if (!play_state_if.fail()) {
    play_state_if.open(".whitenoise-bt-controller.state.play");
    play_state_if >> play_state_txt;

    if (play_state_txt == "true") {
      std::cerr << "restoring playing state" << std::endl;
      play(ctx);
    }
  }

  play_state_if.close();

  if (ctx.settings.contains("speaker.address")) {
    QBluetoothAddress speaker_addr(ctx.settings.value("speaker.address").toString());
    std::cerr << "restoring speaker device: "
              << speaker_addr.toString().toStdString()
              << std::endl;
    ctx.speaker_device = speaker_addr;
  }
}

void play_interval(app_context &ctx) {
  if (ctx.player_switch) {
    ctx.player_a.setPosition(0);
    ctx.player_a.play();
  } else {
    ctx.player_b.setPosition(0);
    ctx.player_b.play();
  }

  ctx.player_switch = !ctx.player_switch;
}

void stop(app_context &ctx) {
  std::cerr << "stopping sound" << std::endl;
  ctx.player_a.pause();
  ctx.player_a.setPosition(0);
  ctx.player_b.pause();
  ctx.player_b.setPosition(0);
  ctx.player_timer.stop();
  ctx.player_switch = false;
  ctx.playing = false;
  report_status(ctx);
  save_state(ctx);
}

void vol_up(app_context &ctx) {
  std::cerr << "increasing volume by 3%" << std::endl;
  ctx.player_a.setVolume(ctx.player_a.volume() + 3);
  ctx.player_b.setVolume(ctx.player_a.volume());
  report_status(ctx);
  save_state(ctx);
}

void vol_down(app_context &ctx) {
  std::cerr << "reducing volume by 3%" << std::endl;
  ctx.player_a.setVolume(ctx.player_a.volume() - 3);
  ctx.player_b.setVolume(ctx.player_a.volume());
  report_status(ctx);
  save_state(ctx);
}

void set_vol(app_context &ctx, int vol) {
  std::cerr << "setting volume to: " << vol << "%" << std::endl;
  ctx.player_a.setVolume(vol);
  ctx.player_b.setVolume(ctx.player_a.volume());
  report_status(ctx);
  save_state(ctx);
}

void bt_discover(app_context &ctx) {
  ctx.disco_agent.start();
}

void bt_connect(const app_context &ctx, const QBluetoothAddress &address) {
  for (const auto &device : ctx.manager.devices()) {
    std::cerr << "known device: " << device->address().toStdString() << std::endl;

    if (QString::compare(device->address(), address.toString()) == 0) {
      std::cerr << "found device; will connect: " << device->address().toStdString() << std::endl;
      device->connectToDevice();
      return;
    }
  }

  std::cerr << "could not find device to connect: "
            << address.toString().toStdString()
            << std::endl;
}

void bt_device_discovered(app_context &ctx, const QBluetoothDeviceInfo &device) {
  std::cerr << "discovered device: "
            << device.address().toString().toStdString()
            << " - "
            << device.name().toStdString()
            << std::endl;

  for (const auto &socket : ctx.client_sockets) {
    socket->write("BT_DEVICE,");
    socket->write(device.address().toString().replace(",", "_").toUtf8());
    socket->write(",");
    socket->write(device.name().replace(",", "_").toUtf8());
    socket->write("\n");
  }

  // attempt to auto connect if paired
  if (ctx.local_device.pairingStatus(device.address()) != QBluetoothLocalDevice::Unpaired) {
    bool is_connected = false;

    for (const QBluetoothAddress &connected_addr : ctx.local_device.connectedDevices()) {
      if (QString::compare(connected_addr.toString(), device.address().toString()) == 0) {
        is_connected = true;
      }
    }

    if (!is_connected && QString::compare(device.address().toString(), ctx.speaker_device.toString()) == 0) {
      bt_connect(ctx, device.address());
    }
  }
}

void bt_pairing_finished(app_context &ctx,
                         const QBluetoothAddress &address,
                         QBluetoothLocalDevice::Pairing pairing) {

  if (pairing == QBluetoothLocalDevice::Unpaired) {
    std::cerr << "failed to pair device: "
              << address.toString().toStdString()
              << std::endl;
    return;
  }

  std::cerr << "device "
            << address.toString().toStdString()
            << " finished pairing; will attempt to connect"
            << std::endl;
  bt_connect(ctx, address);
}

void bt_connected(app_context &ctx, const QBluetoothAddress &address) {

  if (QString::compare(address.toString(), ctx.speaker_device.toString()) == 0) {
    std::cerr << "speaker device connected: "
              << address.toString().toStdString()
              << std::endl;
    ctx.connected_speaker = true;
    report_status(ctx);
  }
}

void bt_disconnected(app_context &ctx, const QBluetoothAddress &address) {

  if (QString::compare(address.toString(), ctx.speaker_device.toString()) == 0) {
    std::cerr << "speaker device disconnected: "
              << address.toString().toStdString()
              << std::endl;
    ctx.connected_speaker = false;
    report_status(ctx);
  }
}

void bt_pair_or_connect(app_context &ctx, const QBluetoothAddress &address) {
  std::cerr << "connecting to device: " << address.toString().toStdString() << std::endl;

  if (ctx.local_device.pairingStatus(address) == QBluetoothLocalDevice::Unpaired) {
    std::cerr << "device is unpaired; will pair: " << address.toString().toStdString() << std::endl;
    ctx.local_device.requestPairing(address, QBluetoothLocalDevice::AuthorizedPaired);
  } else {
    bt_connect(ctx, address);
  }
}

void bt_unpair_speaker(app_context &ctx) {
  std::cerr << "removing speaker device" << std::endl;
  ctx.local_device.requestPairing(ctx.speaker_device, QBluetoothLocalDevice::Unpaired);
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

    for (unsigned long i = 0; i < cmdv.size(); i++) {
      std::cerr << "cmdv["
                << i
                << "]: "
                << cmdv[i]
                << std::endl;
    }

    std::map<std::string, std::function<void()>> cmd_dispatch;

    cmd_dispatch.emplace("PLAY", [&ctx]() {
      play(ctx);
    });

    cmd_dispatch.emplace("STOP", [&ctx]() {
      stop(ctx);
    });

    cmd_dispatch.emplace("VOL_UP", [&ctx]() {
      vol_up(ctx);
    });

    cmd_dispatch.emplace("VOL_DOWN", [&ctx]() {
      vol_down(ctx);
    });

    cmd_dispatch.emplace("SET_VOL", [&ctx, &cmdv]() {
      set_vol(ctx, std::stoi(cmdv[1]));
    });

    cmd_dispatch.emplace("SCAN", [&ctx]() {
      bt_discover(ctx);
    });

    cmd_dispatch.emplace("CONNECT", [&ctx, &cmdv]() {
      ctx.speaker_device = QBluetoothAddress(QString::fromStdString(cmdv[1]));
      ctx.settings.setValue("speaker.address", QString::fromStdString(cmdv[1]));
      bt_pair_or_connect(ctx, QBluetoothAddress(QString::fromStdString(cmdv[1])));
    });

    cmd_dispatch.emplace("UNPAIR_SPEAKER", [&ctx]() {
      bt_unpair_speaker(ctx);
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

  QObject::connect(socket,
                   &QBluetoothSocket::readyRead,
                   [&ctx, socket]() {
                     read_socket(ctx, socket);
                   });
  QObject::connect(socket,
                   &QBluetoothSocket::disconnected,
                   [&ctx, socket]() {
                     client_disconnected(ctx, socket);
                   });
  ctx.client_sockets.push_back(socket);

  std::cerr << "client connected: " << socket->peerName().toStdString() << std::endl;

  report_status(ctx);
}

int main(int argc, char *argv[]) {
  std::cerr << "starting up whitenoise-bt-controller" << std::endl;
  QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true\nqt.multimedia = true\n*.debug = true"));
  QCoreApplication a(argc, argv);

  app_context ctx = {};

  ctx.player_a.setMedia(QUrl::fromLocalFile("/usr/lib/pink.wav"));
  ctx.player_b.setMedia(QUrl::fromLocalFile("/usr/lib/pink.wav"));

  QObject::connect(&ctx.disco_agent,
                   &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                   [&ctx](const QBluetoothDeviceInfo &device) {
                     bt_device_discovered(ctx, device);
                   });

  QObject::connect(&ctx.local_device,
                   &QBluetoothLocalDevice::pairingFinished,
                   [&ctx](const QBluetoothAddress &address,
                          QBluetoothLocalDevice::Pairing pairing) {
                     bt_pairing_finished(ctx, address, pairing);
                   });

  QObject::connect(&ctx.local_device,
                   &QBluetoothLocalDevice::deviceConnected,
                   [&ctx](const QBluetoothAddress &address) {
                     bt_connected(ctx, address);
                   });

  QObject::connect(&ctx.local_device,
                   &QBluetoothLocalDevice::deviceDisconnected,
                   [&ctx](const QBluetoothAddress &address) {
                     bt_disconnected(ctx, address);
                   });

  QObject::connect(&ctx.player_timer,
                   &QTimer::timeout,
                   [&ctx]() {
                     play_interval(ctx);
                   });

  QBluetoothServer rfcomm_server(QBluetoothServiceInfo::RfcommProtocol, &a);
  QBluetoothServiceInfo service_info;

  if (!ctx.local_device.isValid()) {
    std::cerr << "no valid BT device" << std::endl;
    return 1;
  }

  std::cerr << "powering on device" << std::endl;
  ctx.local_device.powerOn();
  std::cerr << "becoming discoverable" << std::endl;
  ctx.local_device.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

  if (!rfcomm_server.listen(ctx.local_device.address())) {
    std::cerr << "cannot bind server to: "
              << ctx.local_device.address().toString().toStdString()
              << std::endl;
    return 1;
  }

  QObject::connect(&rfcomm_server,
                   &QBluetoothServer::newConnection,
                   [&rfcomm_server, &ctx]() {
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

  service_info.registerService(ctx.local_device.address());

  std::cerr << "initializing BluezQt manager" << std::endl;
  auto mgr_init_job = ctx.manager.init();
  mgr_init_job->start();

  QObject::connect(mgr_init_job,
                   &BluezQt::InitManagerJob::result,
                   [](BluezQt::InitManagerJob *job) {
                     std::cerr << "received result for BluezQt manager initialization" << std::endl;

                     if (job->manager()->isInitialized()) {
                       std::cerr << "BluezQt manager is initialized" << std::endl;
                     } else {
                       std::cerr << "BluezQt manager is not initialized" << std::endl;
                     }

                     if (job->manager()->isOperational()) {
                       std::cerr << "BluezQt manager is operational" << std::endl;
                     } else {
                       std::cerr << "BluezQt manager is not operational" << std::endl;
                     }

                     job->deleteLater();
                   });

  restore_state(ctx);

  QObject::connect(&ctx.scan_timer,
                   &QTimer::timeout,
                   [&ctx]() {
                     bt_discover(ctx);
                   });
  ctx.scan_timer.setSingleShot(true);
  ctx.scan_timer.setInterval(10000);
  ctx.scan_timer.start();

  QObject::connect(&ctx.advertise_timer,
                   &QTimer::timeout,
                   [&ctx]() {
                     ctx.local_device.setHostMode(QBluetoothLocalDevice::HostDiscoverable);
                   });
  ctx.advertise_timer.setInterval(30000);
  ctx.advertise_timer.start();

  auto result = QCoreApplication::exec();

  service_info.unregisterService();

  for (QBluetoothSocket *socket : ctx.client_sockets) {
    delete socket;
  }

  return result;
}
