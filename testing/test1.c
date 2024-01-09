#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <gpiod.h>
#include <time.h>
// #include <linux/time.h>

#define BILLION 1000000000L

int main()
{
    const char *port = "/dev/ttyXR0";
    int baudrate = B2000000;
    char buffer[22];


    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1)
    {
        perror("Error opening serial port");
        return 1;
    }

    struct termios serialConfig;
    if (tcgetattr(fd, &serialConfig) < 0)
    {
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
    if (tcsetattr(fd, TCSAFLUSH, &serialConfig) < 0)
    {
        perror("Error setting serial port attributes");
        close(fd);
        return 1;
    }

    char data_to_write = 0x0F;

// i need to send the data_to_write to the serial port at every exact 1 millisecond, and there is a timeout of 10 microseconds for the read,
// a roughly calculation is below:
// 1. start of 1 millisecond
// 2. write data_to_write to serial port, takes 10 microseconds; time left: 990 microseconds
// 3. read 22 bytes from serial port, need a timeout of 10 microseconds, the data may or may not be arrived in between the provided timeout, we don't bother about it; time left: 980 microseconds
// 4. end of 1 millisecond
// 5. start of 1 millisecond
// 6. write data_to_write to serial port, takes 21 microseconds; time left: 979 microseconds
// 7. read 22 bytes from serial port, need a timeout of 10 microseconds, the data may or may not be arrived in between the provided timeout, we don't bother about it; time left: 969 microseconds
// 8. end of 1 millisecond
// like this, and we need to make sure that the data_to_write to the serial port at every exact 1 millisecond




    // Close the serial port
    close(fd);

    return 0;
}
