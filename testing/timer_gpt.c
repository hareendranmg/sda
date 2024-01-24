#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

// Global file pointer
FILE *filePtr;

// Function to be called at regular intervals
void writeCurrentTime() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    struct tm *localTime = localtime(&currentTime.tv_sec);
    
    fprintf(filePtr, "%02d, %02d, %02d, %03ld, %03ld\n", 
            localTime->tm_hour, localTime->tm_min, localTime->tm_sec,
            currentTime.tv_usec / 1000, currentTime.tv_usec % 1000);
    fflush(filePtr);
}

// Signal handler function
void timerHandler(int signum) {
    // Call the function at every 100 milliseconds
    writeCurrentTime();
}

int main() {
    // Open the file in the main function
    filePtr = fopen("output.txt", "a");
    if (filePtr == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Set up the timer
    struct sigaction sa;
    sa.sa_handler = timerHandler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    // Set the timer to trigger every 100 milliseconds
    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;
    timer.it_value = timer.it_interval;

    setitimer(ITIMER_REAL, &timer, NULL);

    // Keep the program running
    while (1) {
        sleep(1);
    }

    // Close the file before exiting
    fclose(filePtr);

    return 0;
}
