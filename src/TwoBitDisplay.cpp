//
// OneBitDisplay (OLED+LCD library)
// Copyright (c) 2020 BitBank Software, Inc.
// Written by Larry Bank (bitbank@pobox.com)
// Project started 3/23/2020
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifdef _LINUX_
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#define OUTPUT GPIO_OUT
#define INPUT GPIO_IN
#define INPUT_PULLUP GPIO_IN_PULLUP
#define HIGH 1
#define LOW 0
#else // Arduino

#include <Arduino.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif
#include <SPI.h>

#endif // _LINUX_
#include "TwoBitDisplay.h"
// All of the drawing code is in here
#include "tbd.inl"

// Initialization sequences

const unsigned char uc1617s_128128_initbuf[] PROGMEM = {
//      0x31, 0x00, // set APC command
      0x27, // temp compensation
      0x2b, // panel loading (13-18nf)
      0x2f, // internal pump control
      0xeb, // bias = 1/11
      0xa6, // non-inverted
      0xa4, // don't set all pixels on
      0x81, 0x2e, // contrast = 46/64
      0xf1, 0x7f, // set COM end
      0xf2, 0x00, // display line start
      0xf3, 0x7f, // display line end
      0xa3, // line rate
      0xd3, //
      0xd7, //
      0x8b, // auto increment
      0xc0, // LCD mapping
      0xaf // set display enable
};

const unsigned char uc1617s_12896_initbuf[] PROGMEM = {
//      0x31, 0x00, // set APC command
      0x27, // temp compensation
      0x2b, // panel loading (13-18nf)
      0x2f, // internal pump control
      0xea, // bias = 1/10
      0xa4, // don't set all pixels on
      0x81,
      0x68, // set PM=12, vop=12.8v, 4c
      0xa9, // set linerate mux, a2
      0xc8,
      0x0b,
      0x8b, // auto increment
      0xc0, // MY=0, MX=0
      0xf1, 0x5f, // set COM end
      0xd3, // gray shade set
      0xd7, // gray shade set
      0xaf // set display enable
};

static void tbdCachedFlush(TBDISP *pTBD, int bRender);
static void tbdCachedWrite(TBDISP *pTBD, uint8_t *pData, uint8_t u8Len, int bRender);
void tbdSetPosition(TBDISP *pTBD, int x, int y, int bRender);
void tbdWriteCommand(TBDISP *pTBD, unsigned char c);
void tbdWriteDataBlock(TBDISP *pTBD, unsigned char *ucBuf, int iLen, int bRender);
//static void SPI_BitBang(TBDISP *pTBD, uint8_t *pData, int iLen, uint8_t iMOSIPin, uint8_t iSCKPin);

#ifdef _LINUX_
void delay(int iMS)
{
	usleep(iMS * 1000);
} /* delay() */
void delayMicroseconds(int iUS)
{
	usleep(iUS);
} /* delayMicroseconds() */
#endif // _LINUX_
//
// Draw the contents of a memory buffer onto a display
// The sub-window will be clipped if it specifies too large an area
// for the destination display. The source TBDISP structure must have
// a valid back buffer defined
// The top and bottom destination edges will be drawn on byte boundaries (8 rows)
// The source top/bot edges can be on pixel boundaries
// This can be used for partial screen updates
//
void tbdDumpWindow(TBDISP *pTBDSrc, TBDISP *pTBDDest, int srcx, int srcy, int destx, int desty, int width, int height)
{
uint8_t *s,ucTemp[32]; // temp buffer to gather source pixels
int x, y, tx, i;
int iPitch;

   if (pTBDSrc == NULL || pTBDDest == NULL || pTBDSrc->ucScreen == NULL)
      return; // invalid pointers
   if (width > pTBDDest->width)
      width = pTBDDest->width;
   if (height > pTBDDest->height)
      height = pTBDDest->height;
   iPitch = pTBDSrc->width;
   for (y=0; y<height; y+=8)
   {
      tbdSetPosition(pTBDDest, destx, (desty+y)/8, 1);
      for (x=0; x<width; x+=32)
      {
         tx = 32;
         if (width-x < 32) tx = width-x;
         s = &pTBDSrc->ucScreen[((srcy+y)/8)*iPitch + srcx+x];
	 if (srcy & 7) // need to shift the bits to get 8 rows of src data
         {
            uint8_t uc, ucShift = srcy & 7;
            for (i=0; i<tx; i++)
            { // combine current and next line to capture 8 pixels
               uc = s[0] >> ucShift;
               uc |= s[iPitch] << (7-ucShift);
               ucTemp[i] = uc;
            }
            tbdCachedWrite(pTBDDest, ucTemp, tx, 1);
         }
         else
         { // simpler case
            tbdCachedWrite(pTBDDest, s, tx, 1); // just copy it
         }
      } // for x
   } // for y
   tbdCachedFlush(pTBDDest, 1);
} /* tbdDumpWindow() */

