#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "playerconfig.h"
#include "wifi.h"
#include "channel.h"


char* wifi_ssid = NULL;
char* wifi_pass = NULL;


void wifi_setSSID(char* ssid, int len){

	if(wifi_ssid != NULL)
		free(wifi_ssid);

	wifi_ssid = malloc(len+1);

	copyFromUnicode(ssid, wifi_ssid, len);

}


void wifi_setPass(char* pass, int len){

	//Todo: save password in an encrypted version.
	//	Password is writen to flah in blank-version at the moment

	if(wifi_pass != NULL)
		free(wifi_pass);

	wifi_pass = malloc(len+1);

	copyFromUnicode(pass, wifi_pass, len);

}


char* wifi_getSSID(){
	printf("wifi - SSID: %s\n", wifi_pass);
	return wifi_ssid;
}

char* wifi_getPass(){
	//printf("wifi - Pass: %s\n", wifi_pass);
	return wifi_pass;
}





