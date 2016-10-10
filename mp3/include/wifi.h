#ifndef _WIFI_H
#define _WIFI_H


//Set-Functions
void wifi_setSSID(char* ssid, int len);
void wifi_setPass(char* pass, int len);

//Get-Functions
char* wifi_getSSID();
char* wifi_getPass();

#endif

