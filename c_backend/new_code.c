#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

int main()
{
    const char *port_name = "/dev/ttyXR0";
    char python_command[100];
    snprintf(python_command, sizeof(python_command),
             "python3 /home/root/sda/python_backend/serial_workaround.py %s %d %c %d %d",
             port_name, 1000000, 'M', 8, 1);
    system(python_command);

    int serial_port = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_port < 0)
    {
        perror("Error opening serial port");
        return -1;
    }

    printf("Seriala port %s opened successfully\n", port_name);

    FILE *file = fopen("data2.csv", "w");
    printf("File opened\n");
    if (file == NULL)
    {
        printf("Error opening file\n");
        perror("Error opening file");
        close(serial_port);
        return -1;
    }

    printf("File opened successfully\n");

    unsigned char command1 = 0x4B;
    unsigned char command2 = 0x41;
    unsigned char command3 = 0x42;

    while (1)
    {
        write(serial_port, &command1, 1);
        usleep(100); // 0.0001 seconds
        char received_data[24];
        read(serial_port, received_data, sizeof(received_data));
        fprintf(file, "4B, ");
        for (int i = 0; i < sizeof(received_data); i++)
        {
            fprintf(file, "%02x", received_data[i]);
        }
        fprintf(file, ", ");

        write(serial_port, &command2, 1);
        usleep(100); // 0.0001 seconds
        read(serial_port, received_data, sizeof(received_data));
        fprintf(file, "41, ");
        for (int i = 0; i < sizeof(received_data); i++)
        {
            fprintf(file, "%02x", received_data[i]);
        }
        fprintf(file, ", ");

        write(serial_port, &command3, 1);
        usleep(100); // 0.0001 seconds
        read(serial_port, received_data, 20);
        fprintf(file, "42, ");
        for (int i = 0; i < 20; i++)
        {
            fprintf(file, "%02x", received_data[i]);
        }
        fprintf(file, "\n");

        usleep(1000); // 0.001 seconds
    }

    fclose(file);
    close(serial_port);
    return 0;
}
