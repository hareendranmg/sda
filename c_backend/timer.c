#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#define PORT "/dev/ttyXR0"
#define BAUDRATE 2000000
#define DATABITS 8
#define PARITY ODD
#define STOPBITS 1
#define DELAY_US 10
#define READ_TIMEOUT_US 1
#define BUF_SIZE 22

int main()
{
    // Open serial port
    int fd = open(PORT, O_RDWR | O_NOCTTY);
    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    // Configure serial port parameters
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    options.c_cflag |= CS8 | PARENB | PARODD | CSTOPB;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &options);

    // Set delay for sending & reading
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = DELAY_US;

    // Send 0x0f
    char data = 0x0f;
    if (write(fd, &data, 1) != 1)
    {
        perror("write");
        return 1;
    }
    printf("Sent 0x%02x\n", data);

    // Wait for data with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    if (select(fd + 1, &read_fds, NULL, NULL, &tv) == -1)
    {
        perror("select");
        return 1;
    }

    // Check if data received or timeout
    if (FD_ISSET(fd, &read_fds))
    {
        // Read data
        char buffer[BUF_SIZE];
        int bytes_read = read(fd, buffer, BUF_SIZE);
        if (bytes_read > 0)
        {
            printf("Received %d bytes: ", bytes_read);
            for (int i = 0; i < bytes_read; ++i)
            {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
        }
    }
    else
    {
        printf("Timeout occurred after %d us\n", READ_TIMEOUT_US);
    }

    // Close serial port
    close(fd);

    return 0;
}
