#include <libserialport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    struct sp_port *port;
    sp_get_port_by_name("/dev/ttyXR0", &port);
    sp_open(port, SP_MODE_READ_WRITE);
    sp_set_baudrate(port, 1000000);
    sp_set_bits(port, 8);
    sp_set_parity(port, SP_PARITY_MARK);
    sp_set_stopbits(port, 1);
    sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);

    char commands[][1] = {{0x4B}, {0x41}, {0x42}};
    int response_sizes[] = {24, 24, 20};
    unsigned int timeout = 1;

    FILE *file = fopen("response.csv", "w");
    if (file == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }

    int serial_number = 0;
    while (1)
    {
        fprintf(file, "%d, ", ++serial_number);

        for (int i = 0; i < 3; ++i)
        {
            int result = sp_blocking_write(port, commands[i], sizeof(commands[i]), 1);
            char *buf = malloc(response_sizes[i]);
            int size = response_sizes[i];
            result = sp_blocking_read(port, buf, size, 1);

            fprintf(file, "%02x, ", commands[i][0]);
            for (int j = 0; j < result; ++j)
            {
                fprintf(file, "%02x", buf[j]);
            }
            fprintf(file, ", ");

            free(buf);
        }
        fprintf(file, "\n");

        usleep(10000);
    }

    fclose(file);
    sp_close(port);
    sp_free_port(port);
    return 0;
}
