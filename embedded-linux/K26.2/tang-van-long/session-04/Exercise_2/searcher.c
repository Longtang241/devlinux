#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINELEN 256

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "[SEARCHER] Usage: %s <student_id> <data_file>\n", argv[0]);
        exit(2);
    }

    const char *target_id = argv[1];
    const char *filepath  = argv[2];

    printf("[SEARCHER] PID: %d | PPID: %d\n", getpid(), getppid());
    printf("[SEARCHER] Searching for \"%s\" in %s...\n", target_id, filepath);

    FILE *fp = fopen(filepath, "r");
    if (fp == NULL) {
        perror("[SEARCHER] fopen failed");
        exit(2);
    }

    char line[LINELEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
   
        line[strcspn(line, "\r\n")] = '\0';

        char buf[LINELEN];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *id    = strtok(buf, "|");
        char *name  = strtok(NULL, "|");
        char *cls   = strtok(NULL, "|");
        char *gpaStr = strtok(NULL, "|");

        if (id == NULL || name == NULL || cls == NULL || gpaStr == NULL)
            continue; 

        if (strcmp(id, target_id) == 0) {
            float gpa = atof(gpaStr);
            const char *grade;

            if (gpa >= 8.5)
                grade = "Excellent";
            else if (gpa >= 7.0)
                grade = "Good";
            else if (gpa >= 5.0)
                grade = "Average";
            else
                grade = "Poor";

            printf("========== SEARCH RESULT ==========\n");
            printf("  ID      : %s\n", id);
            printf("  Name    : %s\n", name);
            printf("  Class   : %s\n", cls);
            printf("  GPA     : %.1f\n", gpa);
            printf("  Grade   : %s\n", grade);
            printf("====================================\n");

            fclose(fp);
            exit(0); 
        }
    }


    printf("[SEARCHER] No student found with ID: %s\n", target_id);
    fclose(fp);
    exit(1); 
}