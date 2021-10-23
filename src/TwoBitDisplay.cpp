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

#else // Arduino

#include <Arduino.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif
#include <SPI.h>

#endif // _LINUX_
#include <TwoBitDisplay.h>
// All of the drawing code is in here
#include "tbd.inl"

// Initialization sequences
const unsigned char oled128_initbuf[] PROGMEM = {0x00, 0xae,0xdc,0x00,0x81,0x40,
      0xa1,0xc8,0xa8,0x7f,0xd5,0x50,0xd9,0x22,0xdb,0x35,0xb0,0xda,0x12,
      0xa4,0xa6,0xaf};

const unsigned char uc1617s_initbuf[] PROGMEM = {
//      0x31, 0x00, // set APC command
      0x27, // temp compensation
      0x2b, // panel loading (13-18nf)
      0x2f, // internal pump control
      0xeb, // bias = 1/11
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

const unsigned char oled64x128_initbuf[] PROGMEM ={
0x00, 0xae, 0xd5, 0x51, 0x20, 0xa8, 0x3f, 0xdc, 0x00, 0xd3, 0x60, 0xad, 0x80, 0xa6, 0xa4, 0xa0, 0xc0, 0x81, 0x40, 0xd9, 0x22, 0xdb, 0x35, 0xaf
};

const unsigned char oled64_initbuf[] PROGMEM ={0x00,0xae,0xa8,0x3f,0xd3,0x00,0x40,0xa1,0xc8,
      0xda,0x12,0x81,0xff,0xa4,0xa6,0xd5,0x80,0x8d,0x14,
      0xaf,0x20,0x02};

const unsigned char oled32_initbuf[] PROGMEM  = {
0x00,0xae,0xd5,0x80,0xa8,0x1f,0xd3,0x00,0x40,0x8d,0x14,0xa1,0xc8,0xda,0x02,
0x81,0x7f,0xd9,0xf1,0xdb,0x40,0xa4,0xa6,0xaf};

const unsigned char oled72_initbuf[] PROGMEM ={0x00,0xae,0xa8,0x3f,0xd3,0x00,0x40,0xa1,0xc8,
      0xda,0x12,0x81,0xff,0xad,0x30,0xd9,0xf1,0xa4,0xa6,0xd5,0x80,0x8d,0x14,
      0xaf,0x20,0x02};

const unsigned char uc1701_initbuf[] PROGMEM  = {0xe2, 0x40, 0xa0, 0xc8, 0xa2, 0x2c, 0x2e, 0x2f, 0xf8, 0x00, 0x23, 0x81, 0x28, 0xac, 0x00, 0xa6};

const unsigned char hx1230_initbuf[] PROGMEM  = {0x2f, 0x90, 0xa6, 0xa4, 0xaf, 0x40, 0xb0, 0x10, 0x00};
const unsigned char nokia5110_initbuf[] PROGMEM  = {0x21, 0xa4, 0xb1, 0x04,0x14,0x20,0x0c};

static void tbdCachedFlush(TBDISP *pTBD, int bRender);
static void tbdCachedWrite(TBDISP *pTBD, uint8_t *pData, uint8_t u8Len, int bRender);
void tbdSetPosition(TBDISP *pTBD, int x, int y, int bRender);
void tbdWriteCommand(TBDISP *pTBD, unsigned char c);
void tbdWriteDataBlock(TBDISP *pTBD, unsigned char *ucBuf, int iLen, int bRender);
//static void SPI_BitBang(TBDISP *pTBD, uint8_t *pData, int iLen, uint8_t iMOSIPin, uint8_t iSCKPin);

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
// Write a single line to a Sharp memory LCD
// You must provide the exact number of bytes needed for a complete line
// e.g. for the 144x168 display, pSrc must provide 144 pixels (18 bytes) 
//
void tbdWriteLCDLine(TBDISP *pTBD, uint8_t *pSrc, int iLine)
{
    int x;
    uint8_t c, ucInvert, *d, ucStart;
    uint8_t ucLineBuf[54]; // 400 pixels is max supported width = 50 bytes + 4
    int iPitch = pTBD->width / 8;
    static int iVCOM = 0;

//    if (pTBD == NULL || pSrc == NULL || pTBD->type < SHARP_144x168)
//        return; // invalid request
    if (iLine < 0 || iLine >= pTBD->height)
        return;
    
      ucInvert = (pTBD->invert) ? 0x00 : 0xff;
      digitalWrite(pTBD->iCSPin, HIGH); // active high

      ucStart = 0x80; // write command
      iVCOM++;
      if (iVCOM & 0x100) // flip it every 256 lines
        ucStart |= 0x40; // VCOM bit
      ucLineBuf[1] = ucStart;
      // this code assumes I2C, so the first byte is ignored
      _I2CWrite(pTBD, ucLineBuf, 2); // write command(01) + vcom(02)

     d = &ucLineBuf[2];
     ucLineBuf[1] = pgm_read_byte(&ucMirror[iLine+1]); // current line number
     for (x=0; x<iPitch; x++)
     {
         c = pSrc[0] ^ ucInvert; // we need to brute-force invert it
         *d++ = pgm_read_byte(&ucMirror[c]);
         pSrc++;
     } // for x
    // write this line to the display
    ucLineBuf[iPitch+2] = 0; // end of line
    _I2CWrite(pTBD, ucLineBuf, iPitch+3);
    ucLineBuf[1] = 0;
    _I2CWrite(pTBD, ucLineBuf, 2); // final transfer
    digitalWrite(pTBD->iCSPin, LOW); // de-activate
} /* tbdWriteLCDLine() */

//
// Turn the display on or off
//
void tbdPower(TBDISP *pTBD, int bOn)
{
uint8_t ucCMD;

  if (pTBD->type == LCD_NOKIA5110)
    ucCMD = (bOn) ? 0x20 : 0x24;
  else // all other supported displays
    ucCMD = (bOn) ? 0xaf : 0xae;
  tbdWriteCommand(pTBD, ucCMD);
} /* tbdPower() */
#if !defined( _LINUX_ )

// Controls the LED backlight
void tbdBacklight(TBDISP *pTBD, int bOn)
{
  if (pTBD->iLEDPin != 0xff)
  {
     digitalWrite(pTBD->iLEDPin, (bOn) ? HIGH:LOW);
  }
} /* tbdBacklight() */

//
// Send the command sequence to power up the LCDs
static void LCDPowerUp(TBDISP *pTBD)
{
    int iLen;
    uint8_t *s, uc[32];
    tbdSetDCMode(pTBD, MODE_COMMAND);
    digitalWrite(pTBD->iCSPin, LOW);
    if (pTBD->type == LCD_UC1701 || pTBD->type == LCD_UC1609)
    {
        s = (uint8_t *)uc1701_initbuf;
        iLen = sizeof(uc1701_initbuf);
    }
    else if (pTBD->type == LCD_HX1230)
    {
        s = (uint8_t *)hx1230_initbuf;
        iLen = sizeof(hx1230_initbuf);
    }
    else // Nokia 5110
    {
        s = (uint8_t *)nokia5110_initbuf;
        iLen = sizeof(nokia5110_initbuf);
    }
    memcpy_P(uc, s, iLen);
    if (pTBD->iMOSIPin == 0xff)
       SPI.transfer(s, iLen);
    else
       SPI_BitBang(pTBD, s, iLen, pTBD->iMOSIPin, pTBD->iCLKPin);
    delay(100);
    tbdWriteCommand(pTBD, 0xa5);
    delay(100);
    tbdWriteCommand(pTBD, 0xa4);
    tbdWriteCommand(pTBD, 0xaf);
    digitalWrite(pTBD->iCSPin, HIGH);
    tbdSetDCMode(pTBD, MODE_DATA);
} /* LCDPowerUp() */

//
// Initialize the display controller on an SPI bus
//
void tbdSPIInit(TBDISP *pTBD, int iType, int iDC, int iCS, int iReset, int iMOSI, int iCLK, int iLED, int bFlip, int bInvert, int bBitBang, int32_t iSpeed)
{
uint8_t uc[32], *s;
int iLen;

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
        SPI.begin();
        SPI.beginTransaction(SPISettings(iSpeed, MSBFIRST, SPI_MODE0));
        //  SPI.setClockDivider(16);
        //  SPI.setBitOrder(MSBFIRST);
        //  SPI.setDataMode(SPI_MODE0);
    }

  pTBD->width = 128; // assume 128x64
  pTBD->height = 64;
  if (iType == SHARP_144x168)
  {
      pTBD->width = 144;
      pTBD->height = 168;
      pTBD->iDCPin = 0xff; // no D/C wire on this display
  }
  else if (iType == SHARP_400x240)
  {
      pTBD->width = 400;
      pTBD->height = 240;
      pTBD->iDCPin = 0xff; // no D/C wire on this display
  }
  else if (iType == LCD_UC1609)
  {
      pTBD->width = 192;
      pTBD->height = 64;
  }
  else if (iType == LCD_HX1230)
  {
      pTBD->width = 96;
      pTBD->height = 68;
      pTBD->iDCPin = 0xff; // flag this as being 3-wire SPI
  }
  else if (iType == LCD_NOKIA5110)
  {
      pTBD->width = 84;
      pTBD->height = 48;
  }
  else if (iType == OLED_96x16)
  {
    pTBD->width = 96;
    pTBD->height = 16;
  }
  else if (iType == OLED_64x128)
  {
    pTBD->width = 64;
    pTBD->height = 128;
  }
  else if (iType == OLED_128x32)
    pTBD->height = 32;
  else if (iType == OLED_128x128)
    pTBD->height = 128;
  else if (iType == OLED_64x32)
  {
    pTBD->width = 64;
    pTBD->height = 32;
  }
  else if (iType == OLED_72x40)
  {
    pTBD->width = 72;
    pTBD->height = 40;
  }
  if (iType == OLED_128x32 || iType == OLED_96x16)
  {
     s = (uint8_t *)oled32_initbuf;
     iLen = sizeof(oled32_initbuf);
  }
  else if (iType == OLED_64x128)
  {
     s = (uint8_t *)oled64x128_initbuf;
     iLen = sizeof(oled64x128_initbuf);
  }
  else if (iType == OLED_128x128)
  {
     s = (uint8_t *)oled128_initbuf;
     iLen = sizeof(oled128_initbuf);
  }
  else if (iType < LCD_UC1701)
  {
     s = (uint8_t *)oled64_initbuf;
     iLen = sizeof(oled64_initbuf);
  }
    // OLED
  if (iType < LCD_UC1701)
  {
      memcpy_P(uc, s, iLen); // do it from RAM
      _I2CWrite(pTBD, s, iLen);

      if (bInvert)
      {
        uc[0] = 0; // command
        uc[1] = 0xa7; // invert command
        _I2CWrite(pTBD, uc, 2);
      }
      if (bFlip) // rotate display 180
      {
        uc[0] = 0; // command
        uc[1] = 0xa0;
        _I2CWrite(pTBD, uc, 2);
        uc[0] = 0;
        uc[1] = 0xc0;
        _I2CWrite(pTBD, uc, 2);
      }
  } // OLED
  if (iType == LCD_UC1701 || iType == LCD_HX1230)
  {
      uint8_t cCOM = 0xc0;
      
      LCDPowerUp(pTBD);
      if (iType == LCD_HX1230)
      {
          tbdSetContrast(pTBD, 0); // contrast of 0 looks good
          cCOM = 0xc8;
      }
      if (bFlip) // flip horizontal + vertical
      {
         tbdWriteCommand(pTBD, 0xa1); // set SEG direction (A1 to flip horizontal)
         tbdWriteCommand(pTBD, cCOM); // set COM direction (C0 to flip vert)
      }
      if (bInvert)
      {
         tbdWriteCommand(pTBD, 0xa7); // set inverted pixel mode
      }
  }
  if (iType == LCD_UC1609)
  {
      tbdWriteCommand(pTBD, 0xe2); // system reset
      tbdWriteCommand(pTBD, 0xa0); // set frame rate to 76fps
      tbdWriteCommand(pTBD, 0xeb); // set BR
      tbdWriteCommand(pTBD, 0x2f); // set Power Control
      tbdWriteCommand(pTBD, 0xc4); // set LCD mapping control
      tbdWriteCommand(pTBD, 0x81); // set PM
      tbdWriteCommand(pTBD, 0x90); // set contrast to 144
      tbdWriteCommand(pTBD, 0xaf); // display enable
      if (bFlip) // flip horizontal + vertical
      {  
         tbdWriteCommand(pTBD, 0xa1); // set SEG direction (A1 to flip horizontal)
         tbdWriteCommand(pTBD, 0xc2); // set COM direction (C0 to flip vert)
      }
      if (bInvert)
      {
         tbdWriteCommand(pTBD, 0xa7); // set inverted pixel mode
      }

  }
} /* tbdSPIInit() */
#endif
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

  I2CInit(&pTBD->bbi2c, iSpeed); // on Linux, SDA = bus number, SCL = device address
