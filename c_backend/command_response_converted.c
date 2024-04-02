// ./command_response_raw /dev/ttyXR0 command_response 1000000 M 8 1 102400 csv 5 4b 24 500 41 24 500 42 20 500
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
#include <stddef.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#define MAX_FILE_PATH 256
#define RESPONSE_BUFFER_SIZE 128
#define MAX_DATA_TYPES 1000

// Global variables
volatile sig_atomic_t stop_flag = 0;
int fileCounter = 1;

// data structure to store command, responseBytes and timeoutMicros
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    double timeoutMicros;
    char dataTypes[MAX_DATA_TYPES];
};

// Function prototypes
off_t get_file_size(FILE *file);
void handle_signal(int signum);
void cleanup(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted);
int open_serial_port(const char *serialPortName);
FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, const char *mode);
void writeHeaders(FILE *outputFile, size_t numCommands, const char *mode);
void executeCommands(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, double intervalMillis);

int main(int argc, char *argv[])
{
    if (setpriority(PRIO_PROCESS, 0, -99) == -1)
    {
        perror("setpriority");
        exit(EXIT_FAILURE);
    }

    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_RR);

    // Attempt to set real-time scheduling policy
    if (sched_setscheduler(0, SCHED_RR, &param) == -1)
    {
        perror("sched_setscheduler");
        exit(EXIT_FAILURE);
    }

    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    if (argc < 14 || (argc - 10) % 3 != 0)
    {
        fprintf(stderr, "Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits> <filesize> <file_extension> <intervalMillis> <command1> <responseBytes1> <timeoutMicros1> [<command2> <responseBytes2> <timeoutMicros2> ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    float intervalMillis = atof(argv[9]);
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

    struct CommandPair commandPairs[numCommands];

    if (commandPairs == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < numCommands; ++i)
    {
        commandPairs[i].command = strtol(argv[i * 4 + 10], NULL, 16);
        commandPairs[i].responseBytes = atoi(argv[i * 4 + 11]);
        commandPairs[i].timeoutMicros = atof(argv[i * 4 + 12]);
        strncpy(commandPairs[i].dataTypes, argv[i * 4 + 13], sizeof(commandPairs[i].dataTypes) - 1);
        commandPairs[i].dataTypes[sizeof(commandPairs[i].dataTypes) - 1] = '\0'; // Ensure null termination
    }

    // print commandPairs
    for (size_t i = 0; i < numCommands; ++i)
    {
        printf("commandPairs[%zu].command: %02X\n", i, commandPairs[i].command);
        printf("commandPairs[%zu].responseBytes: %zu\n", i, commandPairs[i].responseBytes);
        printf("commandPairs[%zu].timeoutMicros: %f\n", i, commandPairs[i].timeoutMicros);
        printf("commandPairs[%zu].dataTypes: %s\n", i, commandPairs[i].dataTypes);
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
        exit(EXIT_FAILURE);
    }

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");

    FILE *outputFileRaw = open_output_file(outputFileName, fileExtension, fileCounter, "raw");
    FILE *outputFileConverted = open_output_file(outputFileName, fileExtension, fileCounter, "converted");

    if (!outputFileRaw || !outputFileConverted)
    {
        perror("Error opening output file");
        cleanup(serialPortFD, outputFileRaw, outputFileConverted);
        exit(EXIT_FAILURE);
    }

    writeHeaders(outputFileRaw, numCommands, "raw");
    writeHeaders(outputFileConverted, numCommands, "converted");

    executeCommands(serialPortFD, outputFileRaw, outputFileConverted, filesize, outputFileName, fileExtension, numCommands, commandPairs, intervalMillis);

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serialPortFD, outputFileRaw, outputFileConverted);

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
    if (signum == SIGINT || signum == SIGTERM)
    {
        stop_flag = 1;
    }
}