//
// Turn the display on or off
//
void tbdPower(TBDISP *pTBD, int bOn)
{
uint8_t ucCMD;

    ucCMD = (bOn) ? 0xaf : 0xae;
    tbdWriteCommand(pTBD, ucCMD);
} /* tbdPower() */

// Controls the LED backlight
void tbdBacklight(TBDISP *pTBD, int bOn)
{
  if (pTBD->iLEDPin != 0xff)
  {
     digitalWrite(pTBD->iLEDPin, (bOn) ? HIGH:LOW);
  }
} /* tbdBacklight() */

//
// Initialize the display controller on an SPI bus
//
void tbdSPIInit(TBDISP *pTBD, int iType, int iDC, int iCS, int iReset, int iMOSI, int iCLK, int iLED, int bFlip, int bInvert, int bBitBang, int32_t iSpeed)
{
uint8_t uc[32], *s=NULL;
int iLen=0;

  pTBD->ucScreen = NULL; // start with no backbuffer; user must provide one later
  pTBD->iDCPin = iDC;
  pTBD->iCSPin = iCS;
  pTBD->iMOSIPin = iMOSI;
  pTBD->iCLKPin = iCLK;
  pTBD->iLEDPin = iLED;
  pTBD->type = iType;
  pTBD->flip = bFlip;
  pTBD->invert = bInvert;
  pTBD->wrap = 0; // default - disable text wrap
  pTBD->com_mode = COM_SPI; // communication mode
  if (pTBD->iDCPin != 0xff) // Note - not needed on Sharp Memory LCDs
    pinMode(pTBD->iDCPin, OUTPUT);
  pinMode(pTBD->iCSPin, OUTPUT);
  digitalWrite(pTBD->iCSPin, (pTBD->type < SHARP_144x168)); // set to not-active
  if (bBitBang)
  {
      pinMode(iMOSI, OUTPUT);
      pinMode(iCLK, OUTPUT);
  }

  // Reset it
  if (iReset != -1)
  {
    pinMode(iReset, OUTPUT);
    digitalWrite(iReset, LOW);
    delay(50);
    digitalWrite(iReset, HIGH);
    delay(50);
  }
  if (iLED != -1)
  {
      pinMode(iLED, OUTPUT);
  }
// Initialize SPI
    if (!bBitBang) {
        pTBD->iMOSIPin = 0xff; // mark it as hardware SPI
#ifdef _LINUX_
	pTBD->bbi2c.file_i2c = AIOOpenSPI(SPI_BUS_NUMBER, iSpeed);
#else
        SPI.begin();
        SPI.beginTransaction(SPISettings(iSpeed, MSBFIRST, SPI_MODE0));
#endif
	//  SPI.setClockDivider(16);
        //  SPI.setBitOrder(MSBFIRST);
        //  SPI.setDataMode(SPI_MODE0);
    }

  pTBD->width = 128; // assume 128x128
  pTBD->height = 128;
  if (iType == LCD_UC1617S_128128)
  {
	s = (uint8_t *)uc1617s_128128_initbuf;
	iLen = sizeof(uc1617s_128128_initbuf);
  }
  else if (iType == LCD_UC1617S_12896)
  {
	pTBD->width = 96;
	s = (uint8_t *)uc1617s_12896_initbuf;
	iLen = sizeof(uc1617s_12896_initbuf);
  }
      memcpy_P(uc, s, iLen); // do it from RAM
      _I2CWrite(pTBD, uc, iLen);

      if (bInvert)
      {
	  tbdWriteCommand(pTBD, 0xa7);
      }
      if (bFlip) // rotate display 180
      {
	  tbdWriteCommand(pTBD, 0xa0);
	  tbdWriteCommand(pTBD, 0xc0);
	  pTBD->flip = 1;
      }
} /* tbdSPIInit() */
//
// Set the memory configuration to display the pixels at 0 or 180 degrees (flipped)
// Pass true (1) to flip 180, false (0) to set to 0
//
void tbdSetFlip(TBDISP *pTBD, int iOnOff)
{
   if (pTBD == NULL) return;
   pTBD->flip = iOnOff;
   if (iOnOff) // rotate display 180
   {
      tbdWriteCommand(pTBD, 0xc6); // mirror X+Y
   } else { // non-rotated
      tbdWriteCommand(pTBD, 0xc0); // no mirror
   }
} /* tbdSetFlip() */

