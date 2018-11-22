#include <iostream>

#include <QDebug>
#include <QLoggingCategory>
#include <QString>
#include <QCoreApplication>
#include <QAudioOutput>
#include <QFile>

#include "noise_device.h"

int main(int argc, char *argv[]) {
  std::cerr << "starting up noise-generator-test" << std::endl;
  QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true\nqt.multimedia = true\n*.debug = true"));
  QCoreApplication a(argc, argv);

  QFile sourceFile;
  sourceFile.setFileName("../brown.raw");
  sourceFile.open(QIODevice::ReadOnly);

  QAudioFormat fmt;

  fmt.setSampleRate(44100);
  fmt.setChannelCount(2);
  fmt.setSampleSize(16);
  fmt.setCodec("audio/pcm");
  fmt.setByteOrder(QAudioFormat::LittleEndian);
  fmt.setSampleType(QAudioFormat::UnSignedInt);

  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(fmt)) {
    qWarning() << "Raw audio format not supported by backend, cannot play audio.";
    return -1;
  }

  noise_device dvc;

  QAudioOutput audio(fmt, &a);
  QObject::connect(&audio, &QAudioOutput::stateChanged, [] (QAudio::State state) {
    std::cerr << "audio state changed: " << state << std::endl;
  });
  audio.start(&dvc);

  auto result = QCoreApplication::exec();

  return result;
}