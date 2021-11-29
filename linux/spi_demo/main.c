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
#include <armbianio.h>

#include "../../src/TwoBitDisplay.cpp"

volatile int iStop = 0;

#define DISPLAY_TYPE LCD_UC1617S_128128
//#define DISPLAY_TYPE LCD_UC1617S_12896
#define FLIP180 0
#define INVERT 0
#define LED_PIN 32
#define DC_PIN 22
#define CS_PIN -1
#define RESET_PIN 13
#define SPEED 4000000
#define USEBITBANG 0
#define WIDTH 128
#define HEIGHT 128

TBDISP tbd;
static uint8_t ucBuffer[4096];

void my_handler(int signal)
{
   iStop = 1;
} /* my_handler() */

int main(int argc, const char *argv[])
{
int i;
struct sigaction sigIntHandler;
//size_t ret;

// Set CTRL-C signal handler
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  i = AIOInitBoard("Raspberry Pi");
  if (i == 0) // problem
  {
     printf("Error in AIOInit(); check if this board is supported\n");
     return 0;
  }

    tbdSPIInit(&tbd, DISPLAY_TYPE, DC_PIN, CS_PIN, RESET_PIN, -1, -1, LED_PIN, FLIP180, INVERT, USEBITBANG, SPEED);
    tbdSetBackBuffer(&tbd, ucBuffer);
    tbdBacklight(&tbd, 1);
    // Create some simple content
    tbdFill(&tbd, 0, 1);
    tbdWriteString(&tbd,0,0,"TwoBitDisplay", FONT_8x8, 3, 0, 1);
    usleep(2000000);
    while (!iStop) {
       tbdFill(&tbd, 0, 1);
       for (int i=0; i<WIDTH; i++) {
          tbdDrawLine(&tbd, i, 0, WIDTH-1-i, HEIGHT-1, 3, 1);
       }
       for (int i=0; i<HEIGHT; i++) {
          tbdDrawLine(&tbd, WIDTH-1, i, 0, HEIGHT-1-i, 1, 1);
       }
    }
//    obdEllipse(&obd, 320, 240+64, 150,100, 0, 1);
//    obdRectangle(&obd, 300, 240+32, 340, 240+96, 1, 1);
    tbdFill(&tbd, 0, 1);
    AIOShutdown();
    return 0;
} /* main() */
