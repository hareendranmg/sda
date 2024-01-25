#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <time.h>

#define PORT "/dev/ttyXR0"
#define BAUDRATE B2000000
#define PARITY PARENB | PARODD
#define DATABIT CS8
#define STOPBIT 0
#define TIMEOUT_USEC 100

int main()
{
    int fd;
    struct termios options;
    fd_set read_fds;
    char command = 0x0F;
    char buffer[22];

    struct timespec tx_start_time, tx_end_time, rx_start_time, rx_end_time;

    // Open serial port
    fd = open(PORT, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    // Configure serial port options
    tcgetattr(fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag |= (PARITY | DATABIT | STOPBIT);
    options.c_cflag &= ~CRTSCTS;                        // Disable hardware flow control
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_oflag &= ~OPOST;                          // Raw output
    tcsetattr(fd, TCSANOW, &options);

    // Measure write time
    clock_gettime(CLOCK_MONOTONIC, &tx_start_time);
    // Send the command
    if (write(fd, &command, 1) != 1)
    {
        perror("write");
        close(fd);
        exit(1);
    }
    // usleep(100);
    clock_gettime(CLOCK_MONOTONIC, &tx_end_time);

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_USEC;

    clock_gettime(CLOCK_MONOTONIC, &rx_start_time);
    // Wait for data with timeout
    int num_ready_fds = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (num_ready_fds > 0)
    {
        // Read data
        int bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read > 0)
        {
            printf("Received %d bytes: ", bytes_read);
            for (int i = 0; i < bytes_read; ++i)
            {
                printf("%02X ", (unsigned char)buffer[i]);
            }
            printf("\n");
        }
        else
        {
            perror("read");
        }
    }
    else if (num_ready_fds == 0)
    {
        printf("Timeout occurred\n");
    }
    else
    {
        perror("select");
    }
    clock_gettime(CLOCK_MONOTONIC, &rx_end_time);

    long long write_time_ns = (tx_end_time.tv_sec - tx_start_time.tv_sec) * 1e9 + (tx_end_time.tv_nsec - tx_start_time.tv_nsec);
    double write_time_us = write_time_ns / 1e3;
    long long read_time_ns = (rx_end_time.tv_sec - rx_start_time.tv_sec) * 1e9 + (rx_end_time.tv_nsec - rx_start_time.tv_nsec);
    double read_time_us = read_time_ns / 1e3;

    // Display time taken for write and read in microseconds
    printf("Write time: %.2f us\n", write_time_us);
    printf("Read time: %.2f us\n", read_time_us);

    // Close serial port
    close(fd);
    return 0;
}
