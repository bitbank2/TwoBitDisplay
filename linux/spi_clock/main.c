//
//  main.c
//  TwoBitDisplay library test program
//  Demonstrates many of the features for LCD displays
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include <time.h>
#include <armbianio.h>
#include <accel.h> // my accelerometer library
#include <curl/curl.h>
#include "../../src/TwoBitDisplay.cpp"
#include "Roboto_Black_50.h"

volatile int iStop = 0;

#define DISPLAY_TYPE LCD_UC1617S_128128
//#define DISPLAY_TYPE LCD_UC1617S_12896
#define FLIP180 0
#define INVERT 0
#define BITBANG 0
// LED on GPIO 12
#define LED_PIN 32
#define DC_PIN 22
// use automatic CS control on GPIO 8
#define CS_PIN -1
#define RESET_PIN 13
#define SPEED 4000000
// GPIO 17 is the user button
#define BUTTON0 11
TBDISP tbd;
static uint8_t ucBuffer[4096];
struct tm thetime;
int bFlip = 1; // keeps track of 0/180 degree display orientation
static int iAccelType=-1;
char szCurlBuffer[500];
int iCurlSize;
void my_handler(int signal)
{
   iStop = 1;
} /* my_handler() */

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	iCurlSize = (int)(size*nmemb) - 2; // remove the quotes
	memcpy(szCurlBuffer, &ptr[1], iCurlSize);
	szCurlBuffer[iCurlSize] = 0;
	return size*nmemb;
} /* write_callback() */

void UpdateWeather(void)
{
  CURL *curlHandler = curl_easy_init();
  if (curlHandler) {
      char errorBuffer[500];
      curl_easy_setopt(curlHandler, CURLOPT_URL, "https://wttr.in/?format=\"%C+%t+%h\"");
      curl_easy_setopt(curlHandler, CURLOPT_ERRORBUFFER, errorBuffer);
      curl_easy_setopt(curlHandler, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curlHandler, CURLOPT_WRITEDATA, szCurlBuffer);
      curl_easy_setopt(curlHandler, CURLOPT_FOLLOWLOCATION, 1L);
      /* Perform the request */ 
      CURLcode res = curl_easy_perform(curlHandler);
      /* Check for errors */ 
      if(res != CURLE_OK)
        fprintf(stderr, "CURL failed: %s\n",
                curl_easy_strerror(res));

      /* Clean up */ 
      curl_easy_cleanup(curlHandler);

  } else {
    printf("curl init failed!\n");
  }
} /* UpdateWeather() */

