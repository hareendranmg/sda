// ./command_response /dev/ttyXR7 command_response 2000000 O 8 1 10240000 csv 100 0f 22 100 1e 22 100 12 10 10
// ./command_response /dev/ttyXR0 command_response 2000000 O 8 1 10240000 csv 1000 0f 22 500 hex8,int32,float32,uint32,int16,int32,uint16,uint8
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/select.h>
#include <stddef.h>
#include <sys/stat.h>

#define MAX_FILE_PATH 256
#define MAX_DATA_TYPES 1000

volatile sig_atomic_t stop_flag = 0;
size_t currentFileSize = 0;
int fileCounter = 1;

// Struct for command pair
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    useconds_t timeoutMicros;
    char dataTypes[MAX_DATA_TYPES];
};

// Function prototypes
off_t get_file_size(FILE *file);
void handle_signal(int signum);
void cleanup(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, struct CommandPair *commandPairs);
int open_serial_port(const char *serialPortName);
void set_serial_parameters(int serialPortFD, speed_t baudRate, char parity, int dataBits, int stopBits, struct CommandPair *commandPairs);
FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, const char *mode);
void timespec_to_hhmmssmsus(struct timespec *ts, char *output, size_t size);
int readResponse(FILE *outputFileRaw, FILE *outputFileConverted, int serialPortFD, unsigned char expectedCommand, size_t responseBytes, useconds_t timeoutMicros, struct CommandPair commandPairs);
void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs, const char *mode);
void executeCommands(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, size_t filesize, const char *outputFileNameRaw, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, int intervalMillis);

int main(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

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
        cleanup(-1, NULL, NULL, commandPairs); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    //* Disabled explicit setting of serial parameters because it is done by the Python script
    // set_serial_parameters(serialPortFD, baudRate, parity, dataBits, stopBits, commandPairs);

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

    FILE *outputFileRaw = open_output_file(outputFileName, fileExtension, fileCounter, "raw");
    FILE *outputFileConverted = open_output_file(outputFileName, fileExtension, fileCounter, "converted");

    if (!outputFileRaw || !outputFileConverted)
    {
        perror("Error opening output file");
        cleanup(serialPortFD, NULL, NULL, commandPairs);
    }

    writeHeaders(outputFileRaw, numCommands, commandPairs, "raw");
    writeHeaders(outputFileConverted, numCommands, commandPairs, "converted");

    executeCommands(serialPortFD, outputFileRaw, outputFileConverted, filesize, outputFileName, fileExtension, numCommands, commandPairs, intervalMillis);

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serialPortFD, outputFileRaw, outputFileConverted, commandPairs);

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

void cleanup(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, struct CommandPair *commandPairs)
{
    printf("Cleaning up...\n");

    if (commandPairs != NULL)
    {
        free(commandPairs);
    }

    // TODO: free(commandPairs);
    if (outputFileRaw)
    {
        fflush(outputFileRaw);
        fclose(outputFileRaw);
    }
    if (outputFileConverted)
    {
        fflush(outputFileConverted);
        fclose(outputFileConverted);
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

void set_serial_parameters(int serialPortFD, speed_t baudRate, char parity, int dataBits, int stopBits, struct CommandPair *commandPairs)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serialPortFD, &tty) != 0)
    {
        perror("Error getting serial port attributes");
        cleanup(serialPortFD, NULL, NULL, commandPairs);
        exit(EXIT_FAILURE);
    }

    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;

    // Set parity
    if (parity == 'N')
    {
        tty.c_cflag &= ~(PARENB | PARODD | CMSPAR); // Disable parity
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
        cleanup(serialPortFD, NULL, NULL, commandPairs);
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
        cleanup(serialPortFD, NULL, NULL, commandPairs);
        exit(EXIT_FAILURE);
    }

    if (tcsetattr(serialPortFD, TCSAFLUSH, &tty) != 0)
    {
        perror("Error setting serial port attributes");
        cleanup(serialPortFD, NULL, NULL, commandPairs);
        exit(EXIT_FAILURE);
    }
}

FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, const char *mode)
{
    char newFileName[MAX_FILE_PATH];
    snprintf(newFileName, sizeof(newFileName), "%s_%s_%d.%s", outputFileName, mode, counter, fileExtension);
    FILE *outputFile = fopen(newFileName, "wb");

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(-1, NULL, NULL, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
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

int readResponse(FILE *outputFileRaw, FILE *outputFileConverted, int serialPortFD, unsigned char expectedCommand, size_t responseBytes, useconds_t timeoutMicros, struct CommandPair commandPairs)
{
    unsigned char *buffer = malloc(responseBytes);
    size_t bytesRead = 0;

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
        free(buffer);

        return 0;
    }

    if (FD_ISSET(serialPortFD, &readSet))
    {
        int bytesRead = read(serialPortFD, buffer, responseBytes);
        if (bytesRead > 0)
        {
            size_t remainingBytes = bytesRead;

            for (int i = 0; i < bytesRead; ++i)
            {
                fprintf(outputFileRaw, "%02x", buffer[i]);
            }

            char *dataTypePtr = strtok(commandPairs.dataTypes, ","); // Parse data types string

            while (dataTypePtr != NULL && remainingBytes > 0) {
                if (strcmp(dataTypePtr, "hex8") == 0 && remainingBytes >= 1)
                {
                    fprintf(outputFileConverted, " 0x%02x", buffer[0]);
                    remainingBytes -= 1;
                    buffer += 1;
                }
                else if (strcmp(dataTypePtr, "hex16") == 0 && remainingBytes >= 2)
                {
                    uint16_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " 0x%04x", value);
                    remainingBytes -= 2;
                    buffer += 2;
                }
                else if (strcmp(dataTypePtr, "hex32") == 0 && remainingBytes >= 4)
                {
                    uint32_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " 0x%08x", value);
                    remainingBytes -= 4;
                    buffer += 4;
                }
                else if (strcmp(dataTypePtr, "uint8") == 0 && remainingBytes >= 1)
                {
                    uint8_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " +%u", value);
                    remainingBytes -= 1;
                    buffer += 1;
                }
                else if (strcmp(dataTypePtr, "uint16") == 0 && remainingBytes >= 2)
                {
                    uint16_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " +%u", value);
                    remainingBytes -= 2;
                    buffer += 2;
                }
                else if (strcmp(dataTypePtr, "uint32") == 0 && remainingBytes >= 4)
                {
                    uint32_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " +%u", value);
                    remainingBytes -= 4;
                    buffer += 4;
                }
                else if (strcmp(dataTypePtr, "int32") == 0 && remainingBytes >= 4)
                {
                    int32_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " %d", value);
                    remainingBytes -= 4;
                    buffer += 4;
                }
                else if (strcmp(dataTypePtr, "int16") == 0 && remainingBytes >= 2)
                {
                    int16_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " %d", value);
                    remainingBytes -= 2;
                    buffer += 2;
                }
                else if (strcmp(dataTypePtr, "int8") == 0 && remainingBytes >= 1)
                {
                    int8_t value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " %d", value);
                    remainingBytes -= 1;
                    buffer += 1;
                }
                else if (strcmp(dataTypePtr, "float32") == 0 && remainingBytes >= 4)
                {
                    float value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " %f", value);
                    remainingBytes -= 4;
                    buffer += 4;
                }
                else if (strcmp(dataTypePtr, "float64") == 0 && remainingBytes >= 8)
                {
                    double value;
                    memcpy(&value, buffer, sizeof(value));
                    fprintf(outputFileConverted, " %f", value);
                    remainingBytes -= 8;
                    buffer += 8;
                }
                else
                {
                    // fprintf(stderr, "Not enough bytes to process data type: %s\n", dataTypePtr);
                    break;
                }

                dataTypePtr = strtok(NULL, ","); // Get next data type
            }
        }

        // free(buffer);

        return bytesRead;
    }
    else
    {
        fprintf(outputFileRaw, "timeout");
        fprintf(outputFileConverted, "timeout");
        free(buffer);

        return 9;
    }

    fprintf(outputFileRaw, ", ");
    free(buffer);

    return 0;
}

void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs, const char *mode)
{
    fprintf(outputFile, "slno");
    for (size_t i = 0; i < numCommands; ++i)
    {
        fprintf(outputFile, ", command%d, %s_response%d", i + 1, mode, i + 1);
    }
    fprintf(outputFile, ", tx_time");
    fprintf(outputFile, ", time_elapsed\n");
}

void executeCommands(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, int intervalMillis)
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
            // write start time to file
            fprintf(outputFileRaw, ", %llu", start.tv_sec);
            fprintf(outputFileRaw, ", %llu\n", tx_elapsed);
            fprintf(outputFileConverted, ", %llu", start.tv_sec);
            fprintf(outputFileConverted, ", %llu\n", tx_elapsed);
        }

        currentFileSize = get_file_size(outputFileRaw);
        if (currentFileSize > filesize)
        {
            fclose(outputFileRaw);
            fclose(outputFileConverted);
            outputFileRaw = open_output_file(outputFileName, fileExtension, ++fileCounter, "raw");
            outputFileConverted = open_output_file(outputFileName, fileExtension, fileCounter, "converted");
        }

        start1 = start;
        fprintf(outputFileRaw, "%llu", counter);
        fprintf(outputFileConverted, "%llu", counter);
        for (size_t i = 0; i < numCommands; ++i)
        {
            fprintf(outputFileRaw, ", %02X, ", commandPairs[i].command);
            fprintf(outputFileConverted, ", %02X,", commandPairs[i].command);
            write(serialPortFD, &(commandPairs[i].command), sizeof(commandPairs[i].command));
            usleep(200);
            readResponse(outputFileRaw, outputFileConverted, serialPortFD, commandPairs[i].command, commandPairs[i].responseBytes, commandPairs[i].timeoutMicros, commandPairs[i]);
            tcflush(serialPortFD, TCIOFLUSH);
        }
        // fprintf ", 0\n"
        if (counter == 1)
        {
            fprintf(outputFileRaw, ", 0\n");
            fprintf(outputFileConverted, ", 0\n");
        }
        long elapsedMicroseconds = 0;
        long timeoutMicros = 0;

        currentTime = time(NULL);
        if (currentTime - lastFlushTime >= 120)
        {
            fflush(outputFileRaw);
            fflush(outputFileConverted);
            lastFlushTime = currentTime;
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsedMicroseconds = (now.tv_sec - start.tv_sec) * 1000000 +
                              now.tv_nsec / 1000 - start.tv_nsec / 1000;
        // fprintf(outputFileRaw, "%ld,%ld\n", ((intervalMillis * 1000) - elapsedMicroseconds), (intervalMillis * 1000) - elapsedMicroseconds1);

        usleep((intervalMillis * 1000) - elapsedMicroseconds - 20);
        counter++;
    }
}
