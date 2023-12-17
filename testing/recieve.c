#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define PORT_NAME "/dev/ttyXR0"
#define BAUD_RATE 115200
// #define BAUD_RATE 2000000

int main() {
    FILE *serial_port = fopen(PORT_NAME, "w+");
    
    if (serial_port == NULL) {
        perror("Error opening serial port");
        return 1;
    }

    char received_data[10];

    while (1) {

        // write data to serial port - data  is "Heelo World\n"
        char data[] = "Hello World\n";
        fwrite(data, sizeof(char), sizeof(data), serial_port);

        usleep(1000000);  // Sleep for 1 second

  
        fread(received_data, sizeof(char), 11, serial_port);

      
        printf("Received: ");
        // for (int i = 0; i < 10; i++) {
            printf("%s ", received_data);
        // }
       
        // usleep(1000000);  // Sleep for 1 second
        usleep(1000);  // Sleep for 1 milli second
        // usleep(1);  // Sleep for 1 micro second
    }

    fclose(serial_port);

    return 0;
}