#ifdef _LINUX_
  pTBD->oled_addr = (uint8_t)scl;
#else
  // Reset it
  if (reset != -1)
  {
    pinMode(reset, OUTPUT);
    digitalWrite(reset, HIGH);
    delay(50);
    digitalWrite(reset, LOW);
    delay(50);
    digitalWrite(reset, HIGH);
    delay(10);
  }
  if (iType == LCD_UC1617S) { // special case for this device
    //128x128 I2C LCD
    pTBD->oled_addr = iAddr;
    s = (uint8_t *)uc1617s_initbuf;
    u8Len = sizeof(uc1617s_initbuf);
    _I2CWrite(pTBD, s, u8Len);
    pTBD->width = 128;
    pTBD->height = 128;
    return LCD_OK;
  }
  // find the device address if requested
  if (iAddr == -1 || iAddr == 0 || iAddr == 0xff) // find it
  {
    I2CTest(&pTBD->bbi2c, 0x3c);
    if (I2CTest(&pTBD->bbi2c, 0x3c))
       pTBD->oled_addr = 0x3c;
    else if (I2CTest(&pTBD->bbi2c, 0x3d))
       pTBD->oled_addr = 0x3d;
    else
       return rc; // no display found!
  }
  else
  {
    pTBD->oled_addr = iAddr;
    I2CTest(&pTBD->bbi2c, iAddr);
    if (!I2CTest(&pTBD->bbi2c, iAddr))
       return rc; // no display found
  }
