#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
// #include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <inttypes.h>

#define MAX_FILE_PATH 256

volatile sig_atomic_t stop_flag = 0;
size_t currentFileSize = 0;
int fileCounter = 1;

void handle_signal(int signum)
{
    printf("Caught signal %d\n", signum);
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("Stopping...\n");
        stop_flag = 1;
        printf("Stopped\n");
    }
    printf("Exiting signal handler\n");
}

void cleanup(int serial_fd, FILE *outputFile)
{
    printf("Cleaning up...\n");
    if (outputFile)
        fclose(outputFile);
    if (serial_fd != -1)
        close(serial_fd);
    printf("Cleaned up\n");
}

int open_serial_port(const char *serialPortName)
{
    int serial_fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd == -1)
    {
        perror("Error opening serial port");
    }
    return serial_fd;
}

void set_serial_parameters(int serial_fd, speed_t baudRate, char parity, int dataBits, int stopBits)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0)
    {
        perror("Error getting serial port attributes");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;

    // Set parity
    if (parity == 'N')
    {
        tty.c_cflag &= ~PARENB; // No parity
    }
    else if (parity == 'E')
    {
        tty.c_cflag |= PARENB;  // Enable parity
        tty.c_cflag &= ~PARODD; // Even parity
    }
    else if (parity == 'O')
    {
        tty.c_cflag |= (PARENB | PARODD); // Odd parity
    }

    // Set data bits
    tty.c_cflag &= ~CSIZE;
    if (dataBits == 8)
    {
        tty.c_cflag |= CS8;
    }
    else if (dataBits == 7)
    {
        tty.c_cflag |= CS7;
    }
    else if (dataBits == 6)
    {
        tty.c_cflag |= CS6;
    }
    else if (dataBits == 5)
    {
        tty.c_cflag |= CS5;
    }
    else
    {
        fprintf(stderr, "Invalid number of data bits\n");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    // Set stop bits
    if (stopBits == 1)
    {
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
    }
    else if (stopBits == 2)
    {
        tty.c_cflag |= CSTOPB; // 2 stop bits
    }
    else
    {
        fprintf(stderr, "Invalid number of stop bits\n");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    if (tcsetattr(serial_fd, TCSAFLUSH, &tty) != 0)
    {
        perror("Error setting serial port attributes");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }
}

FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter)
{
    char newFileName[MAX_FILE_PATH];
    snprintf(newFileName, sizeof(newFileName), "%s_%d.%s", outputFileName, counter, fileExtension);
    FILE *outputFile = fopen(newFileName, "wb");
    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(-1, NULL); // Passing -1 as serial_fd to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }
    return outputFile;
}

void timespec_to_hhmmssmsus(struct timespec *ts, char *output, size_t size)
{
    long ms = ts->tv_nsec / 1000000;
    long us = (ts->tv_nsec % 1000000) / 1000;

    struct tm tm_info;
    localtime_r(&(ts->tv_sec), &tm_info);

    snprintf(output, size, "%02d:%02d:%02d.%03ld.%03ld", tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms, us);
}

int main(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    // if (argc != 9)
    // {
    //     fprintf(stderr, "Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits> <filesize> <file_extension>\n", argv[0]);
    //     return EXIT_FAILURE;
    // }

    const char *serialPortName = argv[1];
    const char *outputFileName = argv[2];
    speed_t baudRate = atoi(argv[3]);
    char parity = argv[4][0];
    int dataBits = atoi(argv[5]);
    int stopBits = atoi(argv[6]);
    size_t filesize = atoi(argv[7]);
    const char *fileExtension = argv[8];

    // Execute the Python script before opening the serial port
    char python_command[MAX_FILE_PATH];
    snprintf(python_command, sizeof(python_command),
             "python3 /home/root/sda/python_backend/serial_workaround.py %s %d %c %d %d",
             serialPortName, (int)baudRate, parity, dataBits, stopBits);
    system(python_command);

    int serial_fd = open_serial_port(serialPortName);

    if (serial_fd == -1)
    {
        perror("Error opening serial port");
        cleanup(-1, NULL); // Passing -1 as serial_fd to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    set_serial_parameters(serial_fd, baudRate, parity, dataBits, stopBits);

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");

    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR)};
    if (sched_setscheduler(0, SCHED_RR, &sp) != 0)
    {
        perror("Error setting process priority");
        close(serial_fd);
        return 1;
    }

    char buffer[22];
    char data_to_write = 0x0F;
    char data_to_write1 = 0x1E;
    int bytes_read;
    struct timespec start, now, start1;
    char tx_time_str[30], rx_time_str[30];
    char rx_data[30][3];

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter);

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(serial_fd, NULL); // Passing -1 as serial_fd to avoid closing an invalid file descriptor
    }

    // heading as sl.no, tx_data, tx_time, rx_data, rx_time, tx_elapsed
    fprintf(outputFile, "sl.no,command_1,response_1,command_2,response_2\n");
    // vairable for counter, it must be unsigned long long int
    unsigned long long int counter = 1;
    // varible to store the time difference between start and start1
    unsigned long long int tx_elapsed = 0;

    // flush the serial port
    tcflush(serial_fd, TCIOFLUSH);


    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        // calculate the time difference between start and start1
        // if (counter > 2) {
        //     tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
        //                 start.tv_nsec / 1000 - start1.tv_nsec / 1000;
        //     fprintf(outputFile, "%llu\n", tx_elapsed);
        // }
        start1 = start;
        // timespec_to_hhmmssmsus(&start, tx_time_str, sizeof(tx_time_str));
        fprintf(outputFile, "%llu,%02X,", counter, (unsigned char)data_to_write);

        ssize_t bytes_written = write(serial_fd, &data_to_write, 1);
        usleep(10);
        if (bytes_written < 0)
        {
            perror("Error writing to serial port");
            close(serial_fd);
            return 1;
        }
        ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
        if (bytes_read < 0)
        {
            perror("Error reading from serial port");
            close(serial_fd);
            return 1;
        }
        printf("Read data1: ");
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
            // snprintf(rx_data[i], 3, "%02X", (unsigned char)buffer[i]);
            fprintf(outputFile, "%02X", (unsigned char)buffer[i]);
        }
        // fprintf(outputFile, "%s,", *rx_data);
        printf("\n");

        fprintf(outputFile, ",%02X,", (unsigned char)data_to_write1);

        bytes_written = write(serial_fd, &data_to_write1, 1);
        usleep(10);
        if (bytes_written < 0)
        {
            perror("Error writing to serial port");
            close(serial_fd);
            return 1;
        }
        bytes_read = read(serial_fd, buffer, sizeof(buffer));
        if (bytes_read < 0)
        {
            perror("Error reading from serial port");
            close(serial_fd);
            return 1;
        }
        printf("Read data2: ");
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            printf("%02X ", (unsigned char)buffer[i]);
            fprintf(outputFile, "%02X", (unsigned char)buffer[i]);
        }
        // sprintf(rx_data[bytes_read], "\0");
        fprintf(outputFile, "\n", &buffer);
        printf("\n");

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        timespec_to_hhmmssmsus(&now, rx_time_str, sizeof(rx_time_str));

        // joined_rx_data[0] = '\0';
        // for (ssize_t i = 0; i < bytes_read; ++i)
        // {
        //     strcat(joined_rx_data, rx_data[i]);
        // }
        // fprintf(outputFile, "%s,%s,", joined_rx_data, rx_time_str);
        long elapsedMicroseconds = (now.tv_sec - start.tv_sec) * 1000000 +
                                   now.tv_nsec / 1000 - start.tv_nsec / 1000;
        long timeLeftMicroseconds = 4000 - elapsedMicroseconds;
        if (timeLeftMicroseconds > 0)
        {
            usleep(timeLeftMicroseconds);
        }
        ++counter;
    }

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serial_fd, outputFile);

    // Reset signal handlers to default behavior
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    return EXIT_SUCCESS;
}
