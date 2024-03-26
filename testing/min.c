#include <time.h>
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

    unsigned char hex_commands[][1] = {{0x4B}, {0x41}, {0x42}};
    int response_sizes[] = {24, 24, 20};
    unsigned char response[24];
    FILE *file = fopen("response.csv", "w"); // open the file in write mode
    if (file == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }

    tcflush(serial_port, TCIOFLUSH);
    int serial_number = 0;
    while (1)
    {
        fprintf(file, "%d, ", ++serial_number);

        for (int i = 0; i < 3; ++i)
        {
            write(serial_port, hex_commands[i], sizeof(hex_commands[i]));
            usleep(500);

            int bytesRead = read(serial_port, response, response_sizes[i]);
            tcflush(serial_port, TCOFLUSH);
            if (bytesRead > 0)
            {
                fprintf(file, "%02X, ", hex_commands[i][0]);

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
        }
        fprintf(file, "\n");

        usleep(10000);
    }

    fclose(file);
    close(serial_port);

    return 0;
}