//
// Initializes the OLED controller into "page mode"
//
int tbdI2CInit(TBDISP *pTBD, int iType, int iAddr, int bFlip, int bInvert, int bWire, int sda, int scl, int reset, int32_t iSpeed)
{
unsigned char uc[32];
uint8_t u8Len, *s;
int rc = OLED_NOT_FOUND;

  pTBD->ucScreen = NULL; // reset backbuffer; user must provide one later
  pTBD->type = iType;
  pTBD->flip = bFlip;
  pTBD->invert = bInvert;
  pTBD->wrap = 0; // default - disable text wrap
  pTBD->bbi2c.iSDA = sda;
  pTBD->bbi2c.iSCL = scl;
  pTBD->bbi2c.bWire = bWire;
  pTBD->com_mode = COM_I2C; // communication mode
#ifdef _LINUX_
  if (bWire == 1)
      pTBD->bbi2c.iBus = scl; // on Linux set I2C bus number in the SCL var
#endif // _LINUX_
  I2CInit(&pTBD->bbi2c, iSpeed); // on Linux, SDA = bus number, SCL = device address
  // Reset it
  if (reset != -1)
  {
    pinMode(reset, OUTPUT);
    digitalWrite(reset, HIGH);
    delay(50);
    digitalWrite(reset, LOW);
    delay(50);
    digitalWrite(reset, HIGH);
    delay(100);
  }
  if (iType == LCD_UC1617S_128128 || iType == LCD_UC1617S_12896) { // special case for this device
    //128x128 I2C LCD
    pTBD->oled_addr = iAddr;
//      uc[0] = 0xe2; // reset
//      _I2CWrite(pTBD, uc, 1);
//      delay(150);
      if (iType == LCD_UC1617S_128128) {
          s = (uint8_t *)uc1617s_128128_initbuf;
          u8Len = sizeof(uc1617s_128128_initbuf);
          pTBD->width = 128;
          pTBD->height = 128;
      } else {
          s = (uint8_t *)uc1617s_12896_initbuf;
          u8Len = sizeof(uc1617s_12896_initbuf);
          pTBD->width = 96;
          pTBD->height = 128;
      }
    _I2CWrite(pTBD, s, u8Len);
      if (bFlip) // rotate display 180
      {
        uc[0] = 0xc6; // LCD mapping command
        _I2CWrite(pTBD,uc, 1);
      }
      if (bInvert)
      {
        uc[0] = 0xa7; // invert command
        _I2CWrite(pTBD, uc, 1);
      }
    return LCD_OK;
  }
  return rc;
} /* tbdInit() */
//
// Sends a command to turn on or off the OLED display
//
void oledPower(TBDISP *pTBD, uint8_t bOn)
{
    if (bOn)
      tbdWriteCommand(pTBD, 0xaf); // turn on OLED
    else
      tbdWriteCommand(pTBD, 0xae); // turn off OLED
} /* oledPower() */

