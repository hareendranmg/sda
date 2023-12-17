#include <CppLinuxSerial/SerialPort.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>

using namespace mn::CppLinuxSerial;
using namespace std::this_thread;
using namespace std::chrono;

std::string uint8_to_hex_string(const uint8_t *v, const size_t s) {
  std::stringstream ss;

  ss << std::hex << std::setfill('0');

  for (int i = 0; i < s; i++) {
    ss << std::hex << std::setw(2) << static_cast<int>(v[i]);

    // Add a space after every two characters
    if (i < s - 1) {
      ss << " ";
    }
  }

  return ss.str();
}

std::vector<uint8_t> transcieve(SerialPort& serialPort, std::vector<uint8_t> command, int delay, int size) {
    serialPort.WriteBinary(command);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    std::vector<uint8_t> receivedData(size);
    serialPort.ReadBinary(receivedData);

    // Find the position of the "0F" command in the received data
    auto it = std::search(receivedData.begin(), receivedData.end(), command.begin(), command.end());

    // If "0F" is found, extract the next 40 bytes
    if (it != receivedData.end()) {
        size_t startIndex = std::distance(receivedData.begin(), it);
        std::vector<uint8_t> responseData(receivedData.begin() + startIndex, receivedData.begin() + startIndex + 40);
        return responseData;
    } else {
        // Handle the case where "0F" is not found
        std::cerr << "Error: Command not found in received data." << std::endl;
        return {};
    }
}


int main() {
    const std::string portName = "/dev/ttyXR6";

    SerialPort serialPort(portName, 1000000);
    serialPort.SetTimeout(100); // Block for up to 1000ms to receive data

    serialPort.Open();

    std::vector<uint8_t> command = {0x0F};

    transcieve(serialPort, {0xA5}, 1000, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint8_t> response = transcieve(serialPort, command, 1000, 100);
    std::cout << uint8_to_hex_string(response.data(), response.size()) << std::endl;

    serialPort.Close();

    return 0;
}
