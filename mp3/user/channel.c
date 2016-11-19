#include "esp_common.h"
#include "playerconfig.h"


//Channel-valriables
static int streamCounter=0;
static int currentStream=0;


//Setup stream-settings
#define MAX_ADC 1024
#define OVERLAPP_ADC 5


//GetIP-Address
char **streamPath = NULL;
char **streamHost = NULL;
int *streamPort = NULL;
int streamCount = 0;

//Define Unicode-Lookup-table
const char UnicodeLookupInt[16][16] = {{'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0x00 - 0x0F*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0x10 - 0x1F*/ \
				 {' ' ,'!' ,'\"','#' ,'$' ,'%' ,'&' ,'\'','(' ,')' ,'*' ,'+' ,',' ,'-' ,'.' ,'/' }, /*0x20 - 0x2F*/ \
				 {'0' ,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,':' ,';' ,'<' ,'=' ,'>' ,'?' }, /*0x30 - 0x3F*/ \
				 {'@' ,'A' ,'B' ,'C' ,'D' ,'E' ,'F' ,'G' ,'H' ,'I' ,'J' ,'K' ,'L' ,'M' ,'N' ,'O' }, /*0x40 - 0x4F*/ \
				 {'P' ,'Q' ,'R' ,'S' ,'T' ,'U' ,'V' ,'W' ,'X' ,'Y' ,'Z' ,'[' ,'\\',']' ,'^' , '_'}, /*0x50 - 0x5F*/ \
				 {'`' ,'a' ,'b' ,'c' ,'d' ,'e' ,'f' ,'g' ,'h' ,'i' ,'j' ,'k' ,'l' ,'m' ,'n' ,'o' }, /*0x60 - 0x6F*/ \
				 {'p' ,'q' ,'r' ,'s' ,'t' ,'u' ,'v' ,'w' ,'x' ,'y' ,'z' ,'{' ,'|' ,'}' ,'~' ,'\0'}, /*0x70 - 0x7F*/ \
				 {'`' ,'\0',',' ,'\0','"' ,'\0','\0','\0','^' ,'\0','\0','\0','\0','\0','\0','\0'}, /*0x80 - 0x8F*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0x90 - 0x9F*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0xA0 - 0xAF*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0xB0 - 0xBF*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0xC0 - 0xCF*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0xD0 - 0xDF*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}, /*0xE0 - 0xEF*/ \
				 {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}} /*0xF0 - 0xFF*/ ;


//copy string from with converting from unicode...
void copyFromUnicode(char* src, char* dst, int len){

	int s;
	int d;

	for(s=0; s < len; s++){
		if(src[s] == '%'){
			if(len-s > 2){
				uint8 first = (uint8)src[s+1];
				uint8 second = (uint8)src[s+2];

				bool invalid = false;

				if(first >= 48  &&  first <= 57){
					first -= 48;	
				}
				else if(first >= 65  &&  first <= 70){
					first -= 55;
				}
				else{
					invalid = true;
				}


				if(second >= 48  &&  second <= 57){
					second -= 48;	
				}
				else if(second >= 65  &&  second <= 70){
					second -= 55;
				}
				else{
					invalid = true;
				}

				if(invalid == false){
					dst[d++] = UnicodeLookupInt[first][second];
					s+=2;
					continue;
				}
			}
		}
		
		dst[d++] = src[s];

	}
	
	dst[d++] = '\0';

}

//Get stream-channel from ADC
void channel_read() {

	uint16 adc = system_adc_read();
	printf("adc %d\n", adc);
	uint16 limitMax = (1+currentStream) * MAX_ADC / streamCount + OVERLAPP_ADC;
	uint16 limitMin = (currentStream * MAX_ADC / streamCount< OVERLAPP_ADC)? (0):(currentStream * MAX_ADC / streamCount- OVERLAPP_ADC);

	if(limitMax > MAX_ADC)
		limitMax = MAX_ADC-OVERLAPP_ADC;

	if(adc < limitMin  ||  adc > limitMax)	{
		streamCounter = adc / (MAX_ADC / streamCount);
	}

	printf("adc - new stream = %d\n", streamCounter);

}


//Change Channel
int channel_getStreamCounter(){	//Stream read from ADC
	return streamCounter;
}

void channel_setCurrentStream(int id){	//select a new Stream
	currentStream=id;
}

int channel_getCurrentStream(){	//get selected Stream
	return currentStream;
}


//Change amount of channels
int channel_setStreamCount(int count){
	
	printf("channel - SetStream-Count, count=%d\n", count);
	printf("channel - StreamCount_old = %d\n", streamCount);
	int i = 0;
	
	if(count <= 0  ||  count > 99)
		return -1;

	//Free buffer
	if(streamPath != NULL){
		for(i = 0; i < streamCount; i++){
			if(streamPath[i] != NULL) free(streamPath[i]);
			streamPath[i] = NULL;
		}
		free(streamPath);
		streamPath = NULL;
	}

	if(streamHost != NULL){
		for(i = 0; i < streamCount; i++){
			if(streamHost[i] != NULL) free(streamHost[i]);
			streamHost[i] = NULL;
		}
		free(streamHost);
		streamHost = NULL;
	}

	if(streamPort != NULL){
		free(streamPort);
		streamPort = NULL;
	}


	streamCount = count;

	//allocate buffer
	streamPath = malloc(streamCount * sizeof(char*));
	streamHost = malloc(streamCount * sizeof(char*));
	streamPort = malloc(streamCount * sizeof(int));

	for(i = 0; i < streamCount; i++){
		streamHost[i] = NULL;
		streamPath[i] = NULL;
	}

	return 0;
}



//Change channel-data
int channel_setStreamPort(int i, int port){

	printf("channel - SetStream-Port %d, port = %d\n", i, port);
	
	if(i >= streamCount)
		return -1;

	if(streamPort == NULL)
		return -1;

	streamPort[i] = port;

	return 0;

}


int channel_setStreamPath(int id, char* path, int len){

	printf("channel - SetStream-Path, i=%d, len=%d\n", id, len);

	if(id >= streamCount)
		return -1;

	if(streamPath == NULL)
		return -1;

	if(streamPath[id] != NULL)
		free(streamPath[id]);
	streamPath[id] = NULL;

	streamPath[id] = malloc(len + 1);
	copyFromUnicode(path, streamPath[id], len);


}

int channel_setStreamHost(int id, char* host, int len){

	printf("channel - SetStream-Host\n");

	if(id >= streamCount)
		return -1;

	if(streamHost == NULL)
		return -1;

	if(streamHost[id] != NULL)
		free(streamHost[id]);
	streamHost[id] = NULL;

	streamHost[id] = malloc(len + 1);
	copyFromUnicode(host, streamHost[id], len);

}




//Get Channel-datas
int channel_getStreamCount(){
	return streamCount;
}


char * channel_getStreamHost(int id){
	if(streamHost == NULL)
		return "";

	if(id >= streamCount)
		return "";

	if(streamHost[id] == NULL)
		return "";

	return streamHost[id];
}


int channel_getStreamPort(int id){

	if(streamPort == NULL)
		return -1;

	if(id >= streamCount)
		return -1;

	return streamPort[id];
}


char * channel_getStreamPath(int id){

	if(streamPath == NULL)
		return "";

	if(id >= streamCount)
		return "";

	if(streamPath[id] == NULL)
		return "";

	return streamPath[id];
}

