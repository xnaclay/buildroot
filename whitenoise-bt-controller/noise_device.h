#ifndef WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H
#define WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H

#include <QIODevice>
#include <fstream>

class noise_device : public QIODevice {
 public:

  noise_device() {
    open(QIODevice::ReadOnly);

    // read noise data into buffer
    std::ifstream noise_if("../brown.raw", std::ios::binary | std::ios::ate);

    if (noise_if.bad()) {
      std::cerr <<  "could not open noise buffer" << std::endl;
    } else {
      noise_data_len = std::static_cast<unsigned long>(noise_if.tellg());
      std::cerr << "opened noise buffer file of length: " << noise_data_len << std::endl;

      noise_buffer.reserve(noise_data_len);

      if (noise_if.read(noise_buffer.data(), noise_data_len))
      {
        std::cerr << "read noise buffer" << std::endl;
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

 protected:
  virtual qint64 readData(char *data, qint64 maxlen) override {
    std::cerr << "readData: " << maxlen << std::endl;
    return -1;
  }
  virtual qint64 writeData(const char *data, qint64 len) override {
    return -1;
  }

 private:
  unsigned long pos = 0;
  unsigned long  noise_data_len;
  std::vector<char> noise_buffer;
};

#endif //WHITENOISE_BT_CONTROLLER_NOISE_DEVICE_H