#endif
  // Detect the display controller (SSD1306, SH1107 or SH1106)
  uint8_t u = 0;
  I2CReadRegister(&pTBD->bbi2c, pTBD->oled_addr, 0x00, &u, 1); // read the status register
  u &= 0x0f; // mask off power on/off bit
  if ((u == 0x7 || u == 0xf) && pTBD->type == OLED_128x128) // SH1107
  { // A single SSD1306 display returned 7, so only act on it if the
    // user specified that they're working with a 128x128 display
    rc = OLED_SH1107_3C;
    bFlip = !bFlip; // SH1107 seems to have this reversed from the usual direction
  }
  else if (u == 0x8) // SH1106
  {
    rc = OLED_SH1106_3C;
    pTBD->type = OLED_132x64; // needs to be treated a little differently
  }
  else if (u == 3 || u == 6 || u == 7) // 7=128x64(rare),6=128x64 display, 3=smaller
  {
    rc = OLED_SSD1306_3C;
  }
  if (pTBD->oled_addr == 0x3d)
     rc++; // return the '3D' version of the type

  if (iType == OLED_128x32 || iType == OLED_96x16)
  {
      s = (uint8_t *)oled32_initbuf;
      u8Len = sizeof(oled32_initbuf);
  }
  else if (iType == OLED_128x128)
  {
      s = (uint8_t *)oled128_initbuf;
      u8Len = sizeof(oled128_initbuf);
  }
  else if (iType == OLED_72x40)
  {
      s = (uint8_t *)oled72_initbuf;
      u8Len = sizeof(oled72_initbuf);
  }
  else if (iType == OLED_64x128)
  {
      s = (uint8_t *)oled64x128_initbuf;
      u8Len = sizeof(oled64x128_initbuf);
  }
  else // 132x64, 128x64 and 64x32
  {
      s = (uint8_t *)oled64_initbuf;
      u8Len = sizeof(oled64_initbuf);
  }

    memcpy_P(uc, s, u8Len);
  _I2CWrite(pTBD, uc, u8Len);
  if (bInvert)
  {
    uc[0] = 0; // command
    uc[1] = 0xa7; // invert command
    _I2CWrite(pTBD,uc, 2);
  }
  if (bFlip) // rotate display 180
  {
    uc[0] = 0; // command
    uc[1] = 0xa0;
    _I2CWrite(pTBD,uc, 2);
    uc[1] = 0xc0;
    _I2CWrite(pTBD,uc, 2);
  }
  pTBD->width = 128; // assume 128x64
  pTBD->height = 64;
  if (iType == OLED_96x16)
  {
    pTBD->width = 96;
    pTBD->height = 16;
  }
  else if (iType == OLED_64x128)
  {
    pTBD->width = 64;
    pTBD->height = 128;
  }
  else if (iType == OLED_128x32)
    pTBD->height = 32;
  else if (iType == OLED_128x128)
    pTBD->height = 128;
  else if (iType == OLED_64x32)
  {
    pTBD->width = 64;
    pTBD->height = 32;
  }
  else if (iType == OLED_72x40)
  {
    pTBD->width = 72;
    pTBD->height = 40;
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
        buf[0] = 0x00;
        buf[1] = c;
        buf[2] = d;
        _I2CWrite(pTBD, buf, 3);
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
  if (pTBD->type == LCD_HX1230)
  { // valid values are 0-31, so scale it
      ucContrast >>= 3;
      tbdWriteCommand(pTBD, 0x80 + ucContrast);
  }
  else if (pTBD->type == LCD_NOKIA5110)
  {
      // we allow values of 0xb0-0xbf, so shrink the range
      ucContrast >>= 4;
      tbdWriteCommand(pTBD, 0x21); // set advanced command mode
      tbdWriteCommand(pTBD, 0xb0 | ucContrast);
      tbdWriteCommand(pTBD, 0x20); // set simple command mode
  }
  else // OLEDs + UC1701
      tbdWriteCommand2(pTBD, 0x81, ucContrast);
} /* tbdSetContrast() */

