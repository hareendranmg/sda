#include <stdio.h>
#include <unistd.h>
#include <gpiod.h>

int main(int argc, char *argv[])
{
    char chip[32];
    unsigned int offset;
    int line_value = 0;
    struct gpiod_line *line = NULL;

    if (argc == 3)
    {
        snprintf(chip, sizeof(chip), "gpiochip%s", argv[1]);
        offset = atoi(argv[2]);
    }
    else
    {
        printf("Usage by bank/pin number:\n"
               "\tgpio-toggle OUTPUT-BANK-NUMBER OUTPUT-GPIO-NUMBER\n"
              );
        return EXIT_FAILURE;
    }

    // initialize the gpiod_line
    line = gpiod_line_get(chip, offset);

    // Get the current direction
    // int direction = gpiod_line_direction(line);
    // if (direction < 0)
    // {
    //     printf("Error getting GPIO direction\n");
    //     return 1; // Return an error code
    // }

    // if (direction == GPIOD_LINE_DIRECTION_INPUT)
    // {
    //     // If the GPIO is configured as an input, change it to OUTPUT
    //     if (gpiod_line_direction(line) < 0)
    //     {
    //         printf("Error setting GPIO direction to OUTPUT\n");
    //         return 1; // Return an error code
    //     }
    //     printf("Changed GPIO direction to OUTPUT\n");
    // }
    // else
    // {
    //     printf("GPIO is already configured as an OUTPUT\n");
    // }

    // Main loop
    while (1)
    {
        line_value = !line_value;
        gpiod_ctxless_set_value(chip, offset, line_value, false, "gpio-toggle", NULL, NULL);
        usleep(1000);
        // printf("Setting pin to %d\n", line_value);
        // line_value = !line_value; // Toggle between 0 and 1
        // gpiod_line_set_value(line, line_value);
        // printf("GPIO value set to %d\n", line_value);
        // sleep(1);

    }

    return 0;
}