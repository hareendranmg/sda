// ./command_response /dev/ttyXR7 command_response 2000000 O 8 1 10240000 csv 100 0f 22 100 1e 22 100 12 10 10
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
#include <stddef.h>
#include <sys/stat.h>
#include <arpa/inet.h> // for ntohs, ntohl, and ntohf

#define MAX_FILE_PATH 256

// Global variables
volatile sig_atomic_t stop_flag = 0;
size_t currentFileSize = 0;
int fileCounter = 1;

// data structure to store command, responseBytes and timeoutMicros
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    useconds_t timeoutMicros;
    char dataTypes[100]; // Adjust size as needed
};
double ntohd(double value)
{
    uint64_t ui = *(uint64_t *)&value;
    ui = ((ui & 0x00000000000000ffULL) << 56) |
         ((ui & 0x000000000000ff00ULL) << 40) |
         ((ui & 0x0000000000ff0000ULL) << 24) |
         ((ui & 0x00000000ff000000ULL) << 8) |
         ((ui & 0x000000ff00000000ULL) >> 8) |
         ((ui & 0x0000ff0000000000ULL) >> 24) |
         ((ui & 0x00ff000000000000ULL) >> 40) |
         ((ui & 0xff00000000000000ULL) >> 56);
    return *(double *)&ui;
}

// Function prototypes
off_t get_file_size(FILE *file);
void handle_signal(int signum);
void cleanup(int serialPortFD, FILE *outputFile);
int open_serial_port(const char *serialPortName);
void set_serial_parameters(int serialPortFD, speed_t baudRate, char parity, int dataBits, int stopBits);
FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter);
void timespec_to_hhmmssmsus(struct timespec *ts, char *output, size_t size);
int readResponse(FILE *outputFile, int serialPortFD, unsigned char expectedCommand, size_t responseBytes, useconds_t timeoutMicros, const struct CommandPair commandPairs);
void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs);
void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, int intervalMillis);

int main(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    printf("%d < 14 || (%d - 11) %% 4 != 0 = %d\n", argc, argc, ( (argc - 11) % 3 != 0));

    if (argc < 14 || (argc - 10) % 4 != 0)
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

    size_t numCommands = (argc - 10) / 4;

    struct CommandPair *commandPairs = malloc(numCommands * sizeof(struct CommandPair));

    if (commandPairs == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < numCommands; ++i)
    {
        commandPairs[i].command = strtol(argv[i * 4 + 10], NULL, 16);
        commandPairs[i].responseBytes = atoi(argv[i * 4 + 11]);
        commandPairs[i].timeoutMicros = atoi(argv[i * 4 + 12]);
        strncpy(commandPairs[i].dataTypes, argv[i * 4 + 13], sizeof(commandPairs[i].dataTypes) - 1);
        commandPairs[i].dataTypes[sizeof(commandPairs[i].dataTypes) - 1] = '\0'; // Ensure null termination
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

    int serialPortFD = open_serial_port(serialPortName);

    if (serialPortFD == -1)
    {
        perror("Error opening serial port");
        cleanup(-1, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    set_serial_parameters(serialPortFD, baudRate, parity, dataBits, stopBits);

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");

    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR)};
    if (sched_setscheduler(0, SCHED_RR, &sp) != 0)
    {
        perror("Error setting process priority");
        close(serialPortFD);
        return 1;
    }

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter);

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(serialPortFD, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
    }

    writeHeaders(outputFile, numCommands, commandPairs);

    executeCommands(serialPortFD, outputFile, filesize, outputFileName, fileExtension, numCommands, commandPairs, intervalMillis);

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serialPortFD, outputFile);

    // Reset signal handlers to default behavior
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    return EXIT_SUCCESS;
}

off_t get_file_size(FILE *file)
{
    // Get the file descriptor associated with the FILE pointer
    int fd = fileno(file);
    if (fd == -1)
    {
        perror("Error getting file descriptor");
        return -1; // Error occurred
    }

    // Get file status using fstat
    struct stat fileStat;
    if (fstat(fd, &fileStat) == -1)
    {
        perror("Error getting file status");
        return -1; // Error occurred
    }

    return fileStat.st_size;
}

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

void cleanup(int serialPortFD, FILE *outputFile)
{
    printf("Cleaning up...\n");
    if (outputFile)
    {
        fflush(outputFile);
        fclose(outputFile);
    }
    if (serialPortFD != -1)
        close(serialPortFD);
    printf("Cleaned up\n");
}

int open_serial_port(const char *serialPortName)
{
    int serialPortFD = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serialPortFD == -1)
    {
        perror("Error opening serial port");
    }
    return serialPortFD;
}

