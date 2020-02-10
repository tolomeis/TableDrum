 
#include <stdio.h>
#include <math.h>
#include <complex.h>

#define N 512
 
double PI;// = 3.1415;
typedef double complex cplx;
 
void _fft(cplx buf[], cplx out[], int n, int step){
	if (step < n) {
		_fft(out, buf, n, step * 2);
		_fft(out + step, buf + step, n, step * 2);
 
		for (int i = 0; i < n; i += 2 * step) {
			cplx t = cexp(-I * PI * i / n) * out[i + step];
			buf[i / 2]     = out[i] + t;
			buf[(i + n)/2] = out[i] - t;
		}
	}
}



void compute_fft(cplx buf[]){

	cplx out[2*N];
	cplx bufL[2*N];

	for (int i = 0; i < N; i++){
		out[i] = buf[i];
		bufL[i] = buf[i];
		//zero add 
		out[N+i] = 0;
		bufL[N+i] = 0;
	}

	//for (int i = 0; i < N; i++) out[i] = buf[i];
 
	_fft(bufL, out, 2*N, 1);
	for (int i = 0; i < N; i++) buf[i] = bufL[i];	
}



/*
void *fft_calc(void *arg){
	int ti;
	ti = get_task_index(arg);
	set_activation(ti);

	//unsigned int pulseCount_local = 0;
	//uint32_t fft[N];
	cplx buf[N];
	unsigned int peak = 0;
	unsigned int peak_index = 0;
	
	pthread_mutex_lock(&fftMux);
	pthread_mutex_lock(&pMux);
	for(int i = 0; i < N; i++) buf[i] = pulse[i];
	pthread_mutex_unlock(&pMux);

	compute_fft(buf);
	pthread_mutex_unlock(&fftMux);
}*/