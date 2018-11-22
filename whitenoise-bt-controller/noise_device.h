#ifndef WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H
#define WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H

#include <QIODevice>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>

class noise_device : public QIODevice {
 public:

  noise_device() {
    noise_data_len = 0;

    // read noise data into buffer
    std::ifstream noise_if("brown.raw", std::ios::binary);

    if (!noise_if) {
      std::cerr << "could not open noise buffer" << std::endl;
    } else {
      noise_if.seekg(0, std::istream::end);
      noise_data_len = static_cast<unsigned long>(noise_if.tellg());
      noise_if.seekg(0, std::istream::beg);

      std::cerr << "opened noise buffer file of length: " << noise_data_len << std::endl;

      noise_buffer.resize(noise_data_len);
      noise_if.read(noise_buffer.data(), noise_data_len);

      if (noise_if) {
        std::cerr << "read noise buffer" << std::endl;
        open(QIODevice::ReadOnly);
      } else {
        std::cerr << "failed to read noise buffer" << std::endl;
      }
    }
  }

  bool isSequential() const override {
    return true;
  }

  bool open(OpenMode mode) override {
    std::cerr << "opening noise device" << std::endl;
    setOpenMode(mode);
    return true;
  }

  void close() override {
    std::cerr << "closing noise device" << std::endl;
    QIODevice::close();
  }

  void setVolume(double vol) {
    set_volume = vol;
    target_volume = set_volume;
  }

  double volume() {
    return set_volume;
  }

  void quiet() {
    target_volume = 0;
  }

  void unquiet() {
    target_volume = set_volume;
  }

 protected:
  qint64 readData(char *data, qint64 maxlen) override {
    auto len_to_read = std::min(static_cast<unsigned long>(maxlen), noise_data_len - pos);

    for (unsigned long i = 0; i < len_to_read / 2; i++) {
      int16_t val = *(reinterpret_cast<int16_t *>(noise_buffer.data() + pos + i * 2));
      val = static_cast<int16_t>(val * cur_volume);

      if (cur_volume < target_volume) {
        cur_volume += .000002;
      } else if (cur_volume > target_volume) {
        cur_volume -= .000002;
      }

      std::memcpy(data + i * 2, &val, 2);
    }
    pos += len_to_read;
    pos = pos % noise_data_len;
    return static_cast<qint64>(len_to_read);
  }
  qint64 writeData(const char *data, qint64 len) override {
    return -1;
  }

 private:
  unsigned long pos = 0;
  unsigned long noise_data_len;
  std::vector<char> noise_buffer;
  double set_volume = .5;
  double target_volume = 0;
  double cur_volume = 0;
};

#endif //WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H
