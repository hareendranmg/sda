#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>

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
    int serialPort = setupSerialPort(SERIAL_PORT);
    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_FIFO)};
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
    {
        perror("Error setting process priority");
        close(serialPort);
        return 1;
    }
    char buffer[22];
    char data_to_write = 0x1e;
    int bytes_read;

    // Timing setup
    struct timeval startTime, currentTime;
    gettimeofday(&startTime, NULL);

    while (1)
    {
        ssize_t bytes_written = write(serialPort, &data_to_write, 1);
        // delay of 10 microseconds
        usleep(100);
        // gpiod_ctxless_set_value(chip, offset, 0, false, "gpio-toggle", NULL, NULL);

        if (bytes_written < 0)
        {
            perror("Error writing to serial port");
            close(serialPort);
            return 1;
        }

        // Read 22 bytes
        ssize_t bytes_read = read(serialPort, buffer, sizeof(buffer));
        // gpiod_ctxless_set_value(chip, offset, 0, false, "gpio-toggle", NULL, NULL);
        if (bytes_read < 0)
        {
            perror("Error reading from serial port");
            close(serialPort);
            return 1;
        }

        // Print the read data
        printf("Read data: ");
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
        // Calculate time left in the 1-millisecond interval
        gettimeofday(&currentTime, NULL);
        long elapsedMicroseconds = (currentTime.tv_sec - startTime.tv_sec) * 1000000 +
                                   currentTime.tv_usec - startTime.tv_usec;
        long timeLeftMicroseconds = 5000 - (elapsedMicroseconds % 1000);

        // Delay for the remaining time in the 1-millisecond interval
        if (timeLeftMicroseconds > 0)
        {
            // printf("Sleeping for %ld microseconds\n", timeLeftMicroseconds);
            usleep(timeLeftMicroseconds);
            // printf("Done sleeping\n");
        }
    }

    close(serialPort);
    return 0;
}
