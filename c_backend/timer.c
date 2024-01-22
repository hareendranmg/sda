#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

void timer_handler(int signo)
{
    printf("Hello world\n");
    usleep(2000000);
}

int main()
{
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 500000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 500000;

    signal(SIGALRM, timer_handler);
    setitimer(ITIMER_REAL, &timer, NULL);

    while (1)
    {}

    return 0;
}
