/**********************************************************************************
*	This code is just a copy from following file:
*
* 		Copyright 2013-2015 Espressif Systems
*
* 		FileName: user_main.c
*
* 		Description: Main routines for MP3 decoder.
*
* 		Modification history:
*     			2015/06/01, v1.0 File created.
*
*	GitHub-Repository: https://github.com/espressif/ESP8266_MP3_DECODER.git
*
*	The code was copied in a new file, to sort the code a little bit.
*
*
***********************************************************************************/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "../mad/mad.h"
#include "../mad/stream.h"
#include "../mad/frame.h"
#include "../mad/synth.h"
#include "i2s_freertos.h"

#include "playerconfig.h"

//The mp3 read buffer size. 2106 bytes should be enough for up to 48KHz mp3s according to the sox sources. Used by libmad.
#define READBUFSZ (2106)
static char readBuf[READBUFSZ]; 



//Reformat the 16-bit mono sample to a format we can send to I2S.
static int sampToI2s(short s) {
	//We can send a 32-bit sample to the I2S subsystem and the DAC will neatly split it up in 2
	//16-bit analog values, one for left and one for right.

	//Duplicate 16-bit sample to both the L and R channel
	int samp=s;
	samp=(samp)&0xffff;
	samp=(samp<<16)|samp;
	return samp;
}


//Calculate the number of samples that we add or delete. Added samples means a slightly lower
//playback rate, deleted samples means we increase playout speed a bit. This returns an
//8.24 fixed-point number
int recalcAddDelSamp(int oldVal) {
	int ret;
	long prevUdr=0;
	static int cnt;
	int i;
	static int minFifoFill=0;

	i=spiRamFifoFill();
	if (i<minFifoFill) minFifoFill=i;

	//Do the rest of the calculations plusminus every 100mS (assuming a sample rate of 44KHz)
	cnt++;
	if (cnt<1500) return oldVal;
	cnt=0;

	if (spiRamFifoLen()<10*1024) {
		//The FIFO is very small. We can't do calculations on how much it's filled on average, so another
		//algorithm is called for.
		int tgt=1600; //we want an average of this amount of bytes as the average minimum buffer fill
		//Calculate underruns this cycle
		int udr=spiRamGetUnderrunCt()-prevUdr;
		//If we have underruns, the minimum buffer fill has been lower than 0.
		if (udr!=0) minFifoFill=-1;
		//If we're below our target decrease playback speed, and vice-versa.
		ret=oldVal+((minFifoFill-tgt)*ADD_DEL_BUFFPERSAMP_NOSPIRAM);
		prevUdr+=udr;
		minFifoFill=9999;
	} else {
		//We have a larger FIFO; we can adjust according to the FIFO fill rate.
		int tgt=spiRamFifoLen()/2;
		ret=(spiRamFifoFill()-tgt)*ADD_DEL_BUFFPERSAMP;
	}
	return ret;
}



//This routine is called by the NXP modifications of libmad. It passes us (for the mono synth)
//32 16-bit samples.
void render_sample_block(short *short_sample_buff, int no_samples) {
	//Signed 16.16 fixed point number: the amount of samples we need to add or delete
	//in every 32-sample 
	static int sampAddDel=0;
	//Remainder of sampAddDel cumulatives
	static int sampErr=0;
	int i;
	int samp;

#ifdef ADD_DEL_SAMPLES
	sampAddDel=recalcAddDelSamp(sampAddDel);
#endif


	sampErr+=sampAddDel;
	for (i=0; i<no_samples; i++) {
#if defined(PWM_HACK)
		samp=sampToI2sPwm(short_sample_buff[i]);
#elif defined(DELTA_SIGMA_HACK)
		samp=sampToI2sDeltaSigma(short_sample_buff[i]);
#else
		samp=sampToI2s(short_sample_buff[i]);
#endif
		//Dependent on the amount of buffer we have too much or too little, we're going to add or remove
		//samples. This basically does error diffusion on the sample added or removed.
		if (sampErr>(1<<24)) {
			sampErr-=(1<<24);
			//...and don't output an i2s sample
		} else if (sampErr<-(1<<24)) {
			sampErr+=(1<<24);
			//..and output 2 samples instead of one.
			i2sPushSample(samp);
			i2sPushSample(samp);
		} else {
			//Just output the sample.
			i2sPushSample(samp);
		}
	}
}

