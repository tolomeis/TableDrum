#include "alsa_interface.h"

#include <math.h>
#include <alsa/asoundlib.h>
#include "constants.h"

typedef double complex cplx;	//più facile da scrivere
snd_pcm_t *capture_handle;		//struttura audio gestita da ALSA
double PI;						//serve per FFT

//INIZIALIZZA ALSA
int alsa_inizializza(char *porta, unsigned int *rate) {
	int i;		//indice
	int err;	//coodice di errore
	snd_pcm_hw_params_t *hw_params; 					//struttura dei parametri
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;	//16 bit Little Endian
	snd_pcm_uframes_t size = (*rate) * 4; 				//si imposta lunghezza buffer tale che contenga 4 secondi
	int channels = 2;			//n canali
	PI = atan2(1, 1) * 4; 		//faccio calcolare a lui il PI 

	if ((err = snd_pcm_open (&capture_handle, porta, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n", porta, snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "audio interface opened\n");
	
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params allocated\n");

	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params initialized\n");

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n", snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params access set\n");

	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params format: %d\n", format);

	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, rate,0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params rate set at %d\n", *rate);

	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, channels)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params channels: %d\n", channels);

	if ((err = snd_pcm_hw_params_set_buffer_size_near(capture_handle, hw_params, &size) < 0)) {
		fprintf (stderr, "cannot set channel count (%s)\n",snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "Buffer size set. Actual size: %ld\n", size);

	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "hw_params set\n");

	snd_pcm_hw_params_free (hw_params);
	fprintf(stdout, "hw_params freed\n");

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",snd_strerror (err));
		exit (1);
	}
	if((err = snd_pcm_nonblock(capture_handle, 0))){
		fprintf (stderr, "cannot set blocking mode (%s)\n",snd_strerror (err));
		exit(1);
	}
	printf("blocking mod set\n");

	return 0;

}


int alsa_getSamples(short *buffer, long unsigned int *nframes){
	int av = snd_pcm_readi(capture_handle, buffer, *nframes);

	//se c'è un overrun aspetto e riparto (se succede ovviamente genera dmiss)
	while(!av) av = snd_pcm_recover(capture_handle, av, 0);
}



void alsa_wait_start(){
	
	int avFrames = snd_pcm_avail_update(capture_handle);

	int err = snd_pcm_start(capture_handle);
	if (err < 0) {
		printf("Start error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	int r = snd_pcm_wait(capture_handle, 2000); //aspetta che sia pronta, massimo per 2 secondi

	if(r) printf("ready\n");
	else exit(1);
}


//calcola FFT, è l'effettiva funzione ricorsiva di calcolo della FFT con l'algoritmo Cooley-Tukey
//non può essere usata al di fuori di questo file
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


//calcola fft dell'impulso in pulse[] e mette risultato in pulse[]. USA 2*FFT_FRAMES, (zero padding)
void compute_fft(cplx pulse[]){

	cplx out[2*FFT_NFRAMES];
	cplx buf[2*FFT_NFRAMES];
	
	
	//copio pulse dentro due buffer usati per l'algoritmo, copio fino a TOTFRAMES
	for (int i = 0; i < TOTFRAMES; i++){ 
		out[i] = pulse[i];
		buf[i] = pulse[i];
	}
	
	//ZERO PADDING FINO a 2*FFT_NFRAMES
	for (int i = TOTFRAMES; i < (2*FFT_NFRAMES); i++){
		out[i] = 0;
		buf[i] = 0;
	} 
	
	_fft(buf, out, 2*FFT_NFRAMES, 1);
	
	//copio il risultato in pulse
	for (int i = 0; i < FFT_NFRAMES; i++) pulse[i] = buf[i];
}

//chiude inerfaccia audio
void alsa_close(){
	snd_pcm_close(capture_handle);
}

//calcola crosscorrelazione tra model e pulse, usando frames e con un delay massimo maxdelay. 
//restituisce il valore MASSIMO della crosscorrelazione
int cross_corr(short* model, short *pulse, int frames, int maxdelay){
	double sx, sy, denom, sxy, mx, my;
	int delay;
	int i, j;
	double maxcorr;
	
	//calcolo media di x (impulso) e y (modello)
	mx = 0;
	my = 0;   
	for (i = 0; i < frames; i++) {
		mx += (double) pulse[i];
		my += (double) model[i];
	}
	mx /= (double) frames;
	my /= (double) frames;

	//denominatore,  sqrt(∑((x-mx)^2 * (y-my)^2))
	sx = 0;
	sy = 0;
	for(i = 0; i < frames; i++){
		sx += ((double)(pulse[i]) - mx)*((double)(pulse[i]) - mx);
		sy += ((double)(model[i]) - my)*((double)(model[i]) - my);
	}
	denom = sqrt(sx * sy);

	maxcorr = 0.0;

	//numeratore =  ∑( x(i) * y(i + tau)) zero dove non definiti i segnali
	for (delay = -maxdelay; delay < maxdelay; delay++) {
		sxy = 0;

		for (i = 0; i < frames; i++) {
			j = i + delay;
			if ((j >= 0) && (j < frames))
				sxy += ((double) pulse[i] - mx)*((double)model[j] - my);	
		}

		if ((sxy / denom) > maxcorr)
			maxcorr = (sxy / denom);	
	}
	return FLOAT_TO_INT(maxcorr*100.0);
}


//aggiorna il modello contenuto in filepointer usando l'impulso
//esegue una media campionaria, tiene conto di quanti impulsi sono stati usati per il modello
void model_update(FILE * filepointer, char* filename, short * pulse){

	filepointer = fopen(filename, "rb");

	short modelBuf[TOTFRAMES+1];
	int read = fread(modelBuf, 2, TOTFRAMES +1, filepointer);
	
	short pCount = modelBuf[TOTFRAMES]; //n+1esimo elemento usato come contatore degli impulsi mediati.

	if(!read) pCount = 0; //se è vuoto devo scriverlo da zero

	int written;
	float m, p, count; //count viene usato per operazioni tra float, risparmio conversioni

	//se il modello contiene guà qualcosa allora faccio la media, altrimenti ci copio il segnale
	if(pCount > 0){
		for (int i = 0; i < TOTFRAMES; i++){
			p = (float) pulse[i];
			m = (float) modelBuf[i];
			count = (float)pCount;
				
			m += (p/count); //aggiungo
			m = (m * count / (count + 1.0));
			modelBuf[i] = FLOAT_TO_INT(m);
		}
	} else {
		for(int i = 0; i < TOTFRAMES; i++)
			modelBuf[i] = pulse[i];
	}
	modelBuf[TOTFRAMES] = pCount + 1;
	freopen(filename, "r+b", filepointer);		//resetta i puntatori per lettura /scrittura
	written = fwrite(modelBuf, 2, TOTFRAMES + 1, filepointer);
	freopen(filename, "rb", filepointer);

}