int main(int argc, const char *argv[])
{
int i;
int iLoops = 0;
struct sigaction sigIntHandler;
//size_t ret;

// Set CTRL-C signal handler
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

 
  curl_global_init(CURL_GLOBAL_DEFAULT);
  i = AIOInitBoard("Raspberry Pi");
  if (i == 0) // problem
  {
     printf("Error in AIOInit(); check if this board is supported\n");
     return 0;
  }
  AIOAddGPIO(BUTTON0, GPIO_IN_PULLUP);

  i = accelInit(1, 0x19, TYPE_LIS3DH);
  if (i != 0)
  {
	printf("Error initializing LIS3DH\n");
  } else {
	iAccelType = TYPE_LIS3DH;
	printf("Found LIS3DH!\n");
  }
  if (iAccelType == -1) { // try LSM6DS3
	  i = accelInit(1, 0x6b, TYPE_LSM6DS3);
	  if (i != 0) {
		printf("Error initializing LSM6DS3\n");
	  } else {
		iAccelType = TYPE_LSM6DS3;
		printf("Found LSM6DS3!\n");
	  }
  }
  if (iAccelType == -1) { // try MPU6050
	  i = accelInit(1, 0x68, TYPE_MPU6050);
	  if (i != 0) {
		  printf("Error iniitalizing MPU6050\n");
	  } else {
		  iAccelType = TYPE_MPU6050;
		  printf("Found MPU6050!\n");
	  }
  }

  UpdateWeather(); // get the internet weather info

//void obdSPIInit(OBDISP *pOBD, int iType, int iDC, int iCS, int iReset, int iMOSI, int iCLK, int iLED, int bFlip, int bInvert, int bBitBang, int32_t iSpeed)  
    tbdSPIInit(&tbd, DISPLAY_TYPE, DC_PIN, CS_PIN, RESET_PIN, -1, -1, LED_PIN, FLIP180, INVERT, BITBANG, SPEED);
    printf("width = %d, height = %d\n", tbd.width, tbd.height);
    tbdSetBackBuffer(&tbd, ucBuffer);
    if (LED_PIN != -1)
    	tbdBacklight(&tbd, 1);
    // Create some simple content
    tbdFill(&tbd, 0, 1);
    tbdSetFlip(&tbd, 1);
    memset(&thetime, 0, sizeof(struct tm));
    while (!iStop) {
       char szTemp[128];

       // Check for button press to shut down
       if (AIOReadGPIO(BUTTON0) == 0) { // user button pressed
          tbdFill(&tbd, 0, 1);
	  tbdWriteString(&tbd, 0,0,(char *)"Shutdown", FONT_12x16,3,0,1);
	  tbdWriteString(&tbd, 0,8,(char *)"seconds", FONT_12x16,3,0,1);
	  tbdWriteString(&tbd, 0,14,(char *)"Hold button to cancel", FONT_6x8,3,0,1);
	  for (i=30; i>0; i--) {
              sprintf(szTemp, "in %d  ",i);
	      tbdWriteString(&tbd,0,4,szTemp, FONT_12x16,3,0,1);
	      usleep(1000000);
	      if (i < 27 && AIOReadGPIO(BUTTON0) == 0) {
		      i = -1;
	      }
	  }
	  if (i == 0) { // user wants to shut down
             system("sudo shutdown now");
	  } else { // wait for button release
              while (AIOReadGPIO(BUTTON0) == 0) {
		  usleep(10000);
	      }
	  }
       }

       tbdFill(&tbd, 0, 0);
       {
	  struct tm *temptime;
	  time_t rawtime;
	  rawtime = time(NULL);
	  temptime = localtime(&rawtime);
	  memcpy(&thetime, temptime, sizeof(thetime));
       }
	iLoops++; // update the weather info every 5 minutes
	if (iLoops >= 5 * 60) {
           iLoops = 0;
	   UpdateWeather();
	}
       sprintf(szTemp, "%02d", thetime.tm_hour);
       tbdWriteStringCustom(&tbd, (GFXfont *)&Roboto_Black_50, 0, 36, szTemp,3);
       if (thetime.tm_sec & 1) // leave the colon on all the time
          tbdWriteStringCustom(&tbd, (GFXfont *)&Roboto_Black_50, 57, 31, (char *)":",3);
       sprintf(szTemp,"%02d", thetime.tm_min);
       tbdWriteStringCustom(&tbd, (GFXfont *)&Roboto_Black_50, 70, 36, szTemp, 3);

       { // display weather report info we get from wttr.in
           int i, iTemp=0, iHumidity=0;
	   for (i=0; i<iCurlSize; i++) {
              if (szCurlBuffer[i] == '+' || szCurlBuffer[i] == '-') {
		      szCurlBuffer[i-1] = 0; // terminate conditions string
		      iTemp = atoi(&szCurlBuffer[i]);
		      iHumidity = atoi(&szCurlBuffer[i+6]);
		      break;
	      }
	   }
	   if (iHumidity != 0) {
	      i = strlen(szCurlBuffer);
	      tbdWriteString(&tbd,0,14,szCurlBuffer, (i <= 16) ? FONT_8x8 : FONT_6x8,3,0,0);
	      sprintf(szTemp, "%dC %d%%", iTemp, iHumidity);
	      i = strlen(szTemp);
	      tbdWriteString(&tbd,(128-(i*12))/2,10,szTemp, FONT_12x16, 3,0,0);
	   }
       }
       if (iAccelType != -1) {
	  int X, Y, Z;
          accelReadAValues(&X, &Y, &Z);
	  if (Y > 8000 && bFlip != 0) {
		  tbdSetFlip(&tbd, 0);
		  bFlip = 0;
	  } else if (Y < -8000 && bFlip != 1) {
		  tbdSetFlip(&tbd, 1);
		  bFlip = 1;
	  }
       }
       tbdDumpBuffer(&tbd, NULL);
       usleep(1000000);
    }
    tbdFill(&tbd, 0, 1);
    AIOShutdown();
    curl_global_cleanup();
    return 0;
} /* main() */
