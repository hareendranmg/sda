#include <time.h>
#include <sys/time.h> // Include this header for gettimeofday function
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int main()
{
    int serial_port = open("/dev/ttyXR0", O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serial_port < 0)
    {
        printf("Error opening serial port\n");
        return -1;
    }

    unsigned char hex_command = 0x4B; // Only the first command
    int response_size = 24;           // Assuming response size for the first command is 24
    unsigned char response[24];
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

    while (1)
    {
        gettimeofday(&tv, NULL); // Get the current time with microsecond precision
        milliseconds = tv.tv_usec / 1000;
        microseconds = tv.tv_usec % 1000;

        fprintf(file, "%03d:%03d, ", milliseconds, microseconds); // Log the formatted timestamp

        fprintf(file, "%02X, ", hex_command);
        write(serial_port, &hex_command, sizeof(hex_command));
        // usleep(100);

        int bytesRead = read(serial_port, response, response_size);

        tcflush(serial_port, TCOFLUSH);
        if (bytesRead > 0)
        {
            for (int j = 0; j < bytesRead; ++j)
            {
                fprintf(file, "%02X", response[j]);
            }
            fprintf(file, ", ");
        }
        else
        {
            printf("No data was read\n");
        }

        usleep(200);
        gettimeofday(&tv, NULL); // Get the current time with microsecond precision
        milliseconds = tv.tv_usec / 1000;
        microseconds = tv.tv_usec % 1000;
        fprintf(file, "%03d:%03d, ", milliseconds, microseconds);

        fprintf(file, "\n");
    }

    fclose(file);
    close(serial_port);

    return 0;
}
