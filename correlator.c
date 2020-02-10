
void *correlator(void *arg){
	// devo calcolare media
	//serve un modello di riferimento -> come memorizzo roba in files?
	//FILE *fp;
	/*
    fp = fopen("/tmp/test.txt", "w+");
   	fprintf(fp, "This is testing for fprintf...\n");
   	fputs("This is testing for fputs...\n", fp);
   	fclose(fp);
	*/
	int ti;
	ti = get_task_index(arg);
	set_activation(ti);
	//dato pulse [i]
	//dato ref[i] -> modello
	for(int i = 0; i < numFrames/2; i++ ){
		crc[i] = 0;
		for(int j = 0; i < numFrames; j++){
			crc[i] += pulse[j]*ref[i+j];
		}
	}

}

struct pulseParam {
	uint energy;
	uint count;
	uint freqency;

	//sperimentali:
	uint freqVar;

}
void *energy_calc (void *arg){
	int ti;
	ti = get_task_index(arg);
	set_activation(ti);

	uint energy = 0;
	
	pthread_mutex_lock(&pMux);
	for(int i = 0; i < TOTFRAMES; i++) 
		energy += pulse[i]*pulse[i];
	pthread_mutex_unlock(&pMux);
	
	energy = energy/TOTFRAMES;

	}




// Generico task che legge dal buffer audio
void *task (void *arg){
	int ti = get_task_index(arg);
	uint fftCount_local = 0;
	uint fCount;
	int16_t pulseBuf[TOTFRAMES];
	uint fftBuf[N];

	set_activation(ti); //serve per calcolare dmiss
	while(1){
		pthread_mutex_lock(&fftCount_Mux);
		fCount = fftCount;
		pthread_mutex_unlock(&fftCount_Mux);

		if(fCount > fftCount_local){
			fftCount_local++;
			pthread_mutex_lock(&pMux);
			for(int i = 0; i < TOTFRAMES; i++) pulseBuf[i] = pulse[i];
			pthread_mutex_unlock(&pMux);

			pthread_mutex_lock(&fftMux);
			for(int i = 0; i < N; i++) fftBuf[i] = fft[i];
			pthread_mutex_unlock(&fftMux);
		

		}

		wait_for_period(ti);
	}

}



void *instrum_crash (void *arg){
	int ti = get_task_index(arg);
	uint fftCount_local = 0;
	uint fCount;
	int16_t pulseBuf[TOTFRAMES];
	uint fftBuf[N/2];
	int soglia = 40;
	float crash_affinity;
	float toll = 1.0;

	set_activation(ti); //serve per calcolare dmiss
	while(1){
		pthread_mutex_lock(&fftCount_Mux);
		fCount = fftCount;
		pthread_mutex_unlock(&fftCount_Mux);

		if(fCount > fftCount_local){
			get_pulse_Buffers(pulseBuf, fftBuf);
			
			int val = 0, freq = 0;
			
			for(int i = 0; i < N/2; i++){
				if(fftBuf[i] > val){
					val = fftBuf[i];
					freq = i;
				}
			} //ho il massimo
			//uso sigmoide shiftata fino a soglia
			crash_affinity = 1.0/(1.0 + exp(-toll*(freq - soglia )));
			printf("Crash: %f", crash_affinity);			

		}
		deadline_miss(ti);
		wait_for_period(ti); //Serve?
	}

}

void get_pulse_Buffers(uint *timeBuf, int16_t *freqBuf){
	pthread_mutex_lock(&pMux);
	for(int i = 0; i < TOTFRAMES; i++) timeBuf[i] = pulse[i];
	pthread_mutex_unlock(&pMux);

	pthread_mutex_lock(&fftMux);
	for(int i = 0; i < N/2; i++) freqBuf[i] = cabs(fft[i]);
	pthread_mutex_unlock(&fftMux);
	
}




void graph_pulse(int x, int y, int w, int h, short *pBuf, int nframes){
	BITMAP * buf;
	buf = create_bitmap(w,h);
	rect(buf, 1, 1, w-1, h-1, 15);
	int val; 

	for(int i = 0; i < nframes; i++){
		putpixel(buf, i+2, ((h/2) + (pBuf[i]>>8)), 14);
	}
	blit(buf, screen, 0, 0, x, y, buf->w, buf->h);
}


