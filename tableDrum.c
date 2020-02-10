/*********************************************************************
 _____     _     _        ____                       
|_   _|_ _| |__ | | ___  |  _ \ _ __ _   _ _ __ ___  
  | |/ _` | '_ \| |/ _ \ | | | | '__| | | | '_ ` _ \ 
  | | (_| | |_) | |  __/ | |_| | |  | |_| | | | | | |
  |_|\__,_|_.__/|_|\___| |____/|_|   \__,_|_| |_| |_|
                                                     
SIMONE TOLOMEI
Matricola 531219
Corso di laurea in Ingegneria Robotica e dell'Automazione
Progetto "table sound"
Compilazione: 	$ make
Esecuzione:		$ sudo ./tableDrum hw:0
---------------------------------------------------------------------
Se hw:0 non funziona provare "plugwh:0" o eseguire
$ arecord -l 
E usare hw:X,Y o plughw:X,Y
dove X = numero scheda e Y = numero dispositivo
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <time.h>
#include <allegro.h>

#include "alsa_interface.h"
#include "ptask.h"
#include "constants.h"

void *energy_detect(void *);
void *pulse_grapher(void *);
void *fft_calc(void *);

void *instrum_raid(void *);
void *correlator_TOM(void *);
void *correlator_SNARE(void *);
void *thresholds_updater(void *);
void *graphic_info_task(void *);


int16_t			pulse[TOTFRAMES];   //campioni temporali dell'impulso
pthread_mutex_t	pMux =		PTHREAD_MUTEX_INITIALIZER;

unsigned int	pulseCount = 0;		//conteggio degli impulsi rilevati e processati
pthread_mutex_t	countMux =	PTHREAD_MUTEX_INITIALIZER;

complex			fft[FFT_NFRAMES];	//FFT dell'impulso
pthread_mutex_t	fftMux =	PTHREAD_MUTEX_INITIALIZER;

unsigned int	fftCount = 0;		//contatore delle FFT calcolate (sta al passo con pulseCount)
pthread_mutex_t	fftCount_Mux = PTHREAD_MUTEX_INITIALIZER;

int				tom_threshold = 60;	//soglia di correlazione per il TOM
pthread_mutex_t	tom_Mux = PTHREAD_MUTEX_INITIALIZER;

int				snare_threshold = 60;//soglia di correlazione per lo snare
pthread_mutex_t	snare_Mux = PTHREAD_MUTEX_INITIALIZER;

int				raid_threshold = 6;	//indice della frequenza soglia per il raid
pthread_mutex_t	raid_Mux = PTHREAD_MUTEX_INITIALIZER;

int				snare_corr = 0;		//valore di correlazione per lo snare
pthread_mutex_t	snare_corr_mux = PTHREAD_MUTEX_INITIALIZER;

int				tom_corr = 0;		//valore di correlazione per il TOM
pthread_mutex_t	tom_corr_mux = PTHREAD_MUTEX_INITIALIZER;

int				freq_index = 0;		//indice frequenza 
pthread_mutex_t	freq_mux = PTHREAD_MUTEX_INITIALIZER;


//funzione che copia in timeBuf e freqBuf i campioni temporali e in frequenza dagli array globali, usando i mutex
void get_pulse_Buffers(int16_t *timeBuf, unsigned int *freqBuf){
	pthread_mutex_lock(&pMux);
	for(int i = 0; i < TOTFRAMES; i++)	timeBuf[i] = pulse[i];
	pthread_mutex_unlock(&pMux);
	
	if(freqBuf == NULL) return;
	
	pthread_mutex_lock(&fftMux); 
	for(int i = 0; i < FFT_NFRAMES; i++) freqBuf[i] = cabs(fft[i]);
	pthread_mutex_unlock(&fftMux);	
}

//disegna l'impulso pBuf, di lunghezza nframes, in una finestra larga w, alta h in x,y
void graph_pulse(int x, int y, int w, int h, short *pBuf, int nframes){
	BITMAP * buf;
	buf = create_bitmap(w,h);
	clear_to_color(buf, BKG_COL);
	rect(buf, 1, 1, w-1, h-1, MAIN_COL);	//creo buffer, coloro lo sfondo e disegno riquadro

	for(int i = 0; i < nframes-1; i++){
		// shift di 8 serve per non far andare fuori scala il segnale. Sono interi a 16 bit con segno
		// (h / 2) offset, i valori nulli in questo modo stanno a metà
		line(buf, i + XMARGIN, ((h / 2) + (pBuf[i] >> 8)), (i + 1)+ XMARGIN, ((h / 2) + (pBuf[i + 1] >> 8)), LIN_COLOR);
	}
	blit(buf, screen, 0, 0, x, y, buf->w, buf->h);
}


int main (int argc, char *argv[]) {
	unsigned int sfreq = FREQ;
	clear();
	alsa_inizializza(argv[1], &sfreq); //in argv[1] c'è lidentificativo della scheda audio
	allegro_init();
	set_color_depth(8);
	install_keyboard();
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, W, H, 0, 0);
	install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, 0);
	
	printf("Setting Energy Detector period: %d usec\n", PERIOD);
	printf("Total frames to read: %d, frequency resolution: %f\n", TOTFRAMES, FFT_FREQ);
	
	task_create(energy_detect,		1,	PERIOD,				PERIOD,				DETECT_PRIO);
	task_create(pulse_grapher,		2,	2*GRAPH_PERIOD,		2*GRAPH_PERIOD,		GRAPH_PRIO);
	task_create(fft_calc,			3,	FFT_PERIOD,			FFT_PERIOD,			FFT_PRIO);
	task_create(instrum_raid,		4,	CORR_PERIOD,		CORR_PERIOD,		STRUM_PRIO);
	task_create(correlator_TOM,		5,	CORR_PERIOD,		CORR_PERIOD,		STRUM_PRIO);
	task_create(correlator_SNARE,	6,	CORR_PERIOD,		CORR_PERIOD,		STRUM_PRIO);
	task_create(thresholds_updater, 7,	GRAPH_PERIOD,		GRAPH_PERIOD,		GRAPH_PRIO);
	task_create(graphic_info_task,	8,	2*GRAPH_PERIOD,		2*GRAPH_PERIOD,		GRAPH_PRIO);

	fprintf(stdout, "Application started. Eneter to exit\n");

	while (!key[KEY_ESC]);
	allegro_exit();

	ptask_exit(8); //8 è il numero di task
	pthread_mutex_destroy(&pMux);
	pthread_mutex_destroy(&fftMux);
	pthread_mutex_destroy(&countMux);
	pthread_mutex_destroy(&fftCount_Mux);
	pthread_mutex_destroy(&tom_Mux);
	pthread_mutex_destroy(&snare_Mux);
	pthread_mutex_destroy(&raid_Mux);
	pthread_mutex_destroy(&freq_mux);
	pthread_mutex_destroy(&tom_corr_mux);
	pthread_mutex_destroy(&snare_corr_mux);
	
	printf("Application stopped\n");
	alsa_close();
	fprintf(stdout, "audio interface closed\n");
	exit(1);
}


//************ TASK ENERGY DETECTOR *************
void *energy_detect(void *arg){ 	//controlla il buffer audio, salva TOTFRAMES e segnala quando trova un impulso
	
	long unsigned int nframes; 	//variabile che contiene il n di campioni effettivamente acquisito
	int16_t tpulse[TOTFRAMES];	//array temporaneo che contiene l'impulso
	short samples[TOTFRAMES];	//array dei samples ricevuto da ALSA

	int tready	= 0;		//segnala che il salvataggio dell'impulso è completato
	int rFrames	= 0;		//numero di frame rimanenti da salvare
	int ti = get_task_index(arg);	//Task index
	
	alsa_wait_start();		//avvia il driver audio e attende che sia pronto
	
	set_activation(ti);	
	wait_for_period(ti); 	//sospende per un periodo. Alla riattivazione ci saranno i primi nframes da controllare
	printf("DETECTOR STARTED\n");
	
	while(1) {
		nframes = TOTFRAMES;	//frames (== campioni) da leggere 
		alsa_getSamples(samples, &nframes); //salvo nframes in array samples.

		for(int i = 0; i < nframes; i++){ 
			if(rFrames){ 										// SE CI SONO FRAME RIMANENTI DA SALVARE	
				tpulse[TOTFRAMES - rFrames] = samples[2 * i];	//li salvo in buffer temporaneo partendo da dove ero rimasto
				rFrames--;
				tready = !rFrames; 								//se rFrames = 0 ho finito e segnalo con tReady
			}else if(samples[2*i]> SOGLIA){ 					//altrimenti controllo in cerca di campioni oltre la soglia
				rFrames = TOTFRAMES;							//e in caso positivo mi preparo a salvare TOTframes al ciclo dopo.
			} //Non viene salvato il campione i-esimo, ma non ha alcun effetto sul funzionamento
		}

		if(tready){								//alla fine del ciclo, se il buffer temporaneo è pieno e pronto per essere salvato
			pthread_mutex_lock(&pMux);	
			for(int i = 0; i < TOTFRAMES; i++){	//lo copio nel buffer globale.
				pulse[i] = tpulse[i];		
			}
			pthread_mutex_unlock(&pMux);
			ap_server_signal(&pulseCount, &countMux);	//e aggiorno il contatore globale
			tready = 0;
		}
		
		deadline_miss(ti);
		wait_for_period(ti);
	}
}

//***** Calcola la FFT dell'impulso rilevato.
void *fft_calc(void *arg){
	int ti;
	ti = get_task_index(arg);
	set_activation(ti);
	
	int pulseCount_local = 0; //permette di capire a quale impulso sono arrivato con l'elaborazione
	
	while(1){
		//sospendo task in attesa che pulsecont sia maggiore di pulsecont_local
		ap_server_wait_counter(ti, pulseCount_local, &pulseCount, &countMux);

		//c'è un impulso da trasformare. Lo salvo in fft[] e chiamo la funzione di elaborazione
		pthread_mutex_lock(&fftMux);

		pthread_mutex_lock(&pMux);
		for(int i = 0; i < TOTFRAMES; i++) fft[i] = pulse[i];
		pthread_mutex_unlock(&pMux);
		
		compute_fft(fft); 		//calcola fft dell'impulso contenuto in "fft", risultato in "fft"
		pthread_mutex_unlock(&fftMux);

		//incremento il contatore globale fftCount
		ap_server_signal(&fftCount, &fftCount_Mux);

		pulseCount_local++;		//incremento contatore locale
		deadline_miss(ti);
		wait_for_period(ti);
	}
}

//disegna impulso nel tempo e in frequenza
void *pulse_grapher(void *arg){
	int ti = get_task_index(arg);

	unsigned int 	maxfreq, maxmag = 0; 
	unsigned int 	fftCount_local = 0;
	float 			freq;
	unsigned int 	fCount;
	short 			pBuf[TOTFRAMES];
	unsigned int 	fBuf[FFT_NFRAMES];

	clear_to_color(screen, BKG_COL);
	
	//creo finestra alta 1/3 dello schermo
	BITMAP * buf, * buf_fft;
	const int mW = W - (2 * XMARGIN); 		//largo tutto schermo meno i margini
	const int mH = (H / 3) - (2 * YMARGIN);	//alto un terzo, meno i margini
	buf = create_bitmap(mW, mH);
	clear_to_color(buf, BKG_COL);
	rect(buf, 1, 1, mW - 1, mH - 1, MAIN_COL);

	//analogo ma largo la metà per FFT
	buf_fft = create_bitmap(mW / 2, mH);
	rect(buf_fft, 0, 0, mW / 2 - 1, mH - 1, MAIN_COL);
			
	set_activation(ti); 
	
	while(1){
		ap_server_wait_counter(ti, fftCount_local, &fftCount, &fftCount_Mux); //attendo che ci siano impulsi pronti (con la fft)

		rectfill(buf, 2, 2, mW - 2, mH - 2, BKG_COL);

		//copio array temporale e in freq dell'impulso nei buffer
		get_pulse_Buffers(pBuf, fBuf);
		
		//disegno andamento temporale usando linee che vanno da un campione all'altro (shift di 8 per evitare che vada fuori scala)
		for(int i = 0; i < TOTFRAMES - 1; i++)
			line(buf, (XMARGIN + 2*i), (mH/2 + (pBuf[i] >> 8)), (XMARGIN + 2*(i + 1)), (mH/2 + (pBuf[i + 1] >> 8)), LIN_COLOR);
		
		blit(buf, screen, 0, 0, XMARGIN,YMARGIN, buf->w, buf->h);
		
		//Disegno FFT
		rectfill(buf_fft, 1, 1, mW/2-2, mH-2, BKG_COL);
		maxmag = 0;
		for(int i = 0; i < FFT_NFRAMES; i++){  //disegno una linea verticale per ogni impulso, e in più cerco il val massimo
			if(fBuf[i] > maxmag){
				maxmag = fBuf[i];
				maxfreq = i;
			}
			line(buf_fft, XMARGIN + (4*i), mH-1, XMARGIN + (4*i), (mH-(fBuf[i]>>13)), LIN_COLOR);
		}
		//ridisego la linea del valore massimo in rosso
		line(buf_fft, XMARGIN + (4*maxfreq), mH-1, XMARGIN + (4*maxfreq), (mH-(fBuf[maxfreq]>>13)), 12);
		
		rect(buf_fft, 0, 0, mW/2 - 1, mH - 1, MAIN_COL);
		blit(buf_fft, screen, 0, 0, XMARGIN, (H/3) + YMARGIN , buf->w, buf->h);
		fftCount_local++;

		deadline_miss(ti);
		wait_for_period(ti);
	}
}	

//***** SUONA IL RAID SE LA FREQUENZA È SOPRA LA SOGLIA
void *instrum_raid (void *arg){
	int ti = get_task_index(arg);
	unsigned int fftCount_local = 0;
	unsigned int fCount;
	int16_t pulseBuf[TOTFRAMES];
	unsigned int fftBuf[FFT_NFRAMES];
	
	SAMPLE *drumCrash;
	drumCrash = load_sample("sounds/crash.wav");

	set_activation(ti); 
	double freq_Hz = 0.0;

	while(1){
		//sincronizzo sulla FFT del segnale
		ap_server_wait_counter(ti, fftCount_local, &fftCount, &fftCount_Mux);
		fftCount_local++;
		
		get_pulse_Buffers(pulseBuf, fftBuf);
		
		pthread_mutex_lock(&raid_Mux);
		int threshold = raid_threshold;
		pthread_mutex_unlock(&raid_Mux);

		int val = 0, freq = 0; 
		//cerco la componente massima
		//escludo la componente continua e la prima armonica (sono troppo alte e oscurano gli altri picchi)
		for(int i = 2; i < FFT_NFRAMES; i++){
			if(fftBuf[i] > val){
				val = fftBuf[i];
				freq = i;
			}
		}
		//salvo in var globale
		pthread_mutex_lock(&freq_mux);
		freq_index = freq;
		pthread_mutex_unlock(&freq_mux);

		//se il suo valore (intero) è oltre la soglia allora suono.
		if(freq >= threshold){
			play_sample(drumCrash, VOL, 128, 1000, 0);
			printf("DRUM_CRASH DETECTED\n");
		}

		deadline_miss(ti);
		wait_for_period(ti);
	}

}

//**** ESEGUE CORRELAZIONE CON MODELLO A E SUONA SE OLTRE LA SOGLIA
void *correlator_TOM(void *arg){
	unsigned int	pulseCount_local = 0; 	//variabili di sync
	unsigned int	pCount;				
	int16_t 		pulseBuf[TOTFRAMES];	//buffer per modello
	short 			modelBuf[TOTFRAMES];	//e per impulso
	int 			threshold = 60;			//soglia di comparazione
	int				c; 						//valore cross-correlazione
	
	FILE *model_TOM;	//carico il modello
	model_TOM = fopen("models/m_tom.bin", "rb");
	fread(modelBuf, 2, TOTFRAMES, model_TOM);
	fclose(model_TOM);
	
	int mW = (W/2) - 2*XMARGIN;	//preparo la finestra. larga metà schermo e alta 1/3. Meno un margine per ogni lato
	int mH = (H/3) - 4*YMARGIN; 
	int mX = XMARGIN;
	int mY = ((2*H) / 3) + YMARGIN;
	graph_pulse(mX, mY, mW, mH, modelBuf, TOTFRAMES); //sotto lascio spazio alla barra, larga 1 margine.
	textout_ex(screen, font, "Tom Drum model", mX, mY - YMARGIN, MAIN_COL, -1);

	SAMPLE *drumTom;
	drumTom = load_sample("sounds/tom.wav");

	int ti = get_task_index(arg);
	set_activation(ti);

	while(1){
		ap_server_wait_counter(ti, pulseCount_local, &pulseCount, &countMux);
		pulseCount_local++;
		get_pulse_Buffers(pulseBuf, NULL);
		
		pthread_mutex_lock(&tom_Mux);
		int threshold = tom_threshold;
		pthread_mutex_unlock(&tom_Mux);

		rectfill(screen, mX, (mY + mH) + YMARGIN, (mX+mW), (mY + mH) + 2*YMARGIN , BKG_COL); //Cancello barra. spessore 1 YMARGIN
		
		c = cross_corr(pulseBuf,modelBuf,CORR_FRAMES, 5);

		pthread_mutex_lock(&tom_corr_mux);
		tom_corr = c;
		pthread_mutex_unlock(&tom_corr_mux);
	
		if(c > threshold){
			play_sample(drumTom, VOL, 128, 1000, 0);
			printf("DRUM_KICK DETECTED\n");
			rectfill(screen, mX, (mY + mH) + YMARGIN, mX+(c*mW)/100,(mY + mH) + 2*YMARGIN , 12);   
		}else{
			rectfill(screen, mX, (mY + mH) + YMARGIN, mX+(c*mW)/100,(mY + mH) + 2*YMARGIN , WAIT_COL);   
		}

		printf("correlation: %d\n",c);
		
		//se premuto shift cancella il modello attuale
		if(key[KEY_LSHIFT]){
			model_TOM = fopen("models/m_tom.bin", "wb"); //SE premuto shift lo cancella
			fclose(model_TOM);
		}

		//se premuto T usa l'impulso ricevuto per aggiornare il modello
		if(key[KEY_T]){
			model_update(model_TOM, "models/m_tom.bin", pulseBuf);
			fread(modelBuf, 2, TOTFRAMES, model_TOM);
			fclose(model_TOM);
			graph_pulse(mX, mY, mW, mH, modelBuf, TOTFRAMES);
			printf("model updated\n");
		}
		deadline_miss(ti);
		wait_for_period(ti);
	}
}

//**** ESEGUE CORRELAZIONE CON MODELLO B E SUONA SE OLTRE LA SOGLIA
void *correlator_SNARE(void *arg){
	unsigned int	pulseCount_local = 0;		//per sync
	unsigned int	pCount;
	int16_t 		pulseBuf[TOTFRAMES];		//buffer modello e FFT
	short 			modelBuf[TOTFRAMES];
	int 			threshold;					//soglia
	int				c;							//cross corr

	FILE *model_SNARE;
	model_SNARE = fopen("models/m_snare.bin", "rb");
	fread(modelBuf, 2, TOTFRAMES, model_SNARE);
	fclose(model_SNARE);

	int mW = W/2 - 2*XMARGIN;	//preparo la grafica
	int mH = H/3 - 4*YMARGIN;
	int mX = W/2 + XMARGIN;
	int mY = (2*H) / 3 + YMARGIN;
	graph_pulse(mX, mY, mW, mH, modelBuf, TOTFRAMES);
	textout_ex(screen, font, "Snare Drum model", mX, mY - YMARGIN, MAIN_COL, -1);

	SAMPLE *snare;
	snare = load_sample("sounds/snare.wav");

	int ti = get_task_index(arg);
	set_activation(ti);

	while(1){
		ap_server_wait_counter(ti, pulseCount_local, &pulseCount, &countMux);
		pulseCount_local++;
		get_pulse_Buffers(pulseBuf, NULL);

		pthread_mutex_lock(&snare_Mux);
		threshold = snare_threshold;
		pthread_mutex_unlock(&snare_Mux);

		c = cross_corr(pulseBuf,modelBuf,CORR_FRAMES, 5);

		pthread_mutex_lock(&snare_corr_mux);
		snare_corr = c;
		pthread_mutex_unlock(&snare_corr_mux);	

		rectfill(screen, mX, (mY + mH) + YMARGIN, (mX+mW), (mY + mH) + 2*YMARGIN , BKG_COL); //spessore 2 px
		if(c > threshold){
			play_sample(snare, VOL, 128, 1000, 0);
			printf("SNARE DETECTED: ");
			rectfill(screen, mX, (mY + mH) + YMARGIN, mX+(c*mW)/100,(mY + mH) + 2*YMARGIN , 12);  
		}else{
			rectfill(screen, mX, (mY + mH) + YMARGIN, mX+(c*mW)/100,(mY + mH) + 2*YMARGIN , WAIT_COL);  
		}

		//se premuto shift cancella il modello attuale (wb sovrascrive con un file vuoto)
		if(key[KEY_LSHIFT]){
			model_SNARE = fopen("models/m_snare.bin", "wb"); 
			fclose(model_SNARE);
		}
		
		//se premuto S aggiorna il modello con l'impulso ricevuto
		if(key[KEY_S]){
			model_update(model_SNARE, "models/m_snare.bin", pulseBuf);
			fread(modelBuf, 2, TOTFRAMES, model_SNARE);
			fclose(model_SNARE);
			graph_pulse(mX, mY, mW, mH, modelBuf, TOTFRAMES);
			printf("model updated\n");
		}

		deadline_miss(ti);
		wait_for_period(ti);
	}
}

// Legge dalla tastiera e aggiorna le soglie di correlazione e frequenza
void *thresholds_updater(void *arg){
	int ti = get_task_index(arg);
	set_activation(ti);
	int pressed = 0;

	while(1){
		if(key[KEY_T] && key[KEY_UP]){
			pthread_mutex_lock(&tom_Mux);
			tom_threshold += 5;
			pthread_mutex_unlock(&tom_Mux);
			while(key[KEY_T] && key[KEY_UP]);
		}

		if(key[KEY_T] && key[KEY_DOWN]){
			pthread_mutex_lock(&tom_Mux);
			tom_threshold -= 5;
			pthread_mutex_unlock(&tom_Mux);
			while(key[KEY_T] && key[KEY_DOWN]);
		}
		
		if(key[KEY_S] && key[KEY_UP]){
			pthread_mutex_lock(&snare_Mux);
			snare_threshold += 5;
			pthread_mutex_unlock(&snare_Mux);
			while(key[KEY_S] && key[KEY_UP]);
		}

		if(key[KEY_S] && key[KEY_DOWN]){
			pthread_mutex_lock(&snare_Mux);
			snare_threshold -= 5;
			pthread_mutex_unlock(&snare_Mux);
			while(key[KEY_S] && key[KEY_DOWN]);
		}

		if(key[KEY_R] && key[KEY_UP]){
			pthread_mutex_lock(&raid_Mux);
			raid_threshold += 1;
			pthread_mutex_unlock(&raid_Mux);
			while(key[KEY_R] && key[KEY_UP]);
		}

		if(key[KEY_R] && key[KEY_DOWN]){
			pthread_mutex_lock(&raid_Mux);
			raid_threshold -= 1;
			pthread_mutex_unlock(&raid_Mux);
			while(key[KEY_R] && key[KEY_DOWN]);
		}
	
	wait_for_period(ti);
	}

}

// Scrive info, dmiss, valori di correlazione e soglie.
void *graphic_info_task(void* arg){
	int ti = get_task_index(arg);
	
	int snare, tom, freq, snare_th, tom_th, freq_th; //conterranno i valori dei rispettivi parametri globali
	unsigned int pulseCount_local;
	int row = 1;			//indice della riga, utile quando scrive
	
	int mX = W/2 + XMARGIN;
	int mY = H/3 + YMARGIN;
	int mW = W/2 - 2*XMARGIN;
	int mH = H/3 - 2*YMARGIN;

	BITMAP * buf;
	char s[80];
	set_activation(ti);

	while(1){
		pthread_mutex_lock(&freq_mux);
		freq = freq_index;
		pthread_mutex_unlock(&freq_mux);
		
		pthread_mutex_lock(&snare_corr_mux);
		snare = snare_corr;
		pthread_mutex_unlock(&snare_corr_mux);

		pthread_mutex_lock(&tom_corr_mux);
		tom = tom_corr;
		pthread_mutex_unlock(&tom_corr_mux);

		pthread_mutex_lock(&snare_Mux);
		snare_th = snare_threshold;
		pthread_mutex_unlock(&snare_Mux);

		pthread_mutex_lock(&tom_Mux);
		tom_th = tom_threshold;
		pthread_mutex_unlock(&tom_Mux);

		pthread_mutex_lock(&raid_Mux);
		freq_th = raid_threshold;
		pthread_mutex_unlock(&raid_Mux);

		pthread_mutex_lock(&countMux);
		pulseCount_local = pulseCount;
		pthread_mutex_unlock(&countMux);

		buf = create_bitmap(mW, mH);
		clear_to_color(buf, BKG_COL);
		rect(buf, 0,0, mW-1, mH-1,MAIN_COL);
		

		row = 1;
		textout_ex(buf, font, "Table Drum Machine", XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Total Pulse Counter: %d", pulseCount_local);
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s,"Estimated Frequency: %5.1fHz", ((float)(freq_index*FFT_FREQ)));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), LIVE_COL, -1);

		sprintf(s,"Raid threshold: %5.1fHz ('R' + UP/DOWN to adjust)", ((float)(freq_th*FFT_FREQ)));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);

		sprintf(s, "Snare:  cross-correlation: %d%%", snare);
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), LIVE_COL, -1);
		
		sprintf(s, " - Threshold: %d%% ('S' + UP/DOWN to adjust)", snare_th);
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);

		textout_ex(buf, font, " - Hold S to train model, LSHIFT + S to reset", XMARGIN, TMARGIN*(row++),MAIN_COL, -1);
		
		sprintf(s, "Tom: cross-correlation: %d%%", tom);
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), LIVE_COL, -1);

		sprintf(s, " - Threshold: %d%% ('T' + UP/DOWN to adjust)", tom_th);
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		textout_ex(buf, font, " - Hold T to train model, LSHIFT + T to reset", XMARGIN, TMARGIN*(row++), MAIN_COL, -1);

		sprintf(s, "Dmiss detector task: %d", get_dmiss(1));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss pulse grapher: %d", get_dmiss(2));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss FFT task:: %d", get_dmiss(3));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss Raid task: %d", get_dmiss(4));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss Tom task: %d", get_dmiss(5));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss Snare Task: %d", get_dmiss(6));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss Threshold updater: %d", get_dmiss(7));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		sprintf(s, "Dmiss info and graphics: %d", get_dmiss(8));
		textout_ex(buf, font, s, XMARGIN, TMARGIN*(row++), MAIN_COL, -1);
		
		textout_ex(buf, font, "Press ESC to exit", XMARGIN, TMARGIN*(row++), MAIN_COL, -1);

		blit(buf, screen, 0, 0, mX, mY, buf->w, buf->h);
		deadline_miss(ti);
		wait_for_period(ti);
	}	
}

