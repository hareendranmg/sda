#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
// #include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <inttypes.h>

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

void timespec_to_hhmmssmsus(struct timespec *ts, char *output, size_t size)
{
    long ms = ts->tv_nsec / 1000000;
    long us = (ts->tv_nsec % 1000000) / 1000;

    struct tm tm_info;
    localtime_r(&(ts->tv_sec), &tm_info);

    snprintf(output, size, "%02d:%02d:%02d.%03ld.%03ld", tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms, us);
}

int main()
{
    int serialPort = setupSerialPort(SERIAL_PORT);
    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR)};
    if (sched_setscheduler(0, SCHED_RR, &sp) != 0)
    {
        perror("Error setting process priority");
        close(serialPort);
        return 1;
    }
    char buffer[22];
    char data_to_write = 0x0F;
    char data_to_write1 = 0x1E;
    int bytes_read;
    struct timespec start, now, start1;
    char tx_time_str[30], rx_time_str[30];
    char rx_data[30][3];

    FILE *outputFile = fopen("command_response.csv", "w");
    if (!outputFile)
    {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    // heading as sl.no, tx_data, tx_time, rx_data, rx_time, tx_elapsed
    fprintf(outputFile, "sl.no,tx_data,tx_time,rx_data,rx_time,tx_elapsed\n");
    // vairable for counter, it must be unsigned long long int
    unsigned long long int counter = 1;
    // varible to store the time difference between start and start1
    unsigned long long int tx_elapsed = 0;

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        // calculate the time difference between start and start1
        if (counter > 2) {
            tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
                        start.tv_nsec / 1000 - start1.tv_nsec / 1000;
            fprintf(outputFile, "%llu\n", tx_elapsed);
        }
        start1 = start;
        timespec_to_hhmmssmsus(&start, tx_time_str, sizeof(tx_time_str));
        fprintf(outputFile, "%llu,%02X,%s,", counter, (unsigned char)data_to_write, tx_time_str);
        ssize_t bytes_written = write(serialPort, &data_to_write, 1);
        usleep(10);
        if (bytes_written < 0)
        {
            perror("Error writing to serial port");
            close(serialPort);
            return 1;
        }
        ssize_t bytes_read = read(serialPort, buffer, sizeof(buffer));
        if (bytes_read < 0)
        {
            perror("Error reading from serial port");
            close(serialPort);
            return 1;
        }
        printf("Read data: ");
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
            snprintf(rx_data[i], 3, "%02X", (unsigned char)buffer[i]);
        }
        printf("\n");
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        timespec_to_hhmmssmsus(&now, rx_time_str, sizeof(rx_time_str));
        char joined_rx_data[30];
        joined_rx_data[0] = '\0';
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            strcat(joined_rx_data, rx_data[i]);
        }
        fprintf(outputFile, "%s,%s,", joined_rx_data, rx_time_str);
        long elapsedMicroseconds = (now.tv_sec - start.tv_sec) * 1000000 +
                                   now.tv_nsec / 1000 - start.tv_nsec / 1000;
        long timeLeftMicroseconds = 1000 - elapsedMicroseconds;
        if (timeLeftMicroseconds > 0)
        {
            usleep(timeLeftMicroseconds - 20);
        }
        ++counter;
    }

    close(serialPort);
    fclose(outputFile);
    return 0;
}
