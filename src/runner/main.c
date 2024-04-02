#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include "../file_manager/manager.h"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <wait.h>

#define MAX_PROCESSES 10
#define DEFAULT_TIMEOUT 10

volatile sig_atomic_t time_up = 0;

void handle_signal(int signal_number) {
    if (signal_number == SIGALRM) {
        time_up = 1;
    }
}


int main(int argc, char const *argv[])
{
	printf("Runner\n");
	printf("Holaa\n");
	// Checkear cantidad de argumentos esta bien
	if (argc < 4 || argc > 5) {
        printf("Uso: %s <input> <output> <amount> [<max>]\n", argv[0]);
        return 1;
    }
	
	/*Lectura del input*/
	
	char *file_name = (char *)argv[1];
	InputFile *input_file = read_file(file_name);
	
	
	int amount = atoi(argv[3]);
    int max_timeout = argc == 5 ? atoi(argv[4]) : DEFAULT_TIMEOUT;

	printf("Amount: %d\n", amount);
	printf("Max timeout: %d\n", max_timeout);

	 // Set up signal handler for timeout
	signal(SIGALRM, handle_signal);
	alarm(max_timeout);

	

	// Open output file for writing statistics
    FILE *output_file = fopen(argv[2], "w");
    if (output_file == NULL) {
        perror("fopen");
        exit(1);
    }

	/*Mostramos el archivo de input en consola*/
	printf("Cantidad de lineas: %d\n", input_file->len);
	
	for (int i = 0; i < input_file->len; ++i)
	{
		if (time_up) {
            printf("Timeout reached. Exiting...\n");
            break;
        }

        // Fork process
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {  // Child process
            // Execute program using execvp
            execvp(input_file->lines[i][1], &input_file->lines[i][1]);
            // If execvp fails, print error and exit child process
            perror("execvp");
            exit(1);
        } else {  // Parent process
            // Check if maximum number of processes has been reached
            if (i >= amount) {
                wait(NULL); // Wait for any child process to finish
            }
        }
		
		
		//int argc = atoi(input_file->lines[i][0]);
		//printf("%d ", atoi(input_file->lines[i][0]));
		//printf("%s ", input_file->lines[i][1]);
		//for (int j = 2; j < argc + 2; ++j)
		//{
		//	printf("%s ", input_file->lines[i][j]);
		//}
		
		//printf("\n");
	}

	// Wait for all remaining child processes to finish
    while (wait(NULL) > 0);

    // Close output file
    fclose(output_file);

    // Clean up
    input_file_destroy(input_file);

    return 0;
}