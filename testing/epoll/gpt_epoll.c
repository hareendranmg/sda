#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include <time.h>

#define PORT "/dev/ttyXR0"
#define BAUDRATE B2000000
#define PARITY PARENB | PARODD
#define DATABIT CS8
#define STOPBIT 0
#define TIMEOUT_USEC 0

int main()
{
    int fd, epollfd;
    struct termios options;
    struct epoll_event ev, events;
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

    // Create epoll instance
    epollfd = epoll_create1(0);
    if (epollfd < 0)
    {
        perror("epoll_create1");
        close(fd);
        exit(1);
    }

    // Add serial port file descriptor to epoll
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        perror("epoll_ctl");
        close(fd);
        close(epollfd);
        exit(1);
    }

    // Measure write time
    clock_gettime(CLOCK_MONOTONIC, &tx_start_time);
    // Send the command
    if (write(fd, &command, 1) != 1)
    {
        perror("write");
        close(fd);
        close(epollfd);
        exit(1);
    }
    clock_gettime(CLOCK_MONOTONIC, &tx_end_time);

    clock_gettime(CLOCK_MONOTONIC, &rx_start_time);
    // Wait for data with timeout
    int num_events = epoll_wait(epollfd, &events, 1, TIMEOUT_USEC);
    if (num_events > 0)
    {
        // Read data
        int bytes_read = read(fd, buffer, sizeof(buffer));
        clock_gettime(CLOCK_MONOTONIC, &rx_end_time);

        if (bytes_read > 0)
        {
            printf("Received %zd bytes: ", bytes_read);
            for (ssize_t i = 0; i < bytes_read; ++i)
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
    else if (num_events == 0)
    {
        printf("Timeout occurred\n");
    }
    else
    {
        perror("epoll_wait");
    }

    long long write_time_ns = (tx_end_time.tv_sec - tx_start_time.tv_sec) * 1e9 + (tx_end_time.tv_nsec - tx_start_time.tv_nsec);
    double write_time_us = write_time_ns / 1e3;
    long long read_time_ns = (rx_end_time.tv_sec - rx_start_time.tv_sec) * 1e9 + (rx_end_time.tv_nsec - rx_start_time.tv_nsec);
    double read_time_us = read_time_ns / 1e3;

    // Display time taken for write and read in microseconds
    printf("Write time: %.2f us\n", write_time_us);
    printf("Read time: %.2f us\n", read_time_us);

    // Close serial port and epoll instance
    close(fd);
    close(epollfd);
    return 0;
}