void *model_update(void *arg){
		
	int16_t pulseBuf[TOTFRAMES+1];
	short modelBuf[TOTFRAMES+1];
	get_pulse_Buffers(pulseBuf, NULL);

	FILE *model; 
	model = fopen("model.bin", "r+b");

	fread(modelBuf, 2, TOTFRAMES +1, model);
	int pCount = modelBuf[TOTFRAMES]; //l'ultimo elemento contiene il n di impulsi mediati
	pulseBuf[TOTFRAMES] = Pcount + 1;

	float m, p, count;

	if(pCount != 0){
		for (int i = 0; i < TOTFRAMES; i++){
			p = (float) pulseBuf[i];
			m = (float) modelBuf[i];
			count = (float)pCount;
				
			m += (p/count);
			m = (m * count / (count + 1.0));
			modelBuf[i] = FLOAT_TO_INT(m);
			//modelBuf[i] += (pulseBuf[i] / pulseCount_local);
			//modelBuf[i] = ((modelBuf[i] * pulseCount_local) /(pulseCount_local + 1));
		}
		freopen("model.bin", "r+b", model);
		written = fwrite(modelBuf, 2, TOTFRAMES + 1, model);
	} else written = fwrite(pulseBuf, 2, TOTFRAMES + 1, model);	



}












	//int ti = get_task_index(arg);
	
	uint pulseCount_local = 0;
	
	uint pCount;
	short modelBuf[TOTFRAMES];
	FILE * model;
	model = fopen("model.bin", "w+b");
	set_activation(ti);
	int read, written;


	while(1){
		pthread_mutex_lock(&countMux);
		pCount = pulseCount;
		pthread_mutex_unlock(&countMux);

		if(pCount > pulseCount_local){
			
		
			clear();
			
			read = fread(modelBuf, 2, TOTFRAMES, model);
		
			float p, m, count;

			for(int i = 0; i < (TOTFRAMES/CYCLES); i++){
				gotoxy(i,40);
				printf("-");
				gotoxy(i, 40 + (pulseBuf[i]>>11) );
				printf("•");
			}

			if(pulseCount_local != 0){
				for (int i = 0; i < TOTFRAMES; i++){
					p = (float) pulseBuf[i];
					m = (float) modelBuf[i];
					count = (float)pulseCount_local;
					
					m += (p/count);
					m = (m * count / (count + 1.0));
					modelBuf[i] = FLOAT_TO_INT(m);
					//modelBuf[i] += (pulseBuf[i] / pulseCount_local);
					//modelBuf[i] = ((modelBuf[i] * pulseCount_local) /(pulseCount_local + 1));
				}
				freopen("model.bin", "w+b", model);

				written = fwrite(modelBuf, 2, TOTFRAMES, model);
			} else written = fwrite(pulseBuf, 2, TOTFRAMES, model);

			for(int i = 0; i < (TOTFRAMES/CYCLES); i++){
				gotoxy(i,18);
				printf("-");
				gotoxy(i, 18 + (modelBuf[i]>>11) );
				printf("•");
			}

			gotoxy(0,55);
			pulseCount_local++;

			printf("Written %d samples. Total pulse count: %d\n", written, pulseCount_local);

		}
		wait_for_period(ti);
	}

}


void *server(void *arg){
	int ti;
	ti = get_task_index(arg);

	int pulseCount_local = 0;
	int pCount;
	set_activation(ti);
	
	while(1){
		pthread_mutex_lock(&countMux);
		pCount = pulseCount;
		pthread_mutex_unlock(&countMux);

		if(pCount > pulseCount_local){
			task_activate(fft)
			task_activate(corrlator)
			task_activate(crash)

			sync()
			task_activate(grapher)
			scrivi()
			pulseCount_local++;
		}

		deadline_miss(ti);
		wait_for_period(ti);
	}
}




//VECCHIO MODEL WRITER


void *model_writer(void *arg){
	int ti = get_task_index(arg);
	uint pulseCount_local = 0;
	uint pCount;
	int16_t pulseBuf[TOTFRAMES];
	short modelBuf[TOTFRAMES];
	FILE * model;
	model = fopen("model.bin", "w+b");
	set_activation(ti);
	int read, written;


	while(1){
		pthread_mutex_lock(&countMux);
		pCount = pulseCount;
		pthread_mutex_unlock(&countMux);

		if(pCount > pulseCount_local){
			
			get_pulse_Buffers(pulseBuf, NULL);
		
			clear();
			
			read = fread(modelBuf, 2, TOTFRAMES, model);
		
			float p, m, count;

			for(int i = 0; i < (TOTFRAMES/CYCLES); i++){
				gotoxy(i,40);
				printf("-");
				gotoxy(i, 40 + (pulseBuf[i]>>11) );
				printf("•");
			}

			if(pulseCount_local != 0){
				for (int i = 0; i < TOTFRAMES; i++){
					p = (float) pulseBuf[i];
					m = (float) modelBuf[i];
					count = (float)pulseCount_local;
					
					m += (p/count);
					m = (m * count / (count + 1.0));
					modelBuf[i] = FLOAT_TO_INT(m);
					//modelBuf[i] += (pulseBuf[i] / pulseCount_local);
					//modelBuf[i] = ((modelBuf[i] * pulseCount_local) /(pulseCount_local + 1));
				}
				freopen("model.bin", "w+b", model);

				written = fwrite(modelBuf, 2, TOTFRAMES, model);
			} else written = fwrite(pulseBuf, 2, TOTFRAMES, model);

			for(int i = 0; i < (TOTFRAMES/CYCLES); i++){
				gotoxy(i,18);
				printf("-");
				gotoxy(i, 18 + (modelBuf[i]>>11) );
				printf("•");
			}

			gotoxy(0,55);
			pulseCount_local++;

			printf("Written %d samples. Total pulse count: %d\n", written, pulseCount_local);

		}
		wait_for_period(ti);
	}

}
