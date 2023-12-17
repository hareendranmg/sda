#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define PORT_NAME "/dev/ttyXR0"
#define BAUD_RATE B2000000
#define OUTPUT_FILE "output_file.txt"

int main() {
    int serial_port = open(PORT_NAME, O_RDWR);

    if (serial_port == -1) {
        perror("Error opening serial port");
        return 1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_port, &tty) != 0) {
        perror("Error from tcgetattr");
        return 1;
    }

    cfsetospeed(&tty, BAUD_RATE);
    cfsetispeed(&tty, BAUD_RATE);

    // Set the parameters
    tty.c_cflag     &=  ~PARENB;            // No parity bit
    tty.c_cflag     &=  ~CSTOPB;            // 1 stop bit
    tty.c_cflag     &=  ~CSIZE;             // Clear the size bits
    tty.c_cflag     |=  CS8;                // 8 data bits
    tty.c_cflag     &=  ~CRTSCTS;           // No hardware flow control
    tty.c_cflag     |=  CREAD | CLOCAL;     // Enable receiver, ignore modem control lines

    // Make raw
    cfmakeraw(&tty);

    // Flush port, then apply attributes
    tcflush(serial_port, TCIFLUSH);
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        return 1;
    }

    FILE *output_file = fopen(OUTPUT_FILE, "w");

    if (output_file == NULL) {
        perror("Error opening output file");
        close(serial_port);
        return 1;
    }

    char buffer[1];  // Buffer to read one character at a time

    while (1) {
        ssize_t read_size = read(serial_port, buffer, sizeof(buffer));

        if (read_size > 0) {
            // Write the received character to the output file
            fprintf(output_file, "%c", buffer[0]);
            fflush(output_file);  // Flush the output to ensure immediate writing
        } else if (read_size < 0) {
            perror("Error reading from serial port");
            break;
        }
    }

    fclose(output_file);
    close(serial_port);

    return 0;
}
