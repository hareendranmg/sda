#include <stdio.h>
#include <stddef.h>
#include <stdint.h> // Include <stdint.h> for SIZE_MAX

int main()
{
    printf("Size of size_t: %zu bytes\n", sizeof(size_t));
    printf("Maximum value of size_t: %llu\n", (unsigned long long)SIZE_MAX);

    return 0;
}
