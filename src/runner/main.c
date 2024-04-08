#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include "../file_manager/manager.h"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include "../process/process.h"

#define DEFAULT_TIMEOUT 10
#define MAX_PROCESSES 6

volatile sig_atomic_t time_up = 0;
volatile sig_atomic_t child_finished = 0;
volatile sig_atomic_t start_times[MAX_PROCESSES] = {0};
volatile sig_atomic_t execution_times[MAX_PROCESSES] = {0};
int cantidad_procesos;

typedef struct {
    int id;
    pid_t pid;
    char path[128];
    int status;
    time_t initial_time;
    time_t final_time;
    double execution_time;
    int args;
    char **argv;
    clock_t start_running;
    clock_t start_waiting;
} Process;

Process *processes = NULL;


void handle_signal(int signal_number) {
    if (signal_number == SIGALRM) {
        time_up = 1;
    }
    else if (signal_number == SIGTERM) {
        printf("Received SIGTERM. Exiting...\n");
        exit(0);
    }
    else if (signal_number == SIGCHLD){
        int child_status;
        pid_t terminated_pid;
        while ((terminated_pid = waitpid(-1, &child_status, 0)) > 0) {
            printf("Received SIGCHLD. A child process has finished.\n");
            printf("Child process with PID %d has finished.\n", terminated_pid);
            struct timespec end_time;

            clock_gettime(CLOCK_MONOTONIC, &end_time);

            for (int i = 0; i < cantidad_procesos; ++i) {
                if (processes[i].pid == terminated_pid) {
                    processes[i].final_time = end_time.tv_sec;
                    processes[i].execution_time = difftime(processes[i].final_time, processes[i].initial_time);
                    processes[i].status = WEXITSTATUS(child_status);
                    break;
                }
            }
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
        int execution_time_sec = (int)processes[i].execution_time;
        fprintf(output_file, "%s,%d,%d\n", processes[i].path, execution_time_sec, processes[i].status);
    }
    fclose(output_file);
}

int main(int argc, char const *argv[])
{
	///////////////Lectura del input*//////////////////
	char *file_name = (char *)argv[1];
	InputFile *input_file = read_file(file_name);
    cantidad_procesos = input_file->len;
	
	/////////////// Se guarda Amount y Timeout //////////////////
	int amount = atoi(argv[3]);
    int max_timeout = argc == 5 ? atoi(argv[4]) : DEFAULT_TIMEOUT;

    pid_t child_pids[input_file->len];

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
    const char **program_names = (const char **)malloc(input_file->len * sizeof(const char *));
    double *execution_times = (double *)malloc(input_file->len * sizeof(double));
    int *statuses = (int *)malloc(input_file->len * sizeof(int));
    processes = (Process*)malloc(cantidad_procesos * sizeof(Process));
    int programs_executed = 0; 
    int current_processes = 0;
    
    
	//////////////Iteramos sobre el archivo de input////////////////
	for (int i = 0; i < input_file->len; ++i)
	{

        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time); // Iniciar el temporizador

        //////////// Creamos el proceso fork /////////////
        pid_t pid = fork();

        processes[i].id = i;
        processes[i].pid = pid;
        strcpy(processes[i].path, input_file->lines[i][1]);
        processes[i].status = 0;
        processes[i].initial_time = start_time.tv_sec;
        processes[i].final_time = 0;
        
    
        /////////// Si nos devuelve -1,  existe un error////////////////
        if (pid == -1) {
            perror("fork");
            exit(1);
        } 
        
        /////////// Si nos devuelve 0, entonces estamos en el proceso hijo/////////////////
        else if (pid == 0) {
            // printf("Child process %d (PID: %d)\n", i + 1, getpid());
            int num_args = atoi(input_file->lines[i][0]);
            char *args[num_args + 2]; // + 2 para el comando y el terminador NULL

            // args[0] = (char *)input_file->lines[i][1];
            for (int j = 0; j <= num_args+1; ++j) {
                args[j] = (char *)input_file->lines[i][j + 2];
            }
            
            args[num_args + 1] = NULL;
            processes[i].argv = args;
            current_processes++;
            execvp(input_file->lines[i][1], args);
            
            perror("execvp");
            exit(1);
}
        //////////// Si nos devuelve otro numero, entonces estamos en el proceso padre////////////////
        else {    
           
            child_pids[i] = pid;
            current_processes--;
            program_names[programs_executed] = input_file->lines[i][1];
            programs_executed++;
            for (int j = 0; j < input_file->len; ++j) {
                if (child_pids[j] == 0) {
                    child_pids[j] = pid;
                    break;
                }
            }
        }
	}


    for (int i = 0; i < amount; ++i) {
        if (child_pids[i] != 0) {
            int child_status;
            waitpid(child_pids[i], &child_status, 0);
        }
    }
 
    write_csv(argv[2], program_names, execution_times, statuses, programs_executed);
    fclose(output_file);
    input_file_destroy(input_file);
    free(processes);
    free(program_names);
    free(execution_times);
    free(statuses);

    return 0;
}