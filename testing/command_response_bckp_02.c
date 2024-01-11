#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
// #include <gpiod.h>

int main() {
    const char* port = "/dev/ttyXR0";
    int baudrate = B2000000;
    char buffer[22];

    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening serial port");
        return 1;
    }

    struct termios serialConfig;
    if (tcgetattr(fd, &serialConfig) < 0) {
        perror("Error getting serial port attributes");
        close(fd);
        return 1;
    }

    // Set baud rate
    cfsetispeed(&serialConfig, baudrate);
    cfsetospeed(&serialConfig, baudrate);

    // Set character size to 8 bits, enable odd parity, and set stop bits to 1
    serialConfig.c_cflag |= CS8 | PARENB | PARODD | CSTOPB;

    // Apply the new settings
    if (tcsetattr(fd, TCSAFLUSH, &serialConfig) < 0) {
        perror("Error setting serial port attributes");
        close(fd);
        return 1;
    }

    // Write 0x0F continuously
    char data_to_write = 0x0F;
    for (;;) {
        // gpiod_ctxless_set_value(chip, offset, 1, false, "gpio-toggle", NULL, NULL);
        ssize_t bytes_written = write(fd, &data_to_write, 1);
        // delay of 10 microseconds
        usleep(4);
        // gpiod_ctxless_set_value(chip, offset, 0, false, "gpio-toggle", NULL, NULL);

        if (bytes_written < 0) {
            perror("Error writing to serial port");
            close(fd);
            return 1;
        }

        // Read 22 bytes
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        // gpiod_ctxless_set_value(chip, offset, 0, false, "gpio-toggle", NULL, NULL);
        if (bytes_read < 0) {
            perror("Error reading from serial port");
            close(fd);
            return 1;
        }

        // Print the read data
        printf("Read data: ");
        for (ssize_t i = 0; i < bytes_read; ++i) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");

        // Optional: Add a delay between consecutive writes
        usleep(100000); // 100 microseconds delay
    }

    // Close the serial port
    close(fd);

    return 0;
}
