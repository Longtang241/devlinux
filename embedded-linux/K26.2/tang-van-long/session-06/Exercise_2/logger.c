#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LOG_ERR     "<3>"
#define LOG_WARNING "<4>"
#define LOG_INFO    "<6>"


int main(void)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    srand((unsigned int)time(NULL));


    fprintf(stderr,
            LOG_INFO "Logger service started. PID: %d\n",
            getpid());


    int cycle = 1;
    for (int elapsed = 0; elapsed < 30; elapsed += 2) {

        fprintf(stderr,
                LOG_INFO
                "Service running normally, cycle %d\n",
                cycle);

        fprintf(stderr,
                LOG_WARNING
                "Memory usage high: %d%%\n",
                80 + rand() % 15);

        fprintf(stderr,
                LOG_ERR
                "Failed to connect to database, retry %d\n",
                cycle);

        cycle++;

        sleep(2);
    }
    fprintf(stderr,
            LOG_ERR
            "Fatal error: service is crashing now\n");


    abort();
    return EXIT_FAILURE;
}

