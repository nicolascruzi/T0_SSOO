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

#define DEFAULT_TIMEOUT 0


volatile sig_atomic_t time_up = 0;
volatile sig_atomic_t child_finished = 0;
int cantidad_procesos;
int current_processes = 0;
int processes_left = 0;
int amount;
int terminated_children = 0;

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
  
} Process;

Process *processes = NULL;


void handle_signal(int signal_number) {
    if (signal_number == SIGALRM) {
       
        // Enviar SIGINT a todos los procesos hijos
        printf("Frenando procesos con SIGINT...\n");
        for (int i = 0; i < cantidad_procesos; ++i) {
            if (processes[i].pid > 0) { 
                printf("Matando proceso %d\n", processes[i].pid);
               kill(processes[i].pid, SIGINT);
            }
        }
        sleep(10);
        printf("Matando procesos con SIGTERM...\n");
        //enviar in sigterm a todos los procesos
        for (int i = 0; i < cantidad_procesos; ++i) {
            if (processes[i].pid > 0) { // Asegurarse de que el pid es válido    
                kill(processes[i].pid, SIGTERM);
            }
        }}
        else if (signal_number == SIGTSTP) {
            printf("Recibido SIGTSTP. Terminando todos los procesos hijos...\n");

            // Primero, intentamos terminar los procesos de forma amigable con SIGTERM
            for (int i = 0; i < cantidad_procesos; ++i) {
                if (processes[i].pid > 0) {
                    printf("Enviando SIGTERM al proceso %d\n", processes[i].pid);
                    kill(processes[i].pid, SIGTERM);
                }
            }

            // Damos hasta 10 segundos para que los procesos hijos finalicen amigablemente
            sleep(10);

            // Comprobamos si los procesos hijos terminaron después del SIGTERM
            for (int i = 0; i < cantidad_procesos; ++i) {
                if (processes[i].pid > 0) {
                    // Si el proceso hijo todavía está activo, forzamos la terminación con SIGKILL
                    if (kill(processes[i].pid, 0) == 0) { // kill con señal 0 verifica si el proceso está vivo
                        printf("Proceso %d no terminó después de SIGTERM. Enviando SIGKILL.\n", processes[i].pid);
                        kill(processes[i].pid, SIGKILL);
                    }
                }
            }

        // Después de manejar la terminación de todos los procesos hijos, escribe las estadísticas y finaliza el programa
        write_csv("output.csv"); // Asegúrate de que argv[2] sea accesible aquí, o utiliza una variable global para el nombre del archivo
        exit(0);
        }
    else if (signal_number == SIGINT) {
        
        printf("Received SIGINT. Exiting...\n");
        exit(0);
    }
    else if (signal_number == SIGTERM) {
        printf("Received SIGTERM. Exiting...\n");
        exit(0);
    }
    else if (signal_number == SIGKILL) {
        printf("Received SIGKILL. Exiting...\n");
        exit(0);
    }
    else if (signal_number == SIGCHLD){
        
        int child_status;
        pid_t terminated_pid;
        while ((terminated_pid = waitpid(-1, &child_status, 0)) > 0) {
            current_processes--;
            terminated_children++;
            printf("Child process with PID %d has finished.\n", terminated_pid);
            
           
        
            struct timespec end_time;

            clock_gettime(CLOCK_MONOTONIC, &end_time);

            for (int i = 0; i < cantidad_procesos; ++i) {
                if (processes[i].pid == terminated_pid) {
                    processes[i].final_time = end_time.tv_sec;
                    processes[i].execution_time = difftime(processes[i].final_time, processes[i].initial_time);
                    processes[i].status = WIFEXITED(child_status) ? WEXITSTATUS(child_status) : WTERMSIG(child_status);
                    child_finished = 1;
                    break;
                }
            }
        }
    }
}

///////////// Función para escribir los resultados en un archivo CSV /////////////////
void write_csv(const char *output_filename) {
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("Error al abrir el archivo de salida");
        exit(1);
    }
    for (int i = 0; i < cantidad_procesos; ++i) {
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
    signal(SIGINT, handle_signal);
    signal(SIGTSTP, handle_signal);

    ///////////// Establecer alarma para max_timeout si es proporcionado////////////////
    if (max_timeout > 0) {
        printf("Setting alarm for %d seconds...\n", max_timeout);
        alarm(max_timeout);
    }
    
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
    
    
    
	//////////////Iteramos sobre el archivo de input////////////////
	for (int i = 0; i < input_file->len; ++i)
	{
        
        
        while (current_processes >= amount) {
            pause();  
        }
        
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time); // Iniciar el temporizador
        //////////// Creamos el proceso fork /////////////
        pid_t pid = fork();

        current_processes++;
        programs_executed++;
       

        processes[i].id = i;
        strcpy(processes[i].path, input_file->lines[i][1]);
        processes[i].status = 0;
        processes[i].initial_time = start_time.tv_sec;
        processes[i].final_time = 0;
        processes[i].pid = pid;

        
        
    
        /////////// Si nos devuelve -1,  existe un error////////////////
        if (pid == -1) {
            perror("fork");
            exit(1);
        } 
        
        /////////// Si nos devuelve 0, entonces estamos en el proceso hijo/////////////////
        //quiero controlar el flujo, en caso de que amount iguale a la cantidad de procesos activos corriendo en simultaneo, que se detenga y espere a que uno termine para empezar otro

        else if (pid == 0) {
            printf("Comienzo de proceso hijo de (PID: %d)\n", getpid());
            int num_args = atoi(input_file->lines[i][0]);
            char *args[num_args + 2]; // + 2 para el comando y el terminador NULL
            for (int j = 0; j <= num_args+1; ++j) {
                args[j] = (char *)input_file->lines[i][j + 2];
            }
            args[num_args + 1] = NULL;
            processes[i].argv = args;
            
            

            execvp(input_file->lines[i][1], args);
            perror("execvp");
            exit(1);
}
        //////////// Si nos devuelve otro numero, entonces estamos en el proceso padre////////////////
        else {    
            child_pids[i] = pid;
        }}

    if (amount == 1) {
        while (!child_finished) {
            pause();
        }
    } else { 
        for (int i = 0; i < cantidad_procesos; ++i) {
            if (child_pids[i] != 0) {
                int child_status;
                
                waitpid(child_pids[i], &child_status, 0);
            }
        }
    }

    
    
    for (int i = 0; i < cantidad_procesos; ++i) {
        printf("Proceso PID %d\n", processes[i].pid);
        printf("Proceso con tiempo de ejecución: %f\n", processes[i].execution_time);
        printf("Tiemnpo de primer proceso: %ld\n", processes[i].initial_time);
        printf("Tiemnpo de finalización: %ld\n", processes[i].final_time);
    }
 
    write_csv("output.csv");
    fclose(output_file);
    input_file_destroy(input_file);
    free(processes);
    free(program_names);
    free(execution_times);
    free(statuses);
    return 0;
}