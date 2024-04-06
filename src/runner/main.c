#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include "../file_manager/manager.h"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <wait.h>
#include <errno.h>


#define DEFAULT_TIMEOUT 10

volatile sig_atomic_t time_up = 0;
volatile sig_atomic_t child_finished = 0;
pid_t pid;

void handle_signal(int signal_number) {
    if (signal_number == SIGALRM) {
        time_up = 1;
    }
    else if (signal_number == SIGTERM) {
        printf("Received SIGTERM. Exiting...\n");
        exit(0);
    }
    else if (signal_number == SIGCHLD){
        printf("Received SIGCHLD. A child process has finished.\n");
        int child_status;
        pid_t terminated_pid = waitpid(pid, &child_status, WNOHANG);
        printf("waitpid returned: %d, errno: %d\n", terminated_pid, errno);
        printf("Acaa\n");
        if (terminated_pid > 0) {
            printf("Child process with PID %d has finished.\n", terminated_pid);
            child_finished = 1;
        }
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

int main(int argc, char const *argv[])
{
	///////////////Lectura del input*//////////////////
	char *file_name = (char *)argv[1];
	InputFile *input_file = read_file(file_name);
	
	/////////////// Se guarda Amount y Timeout //////////////////
	int amount = atoi(argv[3]);
    int max_timeout = argc == 5 ? atoi(argv[4]) : DEFAULT_TIMEOUT;

    pid_t child_pids[amount];

	 //////////////Set up signal handler for timeout///////////////
	signal(SIGALRM, handle_signal);
    signal(SIGCHLD, handle_signal);
    signal(SIGTERM, handle_signal);

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
    int current_processes = 0;
    
	//////////////Iteramos sobre el archivo de input////////////////
	for (int i = 0; i < input_file->len; ++i)
	{

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
            printf("PROCESO HIJO: %d\n", current_processes);
            printf("Soy el proceso hijo (PID: %d)\n", getpid());
            int num_args = atoi(input_file->lines[i][0]);
            char *args[num_args + 2]; // + 2 para el comando y el terminador NULL

            // args[0] = (char *)input_file->lines[i][1];
            for (int j = 0; j <= num_args+1; ++j) {
                args[j] = (char *)input_file->lines[i][j + 2];
            }
            
            args[num_args + 1] = NULL;
            current_processes++;
            execvp(input_file->lines[i][1], args);
            
            perror("execvp");
            exit(1);
}
        //////////// Si nos devuelve otro numero, entonces estamos en el proceso padre////////////////
        else {    
            printf("PROCESO PADRE: %d\n", current_processes);
            printf("Soy el proceso padre (PID: %d)\n", getpid());
            printf("Esperando a que el proceso hijo termine...\n");
            // while (current_processes > amount) {
            //         printf("Waiting for a process slot to become available...\n");
            //         int child_status;
            //         pid_t finished_pid = waitpid(pid, &child_status, 0);

            //         // Verificar si wait() devolvió un error
            //         if (finished_pid == -1) {
            //             if (errno == ECHILD) {
            //                 printf("No child processes to wait for.\n");
            //                 break; // Salir del bucle si no hay procesos hijos activos
            //             } else {
            //                 perror("wait");
            //                 // Manejar el error según sea necesario
            //                 break; // Salir del bucle en caso de error
            //             }
            //         }

            //         // Eliminar el PID del proceso hijo terminado de la lista
            //         for (int j = 0; j < amount; ++j) {
            //             if (child_pids[j] == finished_pid) {
            //                 child_pids[j] = 0; // Marcar como proceso terminado
            //                 printf("Process %d finished\n", finished_pid);
            //                 break;
            //             }
            //         }
            //         current_processes--;
            // }
                    
            ///while (!child_finished) {
            // Esperar por la señal SIGCHLD
            //printf("Waiting for SIGCHLD...\n");
            //sleep(3);
            //}
            int child_status;
            waitpid(pid, &child_status, 0);  //Esperar a que el proceso hijo termine
            
            printf("Proceso hijo terminado");
            current_processes--;
            
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
        
            programs_executed++;

            for (int j = 0; j < amount; ++j) {
                if (child_pids[j] == 0) {
                    child_pids[j] = pid;
                    break;
                }
            }
            printf("\n----Program: %s -----Tiempo: %lf --------Status: %d -----------\n\n", program_names[programs_executed - 1], execution_times[programs_executed - 1], statuses[programs_executed - 1]);      
        }
	}

	// Wait for all remaining child processes to finish
    //while (wait(NULL) > 0);

    for (int i = 0; i < amount; ++i) {
        if (child_pids[i] != 0) {
            int child_status;
            waitpid(child_pids[i], &child_status, 0);
        }
    }
    write_csv(argv[2], program_names, execution_times, statuses, programs_executed);
    fclose(output_file);
    input_file_destroy(input_file);
    free(program_names);
    free(execution_times);
    free(statuses);

    return 0;
}