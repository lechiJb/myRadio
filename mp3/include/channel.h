#ifndef _CHANNEL_H_
#define _CHANNEL_H_


//Copy function
void copyFromUnicode(char* src, char* dest, int len);


//Select channel functions
void channel_read();						//Read stream-ID from ADC

int channel_getStreamCounter();					//Get last read stream-ID
int channel_getCurrentStream();					//Get current selected stream-ID
int channel_setCurrentStream(int id);				//Set current selected stream-ID


//Get Functions
int channel_setStreamCount(int count);				//Set amount of stream-channels
int channel_setStreamPort(int i, int port);			//Set port of stream
int channel_setStreamPath(int id, char* path, int len);		//Set Path of stream
int channel_setStreamHost(int id, char* host, int len);		//Set Host of stream

//Set Functions
int channel_getStreamCount();					//Get amount of Stream-channels
char * channel_getStreamHost(int id);				//Get port of stream
int channel_getStreamPort(int id);				//Get Path of stream
char * channel_getStreamPath(int id);				//Get Host of stream

#endif
