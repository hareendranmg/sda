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
#include <sys/select.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#define MAX_FILE_PATH 256
#define MAX_DATA_TYPES 1000

// Global variables
volatile sig_atomic_t stop_flag = 0;
size_t currentFileSize = 0;
int fileCounter = 1;

// data structure to store command, responseBytes and timeoutMicros
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    double timeoutMicros;
};

// Function prototypes
off_t get_file_size(FILE *file);
void handle_signal(int signum);
void cleanup(int serialPortFD, FILE *outputFile, struct CommandPair *commandPairs);
int open_serial_port(const char *serialPortName);
FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, struct CommandPair *commandPairs);
void timespec_to_hhmmssmsus(struct timespec *ts, char *output, size_t size);
int readResponse(FILE *outputFile, int serialPortFD, size_t responseBytes, double timeoutMicros);
void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs);
void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, double intervalMillis);

int main(int argc, char *argv[])
{
    if (setpriority(PRIO_PROCESS, 0, -20) == -1)
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

    if (argc < 13 || (argc - 10) % 3 != 0)
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
        commandPairs[i].timeoutMicros = atof(argv[i * 3 + 12]);
    }

    // print command pairs
    for (size_t i = 0; i < numCommands; ++i)
    {
        printf("Command %zu: %02X %zu %f\n", i, commandPairs[i].command, commandPairs[i].responseBytes, commandPairs[i].timeoutMicros);
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
        cleanup(-1, NULL, commandPairs); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");

    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR)};
    if (sched_setscheduler(0, SCHED_RR, &sp) != 0)
    {
        perror("Error setting process priority");
        cleanup(serialPortFD, NULL, commandPairs);
        exit(EXIT_FAILURE);
    }

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter, commandPairs);

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(serialPortFD, NULL, commandPairs); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    writeHeaders(outputFile, numCommands, commandPairs);

    executeCommands(serialPortFD, outputFile, filesize, outputFileName, fileExtension, numCommands, commandPairs, intervalMillis);

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serialPortFD, outputFile, commandPairs);

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

void cleanup(int serialPortFD, FILE *outputFile, struct CommandPair *commandPairs)
{
    printf("Cleaning up...\n");

    if (commandPairs != NULL)
    {
        free(commandPairs);
    }

    if (outputFile)
    {
        fflush(outputFile);
        fclose(outputFile);
    }

    if (serialPortFD != -1)
    {
        close(serialPortFD);
    }

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

FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter, struct CommandPair *commandPairs)
{
    char newFileName[MAX_FILE_PATH];
    snprintf(newFileName, sizeof(newFileName), "%s_%d.%s", outputFileName, counter, fileExtension);
    FILE *outputFile = fopen(newFileName, "wb");

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(-1, NULL, commandPairs); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
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

void writeHeaders(FILE *outputFile, size_t numCommands, const struct CommandPair *commandPairs)
{
    fprintf(outputFile, "slno");
    for (size_t i = 0; i < numCommands; ++i)
    {
        fprintf(outputFile, ", command%d, response%d", i + 1, i + 1);
    }
    fprintf(outputFile, ", time_elapsed\n");
}

void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, size_t numCommands, struct CommandPair *commandPairs, double intervalMillis)
{
    unsigned long long int counter = 1;
    struct timeval tv;

    //TODO: Remove flush checking variables and related code
    time_t currentTime = time(NULL);
    time_t lastFlushTime = time(NULL);

    struct timespec start, now, start1;
    unsigned long long int tx_elapsed = 0;
    double intervelUs = intervalMillis * 1000; // interval in microseconds with adjustment
    printf("IntervalMs: %f\n", intervalMillis);
    printf("IntervalUs: %f\n", intervelUs);

    // header bytes count
    currentFileSize = 0;

    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        if (counter > 2)
        {
            tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
                         (start.tv_nsec - start1.tv_nsec) / 1000;
            gettimeofday(&tv, NULL);
            char timebuffer[80];
            strftime(timebuffer, sizeof(timebuffer), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
            fprintf(outputFile, ", %s.", timebuffer);
            fprintf(outputFile, "%03d.%03d", tv.tv_usec / 1000, tv.tv_usec % 1000);
            fprintf(outputFile, ", %llu\n", tx_elapsed);
        }

        start1 = start;

        fprintf(outputFile, "%llu", counter);

        for (size_t i = 0; i < numCommands; ++i)
        {
            fprintf(outputFile, ", %02X, ", commandPairs[i].command);
            write(serialPortFD, &commandPairs[i].command, sizeof(commandPairs[i].command));
            unsigned char *buffer = malloc(commandPairs[i].responseBytes);
            usleep(commandPairs[i].timeoutMicros);
            int bytesRead = read(serialPortFD, buffer, commandPairs[i].responseBytes);
            if (bytesRead > 0)
            {
                for (int i = 0; i < bytesRead; ++i)
                {
                    fprintf(outputFile, "%02X", buffer[i]);
                }
            } else {
                fprintf(outputFile, "timeout");
            }
            tcflush(serialPortFD, TCIOFLUSH);
            free(buffer);
        }

        currentFileSize = get_file_size(outputFile);
        if (currentFileSize > filesize)
        {
            fclose(outputFile);
            outputFile = open_output_file(outputFileName, fileExtension, ++fileCounter, commandPairs);
        }

        if (counter == 1)
        {
            fprintf(outputFile, ", 0\n");
        }

        long elapsedMicroseconds = 0;

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsedMicroseconds = ((now.tv_sec - start.tv_sec) * 1000000 ) + ((now.tv_nsec - start.tv_nsec) / 1000);

        if (elapsedMicroseconds < intervelUs)
        {
            usleep(intervelUs - elapsedMicroseconds - 20);
        }

        counter++;
    }
}