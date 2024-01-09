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
    char chip[32] = "gpiochip2";
    unsigned int offset = 8;
    struct gpiod_line *line = NULL;

    line = gpiod_line_get(chip, offset);

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
    struct timespec start_time, end_time;
    long elapsed_time_ns;
    long elapsed_time_us;

    for (;;)
    {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        ssize_t bytes_written = write(fd, &data_to_write, 1);
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read < 0)
        {
            perror("Error reading from serial port");
            close(fd);
            return 1;
        }

        printf("Read data: ");
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");

        usleep(1000); // 1000 microseconds delay
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        elapsed_time_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000;
        printf("Time taken for serial read: %ld microseconds\n", elapsed_time_us);
    }

    // Close the serial port
    close(fd);

    return 0;
}
