#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include "../file_manager/manager.h"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <wait.h>

#define DEFAULT_TIMEOUT 10

volatile sig_atomic_t time_up = 0;

void handle_signal(int signal_number) {
    if (signal_number == SIGALRM) {
        time_up = 1;
    }
    else if (signal_number == SIGTERM) {
        printf("Received SIGTERM. Exiting...\n");
        exit(0);
    }

}

///////////// Función para escribir los resultados en un archivo CSV /////////////////
void write_csv(const char *output_filename, const char **program_names, double *execution_times, int *statuses, int num_programs) {
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("Error al abrir el archivo de salida");
        exit(1);
    }
    
    for (int i = 0; i < num_programs; ++i) {

        int execution_time_int = (int)execution_times[i]; // Truncar al entero más cercano

        fprintf(output_file, "%s,%d,%d\n", program_names[i], execution_time_int, statuses[i]);

    }
    fclose(output_file);
}


void terminate_children() {
    printf("Sending SIGTERM to all children...\n");
    kill(0, SIGTERM); // Send SIGTERM to all child processes in the process group
    sleep(10); // Wait 10 seconds for children to terminate gracefully
    printf("Sending SIGKILL to all children...\n");
    kill(0, SIGKILL); // Send SIGKILL if any child process still exists after waiting
}


int main(int argc, char const *argv[])
{
	printf("Runner\n");

	///////////////Checkear cantidad de argumentos esta bien//////////////////
	if (argc < 4 || argc > 5) {
        printf("Uso: %s <input> <output> <amount> [<max>]\n", argv[0]);
        return 1;
    }
	
	///////////////Lectura del input*//////////////////
	char *file_name = (char *)argv[1];
	InputFile *input_file = read_file(file_name);
	
	/////////////// Se guarda Amount y Timeout //////////////////
	int amount = atoi(argv[3]);
    int max_timeout = argc == 5 ? atoi(argv[4]) : DEFAULT_TIMEOUT;

	 //////////////Set up signal handler for timeout///////////////
	// signal(SIGALRM, handle_signal);
	// alarm(max_timeout);

	
	/////////////// Escribir sobre ouput el error ///////////////7
    FILE *output_file = fopen(argv[2], "w");
    if (output_file == NULL) {
        perror("fopen");
        exit(1);
    }

    ////////// Arreglos para almacenar los resultados ///////////////   
    const char **program_names = (const char **)malloc(amount * sizeof(const char *));
    double *execution_times = (double *)malloc(amount * sizeof(double));
    int *statuses = (int *)malloc(amount * sizeof(int));
    int programs_executed = 0; 

    

	//////////////Iteramos sobre el archivo de input////////////////
	for (int i = 0; i < input_file->len; ++i)
	{
		if (time_up) {
            printf("Timeout reached. Exiting...\n");
            break;
        }

        // Set alarm for maximum timeout
        // alarm(max_timeout);

        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time); // Iniciar el temporizador

        //////////// Creamos el proceso fork /////////////
        pid_t pid = fork();

        /////////// Si nos devuelve -1,  existe un error////////////////
        if (pid == -1) {
            perror("fork");
            exit(1);
        } 
        
        /////////// Si nos devuelve 0, entonces estamos en el proceso hijo/////////////////
        else if (pid == 0) {
            int num_args = atoi(input_file->lines[i][0]);
            char *args[num_args + 2]; // + 2 para el comando y el terminador NULL

            // args[0] = (char *)input_file->lines[i][1];
            for (int j = 0; j <= num_args+1; ++j) {
                args[j] = (char *)input_file->lines[i][j + 2];
            }
            args[num_args + 1] = NULL;

            execv(input_file->lines[i][1], args);
            perror("execvp");
            exit(1);
}
        
        //////////// Si nos devuelve otro numero, entonces estamos en el proceso padre////////////////
        else {  
            int child_status;
            waitpid(pid, &child_status, 0); // Esperar a que el proceso hijo termine
            clock_gettime(CLOCK_MONOTONIC, &end_time); // Finalizar el temporizador

            // Calcular el tiempo de ejecución en nanosegundos
            long start_ns = start_time.tv_sec * 1000000000 + start_time.tv_nsec;
            long end_ns = end_time.tv_sec * 1000000000 + end_time.tv_nsec;
            long execution_time = end_ns - start_ns;
            double execution_time_sec = (double)execution_time / 1000000000.0; // Convertir a segundos

           
            
            // Obtener el estado de salida del proceso hijo
            int child_exit_status = WEXITSTATUS(child_status);
            statuses[programs_executed] = child_exit_status < 0 ? 0 : child_exit_status;

            
            program_names[programs_executed] = input_file->lines[i][1];
            execution_times[programs_executed] = execution_time_sec;
            statuses[programs_executed] = child_exit_status;
            programs_executed++;

            printf("Program: %s\n", program_names[programs_executed - 1]);
            printf("Tiempo Ejecución: %lf\n", execution_times[programs_executed - 1]);
            printf("Status: %d\n", statuses[programs_executed - 1]);
        }



        
	}

	// Wait for all remaining child processes to finish
    while (wait(NULL) > 0);

    write_csv(argv[2], program_names, execution_times, statuses, programs_executed);

    // Close output file
    fclose(output_file);

    // Clean up
    input_file_destroy(input_file);

    // Liberar memoria utilizada
    free(program_names);
    free(execution_times);
    free(statuses);

    return 0;
}