// ./command_response_raw_streaming /dev/ttyUSB! command_response 2000000 M 8 1 10240000 csv 5 4b 24 500 41 24 500 42 20 500
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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <errno.h>

#define MAX_FILE_PATH 256
#define RESPONSE_BUFFER_SIZE 128
#define FIFO_PATH "/tmp/datoscoop_fifo_port1"

// Global variables
volatile sig_atomic_t stop_flag = 0;
int fileCounter = 1;
int fifo_fd = -1;

// data structure to store command, responseBytes and timeoutMicros
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    double timeoutMicros;
};

// Response data structure for FIFO
struct ResponseData
{
    uint64_t sequence_number;                    // maps to 'slno' in CSV
    unsigned char command;                       // The command byte (e.g., 4B, 41, 42)
    unsigned char response[127]; // Raw response bytes
    size_t response_length;                      // Actual length of response
    int timeout_occurred;                        // Flag to indicate timeout
    struct timespec timestamp;                   // For tx_time
    uint64_t elapsed_micros;                     // time_elapsed from CSV
};

// Function prototypes
off_t get_file_size(FILE *file);
void handle_signal(int signum);
void cleanup(int serialPortFD, FILE *outputFile);
int open_serial_port(const char *serialPortName);
FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter);
void writeHeaders(FILE *outputFile, int numCommands);
void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, int numCommands, struct CommandPair *commandPairs, double intervalMillis, unsigned char target_command);
int setup_fifo(const char *target_command);
void cleanup_fifo(void);

