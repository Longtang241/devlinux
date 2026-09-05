#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t running = 1;

void handle_sigterm(int sig)
{
    (void)sig;

    printf("Service shutting down...\n");

    running = 0;
}


int main(void)
{
    setbuf(stdout, NULL);
    struct sigaction sa;

    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }


    printf("Monitor service started. PID: %d\n", getpid());

    int counter = 0;

    while (running) {

        printf("Monitor service running... cycle %d\n", counter++);

        sleep(1);
    }


    printf("Monitor service exited cleanly.\n");

    return EXIT_SUCCESS;
}

