
//COSTANTI PER ACQUISIZIONE IMPULSO
#define SOGLIA 				12000				//soglia di trigger per il detector
#define FREQ				44100				//[Hz] campionamento
#define PERIOD				10000 				//12[us] periodo del task detector
#define TOTFRAMES			((PERIOD*FREQ)/1000000)	//Numero di campioni da leggere in un periodo = numero di campioni acquisiti dal driver in un periodo

#define FFT_NFRAMES			512 						//Numero di campioni su cui fare la FFT. Deve essere potenza di 2. Se maggiore di TOTFRAMES vengono aggiunti zeri
//La funzione fft esegue un ZERO PADDING, quindi esegue su 2*FFT_NFRAMES campioni. 
#define FFT_FREQ			(float)(FREQ/(2*FFT_NFRAMES)) //risoluzione in frequenza della FFT
#define CORR_FRAMES			(TOTFRAMES/2)				// numero di campioni su cui fare la crosscorrelazione

//PERIODI
#define GRAPH_PERIOD	25000		//Periodo task grafico e input
#define FFT_PERIOD 		5000		//Periodo task FFT
#define CORR_PERIOD 	5000		//periodo task correlator (stumenti)

//***** PRIORITÀ
#define GRAPH_PRIO		50
#define STRUM_PRIO		70
#define DETECT_PRIO		90
#define FFT_PRIO		80



//******* GRAFICA ******
#define XMARGIN 5		//margine lungo x
#define YMARGIN 8		//margine lungo y
#define TMARGIN 15		//margine tra righe di testo
#define W 		1200 	//larghezza complessiva
#define H 		990		//altezza complessiva

#define MAIN_COL	0	//colore testo 
#define BKG_COL		7	//sfondo
#define LIN_COLOR	0	//linea degli impulsi
#define WAIT_COL	1	//barra quando sotto soglia
#define LIVE_COL	4	//barra quando sopra soglia

#define VOL			255	//Volume