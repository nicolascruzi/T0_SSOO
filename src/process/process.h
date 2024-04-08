#pragma once
#include <time.h>
#include <signal.h>

struct process {
    int id;
    pid_t pid;
    char path[128];
    int status;
    time_t initial_time;
    time_t final_time;
    int args;
    char **argv;
    clock_t start_running;
    clock_t start_waiting;

};
