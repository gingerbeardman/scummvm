/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Based on the original sources
 *   Faery Tale II -- The Halls of the Dead
 *   (c) 1993-1996 The Wyrmkeep Entertainment Co.
 */

#include "saga2/std.h"
#include "saga2/errors.h"
#include "saga2/gdraw.h"

namespace Saga2 {

#define textStyleBar    (textStyleUnderBar|textStyleHiLiteBar)

#define TempAlloc       malloc
#define TempFree        free

/* ============================================================================ *
                            Text Blitting Routines
 * ============================================================================ */

/*  Notes:

    These functions attempt to render planar fonts onto a chunky bitmap.

    All non-pointer variables are 16 bits or less. (I don't know how
    big pointers need to be).
*/

/*  DrawChar: This function renders a single bitmapped character
    into an offscreen buffer.

    'drawchar' is the ascii code of the character to be drawn.

    'xpos' is the position in the buffer to draw the outline.

    'baseline' is the address of the first scanline in the buffer.
    (it can actually be any scanline in the buffer if we want to
    draw the character at a different y position).
    Note that this has nothing to do with the Amiga "BASELINE" field,
    this routine always renders relative to the top of the font.

    'color' is the pixel value to render into the buffer.

    Note that this code does unaligned int16 writes to byte addresses,
    and will not run on an 68000 or 68010 processor.
*/

#if 1
void DrawChar(gFont *font, int drawchar, int xpos, uint8 *baseline, uint8 color,
              uint16 destwidth) {
	short   w,
	        font_mod;               // width of font in bytes

	uint8   *src,                   // start of char data
	        *dst;                   // start of dest row

	/*
	    This code assumes that the font characters are all byte-aligned,
	    and that there is no extra junk after the character in the bits
	    that pad to the next byte. Also, the code currently makes no
	    provision for "LoChar" or "HiChar" (i.e. there are blank table
	    entries for characters 0-1f). This can be changed if desired.
	*/

	font_mod = font->rowMod;
	src = font->fontdata + font->charXOffset[drawchar];
	dst = baseline + xpos;

	for (w = font->charWidth[drawchar]; w > 0; w -= 8) {
		uint8   *src_row,
		        *dst_row;
		short   h;

		src_row = src;
		dst_row = dst;

		for (h = font->height; h > 0; h--) {
			uint8   *b;
			uint8   s;

			b = dst_row;

			for (s = *src_row; s != 0; s <<= 1) {
				if ((s & 0x80) != 0)
					*b = color;
				++b;
			}
			src_row += font_mod;
			dst_row += destwidth;
		}

		++src;
		dst += 8;
	}
}
#else
static void DrawChar(
    gFont           *font,                  // address of the font
    int             drawchar,               // character index to draw
    int             xpos,                   // x position to render to
    uint8           *baseline,              // base address of line
    gPen            color,                  // color to render
    uint16          destwidth) {            // destination width
	uint16          h,                      // font height counter
	                rowmod;                 // row modulus of font

	int16           charwidth;              // width of character in pixels

	uint8           *chardata,              // pointer to start of char data
	                *destcol;               // pointer to start of dest

	//  point to the first byte of the first scanline of the
	//  source char

	chardata = (uint8 *)(font + 1) + font->charXOffset[ drawchar ];

	//  get the width of the character in pixels

	charwidth = font->charWidth[ drawchar ];

	//  point to the first byte of where we want to place the character

	baseline += xpos;

	//  this loop goes once for each 8 pixels wide that the
	//  character is

	h = font->height;
	rowmod = font->rowMod;

	asm {
		push ds

		mov dx, charwidth   // dx is the char width tracking register
		or  dx, dx
		jmp endwhile
	}
whiletop:
	asm {
		// get the pointer to the source and destination columns.

		lds si, chardata
		les di, baseline
		mov [word ptr destcol], di
		mov [word ptr destcol+2], es

		// this loop goes once for each scanline of the character

		mov cx, h
		inc cx
		jmp heightend
	}
heighttop:
	asm {
		// get the font bit pattern for this scanline

		mov ah, ds:[si]
		xor al, al

		// point to the address of where to start rendering

		/* now, render the pixels. NOTE: Using Carry in assembly,
		    you would probably want to do the shift first and be
		    more efficient.
		*/

		// prepare our loop  render pixels one at a time...
		mov bl, color
		jmp innerbottom
	}
innertop:
	asm {
		add ax, ax
		jnc nostore

		mov es:[di], bl
	}
nostore:
	asm {
		inc di
	}
innerbottom:
	asm {
		or  ax, ax
		jnz innertop

		// increment source and dest pointers to next scanline

		add si, rowmod
		mov bx, [word ptr destcol]
		add bx, destwidth
		mov di, bx
		mov [word ptr destcol], bx
	}
heightend:
	asm {
		loop heighttop

		// add one to chardata
		mov ax, [word ptr chardata]
		inc ax
		mov [word ptr chardata], ax

		// add 8 to baseline
		mov ax, [word ptr baseline]
		add ax, 8
		mov [word ptr baseline], ax

		// sub 8 from charwidth
		sub dx, 8
	}
endwhile:
	asm {
		jle endall
		jmp whiletop
	}
endall:
	asm {
		pop ds
	}
}
#endif

/*  DrawChar3x3Outline:

    This function renders a single bitmapped character into an offscreen
    buffer. The character will be "ballooned", i.e. expanded, by 1 pixel
    in each direction. It does not render the center part of the outlined
    character in a different color -- that must be done as a seperate
    step.

    'drawchar' is the ascii code of the character to be drawn.

    'xpos' is the position in the buffer to draw the outline. Note that
    the first pixel of the outline may be drawn at xpos-1, since the
    outline expands in both directions.

    'baseline' is the address of the first scanline in the buffer.
    (it can actually be any scanline in the buffer if we want to
    draw the character at a different y position).
    Note that this has nothing to do with the Amiga "BASELINE" field,
    this routine always renders relative to the top of the font.

    'color' is the pixel value to render into the buffer.

    This code assumes that the font characters are all byte-aligned,
    and that there is no extra junk after the character in the bits
    that pad to the next byte. Also, the code currently makes no
    provision for "LoChar" or "HiChar" (i.e. there are blank table
    entries for characters 0-1f). This can be changed if desired.

    Note that this code does unaligned int16 writes to byte addresses,
    and will not run on an 68000 or 68010 processor. (Even with the
    speed penalty, this is still pretty fast).
*/

#define SQUARE_OUTLINES 1

#if 1
void DrawChar3x3Outline(gFont *font, int drawchar, int xpos, uint8 *baseline,
                        uint8 color, uint16 destwidth) {
	uint8           *d;
	short           h,              // font height counter
	                rowmod;

	short           charwidth, w;   // width of character in pixels

	uint8           *chardata;      // pointer to start of char data

	uint8           *src, *dst;
	unsigned short  s;
	unsigned short  txt1, txt2, txt3;

	//  point to the first byte of the first scanline of the source char
	chardata = (uint8 *)(font + 1) + font->charXOffset[ drawchar ];

	//  get the width of the character in pixels
	charwidth = font->charWidth[ drawchar ];

	//  point to the first byte of where we want to place the character
	baseline += xpos - 1;

	//  this loop goes once for each 8 pixels wide that the character is
	rowmod = font->rowMod;

	for (w = (charwidth + 7) >> 3; w > 0; w--) {
		src = chardata;
		dst = baseline;

		txt1 = txt2 = 0;

		for (h = font->height + 2; h; h--) {
			d = dst;

			txt3 = txt2;
			txt2 = txt1;
			txt1 = h > 2 ? *src : 0;

			s = txt1 | txt2 | txt3;

			s = s | (s << 1) | (s << 2);

			while (s) {
				if (s & 0x200)
					*d = color;
				++d;
				s <<= 1;
			}

			src += rowmod;
			dst += destwidth;
		}

		chardata++;
		baseline += 8;
	}

}

void DrawChar5x5Outline(gFont *font, int drawchar, int xpos, uint8 *baseline,
                        uint8 color, uint16 destwidth) {
	uint8           *d;
	short           h,              /* font height counter              */
	                rowmod;

	short           charwidth, w;   /* width of character in pixels     */

	uint8           *chardata;      /* pointer to start of char data    */

	uint8           *src, *dst;
	unsigned short  s0, s1;
	unsigned short  txt[ 5 ];

	//  point to the first byte of the first scanline of the source char
	chardata = (uint8 *)(font + 1) + font->charXOffset[ drawchar ];

	//  get the width of the character in pixels
	charwidth = font->charWidth[ drawchar ];

	//  point to the first byte of where we want to place the character
	baseline += xpos - 2;

	//  this loop goes once for each 8 pixels wide that the character is
	rowmod = font->rowMod;

	for (w = (charwidth + 7) >> 3; w > 0; w--) {
		src = chardata;
		dst = baseline;

		txt[ 0 ] = txt[ 1 ] = txt[ 2 ] = txt[ 3 ] = txt[ 4 ] = 0;

		for (h = font->height + 4; h; h--) {
			d = dst;

			txt[ 4 ] = txt[ 3 ];
			txt[ 3 ] = txt[ 2 ];
			txt[ 2 ] = txt[ 1 ];
			txt[ 1 ] = txt[ 0 ];
			txt[ 0 ] = h > 4 ? *src : 0;

			s0 = txt[ 1 ] | txt[ 2 ] | txt[ 3 ];
			s1 = s0 | txt[ 0 ] | txt[ 4 ];

			s0 = s0 | (s0 << 1) | (s0 << 2) | (s0 << 3) | (s0 << 4);
			s0 |= (s1 << 1) | (s1 << 2) | (s1 << 3);

			while (s0) {
				if (s0 & 0x800)
					*d = color;
				++d;
				s0 <<= 1;
			}

			src += rowmod;
			dst += destwidth;
		}

		chardata++;
		baseline += 8;
	}

}
#else
static void DrawChar3x3Outline(
    gFont           *font,
    int             drawchar,
    int             xpos,
    uint8           *baseline,
    gPen            color,
    uint16          destwidth) {
	//  These variables form a 3-stage pipeline, one per scanline

	uint16          txt1,               // bit pattern for current scanline
	                txt2,               // bit pattern for previous scanline
	                txt3,               // bit pattern for one before that
	                h,
	                rowmod;

	int16           charwidth;          // width of character in pixels

	uint8           *chardata,          // pointer to start of char data
	                *destcol;           // pointer to start of dest

	//  point to the first byte of the first scanline of the source char

	chardata = (uint8 *)(font + 1) + font->charXOffset[ drawchar ];

	//  get the width of the character in pixels

	charwidth = font->charWidth[ drawchar ];

	//  point to the first byte of where we want to place the character

	baseline += xpos - 1;

	//  this loop goes once for each 8 pixels wide that the character is,
	//  in other words it processes the data 8 pixels at a time.

	h = font->height;
	rowmod = font->rowMod;

	asm {
		push ds

		mov dx, charwidth   // dx is the char width tracking register
		or  dx, dx
		jmp endwhile
	}
whiletop:
	asm {
		xor ax, ax
		mov txt1, ax
		mov txt2, ax

		// source and destination pointers
		lds si, chardata
		les di, baseline
		mov [word ptr destcol], di
		mov [word ptr destcol+2], es

		mov cx, h
		inc cx
		jmp heightend
	}
heighttop:
	asm {
		mov ax, txt2
		mov txt3, ax

		mov ax, txt1
		mov txt2, ax

		mov ah, ds:[si] // Feed the fifopipe!
		xor al, al

		// some magic square outline or'ing of the last three lines
		mov bx, ax
		shr bx, 1
		or  ax, bx
		shr bx, 1
		or  ax, bx

		// save off txt1 and create txtmask in ax
		mov txt1, ax

		or  ax, txt2
		or  ax, txt3

		// prepare our loop  render pixels one at a time...
		mov bl, color
		jmp innerbottom
	}
innertop:
	asm {
		add ax, ax
		jnc nostore

		mov es:[di], bl
	}
nostore:
	asm {
		inc di
	}
innerbottom:
	asm {
		or  ax, ax
		jnz innertop

		add si, rowmod
		mov bx, [word ptr destcol]
		add bx, destwidth
		mov di, bx
		mov [word ptr destcol], bx
	}
heightend:
	asm {
		loop heighttop

		//  FLUSH OUT THE FIFO

// ********************************* FIRST FIFO
		mov ax, txt1
		or  ax, txt2

//		mov di,bx
		mov bl, color
		jmp innerbottom1
	}
innertop1:
	asm {
		add ax, ax
		jnc nostore1

		mov es:[di], bl
	}
nostore1:
	asm {
		inc di
	}
innerbottom1:
	asm {
		or  ax, ax
		jnz innertop1

		mov bx, [word ptr destcol]
		add bx, destwidth
		mov [word ptr destcol], bx

// ********************************* SECOND FIFO

		mov ax, txt1

		mov di, bx
		mov bl, color
		jmp innerbottom2
	}
innertop2:
	asm {
		add ax, ax
		jnc nostore2

		mov es:[di], bl
	}
nostore2:
	asm {
		inc di
	}
innerbottom2:
	asm {
		or  ax, ax
		jnz innertop2
// ********************************* FINISH UP

		// add one to chardata
		mov ax, [word ptr chardata]
		inc ax
		mov [word ptr chardata], ax

		// add 8 to baseline
		mov ax, [word ptr baseline]
		add ax, 8
		mov [word ptr baseline], ax

		// sub 8 from charwidth
		sub dx, 8
	}
endwhile:
	asm {
		jle endall
		jmp whiletop
	}
endall:
	asm {
		pop ds
	}
}
#endif

/*  A private routine to render a string of characters into a temp
    buffer.
*/

void gPort::drawStringChars(
    char            *str,                   // string to draw
    int16           len,                    // length of string
    gPixelMap       &dest,
    int             xpos,                   // x position to draw it
    int             ypos) {                 // y position to draw it
	uint8           *s,                     // pointer to string
	                drawchar;               // char to draw
	int16           x;                      // current x position
	uint8           *buffer,                // buffer to render to
	                *uBuffer;               // underline buffer
	uint16          rowMod = dest.size.x;   // row modulus of dest
	int16           i;                      // loop index
	uint8           underbar = (textStyles & textStyleBar) != 0;
	bool            underscore;
	int16           underPos;

	// the address to start rendering pixels to.

	underPos = font->baseLine + 2;
	if (underPos > font->height) underPos = font->height;
	buffer = dest.data + (ypos * rowMod);
	uBuffer = buffer + (underPos * rowMod);

	// draw drop-shadow, if any

	if (textStyles & textStyleShadow) {
		x = xpos - 1;
		s = (uint8 *)str;

		if (textStyles & textStyleOutline) { // if outlining
			for (i = 0; i < len; i++) {
				drawchar = *s++;            // draw thick drop shadow
				x += font->charKern[ drawchar ];
				DrawChar3x3Outline(font, drawchar, x, buffer, shPen, rowMod);
				x += font->charSpace[ drawchar ] + textSpacing;
			}
		} else if (textStyles & textStyleThickOutline) { // if outlining
			for (i = 0; i < len; i++) {
				drawchar = *s++;                // draw thick drop shadow
				x += font->charKern[ drawchar ];
				DrawChar5x5Outline(font, drawchar, x, buffer, shPen, rowMod);
				x += font->charSpace[ drawchar ] + textSpacing;
			}
		} else {
			for (i = 0; i < len; i++) {
				drawchar = *s++;            // draw thick drop shadow
				x += font->charKern[ drawchar ];
				DrawChar(font, drawchar, x, buffer + rowMod,
				         shPen, rowMod);
				x += font->charSpace[ drawchar ] + textSpacing;
			}
		}
	}

	// draw outline, if any

	if (textStyles & textStyleOutline) { // if outlining
		x = xpos;
		s = (uint8 *)str;

		for (i = 0; i < len; i++) {
			drawchar = *s++;                // draw thick text
			x += font->charKern[ drawchar ];
			DrawChar3x3Outline(font, drawchar, x, buffer - rowMod,
			                   olPen, rowMod);
			x += font->charSpace[ drawchar ] + textSpacing;
		}
	} else if (textStyles & textStyleThickOutline) { // if thick outlining
		x = xpos;
		s = (uint8 *)str;

		for (i = 0; i < len; i++) {
			drawchar = *s++;                // draw extra thick text
			x += font->charKern[ drawchar ];
			DrawChar5x5Outline(font, drawchar, x, buffer - rowMod * 2,
			                   olPen, rowMod);
			x += font->charSpace[ drawchar ] + textSpacing;
		}
	}

	// draw inner part

	x = xpos;
	s = (uint8 *)str;
	underscore = textStyles & textStyleUnderScore ? TRUE : FALSE;

	for (i = 0; i < len; i++) {
		int16       last_x = x;
		uint8       color = fgPen;

		drawchar = *s++;                // draw thick drop shadow
		if (drawchar == '_' && underbar) {
			len--;
			drawchar = *s++;
			if (textStyles & textStyleUnderBar) underscore = TRUE;
			if (textStyles & textStyleHiLiteBar) color = bgPen;
		}
		x += font->charKern[ drawchar ];
		DrawChar(font, drawchar, x, buffer, color, rowMod);
		x += font->charSpace[ drawchar ] + textSpacing;

		if (underscore) {               // draw underscore
			uint8   *put = uBuffer + last_x;
			int16   width = x - last_x;

			while (width-- > 0) *put++ = color;
			if (!(textStyles & textStyleUnderScore)) underscore = FALSE;
		}
	}
}

//  Draws a string clipped to the current clipping rectangle.
//  Note that if several clipping rectangles were to be used,
//  we would probably do this differently...

int16 gPort::drawClippedString(
    char            *s,                     // string to draw
    int16           len,                    // length of string
    int             xpos,                   // x position to draw it
    int             ypos) {                 // y position to draw it
	int16           clipWidth = 0,          // width of clipped string
	                clipLen;                // chars to draw
	gPixelMap       tempMap;                // temp buffer for text
	uint8           underbar = (textStyles & textStyleBar) != 0;
	int16           xoff = 0,               // offset for outlines
	                yoff = 0;               // offset for outlines
	int16           penMove = 0;            // keep track of pen movement

	//  First, we want to avoid rendering any characters to the
	//  left of the clipping rectangle. Scan the string until
	//  we find a character that is not completely to the left
	//  of the clip.

	while (len > 0) {
		int16       drawchar = *s,
		            charwidth;

		if (drawchar == '_' && underbar) {
			drawchar = s[ 1 ];
			charwidth   = font->charKern[ drawchar ]
			              + font->charSpace[ drawchar ] + textSpacing;

			if (xpos + charwidth >= clip.x) break;
			s++;
		} else {
			charwidth   = font->charKern[ drawchar ]
			              + font->charSpace[ drawchar ] + textSpacing;
			if (xpos + charwidth >= clip.x) break;
		}

		s++;
		len--;
		xpos += charwidth;
		penMove += charwidth;
	}

	//  Now, we also want to only draw that portion of the string
	//  that actually appears in the clip, so scan the rest of the
	//  string until we find a character who's left edge is past
	//  the right edge of the clip.

	for (clipLen = 0; clipLen < len; clipLen++) {
		int16       drawchar = s[ clipLen ];

		if (drawchar == '_' && underbar) continue;

		clipWidth += font->charKern[ drawchar ]
		             + font->charSpace[ drawchar ] + textSpacing;

		if (xpos > clip.x + clip.width) break;
	}

	//  Handle special case of negative kern value of 1st character

	if (font->charKern[ s[ 0 ] ] < 0) {
		int16       kern = - font->charKern[ s[ 0 ] ];

		clipWidth += kern;              // increase size of map to render
		xoff += kern;                   // offset text into map right
		xpos -= kern;                   // offset map into port left
	}

	//  Set up a temporary bitmap to hold the string.

	tempMap.size.x = clipWidth;
	tempMap.size.y = font->height;

	//  Adjust the size and positioning of the temp map due
	//  to text style effects.

	if (textStyles & textStyleOutline) {
		xoff = yoff = 1;
		xpos--;
		ypos--;
		tempMap.size.x += 2;
		tempMap.size.y += 2;
	} else if (textStyles & textStyleThickOutline) {
		xoff = yoff = 2;
		xpos -= 2;
		ypos -= 2;
		tempMap.size.x += 4;
		tempMap.size.y += 4;
	}

	if (textStyles & (textStyleShadow | textStyleUnderScore | textStyleUnderBar)) {
		tempMap.size.x += 1;
		tempMap.size.y += 1;
	}

	if (textStyles & textStyleItalics) {
		int n = (font->height - font->baseLine - 1) / 2;

		if (n > 0) xpos += n;
		tempMap.size.x += tempMap.size.y / 2;
	}

	//  Allocate a temporary bitmap

	if (tempMap.bytes() == 0) return 0;
	tempMap.data = (uint8 *)TempAlloc(tempMap.bytes());
	if (tempMap.data != NULL) {
		//  Fill the buffer with background pen if we're
		//  not doing a transparent blit.

		memset(tempMap.data,
		       (drawMode == drawModeReplace) ? bgPen : 0,
		       tempMap.bytes());

		//  Draw the characters into the buffer

		drawStringChars(s, clipLen, tempMap, xoff, yoff);

		//  apply slant if italics

		if (textStyles & textStyleItalics) {
			int n = (font->height - font->baseLine - 1) / 2;
			int shift = (n > 0 ? n : 0);
			int flag = (font->height - font->baseLine - 1) & 1;

			shift = -shift;

			for (int k = font->height - 1; k >= 0; k--) {
				if (shift < 0) {
					uint8   *dest = tempMap.data + k * tempMap.size.x,
					         *src = dest - shift;
					int     j;

					for (j = 0; j < tempMap.size.x + shift; j++) {
						*dest++ = *src++;
					}
					for (; j < tempMap.size.x; j++) {
						*dest++ = 0;
					}
				} else if (shift > 0) {
					uint8   *dest = tempMap.data + (k + 1) * tempMap.size.x,
					         *src = dest - shift;
					int     j;

					for (j = 0; j < tempMap.size.x - shift; j++) {
						*--dest = *--src;
					}
					for (; j < tempMap.size.x; j++) {
						*--dest = 0;
					}
				}

				flag ^= 1;
				if (flag) shift++;
			}
		}

		//  blit the temp buffer onto the screen.

		bltPixels(tempMap, 0, 0,
		          xpos, ypos,
		          tempMap.size.x, tempMap.size.y);

		TempFree(tempMap.data);
	}

	//  Now, we still need to scan the rest of the string
	//  so that we can move the pen position by the right amount.

//	penMove += clipWidth;
	for (clipLen = 0; clipLen < len; clipLen++) {
		int16       drawchar = s[ clipLen ];

		if (drawchar == '_' && underbar) continue;

		penMove += font->charKern[ drawchar ]
		           + font->charSpace[ drawchar ] + textSpacing;
	}

	return penMove;
}

/********* gtext.cpp/gPort::drawText *********************************
*
*   NAME
*       gPort::drawText -- draw a text string into a gPort
*
*   SYNOPSIS
*       port.drawText( str, length );
*
*       void gPort::drawText( char *, int16 = -1 );
*
*   FUNCTION
*       This function draws a string of text at the current pen
*       position in the current pen color. The pen position is
*       updated to the end of the text.
*
*       The text will be positioned such that the top-left corner
*       of the first character will be at the pen position.
*
*   INPUTS
*       str         A string of characters.
*
*       length      [Optional parameter] If supplied, it indicates
*                   the length of the string; Otherwise, strlen()
*                   is used.
*
*   RESULT
*       none
*
*   SEE ALSO
*       gPort class
*
**********************************************************************
*/
void gPort::drawText(
    char            *str,                   /* string to draw               */
    int16           length) {
	if (length < 0) length = strlen(str);

	if (length > 0)
		penPos.x += drawClippedString(str, length, penPos.x, penPos.y);
}

/********* gtext.cpp/gPort::drawTextInBox *********************************
*
*   NAME
*       gPort::drawTextInBox -- draw text within a box
*
*   SYNOPSIS
*       port.drawTextInBox( str, len, rect, pos, borderSpace );
*
*       void gPort::drawTextInBox( char *, int16, const Rect16 &,
*       /c/ int16, Point16 );
*
*   FUNCTION
*       This function can draw a text string centered or justified
*       within a rectangular area, and clipped to that area as well.
*       The text string is drawn in the current pen color, and with
*       the current draw mode.
*
*   INPUTS
*       str         The text to draw.
*
*       len         The length of the string or -1 to indicate a
*                   null-terminated string.
*
*       rect        The rectangle to draw the text within.
*
*       pos         How to position the text string within the
*                   rectangle. The default (0) is to center the
*                   string both horizontally and vertically; However,
*                   the following flags will modify this:
*
*       /i/         textPosLeft -- draw text left-justified.
*
*       /i/         textPosRight -- draw text right-justified.
*
*       /i/         textPosHigh -- draw text flush with top edge.
*
*       /i/         textPosLow -- draw text flush with bottom edge.
*
*       borderSpace A Point16 object, which indicates how much space
*                   (in both x and y) to place between the text and
*                   the border when the text is justified to a
*                   particular edge. Does not apply when text is centered.
*
*   RESULT
*       none
*
*   SEE ALSO
*       gPort class
*
**********************************************************************
*/
void gPort::drawTextInBox(
    char            *str,
    int16           length,
    const Rect16    &r,
    int16           pos,
    Point16         borders) {
	int16           height, width;
	int16           x, y;
	Rect16          newClip,
	                saveClip = clip;

	if (!font)
		return;

	height = font->height;
	width  = TextWidth(font, str, length, textStyles);

	if (textStyles & (textStyleUnderScore | textStyleUnderBar)) {
		if (font->baseLine + 2 >= font->height) height++;
	}

	//  Calculate x position of text string

	if (pos & textPosLeft) x = r.x + borders.x;
	else if (pos & textPosRight) x = r.x + r.width - width - borders.x;
	else x = r.x + (r.width - width) / 2;

	//  Calculate y position of text string

	if (pos & textPosHigh) y = r.y + borders.y;
	else if (pos & textPosLow) y = r.y + r.height - height - borders.y;
	else y = r.y + (r.height - height) / 2;

	//  Calculate clipping region

	clip = intersect(clip, r);

	//  Draw the text

	moveTo(x, y);
	drawText(str, length);

	//  Restore the clipping region

	clip = saveClip;
}

//  Attach to gFont?

/********* gtext.cpp/TextWidth ***************************************
*
*   NAME
*       TextWidth -- returns length of text string in pixels
*
*   SYNOPSIS
*       width = TextWidth( font, str, len, styles );
*
*       int16 TextWidth( gFont *, char *, int16, int16 );
*
*   FUNCTION
*       This function computes the length of a text string in pixels
*       for a given font and text style.
*
*   INPUTS
*       font        The text font of the text.
*
*       str         The text string to measure.
*
*       len         The length of the string, or -1 to use strlen()
*
*       styles      The text rendering style (see gPort::setStyle).
*                   0 = normal.
*
*   RESULT
*       The width of the text.
*
**********************************************************************
*/
int16 TextWidth(gFont *font, char *s, int16 length, int16 styles) {
	int16           count = 0;

	if (length < 0) length = strlen(s);

	while (length--) {
		uint8       chr = *s++;

		if (chr == '_' && (styles & textStyleBar)) continue;

		count += font->charKern[ chr ] + font->charSpace[ chr ];
	}

	if (styles & textStyleItalics) {
		count += (font->baseLine + 1) / 2 +
		         (font->height - font->baseLine - 1) / 2;
	}
	if (styles & textStyleOutline) count += 2;
	else if (styles & textStyleThickOutline) count += 4;
	if (styles & textStyleShadow) count += 1;

	return count;
}

/********* gtext.cpp/WhichChar ***************************************
*
*   NAME
*       WhichChar -- search for character under mouse pointer
*
*   SYNOPSIS
*       charNum = WhichChar( font, str, pick, maxLen );
*
*       int16 WhichChar( gFont *, uint8 *, int16, int16 );
*
*   FUNCTION
*       This function is used by the gTextBox class to click on
*       a character within a text box. It computes the width of
*       each character in the string (as it would be rendered
*       on the screen) and determines which of those characters
*       the "pick" position falls upon.
*
*   INPUTS
*       font        The font to use in the character-width calculation.
*
*       str         The string to search.
*
*       pick        The pick position, relative to the start of the string.
*
*       maxLen      The length of the string.
*
*   RESULT
*       The index of the selected character.
*
**********************************************************************
*/
int16 WhichChar(gFont *font, uint8 *s, int16 length, int16 maxLen) {
	int16           count = 0;

	if (maxLen == -1) maxLen = strlen((char *)s);

	for (count = 0; count < maxLen; count++) {
		uint8       chr = *s++;

		length -= font->charKern[ chr ] + font->charSpace[ chr ];
		if (length < 0) break;
	}
	return count;
}

//  Searches for the insertion point between chars under the cursor

/********* gtext.cpp/WhichIChar **************************************
*
*   NAME
*       WhichIChar -- search for insertion point between characters
*
*   SYNOPSIS
*       charNum = WhichIChar( font, str, pick, maxLen );
*
*       int16 WhichIChar( gFont *, uint8 *, int16, int16 );
*
*   FUNCTION
*       This function is used by the gTextBox class to select
*       the position of the insertion point when the text box
*       is clicked on. It computes the width of each character
*       in the string (as it would be rendered
*       on the screen) and determines which two characters the
*       pick position would fall between, in other words it finds
*       the nearest insertion point between characters.
*
*   INPUTS
*       font        The font to use in the character-width calculation.
*
*       str         The string to search.
*
*       pick        The pick position, relative to the start of the string.
*
*       maxLen      The length of the string.
*
*   RESULT
*       The index of the selected character.
*
**********************************************************************
*/
int16 WhichIChar(gFont *font, uint8 *s, int16 length, int16 maxLen) {
	int16           count = 0;

	if (maxLen == -1) maxLen = strlen((char *)s);

	for (count = 0; count < maxLen; count++) {
		uint8       chr = *s++;
		int16       width;

		width = font->charKern[ chr ] + font->charSpace[ chr ];

		if (length < width / 2) break;
		length -= width;
	}
	return count;
}

/****** gtext.cpp/GTextWrap *****************************************
*
*   NAME
*       GTextWrap -- given text buffer and pixel width, find wrap point
*
*   SYNOPSIS
*       offset = GTextWrap( font, buffer, count, width, styles );
*
*       int32 GTextWrap( gFont *, char *, uint16 &, uint16, int16 );
*
*   FUNCTION
*       This function finds the point in a text buffer that text
*       wrapping would have to occur given the indicated pixel width.
*       Linefeeds are considered implied wrap points. Tabs are not
*       expanded.
*
*   INPUTS
*       font    a gFont that the text will be rendered in.
*       buffer  a NULL-terminated text buffer.
*       count   reference to variable to store count of characters
*               to render before wrap point.
*       width   width in pixels to wrap point.
*       styles  possible text styles.
*
*   RESULT
*       offset  amount to add to buffer for next call to GTextWrap,
*               or -1 if the NULL-terminator was reached.
*
**********************************************************************
*/
int32 GTextWrap(gFont *font, char *mark, uint16 &count, uint16 width, int16 styles) {
	char        *text = mark;
	char        *ntext, *atext, *ltext;
	uint16      pixlen;

	if (!strchr(text, '\n')) {
		count = strlen(text);
		pixlen = TextWidth(font, text, count, styles);
		if (pixlen <= width) return -1;
	}

	atext = text;
	while (1) {
		ntext = strchr(atext, ' ');
		ltext = strchr(atext, '\n');

		if (ntext == NULL || (ltext != NULL && ltext < ntext)) {
			pixlen = TextWidth(font, text, ltext - text, styles);
			if (pixlen <= width) {
				count = ltext - text;
				return count + 1;
			}

			if (ntext == NULL) {
				if (atext == text) break;

				count = atext - text - 1;
				return count + 1;
			}
		}

		pixlen = TextWidth(font, text, ntext - text, styles);
		if (pixlen > width) {
			if (atext == text) break;

			count = atext - text - 1;
			return count + 1;
		}

		atext = ntext + 1;
	}

	if (atext == text) {
		// current text has no spaces AND doesn't fit

		count = strlen(text);
		while (--count) {
			pixlen = TextWidth(font, text, count, styles);
			if (pixlen <= width) return count;
		}
	} else {
		count = atext - text - 1;
		return count + 1;
	}

	return -1;
}

#if 0
int16 X_TextWrap(
    char                *lines[],
    int16               line_chars[],
    int16               line_pixels[],
    char                *text,                  // the text to render
    int16               width                   // width to constrain text
) {
	int16               i,                      // loop counter
	                    line_start,             // start of current line
	                    last_space,             // last space encountered
	                    last_space_pixels,      // pixel pos of last space
	                    pixel_len,              // pixel length of line
	                    line_count = 0;         // number of lines

	lines[ line_count ] = text;
	last_space = -1;
	line_start = 0;
	pixel_len = 0;

	//  For each character in the string, check for word wrap

	for (i = 0; ; i++) {
		uint8           c = text[ i ];

		if (c == '\n' || c == '\r' || c == '\0') {  // if deliberate end of line
			line_chars[ line_count ] = i - line_start;  //
			line_pixels[ line_count ] = pixel_len;
			line_start = i + 1;
			if (c == '\0') {
				line_count++;
				break;
			}
			lines[ ++line_count ] = &text[ line_start ];
			last_space = -1;
			pixel_len = 0;
			continue;
		} else if (c == ' ') {
			last_space = i;
			last_space_pixels = pixel_len;
		}

		pixel_len +=
		    font->char_kern[ c ] + font->char_space[ c ];

		if (pixel_len > width - 2 && last_space > 0) {
			line_chars[ line_count ] = last_space - line_start;
			line_pixels[ line_count ] = last_space_pixels;
			line_start = last_space + 1;
			lines[ ++line_count ] = &text[ line_start ];

			last_space = -1;
			pixel_len = 0;

			i = line_start - 1;
		}
	}
	return line_count;
}

void XS_DrawTextWrapped(
    xImage              *dest,                  // where to draw to
    char                *text,                  // the text to render
    xBox                *box,                   // box to constrain text
    int16               text_color,             // color of text
    int16               outline_color           // color of outline
) {
	int16               i,                      // loop counter
	                    line_count = 0;         // number of lines

	char                *lines[ 10 ];           // pointer to each line

	int16               line_chars[ 10 ],       // # of chars in line
	                    line_pixels[ 10 ],      // # of pixels in line
	                    ypos = box->Top,        // top of box
	                    row_height;
	int16               save_flags = x_DrawFlags;

	line_count = X_TextWrap(lines, line_chars, line_pixels,
	                        text, box->Width);

	row_height = theFont->height + ((outline_color > 0) ? 2 : 0);
	if (outline_color > 0) ypos += 1;

	//  Vertically center it in box

	if (box->Height >= line_count * row_height)
		ypos += (box->Height - line_count * row_height) / 2;

	X_Masking(TRUE);
	X_Clipping(FALSE);

	for (i = 0; i < line_count; i++) {
		DrawFixedString(dest,
		                lines[ i ], line_chars[ i ],
		                box->Left + (box->Width - line_pixels[ i ]) / 2 + 1,
		                ypos,
		                text_color, outline_color, 0);

		ypos += row_height;
		if (ypos >= box->Top + box->Height) break;
	}
	x_DrawFlags = save_flags;
}

//  Figures the height of a wrapped text box.

int16 X_WrappedTextHeight(
    char                *text,                  // the text to render
    int16               width,                  // box to constrain text
    int16               outlined                // color of outline
) {
	int16               line_count = 0;         // number of lines

	char                *lines[ 10 ];           // pointer to each line

	int16               line_chars[ 10 ],       // # of chars in line
	                    line_pixels[ 10 ],      // # of pixels in line
	                    row_height;

	line_count = X_TextWrap(lines, line_chars, line_pixels,
	                        text, width);

	row_height = theFont->height + (outlined ? 2 : 0);

	if (outlined > 0) return (line_count * row_height) + 1;
	return (line_count * row_height);
}
#endif

} // end of namespace Saga2