int main(int argc, char *argv[])
{
    // TODO: Uncomment this block after testing
    // if (setpriority(PRIO_PROCESS, 0, -99) == -1)
    // {
    //     perror("setpriority");
    //     exit(EXIT_FAILURE);
    // }

    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_RR);

    // Attempt to set real-time scheduling policy
    // TODO: Uncomment this block after testing
    // if (sched_setscheduler(0, SCHED_RR, &param) == -1)
    // {
    //     perror("sched_setscheduler");
    //     exit(EXIT_FAILURE);
    // }

    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    if (argc < 11 || (argc - 11) % 3 != 0)
    {
        fprintf(stderr, "Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits> <filesize> <file_extension> <intervalMillis> <target_command> <command1> <responseBytes1> <timeoutMicros1> [<command2> <responseBytes2> <timeoutMicros2> ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    float intervalMillis = atof(argv[9]);
    if (intervalMillis <= 0)
    {
        fprintf(stderr, "Interval must be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    // Extract target command (the command whose response we want to send to Qt)
    unsigned char target_command = strtol(argv[10], NULL, 16);

    // Setup FIFO
    if (setup_fifo(argv[10]) == -1)
    {
        fprintf(stderr, "Failed to setup FIFO\n");
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

    size_t numCommands = (argc - 10) / 3;

    struct CommandPair commandPairs[numCommands];

    if (commandPairs == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < numCommands; ++i)
    {
        commandPairs[i].command = strtol(argv[i * 3 + 11], NULL, 16);
        commandPairs[i].responseBytes = atoi(argv[i * 3 + 12]);
        commandPairs[i].timeoutMicros = atof(argv[i * 3 + 13]);
    }

    // print command pairs
    for (size_t i = 0; i < numCommands; ++i)
    {
        printf("Command %zu: %02X %zu %f\n", i, commandPairs[i].command, commandPairs[i].responseBytes, commandPairs[i].timeoutMicros);
    }

    // Execute the Python script before opening the serial port
    char python_command[MAX_FILE_PATH];
    snprintf(python_command, sizeof(python_command),
             "python3 /home/hareendran/c_dev/sda/python_backend/serial_workaround.py %s %d %c %d %d",
             serialPortName, (int)baudRate, parity, dataBits, stopBits);
    system(python_command);

    int serialPortFD = open_serial_port(serialPortName);

    if (serialPortFD == -1)
    {
        perror("Error opening serial port");
        cleanup(-1, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Register cleanup_fifo for program exit
    atexit(cleanup_fifo);

    printf("Starting...\n");

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter);

    if (!outputFile)
    {
        perror("Error opening output file");
        cleanup(serialPortFD, NULL); // Passing -1 as serialPortFD to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }

    writeHeaders(outputFile, numCommands);

    executeCommands(serialPortFD, outputFile, filesize, outputFileName, fileExtension, numCommands, commandPairs, intervalMillis, target_command);

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
    if (signum == SIGINT || signum == SIGTERM)
    {
        stop_flag = 1;
    }
}

void cleanup(int serialPortFD, FILE *outputFile)
{
    printf("Cleaning up...\n");

    if (outputFile)
    {
        if (fclose(outputFile) == EOF)
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

FILE *open_output_file(const char *outputFileName, const char *fileExtension, int counter)
{
    char newFileName[MAX_FILE_PATH];
    snprintf(newFileName, sizeof(newFileName), "%s_raw_%d.%s", outputFileName, counter, fileExtension);
    FILE *outputFile = fopen(newFileName, "wb");

    if (!outputFile)
    {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    return outputFile;
}

void writeHeaders(FILE *outputFile, int numCommands)
{
    fprintf(outputFile, "slno");
    for (int i = 0; i < numCommands; ++i)
    {
        fprintf(outputFile, ", command%d, raw_response%d", i + 1, i + 1);
    }
    fprintf(outputFile, ", tx_time, time_elapsed\n");
}

int setup_fifo(const char *target_command)
{
    // Create FIFO if it doesn't exist
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST)
    {
        perror("mkfifo failed");
        return -1;
    }

    // Open FIFO for writing only, with both O_NONBLOCK and O_WRONLY
    printf("Opening FIFO for writing: %s\n", FIFO_PATH);
    fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1)
    {
        // If opening failed, try again with O_NONBLOCK
        fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
        if (fifo_fd == -1)
        {
            perror("open fifo failed both blocking and non-blocking");
            return -1;
        }
        printf("Opened FIFO in non-blocking mode\n");
    }
    else
    {
        printf("Opened FIFO in blocking mode\n");
    }

    // Set the fd to non-blocking mode after opening
    int flags = fcntl(fifo_fd, F_GETFL);
    flags |= O_NONBLOCK;
    if (fcntl(fifo_fd, F_SETFL, flags) == -1)
    {
        perror("fcntl failed");
        close(fifo_fd);
        return -1;
    }

    return 0;
}

void cleanup_fifo(void)
{
    if (fifo_fd != -1)
    {
        close(fifo_fd);
    }
    unlink(FIFO_PATH);
}

void executeCommands(int serialPortFD, FILE *outputFile, size_t filesize, const char *outputFileName, const char *fileExtension, int numCommands, struct CommandPair *commandPairs, double intervalMillis, unsigned char target_command)
{
    int bytesRead = 0;
    int i = 0;
    int j = 0;

    unsigned long long int counter = 1;
    unsigned long long int tx_elapsed = 0;
    long elapsedMicroseconds = 0;

    struct timeval tv;
    struct timespec start, now, start1;
    struct timespec last_fifo_update = {0, 0}; // Track last FIFO update time
    struct timespec current_time;
    struct timespec file_timestamp;

    double intervelUs = intervalMillis * 1000; // interval in microseconds with adjustment
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE];
    char timebuffer[80];

    // header bytes count
    size_t currentFileSize = 0;

    struct ResponseData response_data;
    const long FIFO_UPDATE_INTERVAL_NS = 1000000000; // 1 second in nanoseconds

    while (!stop_flag)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        if (counter > 2)
        {
            tx_elapsed = (start.tv_sec - start1.tv_sec) * 1000000 +
                         (start.tv_nsec - start1.tv_nsec) / 1000;
            gettimeofday(&tv, NULL);
            strftime(timebuffer, sizeof(timebuffer), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
            fprintf(outputFile, ", %s.", timebuffer);
            fprintf(outputFile, "%03ld.%03ld", tv.tv_usec / 1000, tv.tv_usec % 1000);
            fprintf(outputFile, ", %llu\n", tx_elapsed);
        }

        start1 = start;

        fprintf(outputFile, "%llu", counter);

        for (i = 0; i < numCommands; ++i)
        {
            fprintf(outputFile, ", %02X, ", commandPairs[i].command);
            write(serialPortFD, &commandPairs[i].command, sizeof(commandPairs[i].command));
            usleep(commandPairs[i].timeoutMicros);
            bytesRead = read(serialPortFD, responseBuffer, commandPairs[i].responseBytes);
            if (bytesRead > 0)
            {
                for (j = 0; j < bytesRead; ++j)
                {
                    fprintf(outputFile, "%02X", responseBuffer[j]);
                }
            }
            else
            {
                fprintf(outputFile, "timeout");
            }

            if (commandPairs[i].command == target_command && fifo_fd != -1)
            {

                clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);

                // Calculate time difference
                long time_diff_ns = (current_time.tv_sec - last_fifo_update.tv_sec) * 1000000000L + (current_time.tv_nsec - last_fifo_update.tv_nsec);

                // Only send if 1 second has elapsed since last update
                if (time_diff_ns >= FIFO_UPDATE_INTERVAL_NS)
                {
                    response_data.sequence_number = counter - 1;
                    response_data.command = commandPairs[i].command;
                    response_data.timeout_occurred = (bytesRead <= 0);

                    if (bytesRead > 0)
                    {
                        memcpy(response_data.response, responseBuffer, bytesRead);
                        response_data.response_length = bytesRead;
                    }
                    else
                    {
                        response_data.response_length = 0;
                    }

                    file_timestamp.tv_sec = tv.tv_sec;
                    file_timestamp.tv_nsec = tv.tv_usec * 1000;

                    response_data.timestamp = file_timestamp;
                    response_data.elapsed_micros = tx_elapsed;

                    // Non-blocking write to FIFO
                    write(fifo_fd, &response_data, sizeof(response_data));

                    // Update the last FIFO update time
                    last_fifo_update = current_time;
                }
            }

            tcflush(serialPortFD, TCIOFLUSH);
        }

        currentFileSize = get_file_size(outputFile);
        if (currentFileSize > filesize)
        {
            fclose(outputFile);
            outputFile = open_output_file(outputFileName, fileExtension, ++fileCounter);
        }

        if (counter == 1)
        {
            fprintf(outputFile, ", 0\n");
        }

        elapsedMicroseconds = 0;

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsedMicroseconds = ((now.tv_sec - start.tv_sec) * 1000000 ) + ((now.tv_nsec - start.tv_nsec) / 1000);

        if (elapsedMicroseconds < intervelUs)
        {
            usleep(intervelUs - elapsedMicroseconds - 20);
        }

        counter++;
    }
}
