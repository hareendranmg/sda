#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#define SERIAL_PORT "/dev/ttyXR0"

int setupSerialPort(const char *portName)
{
    int serialPort = open(portName, O_RDWR | O_NOCTTY | O_NDELAY);

    if (serialPort == -1)
    {
        perror("Error opening serial port");
        exit(EXIT_FAILURE);
    }

    struct termios serialConfig;
    if (tcgetattr(serialPort, &serialConfig) < 0)
    {
        perror("Error getting serial port attributes");
        close(serialPort);
        return 1;
    }

    // Set baud rate
    cfsetispeed(&serialConfig, B2000000);
    cfsetospeed(&serialConfig, B2000000);

    // Set character size to 8 bits, enable odd parity, and set stop bits to 1
    serialConfig.c_cflag |= CS8 | PARENB | PARODD | CSTOPB;

    // Apply the new settings
    if (tcsetattr(serialPort, TCSAFLUSH, &serialConfig) < 0)
    {
        perror("Error setting serial port attributes");
        close(serialPort);
        return 1;
    }

    return serialPort;
}

int main()
{
    int serialPortFD = setupSerialPort(SERIAL_PORT);
    unsigned char hexCommand = 0x0F;

    for (;;)
    {
        write(serialPortFD, &hexCommand, sizeof(hexCommand));
        usleep(10);
        char buffer[22];
        ssize_t bytesRead = read(serialPortFD, buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
            printf("Received: ");
            for (ssize_t i = 0; i < bytesRead; ++i)
            {
                printf("%02X ", (unsigned char)buffer[i]);
            }
            printf("\n");
        }
        else
        {
            printf("Error reading from serial port or incorrect number of bytes received\n");
            // break;
        }
        usleep(1000);
    }

    close(serialPortFD);

    return 0;
}
