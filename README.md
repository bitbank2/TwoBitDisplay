TwoBitDisplay (2-bpp LCD library)<br>
---------------------------------
Project started 10/23/2021<br>
Copyright (c) 2021 BitBank Software, Inc.<br>
Written by Larry Bank<br>
bitbank@pobox.com<br>
<br>
![TwoBitDisplay](/demo.jpg?raw=true "TwoBitDisplay")
<br>
The purpose of this code is to easily control monochrome 2-bit per pixel LCD displays. The displays can be connected to the traditional I2C or SPI bus, or you can use GPIO pins to bit bang the signals.<br>
<br>
On AVR microcontrollers, there is an optimized option to speed up access to the GPIO pins to allow speeds which match or exceed normal I2C speeds. The pins are numbered with the Port letter as the first digit followed by the bit number. For example, To use bit 0 of Port B, you would reference pin number 0xb0.<br>
<br>

Features:<br>
---------<br>
- Supports any number of simultaneous displays of any type (mix and match)<br>
- Supports 128x128 and 96x128 (UC1617S) LCDs<br>
- Virtual displays of any size which can be drawn across multiple physical displays
- Drive displays from I2C, SPI or any GPIO pins (virtual I2C/SPI)<br>
- Includes 5 sizes of fixed fonts (6x8, 8x8, 12x16, 16x16, 16x32)<br>
- Text drawing at any fractional scale (e.g. 1.25x), and any of 4 directions/rotations<br>
- Can use Adafruit_GFX format bitmap fonts (proportional and fixed)<br>
- Deferred rendering allows preparing a back buffer, then displaying it (usually faster)<br>
- Text cursor position with optional line wrap<br>
- A function to load a Windows BMP file<br>
- Optimized Bresenham line drawing<br>
- Optimized Bresenham outline and filled ellipse drawing<br>
- Optimized outline and filled rectangle drawing<br>
- Optional backing RAM (needed for some text and drawing functions)<br>
- 16x16 Tile/Sprite drawing at any angle.<br>
<br>
This code depends on the BitBang_I2C library. You can download it here:<br>
https://github.com/bitbank2/BitBang_I2C
<br>
See the Wiki for help getting started<br>
https://github.com/bitbank2/TwoBitDisplay/wiki <br>
<br>

![Fonts](/fonts_opt.jpg?raw=true "fonts")
A few words about fonts<br>
-----------------------<br>

The library includes 3 fixed fonts (6x8, 8x8 and 16x32). The other 2 fonts offer 2x stretched versions (12x16 from 6x8 and 16x16 from 8x8). A simple smoothing algorithm is applied to the stretched 6x8 font to make it look better. In the photo above are the 8x8, 6x8 and 12x16 fonts. Only 96 ASCII characters are defined per font to save space. To use more elaborate fonts with more extensive character support, use Adafruit_GFX format bitmap fonts with the `tbdWriteStringCustom()` function.<br>

Instructions for use:<br>
---------------------<br>
Start by initializing the library. Either using hardware I2C, bit-banged I2C or SPI to talk to the display. The typical MCU only allows setting the I2C speed up to 400Khz, but these LCD displays can handle a much faster signal. With the bit-bang code, you can usually specify a stable 800Khz clock and with Cortex-M0 targets, the hardware I2C can be told to be almost any speed, but the displays I've tested tend to stop working beyond 1.6Mhz.<br>
<br>
After initializing the display you can begin drawing text or graphics on it. The final parameter of all of the drawing functions is a render flag. When true, the graphics will be sent to the internal backing buffer (when available) and sent to the display. You optionally pass the library a backing buffer (if your MCU has enough RAM) with the obdSetBackBuffer() function. When the render flag is false, the graphics will only be drawn into the internal buffer. Once you're ready to send the pixels to the display, call obdDumpBuffer(NULL) and it will copy the internal buffer in its entirety to the display.<br>
<br>

If you find this code useful, please consider sending a donation or becomming a Github sponsor.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SR4F44J2UR8S4)