void set_serial_parameters(int serialPortFD, speed_t baudRate, char parity, int dataBits, int stopBits)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serialPortFD, &tty) != 0)
    {
        perror("Error getting serial port attributes");
        cleanup(serialPortFD, NULL);
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
        cleanup(serialPortFD, NULL);
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
        cleanup(serialPortFD, NULL);
        exit(EXIT_FAILURE);
    }

    if (tcsetattr(serialPortFD, TCSAFLUSH, &tty) != 0)
    {
        perror("Error setting serial port attributes");
        cleanup(serialPortFD, NULL);
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
        cleanup(-1, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
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

int readResponse(FILE *outputFile, int serialPortFD, unsigned char expectedCommand, size_t responseBytes, useconds_t timeoutMicros, const struct CommandPair commandPairs)
{
    unsigned char buffer[responseBytes];
    size_t bytesRead = 0;
    unsigned char command;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(serialPortFD, &readSet);

    struct timeval timeout;
    timeout.tv_sec = timeoutMicros / 1000000;
    timeout.tv_usec = timeoutMicros;

    int selectResult = select(serialPortFD + 1, &readSet, NULL, NULL, &timeout);

    if (selectResult == -1)
    {
        perror("Error from select");

        return 0;
    }

    if (FD_ISSET(serialPortFD, &readSet))
    {
        int bytesRead = read(serialPortFD, buffer, responseBytes);
        if (bytesRead > 0)
        {
            for (int i = 0; i < bytesRead; ++i)
            {
                fprintf(outputFile, "%02x", buffer[i]);
            }
            fprintf(outputFile, "start");
            char *dataTypePtr = strtok(commandPairs.dataTypes, ","); // Parse data types string
            // copy the buffer to a new pointer
            unsigned char *buffer_new = buffer;

            while (dataTypePtr != NULL)
            {
                if (strcmp(dataTypePtr, "hex8") == 0)
                {
                    fprintf(outputFile, ",hex8 %02x", buffer_new[0]);
                    buffer_new += 1;
                }
                else if (strcmp(dataTypePtr, "hex16") == 0)
                {
                    uint16_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",hex16 %04x", value);
                    buffer_new += 2;
                }
                else if (strcmp(dataTypePtr, "hex32") == 0)
                {
                    uint32_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",hex32 %08x", value);
                    buffer_new += 4;
                }
                else if (strcmp(dataTypePtr, "uint8") == 0)
                {
                    uint8_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",uint8 %u", value);
                    buffer_new += 1;
                }
                else if (strcmp(dataTypePtr, "uint16") == 0)
                {
                    uint16_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",uint16 %u", value);
                    buffer_new += 2;
                }
                else if (strcmp(dataTypePtr, "uint32") == 0)
                {
                    uint32_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",uint32 %u", value);
                    buffer_new += 4;
                }
                else if (strcmp(dataTypePtr, "int32") == 0)
                {
                    int32_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",int32 %d", value);
                    buffer_new += 4;
                }
                else if (strcmp(dataTypePtr, "int16") == 0)
                {
                    int16_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",int16 %d", value);
                    buffer_new += 2;
                }
                else if (strcmp(dataTypePtr, "int8") == 0)
                {
                    int8_t value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",int8 %d", value);
                    buffer_new += 1;
                }
                else if (strcmp(dataTypePtr, "float32") == 0)
                {
                    float value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",float32 %f", value);
                    buffer_new += 4;
                }
                else if (strcmp(dataTypePtr, "float64") == 0)
                {
                    double value;
                    memcpy(&value, buffer_new, sizeof(value));
                    fprintf(outputFile, ",float64 %f", value);
                    buffer_new += 8;
                }
                else
                {
                    fprintf(outputFile, ",*** ");
                }

                dataTypePtr = strtok(NULL, ","); // Get next data type
            }
            fprintf(outputFile, "end");
        }

        return bytesRead;
    }
    else
    {
        fprintf(outputFile, "timeout");

        return 9;
    }

    fprintf(outputFile, ", ");

    return 0;
}

void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs)
{
    fprintf(outputFile, "slno");
    for (size_t i = 0; i < numCommands; ++i)
    {
        fprintf(outputFile, ", command%d, response%d", i + 1, i + 1);
    }
    fprintf(outputFile, ", time_elapsed\n");
}

void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, int intervalMillis)
{
    unsigned long long int counter = 1;

    time_t currentTime = time(NULL);
    time_t lastFlushTime = time(NULL);

    struct timespec start, now, start1;
    unsigned long long int tx_elapsed = 0;

    // header bytes count
    currentFileSize = 0;

    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        if (counter > 2)
        {
            tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
                         start.tv_nsec / 1000 - start1.tv_nsec / 1000;
            fprintf(outputFile, ", %llu\n", tx_elapsed);
        }

        currentFileSize = get_file_size(outputFile);
        if (currentFileSize > filesize)
        {
            fclose(outputFile);
            outputFile = open_output_file(outputFileName, fileExtension, ++fileCounter);
        }

        start1 = start;
        fprintf(outputFile, "%llu", counter);
        for (size_t i = 0; i < numCommands; ++i)
        {
            fprintf(outputFile, ", %02X, ", commandPairs[i].command);
            write(serialPortFD, &(commandPairs[i].command), sizeof(commandPairs[i].command));
            usleep(200);
            readResponse(outputFile, serialPortFD, commandPairs[i].command, commandPairs[i].responseBytes, commandPairs[i].timeoutMicros, commandPairs[i]);
            tcflush(serialPortFD, TCIOFLUSH);
        }
        // fprintf ", 0\n"
        if (counter == 1)
        {
            fprintf(outputFile, ", 0\n");
        }
        long elapsedMicroseconds = 0;
        long timeoutMicros = 0;

        currentTime = time(NULL);
        if (currentTime - lastFlushTime >= 120)
        {
            // printf("Flushing...\n");
            fflush(outputFile);
            lastFlushTime = currentTime;
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsedMicroseconds = (now.tv_sec - start.tv_sec) * 1000000 +
                              now.tv_nsec / 1000 - start.tv_nsec / 1000;
        // fprintf(outputFile, "%ld,%ld\n", ((intervalMillis * 1000) - elapsedMicroseconds), (intervalMillis * 1000) - elapsedMicroseconds1);

        usleep((intervalMillis * 1000) - elapsedMicroseconds - 20);
        counter++;
    }
}
