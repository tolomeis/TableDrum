#include <complex.h>
#include <stdio.h>

#define clear() printf("\033[H\033[J")
#define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))
#define goup(x) printf("\033[%dA", (x))
#define godown(x) printf("\033[%dB", (x))
#define FLOAT_TO_INT(x) ((x)>=0?(int)((x)+0.5):(int)((x)-0.5))


int alsa_inizializza(char*, unsigned int*);

int alsa_getSamples(short*, long unsigned int *);

int alsa_compute_array(short **samples);

unsigned int alsa_request_samples(short *samp_array, long unsigned int *nframes); //__attribute__((always_inline));

void alsa_wait_start();

void alsa_complete();

void compute_fft(complex *);

void alsa_close(void);

int cross_corr(short *, short *, int, int);

void model_update(FILE *, char*, short*);

