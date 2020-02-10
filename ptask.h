//----------------------------------------
//	LIBRERIA PER TASK PERIODICHE
//----------------------------------------
#include <pthread.h>

typedef struct timespec* timePoint;

void time_copy(timePoint, struct timespec);

void time_add_ms(timePoint, int);

void time_add_us(timePoint, int);

int time_cmp(struct timespec, struct timespec);
void set_activation(int);

int get_task_index(void*);

int deadline_miss(int);
void show_dmiss(int);
int get_dmiss(int);

void wait_for_period(int);

int task_create( void* (*)(void *), int, int, int, int);

void ptask_exit(int n);

//Sospende il task PER UN PERIODO finché extCount non supera localCount. 
//Realizza una sorta di Server Polling
int ap_server_wait_counter(int id, unsigned int threshold, unsigned int * globalCount, pthread_mutex_t *mux);

//Incrementa un contatore globale
int ap_server_signal(unsigned int * globalCount, pthread_mutex_t *mux);

void printTime(void);
