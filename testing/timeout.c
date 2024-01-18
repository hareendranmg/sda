#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define SERIAL_PORT "/dev/ttyXR7"

// Define a structure to hold command and response pairs
struct CommandPair
{
    unsigned char command;
    size_t responseBytes;
    useconds_t timeoutMicros;
};

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
    usleep(1000);
    readResponse(serialPortFD, commandPair->command, commandPair->responseBytes, commandPair->timeoutMicros);
}

int main(int argc, char *argv[])
{
    if (argc < 4 || (argc - 2) % 3 != 0)
    {
        fprintf(stderr, "Usage: %s <intervalMillis> <command1> <responseBytes1> <timeoutMicros1> [<command2> <responseBytes2> <timeoutMicros2> ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int intervalMillis = atoi(argv[1]);
    if (intervalMillis <= 0)
    {
        fprintf(stderr, "Interval must be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    int serialPortFD = setupSerialPort(SERIAL_PORT);

    // Calculate the number of command-response pairs
    size_t numCommands = (argc - 2) / 3;

    // Create an array of command and response pairs
    struct CommandPair *commandPairs = malloc(numCommands * sizeof(struct CommandPair));

    if (commandPairs == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Populate the array with command and response pairs
    for (size_t i = 0; i < numCommands; ++i)
    {
        commandPairs[i].command = strtol(argv[i * 3 + 2], NULL, 16);
        commandPairs[i].responseBytes = atoi(argv[i * 3 + 3]);
        commandPairs[i].timeoutMicros = atoi(argv[i * 3 + 4]);
    }

    for (;;)
    {
        for (size_t i = 0; i < numCommands; ++i)
        {
            sendAndReceive(serialPortFD, &commandPairs[i]);
        }
        usleep(intervalMillis * 1000);
    }

    // Close the serial port
    close(serialPortFD);

    // Free allocated memory
    free(commandPairs);

    return 0;
}