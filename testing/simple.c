#include <time.h>
#include <sys/time.h> // Include this header for gettimeofday function
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>

int serial_read(int fd, void *buffer, unsigned int count)
{
    // Initialize the file descriptor set.
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    // Initialize the timeout.
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;

    int nbytes = 0;
    while (nbytes < count)
    {
        // See if there is data available.
        int rc = select(fd + 1, &fds, NULL, NULL, &tv);
        // TODO: Recalculate timeout here!
        if (rc < 0)
        {
            return -1; // Error during select call.
        }
        else if (rc == 0)
            break; // Timeout.

        // Read the available data.
        int n = read(fd, buffer + nbytes, count - nbytes);
        if (n < 0)
        {
            return -1; // Error during read call.
        }
        else if (n == 0)
            break; // EOF reached.

        // Increase the number of bytes read.
        nbytes += n;
    }

    // Return the number of bytes read.
    return nbytes;
}

int main()
{

    if (setpriority(PRIO_PROCESS, 0, -99) == -1)
    {
        perror("setpriority");
        return 1;
    }

    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_RR);

    // Attempt to set real-time scheduling policy
    if (sched_setscheduler(0, SCHED_RR, &param) == -1)
    {
        perror("sched_setscheduler");
    }

    int serial_port = open("/dev/ttyXR0", O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serial_port < 0)
    {
        printf("Error opening serial port\n");
        return -1;
    }

    unsigned char hex_command = 0x4B; // Only the first command
    int response_size = 24;            // Assuming response size for the first command is 24
    unsigned char response[24];

    unsigned char hex_command2 = 0x41; // Only the first command
    int response_size2 = 24;            // Assuming response size for the first command is 24
    unsigned char response2[24];

    unsigned char hex_command3 = 0x42; // Only the first command
    int response_size3 = 20;            // Assuming response size for the first command is 24
    unsigned char response3[20];

    FILE *file = fopen("response.csv", "w"); // open the file in write mode
    if (file == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }

    tcflush(serial_port, TCIOFLUSH);
    int serial_number = 0;
    struct timeval tv;
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[20];
    int milliseconds;
    int microseconds;
    char timebuffer[80];

    while (1)
    {
        gettimeofday(&tv, NULL); // Get the current time with microsecond precision
        strftime(timebuffer, sizeof(timebuffer), "%M:%S", localtime(&tv.tv_sec));
        milliseconds = tv.tv_usec / 1000;
        microseconds = tv.tv_usec % 1000;

        fprintf(file, "%s %03d:%03d, ", timebuffer, milliseconds, microseconds); // Log the formatted timestamp

        fprintf(file, "%02X, ", hex_command);
        write(serial_port, &hex_command, sizeof(hex_command));
        usleep(300);
        int bytesRead = read(serial_port, response, response_size);
        tcflush(serial_port, TCOFLUSH);
        if (bytesRead > 0)
        {
            for (int j = 0; j < bytesRead; ++j)
            {
                fprintf(file, "%02X", response[j]);
            }
            fprintf(file, ", ");
        } else {
            fprintf(file, "timeout, ");
        }

        fprintf(file, "%02X, ", hex_command2);
        write(serial_port, &hex_command2, sizeof(hex_command2));
        usleep(300);

        bytesRead = read(serial_port, response2, response_size2);
        tcflush(serial_port, TCOFLUSH);
        if (bytesRead > 0)
        {
            for (int j = 0; j < bytesRead; ++j)
            {
                fprintf(file, "%02X", response2[j]);
            }
            fprintf(file, ", ");
        } else {
            fprintf(file, "timeout, ");
        }

        fprintf(file, "%02X, ", hex_command3);
        write(serial_port, &hex_command3, sizeof(hex_command3));
        usleep(300);

        bytesRead = read(serial_port, response3, response_size3);
        tcflush(serial_port, TCOFLUSH);
        if (bytesRead > 0)
        {
            for (int j = 0; j < bytesRead; ++j)
            {
                fprintf(file, "%02X", response3[j]);
            }
            fprintf(file, "");
        } else {
            fprintf(file, "timeout");
        }

        gettimeofday(&tv, NULL); // Get the current time with microsecond precision
        milliseconds = tv.tv_usec / 1000;
        microseconds = tv.tv_usec % 1000;
        strftime(timebuffer, sizeof(timebuffer), "%M:%S", localtime(&tv.tv_sec));
        fprintf(file, "%s %03d:%03d, ", timebuffer, milliseconds, microseconds); // Log the formatted timestamp

        fprintf(file, "\n");
        usleep(5000);
        // nanosleep((const struct timespec[]){{0, 5000000L}}, NULL);
    }

    fclose(file);
    close(serial_port);

    return 0;
}
