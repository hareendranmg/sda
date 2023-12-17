#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#define PORT_NAME "/dev/ttyUSB0"
#define BAUD_RATE B2000000

void delay(int milliseconds)
{
    usleep(milliseconds * 1000);
}

int main()
{
    int serial_port = open(PORT_NAME, O_RDWR);

    if (serial_port == -1)
    {
        perror("Error opening serial port");
        return 1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_port, &tty) != 0)
    {
        perror("Error from tcgetattr");
        return 1;
    }

    cfsetospeed(&tty, BAUD_RATE);
    cfsetispeed(&tty, BAUD_RATE);

    // Set the parameters
    tty.c_cflag &= ~PARENB;        // No parity bit
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;         // Clear the size bits
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

    // Make raw
    cfmakeraw(&tty);

    // Flush port, then apply attributes
    tcflush(serial_port, TCIFLUSH);
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0)
    {
        perror("Error from tcsetattr");
        return 1;
    }

    FILE *file = fopen("sample-2mb-text-file.txt", "rb");

    if (file == NULL)
    {
        perror("Error opening input file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long total_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    srand(time(NULL));

    while (total_bytes > 0)
    {
        // Generate a random chunk size between 10 and 20 bytes
        int chunk_size = rand() % 1 + 210;
        // int chunk_size = 20;

        // Read a chunk of data from the file
        char buffer[20]; // Adjust the buffer size as needed
        size_t read_size = fread(buffer, 1, chunk_size, file);

        if (read_size > 0)
        {
            // Send the chunk of data through the serial port
            ssize_t write_size = write(serial_port, buffer, read_size);

            if (write_size < 0)
            {
                perror("Error writing to serial port");
                break;
            }
            else if (write_size != read_size)
            {
                perror("Error writing to serial port: incomplete write");
                break;
            }
            else
            {
                total_bytes -= write_size;
                printf("Wrote %ld bytes, Remaining: %ld bytes\n", write_size, total_bytes);

                if (total_bytes <= 0)
                {
                    printf("File sent successfully. Exiting...\n");
                    break; // Break out of the loop when the file is fully sent
                }
            }
        }
        else
        {
            // If the end of the file is reached, rewind and continue
            fseek(file, 0, SEEK_SET);
        }

        // Add a delay (adjust as needed)
        delay(5);
    }

    fclose(file);
    close(serial_port);

    return 0;
}
