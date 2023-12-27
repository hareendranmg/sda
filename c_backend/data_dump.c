#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_FILE_PATH 256

volatile sig_atomic_t stop_flag = 0;

void handle_signal(int signum) {
    printf("Caught signal %d\n", signum);
    if (signum == SIGINT || signum == SIGTERM) {
        printf("Stopping...\n");
        stop_flag = 1;
        printf("Stopped\n");
    }
    printf("Exiting signal handler\n");
}

void cleanup(int serial_fd, FILE *outputFile) {
    printf("Cleaning up...\n");
    if (outputFile) fclose(outputFile);
    if (serial_fd != -1) close(serial_fd);
    printf("Cleaned up\n");
}

void rename_output_file(const char *outputFileName)
{
    char old_file_name[MAX_FILE_PATH];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(old_file_name, sizeof(old_file_name), "%Y%m%d_%H%M%S", t);

    char *file_name = strrchr(outputFileName, '/');
    char *file_name_without_path = file_name ? file_name + 1 : outputFileName;
    char *file_name_without_ext = strrchr(file_name_without_path, '.');
    char *file_ext = file_name_without_ext ? file_name_without_ext : "";

    // Use strncpy for string operations
    strncpy(old_file_name, outputFileName, sizeof(old_file_name) - strlen(file_ext) - 1);
    strncat(old_file_name, "_old_", sizeof(old_file_name) - strlen(old_file_name) - 1);
    strncat(old_file_name, old_file_name, sizeof(old_file_name) - strlen(old_file_name) - 1);
    strncat(old_file_name, file_ext, sizeof(old_file_name) - strlen(old_file_name) - 1);

    printf("Renaming %s to %s\n", outputFileName, old_file_name);
    rename(outputFileName, old_file_name);
}

int open_serial_port(const char *serialPortName) {
    int serial_fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd == -1) {
        perror("Error opening serial port");
    }
    return serial_fd;
}

void set_serial_parameters(int serial_fd, int baudRate, char parity, int dataBits, int stopBits) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0) {
        perror("Error getting serial port attributes");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;

    // Set parity
    if (parity == 'N') {
        tty.c_cflag &= ~PARENB; // No parity
    } else if (parity == 'E') {
        tty.c_cflag |= PARENB;  // Enable parity
        tty.c_cflag &= ~PARODD; // Even parity
    } else if (parity == 'O') {
        tty.c_cflag |= (PARENB | PARODD); // Odd parity
    }

    // Set data bits
    tty.c_cflag &= ~CSIZE;
    if (dataBits == 8) {
        tty.c_cflag |= CS8;
    } else if (dataBits == 7) {
        tty.c_cflag |= CS7;
    } else {
        fprintf(stderr, "Invalid number of data bits\n");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    // Set stop bits
    if (stopBits == 1) {
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
    } else if (stopBits == 2) {
        tty.c_cflag |= CSTOPB; // 2 stop bits
    } else {
        fprintf(stderr, "Invalid number of stop bits\n");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("Error setting serial port attributes");
        cleanup(serial_fd, NULL);
        exit(EXIT_FAILURE);
    }
}

FILE *open_output_file(const char *outputFileName) {
    FILE *outputFile = fopen(outputFileName, "wb");
    if (!outputFile) {
        perror("Error opening output file");
        cleanup(-1, NULL); // Passing -1 as serial_fd to avoid closing an invalid file descriptor
        exit(EXIT_FAILURE);
    }
    return outputFile;
}

int main(int argc, char *argv[]) {
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    if (argc != 7)
    {
        printf("Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *serialPortName = argv[1];
    const char *outputFileName = argv[2];
    int baudRate = atoi(argv[3]);
    char parity = argv[4][0];
    int dataBits = atoi(argv[5]);
    int stopBits = atoi(argv[6]);

    // Execute the Python script before opening the serial port
    char python_command[256];
    snprintf(python_command, sizeof(python_command), "python3 /home/root/sda/python_backend/serial_workaround.py %s %d %c %d %d", serialPortName, baudRate, parity, dataBits, stopBits);
    system(python_command);

    int serial_fd = open_serial_port(serialPortName);

    if (access(outputFileName, F_OK) != -1) {
        rename_output_file(outputFileName);
    }

    FILE *outputFile = open_output_file(outputFileName);

    set_serial_parameters(serial_fd, baudRate, parity, dataBits, stopBits);

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting...\n");
    char buffer[BUFFER_SIZE];

    while (!stop_flag) {
        ssize_t bytesRead = read(serial_fd, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            fwrite(buffer, 1, bytesRead, outputFile);
            fflush(outputFile);
        } else if (bytesRead < 0) {
            perror("Error reading from serial port");
            cleanup(serial_fd, outputFile);
            return EXIT_FAILURE;
        } else {
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
