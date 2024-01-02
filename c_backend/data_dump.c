#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#define BUFFER_SIZE 1024
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

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0)
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

int main(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    if (argc != 9)
    {
        fprintf(stderr, "Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits> <filesize> <file_extension>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *serialPortName = argv[1];
    const char *outputFileName = argv[2];
    speed_t baudRate = atoi(argv[3]);
    char parity = argv[4][0];
    int dataBits = atoi(argv[5]);
    int stopBits = atoi(argv[6]);
    size_t filesize = atoi(argv[7]);
    const char *fileExtension = argv[8];

    if (baudRate < 0 || baudRate > 4000000)
    {
        fprintf(stderr, "Invalid baud rate\n");
        return EXIT_FAILURE;
    }

    // the filesize is in bytes, we need to check if it is fall within the range of 1MB to 1GB
    // if (filesize < 1000000 || filesize > 1000000000)
    // {
    //     fprintf(stderr, "Invalid filesize\n");
    //     return EXIT_FAILURE;
    // }

    // Execute the Python script before opening the serial port
    char python_command[MAX_FILE_PATH];
    snprintf(python_command, sizeof(python_command),
             "python3 /home/root/sda/python_backend/serial_workaround.py %s %d %c %d %d",
             serialPortName, (int)baudRate, parity, dataBits, stopBits);
    system(python_command);

    int serial_fd = open_serial_port(serialPortName);

    FILE *outputFile = open_output_file(outputFileName, fileExtension, fileCounter);

    set_serial_parameters(serial_fd, baudRate, parity, dataBits, stopBits);

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");
    char buffer[BUFFER_SIZE];

    while (!stop_flag)
    {
        ssize_t bytesRead = read(serial_fd, buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
            if (currentFileSize + bytesRead > filesize)
            {
                fclose(outputFile);
                outputFile = open_output_file(outputFileName, fileExtension, ++fileCounter);
                currentFileSize = 0;
            }

            fwrite(buffer, 1, bytesRead, outputFile);
            fflush(outputFile);

            currentFileSize += bytesRead;
        }
        else if (bytesRead < 0)
        {
            perror("Error reading from serial port");
            cleanup(serial_fd, outputFile);
            return EXIT_FAILURE;
        }
        else
        {
            // No data read
        }
    }

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serial_fd, outputFile);

    // Reset signal handlers to default behavior
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    return EXIT_SUCCESS;
}