//
// Special case for Sharp Memory LCD
//
static void SharpDumpBuffer(TBDISP *pTBD, uint8_t *pBuffer)
{
int x, y;
uint8_t c, ucInvert, *s, *d, ucStart;
uint8_t ucLineBuf[56];
int iPitch = pTBD->width / 8;
static uint8_t ucVCOM = 0;
int iBit;
uint8_t ucMask;

  ucInvert = (pTBD->invert) ? 0x00 : 0xff;
  digitalWrite(pTBD->iCSPin, HIGH); // active high
 
    ucLineBuf[0] = 0;
  ucStart = 0x80; // write command
  if (ucVCOM)
    ucStart |= 0x40; // VCOM bit
  ucLineBuf[1] = ucStart;
  // this code assumes I2C, so the first byte is ignored
  _I2CWrite(pTBD, ucLineBuf, 2); // write command(01) + vcom(02)
  ucVCOM = !ucVCOM; // need to toggle this each transaction

 // We need to flip and invert the image in code because the Sharp memory LCD
 // controller only has the simplest of commands for data writing
  if (pTBD->flip)
  {
     for (y=0; y<pTBD->height; y++) // we have to write the memory in the wrong direction
     {  
         ucMask = 0x80 >> (y & 7);
        s = &pBuffer[pTBD->width - 1 + (pTBD->width * ((pTBD->height - 1 - y) >> 3))]; // point to last line first
        d = &ucLineBuf[2];
        ucLineBuf[1] = pgm_read_byte(&ucMirror[y+1]); // current line number
        for (x=0; x<pTBD->width/8; x++)
        {  
           c = ucInvert; // we need to brute-force invert it
            for (iBit=7; iBit>=0; iBit--)
            {
                if (s[0] & ucMask)
                    c ^= (1 << iBit);
                s--;
            }
           *d++ = c;
        } // for y
        // write this line to the display
        ucLineBuf[iPitch+2] = 0; // end of line
        _I2CWrite(pTBD, ucLineBuf, iPitch+3);
     } // for x
  }
  else // normal orientation
  {
     for (y=0; y<pTBD->height; y++) // we have to write the memory in the wrong direction
     {
        ucMask = 1 << (y & 7);
        s = &pBuffer[pTBD->width * (y >> 3)]; // point to last line first
        d = &ucLineBuf[2];
        
        ucLineBuf[1] = pgm_read_byte(&ucMirror[y+1]); // current line number
        for (x=0; x<pTBD->width/8; x++)
        {
            c = ucInvert;
            for (iBit=7; iBit>=0; iBit--)
            {
                if (s[0] & ucMask)
                    c ^= (1 << iBit);
                s++;
            }
           *d++ = c;
        } // for y
        // write this line to the display
        ucLineBuf[iPitch+2] = 0; // end of line
        _I2CWrite(pTBD, ucLineBuf, iPitch+3);
     } // for x
  }
  ucLineBuf[1] = 0;
  _I2CWrite(pTBD, ucLineBuf, 2); // final transfer
  digitalWrite(pTBD->iCSPin, LOW); // de-activate
} /* SharpDumpBuffer() */
//
// Dump a screen's worth of data directly to the display
// Try to speed it up by comparing the new bytes with the existing buffer
//
void tbdDumpBuffer(TBDISP *pTBD, uint8_t *pBuffer)
{
int x, y, iPitch;
int iLines, iCols;
uint8_t bNeedPos;
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