//Called by the NXP modificationss of libmad. Sets the needed output sample rate.
static oldRate=0;
void ICACHE_FLASH_ATTR set_dac_sample_rate(int rate) {
	if (rate==oldRate) return;
	oldRate=rate;
	printf("Rate %d\n", rate);

#ifdef ALLOW_VARY_SAMPLE_BITS
	i2sSetRate(rate, 0);
#else
	i2sSetRate(rate, 1);
#endif
}

static enum  mad_flow ICACHE_FLASH_ATTR input(struct mad_stream *stream) {
	int n, i;
	int rem, fifoLen;
	//Shift remaining contents of buf to the front
	rem=stream->bufend-stream->next_frame;
	memmove(readBuf, stream->next_frame, rem);

	while (rem<sizeof(readBuf)) {
		n=(sizeof(readBuf)-rem); 	//Calculate amount of bytes we need to fill buffer.
		i=spiRamFifoFill();
		if (i<n) n=i; 				//If the fifo can give us less, only take that amount
		if (n==0) {					//Can't take anything?
			//Wait until there is enough data in the buffer. This only happens when the data feed 
			//rate is too low, and shouldn't normally be needed!
//			printf("Buf uflow, need %d bytes.\n", sizeof(readBuf)-rem);

			//We both silence the output as well as wait a while by pushing silent samples into the i2s system.
			//This waits for about 200mS
			for (n=0; n<441*2; n++) i2sPushSample(0);
		} else {
			//Read some bytes from the FIFO to re-fill the buffer.
			spiRamFifoRead(&readBuf[rem], n);
			rem+=n;
		}
	}

	//Okay, let MAD decode the buffer.
	mad_stream_buffer(stream, readBuf, sizeof(readBuf));
	return MAD_FLOW_CONTINUE;
}

//Routine to print out an error
static enum mad_flow ICACHE_FLASH_ATTR error(void *data, struct mad_stream *stream, struct mad_frame *frame) {
	printf("Dec ERR 0x%04x (%s)\n", stream->error, mad_stream_errorstr(stream));
	return MAD_FLOW_CONTINUE;
}


//This is the main mp3 decoding task. It will grab data from the input buffer FIFO in the SPI ram and
//output it to the I2S port.
void ICACHE_FLASH_ATTR tskmad(void *pvParameters) {
	int r;
	struct mad_stream *stream;
	struct mad_frame *frame;
	struct mad_synth *synth;

	//Allocate structs needed for mp3 decoding
	stream=malloc(sizeof(struct mad_stream));
	frame=malloc(sizeof(struct mad_frame));
	synth=malloc(sizeof(struct mad_synth));

	if (stream==NULL) { printf("MAD_ERR: malloc(stream)\n"); return; }
	if (synth==NULL) { printf("MAD_ERR: malloc(synth)\n"); return; }
	if (frame==NULL) { printf("MAD_ERR: malloc(frame)\n"); return; }

	//Initialize I2S
	i2sInit();

	printf("MAD: start\n");
	//Initialize mp3 parts
	mad_stream_init(stream);
	mad_frame_init(frame);
	mad_synth_init(synth);
	while(1) {


		input(stream); //calls mad_stream_buffer internally
		while(1) {
			r=mad_frame_decode(frame, stream);
			if (r==-1) {
	 			if (!MAD_RECOVERABLE(stream->error)) {
					//We're most likely out of buffer and need to call input() again
					break;
				}
				error(NULL, stream, frame);

			}
			mad_synth_frame(synth, frame);

		}

	}

	vTaskDelete(NULL);

}
