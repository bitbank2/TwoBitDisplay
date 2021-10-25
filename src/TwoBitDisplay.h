#ifndef __TWOBITDISPLAY__
#define __TWOBITDISPLAY__

#include <BitBang_I2C.h>

// Proportional font data taken from Adafruit_GFX library
/// Font data stored PER GLYPH
#if !defined( _ADAFRUIT_GFX_H ) && !defined( _GFXFONT_H_ )
#define _GFXFONT_H_
typedef struct {
  uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
  uint8_t width;         ///< Bitmap dimensions in pixels
  uint8_t height;        ///< Bitmap dimensions in pixels
  uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
  int8_t xOffset;        ///< X dist from cursor pos to UL corner
  int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
  uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
  GFXglyph *glyph;  ///< Glyph array
  uint8_t first;    ///< ASCII extents (first char)
  uint8_t last;     ///< ASCII extents (last char)
  uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;
#endif // _ADAFRUIT_GFX_H

typedef struct tbdstruct
{
uint8_t oled_addr; // requested address or 0xff for automatic detection
uint8_t wrap, flip, invert, type;
uint8_t *ucScreen;
int iCursorX, iCursorY;
int width, height;
int iScreenOffset;
BBI2C bbi2c;
uint8_t com_mode; // communication mode (I2C / SPI)
uint8_t mode; // data/command mode for 9-bit SPI
uint8_t iDCPin, iMOSIPin, iCLKPin, iCSPin;
uint8_t iLEDPin; // backlight
uint8_t bBitBang;
} TBDISP;

typedef char * (*SIMPLECALLBACK)(int iMenuItem);

// Make the Linux library interface C instead of C++
#if defined(_LINUX_) && defined(__cplusplus)
extern "C" {
#endif

#if !defined(BITBANK_LCD_MODES)
#define BITBANK_LCD_MODES
typedef enum
{
 MODE_DATA = 0,
 MODE_COMMAND
} DC_MODE;
#endif

typedef enum
{
  COM_I2C = 0,
  COM_SPI,
} COM_MODE;

typedef enum
{
  ROT_0 = 0,
  ROT_90,
  ROT_180,
  ROT_270
} FONT_ROTATION;

// These are defined the same in my SPI_LCD library
#ifndef SPI_LCD_H

// 5 possible font sizes: 8x8, 16x32, 6x8, 12x16 (stretched from 6x8 with smoothing), 16x16 (stretched from 8x8)
enum {
   FONT_6x8 = 0,
   FONT_8x8,
   FONT_12x16,
   FONT_16x16,
   FONT_16x32,
   FONT_COUNT
};
// For backwards compatibility, keep the old names valid
#define FONT_NORMAL FONT_8x8
#define FONT_SMALL FONT_6x8
#define FONT_STRETCHED FONT_16x16
#define FONT_LARGE FONT_16x32
#endif

// Display type for init function
enum {
  OLED_128x128 = 1,
  OLED_128x32,
  OLED_128x64,
  OLED_132x64,
  OLED_64x128,
  OLED_64x32,
  OLED_96x16,
  OLED_72x40,
  LCD_UC1617S_128128,
  LCD_UC1617S_12896,
  LCD_UC1701,
  LCD_UC1609,
  LCD_HX1230,
  LCD_NOKIA5110,
  LCD_VIRTUAL,
  SHARP_144x168,
  SHARP_400x240
};

// Rotation and flip angles to draw tiles
enum {
  ANGLE_0=0,
  ANGLE_90,
  ANGLE_180,
  ANGLE_270,
  ANGLE_FLIPX,
  ANGLE_FLIPY
};

// Return value from tbd tbdI2CInit()
enum {
  OLED_NOT_FOUND = -1, // no display found
  OLED_SSD1306_3C,  // SSD1306 found at 0x3C
  OLED_SSD1306_3D,  // SSD1306 found at 0x3D
  OLED_SH1106_3C,   // SH1106 found at 0x3C
  OLED_SH1106_3D,   // SH1106 found at 0x3D
  OLED_SH1107_3C,  // SH1107
  OLED_SH1107_3D,
  LCD_OK,
  LCD_ERROR
};
//
// Create a virtual display of any size
// The memory buffer must be provided at the time of creation
//
void tbdCreateVirtualDisplay(TBDISP *pTBD, int width, int height, uint8_t *buffer);
// Constants for the tbdCopy() function
// Output format options -
#define TBD_LSB_FIRST     0x001
#define TBD_MSB_FIRST     0x002
#define TBD_VERT_BYTES    0x004
#define TBD_HORZ_BYTES    0x008
// Orientation options -
#define TBD_ROTATE_90     0x010
#define TBD_FLIP_VERT     0x020
#define TBD_FLIP_HORZ     0x040
#define TBD_INVERT        0x080
// Copy the current bitmap buffer from its native form (LSB_FIRST, VERTICAL_BYTES) to the requested form
// A copy of the same format will just do a memcpy
int tbdCopy(TBDISP *pTBD, int iFlags, uint8_t *pDestination);
//
// Draw the contents of a memory buffer onto a display
// The sub-window will be clipped if it specifies too large an area
// for the destination display. The source TBDISP structure must have
// a valid back buffer defined
// The top and bottom destination edges will be drawn on byte boundaries (8 rows)
// The source top/bot edges can be on pixel boundaries
//
void tbdDumpWindow(TBDISP *pTBDSrc, TBDISP *pTBDDest, int srcx, int srcy, int destx, int desty, int width, int height);
//
// Write a single line to a Sharp memory LCD
// You must provide the exact number of bytes needed for a complete line
// e.g. for the 144x168 display, pSrc must provide 144 pixels (18 bytes)
//
void tbdWriteLCDLine(TBDISP *pTBD, uint8_t *pSrc, int iLine);
//
// Initializes the display controller into "page mode" on I2C
// If SDAPin and SCLPin are not -1, then bit bang I2C on those pins
// Otherwise use the Wire library.
// If you don't need to use a separate reset pin, set it to -1
//
int tbdI2CInit(TBDISP *pTBD, int iType, int iAddr, int bFlip, int bInvert, int bWire, int iSDAPin, int iSCLPin, int iResetPin, int32_t iSpeed);
//
// Initialize an SPI version of the display
//
void tbdSPIInit(TBDISP *pTBD, int iType, int iDC, int iCS, int iReset, int iMOSI, int iCLK, int iLED, int bFlip, int bInvert, int iBitBang, int32_t iSpeed);

//
// Provide or revoke a back buffer for your OLED graphics
// This allows you to manage the RAM used by ss_oled on tiny
// embedded platforms like the ATmega series
// Pass NULL to revoke the buffer. Make sure you provide a buffer
// large enough for your display (e.g. 128x64 needs 1K - 1024 bytes)
//
void tbdSetBackBuffer(TBDISP *pTBD, uint8_t *pBuffer);
//
// Sets the brightness (0=off, 255=brightest)
//
void tbdSetContrast(TBDISP *pTBD, unsigned char ucContrast);
//
// Load a 1-bpp Windows bitmap
// Pass the pointer to the beginning of the BMP file
// First pass version assumes a full screen bitmap
//
int tbdLoadBMP(TBDISP *pTBD, uint8_t *pBMP, int x, int y, int bInvert);
//
// Power up/down the display
// useful for low power situations
//
void tbdPower(TBDISP *pTBD, int bOn);
//
// Set the current cursor position
// The column represents the pixel column (0-127)
// The row represents the text row (0-7)
//
void tbdSetCursor(TBDISP *pTBD, int x, int y);

//
// Turn text wrap on or off for the tbdWriteString() function
//
void tbdSetTextWrap(TBDISP *pTBD, int bWrap);
//
// Draw a string of normal (8x8), small (6x8) or large (16x32) characters
// At the given col+row with the given scroll offset. The scroll offset allows you to
// horizontally scroll text which does not fit on the width of the display. The offset
// represents the pixels to skip when drawing the text. An offset of 0 starts at the beginning
// of the text.
// The system remembers where the last text was written (the cursor position)
// To continue writing from the last position, set the x,y values to -1
// The text can optionally wrap around to the next line by calling oledSetTextWrap(true);
// otherwise text which would go off the right edge will not be drawn and the cursor will
// be left "off screen" until set to a new position explicitly
//
//  Returns 0 for success, -1 for invalid parameter
//
int tbdWriteString(TBDISP *pTBD, int x, int y, char *szMsg, int iSize, int iFGColor, int iBGColor, int bRender);
//
// Draw a string with a fractional scale in both dimensions
// the scale is a 16-bit integer with and 8-bit fraction and 8-bit mantissa
// To draw at 1x scale, set the scale factor to 256. To draw at 2x, use 512
// The output must be drawn into a memory buffer, not directly to the display
// The string can be drawn in one of 4 rotations (ROT_0, ROT_90, ROT_180, ROT_270)
//
int tbdScaledString(TBDISP *pTBD, int x, int y, char *szMsg, int iSize, int ucColor, int iXScale, int iYScale, int iRotation);
//
// Draw a string in a proportional font you supply
// Requires a back buffer
//
int tbdWriteStringCustom(TBDISP *pTBD, GFXfont *pFont, int x, int y, char *szMsg, uint8_t ucColor);
//
// Get the width of text in a custom font
//
void tbdGetStringBox(GFXfont *pFont, char *szMsg, int *width, int *top, int *bottom);
//
// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
//
void tbdFill(TBDISP *pTBD, unsigned char ucData, int bRender);
//
// Set (or clear) an individual pixel
// The local copy of the frame buffer is used to avoid
// reading data from the display controller
// (which isn't possible in most configurations)
// This function needs the USE_BACKBUFFER macro to be defined
// otherwise, new pixels will erase old pixels within the same byte
//
int tbdSetPixel(TBDISP *pTBD, int x, int y, unsigned char ucColor, int bRender);
//
// Dump an entire custom buffer to the display
// useful for custom animation effects
//
void tbdDumpBuffer(TBDISP *pTBD, uint8_t *pBuffer);
//
// Render a window of pixels from a provided buffer or the library's internal buffer
// to the display. The row values refer to byte rows, not pixel rows due to the memory
// layout of OLEDs. Pass a src pointer of NULL to use the internal backing buffer
// returns 0 for success, -1 for invalid parameter
//
int tbdDrawGFX(TBDISP *pTBD, uint8_t *pSrc, int iSrcCol, int iSrcRow, int iDestCol, int iDestRow, int iWidth, int iHeight, int iSrcPitch);

//
// Draw a line between 2 points
//
void tbdDrawLine(TBDISP *pTBD, int x1, int y1, int x2, int y2, uint8_t ucColor, int bRender);
//
// Play a frame of animation data
// The animation data is assumed to be encoded for a full frame of the display
// Given the pointer to the start of the compressed data,
// it returns the pointer to the start of the next frame
// Frame rate control is up to the calling program to manage
// When it finishes the last frame, it will start again from the beginning
//
uint8_t * tbdPlayAnimFrame(TBDISP *pTBD, uint8_t *pAnimation, uint8_t *pCurrent, int iLen);
void tbdWriteCommand(TBDISP *pTBD, unsigned char c);
void tbdSetPosition(TBDISP *pTBD, int x, int y, int bRender);
void tbdWriteDataBlock(TBDISP *pTBD, unsigned char *ucBuf, int iLen, int bRender);
//
// Scroll the internal buffer by 1 scanline (up/down)
// width is in pixels, lines is group of 8 rows
// Returns 0 for success, -1 for invalid parameter
//
int tbdScrollBuffer(TBDISP *pTBD, int iStartCol, int iEndCol, int iStartRow, int iEndRow, int bUp);
//
// Draw a sprite of any size in any position
// If it goes beyond the left/right or top/bottom edges
// it's trimmed to show the valid parts
// This function requires a back buffer to be defined
// The priority color (0 or 1) determines which color is painted 
// when a 1 is encountered in the source image.
// e.g. when 0, the input bitmap acts like a mask to clear
// the destination where bits are set.
//
void tbdDrawSprite(TBDISP *pTBD, uint8_t *pSprite, int cx, int cy, int iPitch, int x, int y, uint8_t iPriority);
//
// Draw a 16x16 tile in any of 4 rotated positions
// Assumes input image is laid out like "normal" graphics with
// the MSB on the left and 2 bytes per line
// On AVR, the source image is assumed to be in FLASH memory
// The function can draw the tile on byte boundaries, so the x value
// can be from 0 to 112 and y can be from 0 to 6
//
void tbdDrawTile(TBDISP *pTBD, const uint8_t *pTile, int x, int y, int iRotation, int bInvert, int bRender);
//
// Draw an outline or filled ellipse
//
void tbdEllipse(TBDISP *pTBD, int iCenterX, int iCenterY, int32_t iRadiusX, int32_t iRadiusY, uint8_t ucColor, uint8_t bFilled);
//
// Draw an outline or filled rectangle
//
void tbdRectangle(TBDISP *pTBD, int x1, int y1, int x2, int y2, uint8_t ucColor, uint8_t bFilled);

//
// Turn the LCD backlight on or off
//
void tbdBacklight(TBDISP *pODB, int bOn);

#if defined(_LINUX_) && defined(__cplusplus)
}
#endif // _LINUX_

#endif // __TWOBITDISPLAY__