void cleanup(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted)
{
    printf("Cleaning up...\n");

    if (outputFileRaw)
    {
        if (fclose(outputFileRaw) == EOF)
        {
            perror("Error closing output file");
            exit(EXIT_FAILURE);
        }
    }

    if (outputFileConverted)
    {
        if (fclose(outputFileConverted) == EOF)
        {
            perror("Error closing output file");
            exit(EXIT_FAILURE);
        }
    }

    if (serialPortFD != -1)
    {
        if (close(serialPortFD) == -1)
        {
            perror("Error closing serial port");
            exit(EXIT_FAILURE);
        }
    }

    printf("Cleaned up\n");
}

int open_serial_port(const char *serialPortName)
{
    int serialPortFD = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serialPortFD == -1)
    {
        perror("Error opening serial port");
        exit(EXIT_FAILURE);
    }
    return serialPortFD;
}

FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, const char *mode)
{
    char newFileName[MAX_FILE_PATH];
    snprintf(newFileName, sizeof(newFileName), "%s_%s_%d.%s", outputFileName, mode, counter, fileExtension);
    FILE *outputFileRaw = fopen(newFileName, "wb");

    if (!outputFileRaw)
    {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    return outputFileRaw;
}

void writeHeaders(FILE *outputFile, size_t numCommands, const char *mode)
{
    fprintf(outputFile, "slno");
    for (size_t i = 0; i < numCommands; ++i)
    {
        fprintf(outputFile, ", command%d, %s_response%d", i + 1, mode, i + 1);
    }
    fprintf(outputFile, ", tx_time, time_elapsed\n");
}

void executeCommands(int serialPortFD, FILE *outputFileRaw, FILE *outputFileConverted, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, double intervalMillis)
{
    int bytesRead = 0;
    int remainingBytes = 0;
    int i = 0;
    int j = 0;

    unsigned long long int counter = 1;
    unsigned long long int tx_elapsed = 0;
    long elapsedMicroseconds = 0;

    struct timeval tv;
    struct timespec start, now, start1;

    double intervelUs = intervalMillis * 1000; // interval in microseconds with adjustment

    char timebuffer[80];

    // header bytes count
    size_t currentFileSize = 0;

    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        if (counter > 2)
        {
            tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
                         (start.tv_nsec - start1.tv_nsec) / 1000;
            gettimeofday(&tv, NULL);
            strftime(timebuffer, sizeof(timebuffer), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
            fprintf(outputFileRaw, ", %s.", timebuffer);
            fprintf(outputFileRaw, "%03d.%03d", tv.tv_usec / 1000, tv.tv_usec % 1000);
            fprintf(outputFileRaw, ", %llu\n", tx_elapsed);
            fprintf(outputFileConverted, ", %s.", timebuffer);
            fprintf(outputFileConverted, "%03d.%03d", tv.tv_usec / 1000, tv.tv_usec % 1000);
            fprintf(outputFileConverted, ", %llu\n", tx_elapsed);
        }

        start1 = start;

        fprintf(outputFileRaw, "%llu", counter);
        fprintf(outputFileConverted, "%llu", counter);

        for (i = 0; i < numCommands; ++i)
        {
            unsigned char *responseBuffer = NULL;
            responseBuffer = malloc(commandPairs[i].responseBytes);

            if (responseBuffer == NULL)
            {
                perror("Error allocating memory");
                cleanup(serialPortFD, outputFileRaw, outputFileConverted);
                exit(EXIT_FAILURE);
                // Handle the error appropriately, e.g., exit the program or skip the current iteration
            }

            fprintf(outputFileRaw, ", %02X, ", commandPairs[i].command);
            fprintf(outputFileConverted, ", %02X, ", commandPairs[i].command);
            write(serialPortFD, &commandPairs[i].command, sizeof(commandPairs[i].command));
            usleep(commandPairs[i].timeoutMicros);
            bytesRead = read(serialPortFD, responseBuffer, commandPairs[i].responseBytes);
            if (bytesRead > 0)
            {
                for (j = 0; j < bytesRead; ++j)
                {
                    fprintf(outputFileRaw, "%02X", responseBuffer[j]);
                }

                remainingBytes = bytesRead;

                char dataTypes[MAX_DATA_TYPES];
                strncpy(dataTypes, commandPairs[i].dataTypes, sizeof(dataTypes) - 1);
                dataTypes[sizeof(dataTypes) - 1] = '\0'; // Ensure null termination

                char *dataTypePtr = strtok(dataTypes, ","); // Parse data types string
                unsigned char *tempBuffer = responseBuffer;

                while (dataTypePtr != NULL && remainingBytes > 0)
                {
                    if (strcmp(dataTypePtr, "hex8") == 0 && remainingBytes >= 1)
                    {
                        fprintf(outputFileConverted, " 0x%02x", tempBuffer[0]);
                        remainingBytes -= 1;
                        tempBuffer += 1;
                    }
                    else if (strcmp(dataTypePtr, "hex16") == 0 && remainingBytes >= 2)
                    {
                        uint16_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " 0x%04x", value);
                        remainingBytes -= 2;
                        tempBuffer += 2;
                    }
                    else if (strcmp(dataTypePtr, "hex32") == 0 && remainingBytes >= 4)
                    {
                        uint32_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " 0x%08x", value);
                        remainingBytes -= 4;
                        tempBuffer += 4;
                    }
                    else if (strcmp(dataTypePtr, "uint8") == 0 && remainingBytes >= 1)
                    {
                        uint8_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " +%u", value);
                        remainingBytes -= 1;
                        tempBuffer += 1;
                    }
                    else if (strcmp(dataTypePtr, "uint16") == 0 && remainingBytes >= 2)
                    {
                        uint16_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " +%u", value);
                        remainingBytes -= 2;
                        tempBuffer += 2;
                    }
                    else if (strcmp(dataTypePtr, "uint32") == 0 && remainingBytes >= 4)
                    {
                        uint32_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " +%u", value);
                        remainingBytes -= 4;
                        tempBuffer += 4;
                    }
                    else if (strcmp(dataTypePtr, "int32") == 0 && remainingBytes >= 4)
                    {
                        int32_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " %d", value);
                        remainingBytes -= 4;
                        tempBuffer += 4;
                    }
                    else if (strcmp(dataTypePtr, "int16") == 0 && remainingBytes >= 2)
                    {
                        int16_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " %d", value);
                        remainingBytes -= 2;
                        tempBuffer += 2;
                    }
                    else if (strcmp(dataTypePtr, "int8") == 0 && remainingBytes >= 1)
                    {
                        int8_t value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " %d", value);
                        remainingBytes -= 1;
                        tempBuffer += 1;
                    }
                    else if (strcmp(dataTypePtr, "float32") == 0 && remainingBytes >= 4)
                    {
                        float value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " %f", value);
                        remainingBytes -= 4;
                        tempBuffer += 4;
                    }
                    else if (strcmp(dataTypePtr, "float64") == 0 && remainingBytes >= 8)
                    {
                        double value;
                        memcpy(&value, tempBuffer, sizeof(value));
                        fprintf(outputFileConverted, " %f", value);
                        remainingBytes -= 8;
                        tempBuffer += 8;
                    }
                    else
                    {
                        // fprintf(stderr, "Not enough bytes to process data type: %s\n", dataTypePtr);
                        break;
                    }

                    dataTypePtr = strtok(NULL, ","); // Get next data type
                }
            }
            else
            {
                fprintf(outputFileRaw, "timeout");
                fprintf(outputFileConverted, "timeout");
            }

            tcflush(serialPortFD, TCIOFLUSH);
            free(responseBuffer);
        }

        currentFileSize = get_file_size(outputFileRaw);
        if (currentFileSize > filesize)
        {
            fclose(outputFileRaw);
            fclose(outputFileConverted);
            outputFileRaw = open_output_file(outputFileName, fileExtension, ++fileCounter, "raw");
            outputFileConverted = open_output_file(outputFileName, fileExtension, fileCounter, "converted");
        }

        if (counter == 1)
        {
            fprintf(outputFileRaw, ", 0\n");
            fprintf(outputFileConverted, ", 0\n");
        }

        elapsedMicroseconds = 0;

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsedMicroseconds = ((now.tv_sec - start.tv_sec) * 1000000) + ((now.tv_nsec - start.tv_nsec) / 1000);

        if (elapsedMicroseconds < intervelUs)
        {
            usleep(intervelUs - elapsedMicroseconds - 20);
        }

        counter++;
    }
}
