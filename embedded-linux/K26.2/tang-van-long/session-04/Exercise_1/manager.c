#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NAMELEN 50
typedef struct {
    int   id;
    char  name[NAMELEN];
    int   quantity;
    float unit_price;
} Order;

void process_order(Order o)
{
    float total = o.quantity * o.unit_price;
    printf("[CHILD-%d] PID: %d | PPID: %d\n", o.id, getpid(), getppid());
    printf("[CHILD-%d] %s x%d — Total: %.0f VND\n", o.id, o.name, o.quantity, total);
    printf("[CHILD-%d] Processing... (sleep 2s)\n\n", o.id);
    sleep(2);

    if (o.quantity <= 0 || o.unit_price <= 0) {
        printf("[CHILD-%d] ERROR: invalid order data (quantity=%d, unit_price=%.0f) -> exit(1)\n",
               o.id, o.quantity, o.unit_price);
        exit(1);
    }
}

int main(void)
{
    Order orders[3] = {
        {1, "Backpack", 2, 350000},
        {2, "Shoes",    1, 500000},   
        {3, "Hat",      3, 120000}
    };

    pid_t pids[3];

    printf("===================================================\n");
    printf("   ORDER PROCESSING SYSTEM — MANAGER (fork+wait)\n");
    printf("===================================================\n");

    printf("[MANAGER] PID: %d — spawning 3 child processes...\n\n", getpid());

    for (int i = 0; i < 3; i++) {
        fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork fail");
            exit(1);
        }
        if (pid == 0) {
            process_order(orders[i]);
            exit(0);
        }
        pids[i] = pid;
        printf("[MANAGER] fork() order #%d → child PID: %d\n", orders[i].id, pid);
    }
    printf("[MANAGER] All 3 children spawned. Starting waitpid()...\n\n");

    int successful = 0;
    int failed = 0;
    int status;

    for (int i = 0; i < 3; i++) {
        pid_t ret = waitpid(pids[i], &status, 0);
        if (ret > 0) {
            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);

                if (code == 0) {
                    printf("[MANAGER] waitpid(%d) — order #%d: exit code=%d → SUCCESS\n",pids[i], orders[i].id, code);
                    successful++;
                } else {
                    printf("[MANAGER] waitpid(%d) — order #%d: exit code=%d → FAILED\n",pids[i], orders[i].id, code);
                    failed++;
                }
            } else if (WIFSIGNALED(status)) {
                printf("[MANAGER] waitpid(%d) — order #%d: killed by signal %d → FAILED\n",pids[i], orders[i].id, WTERMSIG(status));
                failed++;
            }
        }
    }

    float total_revenue = 0;
    for (int i = 0; i < 3; i++) {
        if (orders[i].quantity > 0 && orders[i].unit_price > 0)
            total_revenue += orders[i].quantity * orders[i].unit_price;
    }

    printf("\n================= SUMMARY =================\n");
    printf("  Total orders    : 3\n");
    printf("  Successful      : %d\n", successful);
    printf("  Failed          : %d\n", failed);
    printf("  Total revenue   : %.0f VND\n", total_revenue);
    printf("===========================================\n");

    return 0;
}