#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    int agent_id;
    char customer[50];
    int seats_wanted;
} BookingRequest;


/* Shared booking data */
BookingRequest requests[5] = {
    {1, "Nguyen Van An",  2},
    {2, "Tran Thi Bich",   1},
    {3, "Le Van Cuong",    3},
    {4, "Pham Thi Dung",   1},
    {5, "Hoang Van Em",    2}
};

int seats_available = 10;

/* Statistics */
int seats_sold = 0;
int failed_bookings = 0;

/* Mutex protecting shared seat data */
pthread_mutex_t seat_lock;


/*
 * Thread function for processing one booking request.
 */
void *book_ticket(void *arg)
{
    BookingRequest *request = (BookingRequest *)arg;
    sleep(1);
    unsigned long tid = (unsigned long)pthread_self();

    printf("[Agent %d | TID %lu] Booking %d seat%s for %s...\n",
           request->agent_id,
           tid,
           request->seats_wanted,
           request->seats_wanted == 1 ? "" : "s",
           request->customer);
    pthread_mutex_lock(&seat_lock);

    if (seats_available >= request->seats_wanted) {
        seats_available -= request->seats_wanted;
        seats_sold += request->seats_wanted;

        printf("[Agent %d] CONFIRMED: %d seat%s for %s. "
               "Remaining: %d\n",
               request->agent_id,
               request->seats_wanted,
               request->seats_wanted == 1 ? "" : "s",
               request->customer,
               seats_available);

    } else {

        failed_bookings++;

        printf("[Agent %d] SOLD OUT: needs %d seats, "
               "only %d left - booking failed.\n",
               request->agent_id,
               request->seats_wanted,
               seats_available);
    }

    pthread_mutex_unlock(&seat_lock);

    return NULL;
}


int main(void)
{
    pthread_t threads[5];

    printf("==============================================\n");
    printf("   TICKET BOOKING SYSTEM (5 agents, 10 seats)\n");
    printf("==============================================\n");

    /*
     * Initialize the mutex before creating threads.
     */
    if (pthread_mutex_init(&seat_lock, NULL) != 0) {
        perror("pthread_mutex_init");
        return EXIT_FAILURE;
    }

    /*
     * Create 5 booking-agent threads.
     */
    for (int i = 0; i < 5; i++) {
        if (pthread_create(&threads[i],
                           NULL,
                           book_ticket,
                           &requests[i]) != 0) {

            perror("pthread_create");

            /*
             * Destroy mutex before exiting.
             */
            pthread_mutex_destroy(&seat_lock);
            return EXIT_FAILURE;
        }
    }

    /*
     * Wait for all 5 agents to finish.
     */
    for (int i = 0; i < 5; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");

            pthread_mutex_destroy(&seat_lock);
            return EXIT_FAILURE;
        }
    }

    printf("\n");

    /*
     * Print final summary.
     */
    printf("================ SUMMARY ================\n");
    printf("  Total seats     : %d\n", 10);
    printf("  Seats sold      : %d\n", seats_sold);
    printf("  Seats remaining : %d\n", seats_available);
    printf("  Failed bookings : %d\n", failed_bookings);
    printf("==========================================\n");

    /*
     * Destroy the mutex after all threads have finished.
     */
    pthread_mutex_destroy(&seat_lock);

    return EXIT_SUCCESS;
}

