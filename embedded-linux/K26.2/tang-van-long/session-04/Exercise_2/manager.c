#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

#define IDLEN 32
#define DATAFILE "students.txt"
#define SEACHER "./searcher"

int main(void){
    char input[IDLEN];
    printf("=============================================\n");
    printf("   STUDENT LOOKUP SYSTEM — MANAGER\n");
    printf("   (fork + execve | file: %s)\n", DATAFILE);
    printf("=============================================\n");
    printf("[MANAGER] PID: %d\n", getpid());
    printf("Enter student ID ('quit' to exit).\n");

    while(1){
        printf("-------------------------------------------\n");
        printf("Student ID: ");
        fflush(stdout);
        if(fgets(input, sizeof(input), stdin) == NULL){
            printf("\n[MANAGER] Exiting. Goodbye!\n");
            break;
        }
        input[strcspn(input, "\r\n")] = '\0';
        if(strcmp(input, "quit") == 0){
            printf("[MANAGER] Exiting. Goodbye!\n");
            break;
        }
        if(strlen(input) == 0){
            continue;
        }
        fflush(stdout);
        pid_t pid = fork();

        if (pid < 0){
            perror("[MANAGER] fork fail");
            continue;
        }
        if(pid == 0){
            char *args[] = {SEACHER, input, DATAFILE, NULL};
            execve(SEACHER, args, environ);
            perror("execve fail");
            exit(2);
        }
        printf("[MANAGER] fork() → child PID: %d\n", pid);
        printf("[MANAGER] Waiting for child (waitpid)...\n");

        int status;
        pid_t ret = waitpid(pid, &status, 0);
        if(ret < 0){
            printf("[MANAGER] waitpid fail");
            continue;
        }else{
            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                const char *result;
    
                switch (code) {
                    case 0:  
                        result = "Found";       
                        break;
                    case 1:  
                        result = "Not found";   
                        break;
                    case 2:  
                        result = "Error";       
                        break;
                    default: 
                        result = "Unknown";     
                        break;
                }
    
                printf("[MANAGER] Child (PID %d) exited. code=%d → %s\n", pid, code, result);
            } else if (WIFSIGNALED(status)) {
                printf("[MANAGER] Child (PID %d) killed by signal %d\n", pid, WTERMSIG(status));
            }
        } 
    }
    return 0; 
}