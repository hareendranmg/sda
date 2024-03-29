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
#include <sys/select.h>

struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    useconds_t timeoutMicros;
};

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

int readResponse(int serialPortFD, unsigned char expectedCommand, size_t responseBytes, useconds_t timeoutMicros)
{
    char buffer[256]; // Adjust the buffer size as needed
    size_t bytesRead = 0;
    unsigned char command;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(serialPortFD, &readSet);

    struct timeval timeout;
    timeout.tv_sec = timeoutMicros / 1000000;
    timeout.tv_usec = timeoutMicros % 1000000;

    // Wait for data to be ready to read or until timeout
    int selectResult = select(serialPortFD + 1, &readSet, NULL, NULL, &timeout);

    if (selectResult == -1)
    {
        perror("Error from select");
        exit(EXIT_FAILURE);
    }
    else if (selectResult == 0)
    {
        // Handle timeout
        printf("Timeout occurred for command %02X\n", expectedCommand);
        return 1;
    }

    // Read until the expected command byte is received
    do
    {
        ssize_t readResult = read(serialPortFD, &command, 1);

        if (readResult == -1)
        {
            perror("Error reading from serial port");
            exit(EXIT_FAILURE);
        }

        buffer[bytesRead++] = command;
    } while (command != expectedCommand && bytesRead < responseBytes);

    // Read the remaining response bytes
    ssize_t readResult = read(serialPortFD, buffer + bytesRead, responseBytes - bytesRead);

    if (readResult == -1)
    {
        perror("Error reading from serial port");
        exit(EXIT_FAILURE);
    }

    // Print the received response
    printf("Received %02X: ", expectedCommand);
    for (size_t i = 0; i < responseBytes; ++i)
    {
        printf("%02X ", (unsigned char)buffer[i]);
    }
    printf("\n");

    return 0;
}

void sendAndReceive(int serialPortFD, const struct CommandPair *commandPair)
{
    write(serialPortFD, &(commandPair->command), sizeof(commandPair->command));
    usleep(200);
    readResponse(serialPortFD, commandPair->command, commandPair->responseBytes, commandPair->timeoutMicros);
}


int main(int argc, char *argv[])
{
    // printf("argc: %d\n", argc);
    // print arguments with their index + 1
    // for (int i = 0; i < argc; i++)
    // {
    //     printf("argv[%d]: %s\n", i+1, argv[i]);
    // }

    if (argc < 14 || (argc - 10) % 3 != 0)
    {
        fprintf(stderr, "Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits> <filesize> <file_extension> <intervalMillis> <command1> <responseBytes1> <timeoutMicros1> [<command2> <responseBytes2> <timeoutMicros2> ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int intervalMillis = atoi(argv[9]);
    if (intervalMillis <= 0)
    {
        fprintf(stderr, "Interval must be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    const char *serialPortName = argv[1];
    const char *outputFileName = argv[2];
    speed_t baudRate = atoi(argv[3]);
    char parity = argv[4][0];
    int dataBits = atoi(argv[5]);
    int stopBits = atoi(argv[6]);
    size_t filesize = atoi(argv[7]);
    const char *fileExtension = argv[8];

    size_t numCommands = (argc - 9) / 3;

    struct CommandPair *commandPairs = malloc(numCommands * sizeof(struct CommandPair));

    if (commandPairs == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < numCommands; ++i)
    {
        commandPairs[i].command = strtol(argv[i * 3 + 10], NULL, 16);
        commandPairs[i].responseBytes = atoi(argv[i * 3 + 11]);
        commandPairs[i].timeoutMicros = atoi(argv[i * 3 + 12]);
    }


    // print command pairs
    for (size_t i = 0; i < numCommands; ++i)
    {
        printf("Command %zu: %02X %zu %u\n", i, commandPairs[i].command, commandPairs[i].responseBytes, commandPairs[i].timeoutMicros);
    }

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

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter);

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(serial_fd, NULL); // Passing -1 as serial_fd to avoid closing an invalid file descriptor
    }

    // heading as sl.no, tx_data, tx_time, rx_data, rx_time, tx_elapsed
    fprintf(outputFile, "sl.no,command_1,response_1,command_2,response_2\n");
    unsigned long long int counter = 1;
    // flush the serial port
    // tcflush(serial_fd, TCIOFLUSH);

    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        ssize_t bytes_written = write(serial_fd, &data_to_write, 1);
        fprintf(outputFile, "%llu,%02X,", counter, (unsigned char)data_to_write);

        //**************************** Command 1 ********************************//
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serial_fd, &readSet);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 40;
        int selectResult1 = select(serial_fd + 1, &readSet, NULL, NULL, &timeout);

        if (selectResult1 == -1)
        {
            perror("Error from select");
        }

        if (selectResult1 > 0)
        {
            char buffer[22];
            ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
            tcflush(serial_fd, TCIOFLUSH);

            if (bytes_read > 0)
            {
                fprintf(outputFile, "%d,", bytes_read);
                printf("Read data1: ");
                for (ssize_t i = 0; i < bytes_read; ++i)
                {
                    printf("%02X ", (unsigned char)buffer[i]);
                    fprintf(outputFile, "%02X", (unsigned char)buffer[i]);
                }
                printf("\n");
            }
        }
        else
        {
            fprintf(outputFile, "0,");
        }
        //***********************************************************************
        usleep(100);
        //*******************************Command 2*******************************
        bytes_written = write(serial_fd, &data_to_write1, 1);
        fprintf(outputFile, ",%02X,", (unsigned char)data_to_write1);

        timeout.tv_sec = 0;
        timeout.tv_usec = 40;
        selectResult1 = select(serial_fd + 1, &readSet, NULL, NULL, &timeout);

        if (selectResult1 == -1)
        {
            perror("Error from select");
        }

        if (selectResult1 > 0)
        {
            char buffer[22];
            ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
            // tcflush(serial_fd, TCIOFLUSH);

            if (bytes_read > 0)
            {
                fprintf(outputFile, "%d,", bytes_read);
                printf("Read data2: ");
                for (ssize_t i = 0; i < bytes_read; ++i)
                {
                    printf("%02X ", (unsigned char)buffer[i]);
                    fprintf(outputFile, "%02X", (unsigned char)buffer[i]);
                }
                printf("\n");
            }
        }
        else
        {
            fprintf(outputFile, "0,");
        }
        //***********************************************************************
        fprintf(outputFile, "\n");

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        timespec_to_hhmmssmsus(&now, rx_time_str, sizeof(rx_time_str));

        long elapsedMicroseconds = (now.tv_sec - start.tv_sec) * 1000000 +
                                   now.tv_nsec / 1000 - start.tv_nsec / 1000;
        long timeLeftMicroseconds = 4000 - elapsedMicroseconds;
        if (timeLeftMicroseconds > 0)
        {
            printf("Sleeping for %ld microseconds\n", timeLeftMicroseconds);
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