//
// Bit Bang the data on GPIO pins
//
void SPI_BitBang(TBDISP *pTBD, uint8_t *pData, int iLen, uint8_t iMOSIPin, uint8_t iSCKPin)
{
int i;
uint8_t c;
// We can access the GPIO ports much quicker on AVR by directly manipulating
// the port registers
#ifdef __AVR__
volatile uint8_t *outSCK, *outMOSI; // port registers for fast I/O
uint8_t port, bitSCK, bitMOSI; // bit mask for the chosen pins

    port = digitalPinToPort(iMOSIPin);
    outMOSI = portOutputRegister(port);
    bitMOSI = digitalPinToBitMask(iMOSIPin);
    port = digitalPinToPort(iSCKPin);
    outSCK = portOutputRegister(port);
    bitSCK = digitalPinToBitMask(iSCKPin);

#endif

   while (iLen)
   {
      c = *pData++;
      if (pTBD->iDCPin == 0xff) // 3-wire SPI, write D/C bit first
      {
#ifdef __AVR__
          if (pTBD->mode == MODE_DATA)
             *outMOSI |= bitMOSI;
          else
             *outMOSI &= ~bitMOSI;
          *outSCK |= bitSCK; // toggle clock
          *outSCK &= ~bitSCK; // no delay needed on SPI devices since AVR is slow
#else
          digitalWrite(iMOSIPin, (pTBD->mode == MODE_DATA));
          digitalWrite(iSCKPin, HIGH);
          delayMicroseconds(0);
          digitalWrite(iSCKPin, LOW);
#endif
      }
      if (c == 0 || c == 0xff) // quicker for all bits equal
      {
#ifdef __AVR__
         if (c & 1)
            *outMOSI |= bitMOSI;
         else
            *outMOSI &= ~bitMOSI;
         for (i=0; i<8; i++)
         {
            *outSCK |= bitSCK;
            *outSCK &= ~bitSCK;
         }
#else
         digitalWrite(iMOSIPin, (c & 1));
         for (i=0; i<8; i++)
         {
            digitalWrite(iSCKPin, HIGH);
            delayMicroseconds(0);
            digitalWrite(iSCKPin, LOW);
         }
#endif
      }
      else
      {
         for (i=0; i<8; i++)
         {
#ifdef __AVR__
            if (c & 0x80) // MSB first
               *outMOSI |= bitMOSI;
            else
               *outMOSI &= ~bitMOSI;
            *outSCK |= bitSCK;
            c <<= 1;
            *outSCK &= ~bitSCK;
#else
            digitalWrite(iMOSIPin,  (c & 0x80) != 0); // MSB first
            digitalWrite(iSCKPin, HIGH);
            c <<= 1;
            delayMicroseconds(0);
            digitalWrite(iSCKPin, LOW);
#endif
        }
      }
      iLen--;
   }
} /* SPI_BitBang() */

// Sets the D/C pin to data or command mode
void tbdSetDCMode(TBDISP *pTBD, int iMode)
{
  if (pTBD->iDCPin == 0xff) // 9-bit SPI
      pTBD->mode = (uint8_t)iMode;
  else // set the GPIO line
      digitalWrite(pTBD->iDCPin, (iMode == MODE_DATA));
} /* tbdSetDCMode() */

static void tbdWriteCommand2(TBDISP *pTBD, unsigned char c, unsigned char d)
{
unsigned char buf[4];

    if (pTBD->com_mode == COM_I2C) {// I2C device
        buf[1] = c;
        buf[2] = d;
        _I2CWrite(pTBD, buf, 2);
    } else { // must be SPI
        tbdWriteCommand(pTBD, c);
        tbdWriteCommand(pTBD, d);
    }
} /* tbdWriteCommand2() */

//
// Sets the brightness (0=off, 255=brightest)
//
void tbdSetContrast(TBDISP *pTBD, unsigned char ucContrast)
{
   tbdWriteCommand2(pTBD, 0x81, ucContrast);
} /* tbdSetContrast() */

//
// Dump a screen's worth of data directly to the display
// Try to speed it up by comparing the new bytes with the existing buffer
//
void tbdDumpBuffer(TBDISP *pTBD, uint8_t *pBuffer)
{
int x, y, iPitch;
int iLines, iCols;
//uint8_t bNeedPos;
uint8_t *pSrc = pTBD->ucScreen;
    
  iPitch = pTBD->width;
  if (pTBD->type == LCD_VIRTUAL) // wrong function for this type of display
    return;
  if (pBuffer == NULL) // dump the internal buffer if none is given
    pBuffer = pTBD->ucScreen;
  if (pBuffer == NULL)
    return; // no backbuffer and no provided buffer
  
  iLines = pTBD->height >> 2;
  iCols = pTBD->width >> 4;
  for (y=0; y<iLines; y++)
  {
    for (x=0; x<iCols; x++) // wiring library has a 32-byte buffer, so send 16 bytes so that the data prefix (0x40) can fit
    {
      if (pTBD->ucScreen == NULL || pBuffer == pSrc || memcmp(pSrc, pBuffer, 16) != 0) // doesn't match, need to send it
      {
        tbdSetPosition(pTBD, x*16, y, 1);
        tbdWriteDataBlock(pTBD, pBuffer, 16, 1);
      }
      pSrc += 16;
      pBuffer += 16;
    } // for x
    pSrc += (iPitch - pTBD->width); // for narrow displays, skip to the next line
    pBuffer += (iPitch - pTBD->width);
  } // for y
} /* tbdDumpBuffer() */

