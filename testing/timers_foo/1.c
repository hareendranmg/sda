#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>

int main()
{
    struct timeval tval_before, tval_after, tval_result;
    int counter=0;
    bool stop=false;
   
    gettimeofday(&tval_before, NULL);
    while(stop!=true)
    {
        gettimeofday(&tval_after, NULL);
        timersub(&tval_after, &tval_before, &tval_result);
        double time_elapsed = (double)tval_result.tv_sec + ((double)tval_result.tv_usec/1000000.0f);
        
        while(time_elapsed < 0.001)  //1ms; you can change your desired time interval here
        {
            gettimeofday(&tval_after, NULL);
            timersub(&tval_after, &tval_before, &tval_result);
            time_elapsed = (double)tval_result.tv_sec + ((double)tval_result.tv_usec/1000000.0f);
        }
        gettimeofday(&tval_before, NULL);
        
        if (counter==10000)
        {
            stop=true;
        }
            
        else 
        {
            counter++;
        }
    }
    return 0;
}