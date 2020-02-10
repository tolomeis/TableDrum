//----------------------------------------
//	LIBRERIA PER TASK PERIODICHE
//----------------------------------------
#include "ptask.h"


#include <stdio.h>
//#include <stdlib.h>
//#include <math.h>
#include <pthread.h>
#include <time.h>


#define STK_SIZE 40960*10 //4MB

#define NT 100

pthread_t		tid[NT];

struct task_par {	// parametri real-time
			// questa struttura va inizializzata prima di chiamare pthread_create() e va passata come argomento del thread
	int argument;		// argomento del task (codice identificativo)
	long wcet;		// (us)
	int period;		// (us)
	int deadline;		// relativa (us)
	int priority;		// in [0,99]
	int dmiss;		// numero di deadline miss
	struct timespec at;	// prossimo activation time
	struct timespec dl;	// deadline assoluta
};

struct task_par		tp[NT];

// copia ts in td
void time_copy(struct timespec *td, struct timespec ts) {	
	td->tv_sec = ts.tv_sec;
	td->tv_nsec = ts.tv_nsec;
}

void time_add_ms(struct timespec *t, int ms) {	
	
	t->tv_sec += ms/1000;
	t->tv_nsec += (ms%1000)*1000000;//1 milione di nsec = 1 ms
	
	if (t->tv_nsec > 1000000000) {//1 miliardo di nsec = 1 sec
		t->tv_nsec -= 1000000000;//1 miliardo
		t->tv_sec += 1;
	}
}

void time_add_us(struct timespec *t, int us) {	
	
	t->tv_sec += us/1000000;
	t->tv_nsec += (us%1000000)*1000;//1 milione di nsec = 1 ms
	
	if (t->tv_nsec > 1000000000) {//1 miliardo di nsec = 1 sec
		t->tv_nsec -= 1000000000;//1 miliardo
		t->tv_sec += 1;
	}
}

int time_cmp(struct timespec t1, struct timespec t2) {	// ritorna 0 se le due variabili temporali sono uguali, 1 se la prima
							// e' maggiore, -1 se la seconda e' maggiore
	if (t1.tv_sec > t2.tv_sec) return 1;
	if (t1.tv_sec < t2.tv_sec) return -1;
	if (t1.tv_nsec > t2.tv_nsec) return 1;
	if (t1.tv_nsec < t2.tv_nsec) return -1;
	return 0;
}

void set_activation(int id) {	// legge il tempo corrente e computa il prossimo activation time e la deadline assoluta del task id
				// NOTA: il timer non e' settato su interrupt
	struct timespec now;
	
	clock_gettime(CLOCK_MONOTONIC, &now);	// copia in now il valore del clock specificato da CLOCK_MONOTONIC
	
	time_copy(&(tp[id].at), now);
	time_copy(&(tp[id].dl), now);
	time_add_us(&(tp[id].at), tp[id].period);
	time_add_us(&(tp[id].dl), tp[id].deadline);
}


int get_task_index(void* arg) {		
	struct task_par *t_p;		
	// argomento di un thread è passato come un puntatore a void. Prima converto a puntatore a task par
	t_p = (struct task_par *)arg;
	return t_p->argument;
}

int deadline_miss(int id) {		
	// se il thread e' ancora in esecuzione quando ri-attivato, incrementa il valore di
	// dmiss e ritorna 1, altrimenti ritorna 0
	struct timespec now;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	
	if ( time_cmp(now, tp[id].dl) > 0 ) {	// se now e' maggiore del tempo di deadiline della task id
	
		tp[id].dmiss++;
		return 1;	// c'e' stato un deadline miss
	}
	return 0;
}

void show_dmiss(int id) {
	
	printf("%d \n", tp[id].dmiss);
}

int get_dmiss(int id){
	return tp[id].dmiss;
}

void wait_for_period(int id) {	
	// sospende la chiamata al thread fino alla prossima attivazione e, quando svegliato,
	// aggiorna l'activation time e la deadline
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &(tp[id].at), NULL);	// sospende l'esecuzione del thread chiamato fino a che
	// CLOCK_MONOTONIC non raggiunge il tempo specificato da &t.
	// TIMER_ABSTIME significa che t e' interpretato come assoluto
	
	time_add_us(&(tp[id].at), tp[id].period);
	time_add_us(&(tp[id].dl), tp[id].period);
}

int task_create( void* (*task)(void *), int id, int period, int drel, int prio ) {
	pthread_attr_t	myattr;
	struct	sched_param mypar;	// sched_param contiene solo sched_priority
	int tret;
	
	tp[id].argument = id;
	tp[id].period = period;
	tp[id].deadline = drel;
	tp[id].priority = prio;
	tp[id].dmiss = 0;
	
	pthread_attr_init(&myattr);

	void *stack;

	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
	pthread_attr_setstacksize(&myattr, STK_SIZE);
	
	
	mypar.sched_priority = tp[id].priority;
	pthread_attr_setschedparam(&myattr, &mypar);
	
	tret = pthread_create( &tid[id], &myattr, task, (void*)(&tp[id]) );
	
	return tret;
}

//termina tutti i task da 1 a n
void ptask_exit(int n){
	int i;
	for (i = 1; i <= n; i++)
		pthread_cancel(tid[i]);
}


//Realizza una sorta di server aperiodico. Controlla la var globale, se è sotto la soglia sospende PER UN PERIODO e ripete.
int ap_server_wait_counter(int id, unsigned int threshold, unsigned int *globalCount, pthread_mutex_t * mux){
	unsigned int gCount;

	//copio var globale
	pthread_mutex_lock(mux);
	gCount = *globalCount; 
	pthread_mutex_unlock(mux);
	
	while(gCount <= threshold){ //sospendo per un periodo il task id finché la var non raggiunge la soglia
		deadline_miss(id);
		wait_for_period(id);

		pthread_mutex_lock(mux);
		gCount = *globalCount;
		pthread_mutex_unlock(mux);
		
	}
}

//Incrementa il contatore globale, in modo che i task sospei con wait_counter vengano svegliati al prossimo periodo
int ap_server_signal(unsigned int * globalCount, pthread_mutex_t *mux){
	pthread_mutex_lock(mux);
	(*globalCount)++;
	pthread_mutex_unlock(mux);
}

//printa su schermo il tempo di esecuzione. Per misurare latenza
void printTime(void){
	struct timespec now;
	
	clock_gettime(CLOCK_MONOTONIC, &now);	
	printf("%ld sec, %ld nsec \n", now.tv_sec, now.tv_nsec);
}