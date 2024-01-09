#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>

#define SERIAL_PORT "/dev/ttyXR0"

// Function to set up the serial port
int setupSerialPort(const char *portName)
{
    int serialPort = open(portName, O_RDWR | O_NOCTTY | O_NDELAY);

    if (serialPort == -1)
    {
        perror("Error opening serial port");
        exit(EXIT_FAILURE);
    }

    struct termios serialConfig;
    tcgetattr(serialPort, &serialConfig);
    cfsetispeed(&serialConfig, B2000000); // Set baud rate to 2000000
    cfsetospeed(&serialConfig, B2000000);
    serialConfig.c_cflag |= PARENB;  // Enable parity
    serialConfig.c_cflag |= PARODD;  // Set odd parity
    serialConfig.c_cflag &= ~CSTOPB; // 1 stop bit
    serialConfig.c_cflag &= ~CSIZE;
    serialConfig.c_cflag |= CS8; // 8 data bits
    tcsetattr(serialPort, TCSANOW, &serialConfig);

    return serialPort;
}

// Function to write data to the serial port
void writeToSerial(int serialPort, const char *data)
{
    write(serialPort, data, strlen(data));
}

// Function to read data from the serial
void readFromSerial(int serialPort, char *buffer, int bufferSize)
{
    read(serialPort, buffer, bufferSize);
}

int main()
{
    int serialPort = setupSerialPort(SERIAL_PORT);
    char buffer[22];
    char data_to_write = 0x0F;

    // Timing setup
    struct timeval startTime, currentTime;
    gettimeofday(&startTime, NULL);

    while (1)
    {
        // Write data to the serial port
        writeToSerial(serialPort, &data_to_write);
        readFromSerial(serialPort, buffer, sizeof(buffer));
        printf("Read data: ");
        for (ssize_t i = 0; i < sizeof(buffer); ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
        // Calculate time left in the 1-millisecond interval
        gettimeofday(&currentTime, NULL);
        long elapsedMicroseconds = (currentTime.tv_sec - startTime.tv_sec) * 1000000 +
                                   currentTime.tv_usec - startTime.tv_usec;
        long timeLeftMicroseconds = 1000 - (elapsedMicroseconds % 1000);

        // Delay for the remaining time in the 1-millisecond interval
        if (timeLeftMicroseconds > 0)
        {
            usleep(timeLeftMicroseconds);
        }
    }

    close(serialPort);
    return 0;
}
