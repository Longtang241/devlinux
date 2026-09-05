#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define QUEUE_SIZE 5
#define NUM_PRODUCERS 3
#define DOCS_PER_PRODUCER 3
#define TOTAL_DOCUMENTS 9


typedef struct {
    int doc_id;
    char filename[60];
    int pages;
} Document;


/*
 * Shared circular queue.
 */
Document queue[QUEUE_SIZE];

int head = 0;
int tail = 0;
int count = 0;
int all_sent = 0;

/*
 * Statistics.
 */
int documents_submitted = 0;
int documents_printed = 0;
int total_pages_printed = 0;


/*
 * Synchronization objects.
 */
pthread_mutex_t q_lock;

pthread_cond_t not_full;
pthread_cond_t not_empty;

Document producer_documents[NUM_PRODUCERS][DOCS_PER_PRODUCER] = {

    {
        {1, "report_Q1.pdf", 12},
        {2, "slides.pdf",    20},
        {3, "summary.pdf",    4}
    },

    {
        {4, "contract.pdf",  5},
        {5, "memo.pdf",      2},
        {6, "budget.pdf",    7}
    },

    {
        {7, "invoice.pdf",   3},
        {8, "proposal.pdf",  8},
        {9, "presentation.pdf", 5}
    }
};


/*
 * Producer thread.
 */
void *producer(void *arg)
{
    int producer_id = *(int *)arg;

    for (int i = 0; i < DOCS_PER_PRODUCER; i++) {

        Document doc = producer_documents[producer_id - 1][i];

        pthread_mutex_lock(&q_lock);
        while (count == QUEUE_SIZE) {

            printf("[Producer %d] Queue full - waiting...\n",
                   producer_id);

            pthread_cond_wait(&not_full, &q_lock);
        }
        queue[tail] = doc;

        tail = (tail + 1) % QUEUE_SIZE;

        count++;
        documents_submitted++;

        printf("[Producer %d] Submitting: %-20s (%d pages) "
               "- queue: %d/%d\n",
               producer_id,
               doc.filename,
               doc.pages,
               count,
               QUEUE_SIZE);

        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&q_lock);
        usleep(100000);
    }

    return NULL;
}

void *printer(void *arg)
{
    (void)arg;

    while (1) {

        pthread_mutex_lock(&q_lock);
        while (count == 0 && !all_sent) {
            pthread_cond_wait(&not_empty, &q_lock);
        }
        if (count == 0 && all_sent) {

            pthread_mutex_unlock(&q_lock);
            break;
        }
        Document doc = queue[head];

        head = (head + 1) % QUEUE_SIZE;

        count--;

        documents_printed++;
        total_pages_printed += doc.pages;


        printf("[Printer]    Printing: %-20s (%d pages) "
               "- queue: %d/%d\n",doc.filename, doc.pages, count, QUEUE_SIZE);
        pthread_cond_signal(&not_full);

        pthread_mutex_unlock(&q_lock);
        sleep(1);
    }
    printf("[Printer]    All documents printed. Exiting.\n");

    return NULL;
}


int main(void)
{
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t printer_thread;

    int producer_ids[NUM_PRODUCERS] = {
        1, 2, 3
    };


    printf("==============================================\n");
    printf("   OFFICE PRINT QUEUE (3 producers, 1 printer)\n");
    printf("   Queue capacity: 5 documents\n");
    printf("==============================================\n\n");


    /*
     * Initialize mutex.
     */
    if (pthread_mutex_init(&q_lock, NULL) != 0) {
        perror("pthread_mutex_init");
        return EXIT_FAILURE;
    }


    /*
     * Initialize condition variables.
     */
    if (pthread_cond_init(&not_full, NULL) != 0) {
        perror("pthread_cond_init");

        pthread_mutex_destroy(&q_lock);
        return EXIT_FAILURE;
    }


    if (pthread_cond_init(&not_empty, NULL) != 0) {
        perror("pthread_cond_init");

        pthread_cond_destroy(&not_full);
        pthread_mutex_destroy(&q_lock);

        return EXIT_FAILURE;
    }


    /*
     * Start printer thread.
     */
    if (pthread_create(&printer_thread,
                       NULL,
                       printer,
                       NULL) != 0) {

        perror("pthread_create");

        pthread_cond_destroy(&not_empty);
        pthread_cond_destroy(&not_full);
        pthread_mutex_destroy(&q_lock);

        return EXIT_FAILURE;
    }


    /*
     * Start 3 producer threads.
     */
    for (int i = 0; i < NUM_PRODUCERS; i++) {

        if (pthread_create(&producer_threads[i],
                           NULL,
                           producer,
                           &producer_ids[i]) != 0) {

            perror("pthread_create");

            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producer_threads[i], NULL);
    }

    pthread_mutex_lock(&q_lock);

    all_sent = 1;

    pthread_cond_broadcast(&not_empty);

    pthread_mutex_unlock(&q_lock);

    pthread_join(printer_thread, NULL);


    printf("\n");
    printf("================ SUMMARY ================\n");
    printf("  Documents submitted : %d\n", documents_submitted);
    printf("  Documents printed   : %d\n", documents_printed);
    printf("  Total pages printed : %d\n", total_pages_printed);
    printf("==========================================\n");

    pthread_cond_destroy(&not_empty);
    pthread_cond_destroy(&not_full);
    pthread_mutex_destroy(&q_lock);


    return EXIT_SUCCESS;
}

