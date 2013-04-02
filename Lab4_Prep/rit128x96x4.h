//*****************************************************************************
//
// rit128x96x4.h - Prototypes for the driver for the RITEK 128x96x4 graphical
//                   OLED display.
//
// Copyright (c) 2007-2010 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 6075 of the EK-LM3S8962 Firmware Package.
//
// 
//*****************************************************************************
// Additional functions added by Jonathan Valvano 10/27/2010
// added a fixed point output, line plot, bar plot, and dBfs plot

//*****************************************************************************

#ifndef __RIT128X96X4_H__
#define __RIT128X96X4_H__

//*****************************************************************************
//
// Prototypes for the driver APIs.
//
//*****************************************************************************


/* OLED defines */
#define OLED_BUFFER 2
#define OLED_LINES 5
#define OLED_COLUMNS 21

/* OLED macros */
#define OLED_MIN(a, b) (a < b ? a : b)
#define _OLED_Rollback(c, loc) {\
															int i;\
															for(i = 0; i < OLED_LINES; i++) {\
																strcpy(c[i], c[i + 1]);\
																_OLED_Message(loc, i, clear, 0);\
																_OLED_Message(loc, i, c[i], OLED_Get_Color());\
															}\
													}

static enum
{
	TOP, BOTTOM, NUM_DEVICES
} OLED_ENUM;

void OLED_Init(unsigned char color);
void OLED_Set_Color(unsigned char color);
/*inline*/ unsigned char OLED_Get_Color(void);
void OLED_Out(int device, const char * string);
void OLED_Clear(int device);
void _OLED_Message(int device, unsigned int line, const char *string, unsigned char color);
static int _OLED_Find(const char* string, char c);

extern void RIT128x96x4Clear(void);
extern void RIT128x96x4StringDraw(const char *pcStr,
                                    unsigned long ulX,
                                    unsigned long ulY,
                                    unsigned char ucLevel);
extern void RIT128x96x4ImageDraw(const unsigned char *pucImage,
                                   unsigned long ulX,
                                   unsigned long ulY,
                                   unsigned long ulWidth,
                                   unsigned long ulHeight);
extern void RIT128x96x4Init(unsigned long ulFrequency);
extern void RIT128x96x4Enable(unsigned long ulFrequency);
extern void RIT128x96x4Disable(void);
extern void RIT128x96x4DisplayOn(void);
extern void RIT128x96x4DisplayOff(void);


// *************** RIT128x96x4PlotClear ********************
// Clear the graphics buffer, set X coordinate to 0
// It does not output to display until RIT128x96x4ShowPlot called
// Inputs: ymin and ymax are range of the plot
// four numbers are displayed along left edge of plot
// y0,y1,y2,y3, can be -9 to 99, any number outside this range is skipped
// y3 --          hash marks at number           Ymax
//     |
//    --          hash marks between numbers     Ymin+(5*Yrange)/6
//     |
// y2 --                                         Ymin+(4*Yrange)/6
//     |
//    --                                         Ymin+(3*Yrange)/6
//     |
// y1 --                                         Ymin+(2*Yrange)/6
//     |
//    --                                         Ymin+(1*Yrange)/6
//     |
// y0 --                                         Ymin
// Outputs: none
extern void RIT128x96x4PlotClear(long ymin, long ymax, long y0, long y1, long y2, long y3);
extern void RIT128x96x4PlotReClear(void);

extern void RIT128x96x4PlotPoint(long y);
extern void RIT128x96x4PlotBar(long y);
extern void RIT128x96x4PlotdBfs(long y);
extern void RIT128x96x4PlotNext(void);
extern void RIT128x96x4ShowPlot(void);
extern void RIT128x96x4DecOut2(unsigned long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void Int2Str2(long const n, char *string);
// Display a message on the oLED
// device is 0 for top, 1 for bottom
// line is 0 to 3
// string is an ASCII character string
// value is number to display 
// fix  0 is integer
//      1 is 0.1 fixed point
//      2 is 0.2 fixed point
extern void oLED_Message(int device, int line, char *string, long value, long value2, int fix);

#endif // __RIT128X96X4_H__
