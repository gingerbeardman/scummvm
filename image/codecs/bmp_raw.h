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
 */

#ifndef IMAGE_CODECS_BMP_RAW_H
#define IMAGE_CODECS_BMP_RAW_H

#include "image/codecs/codec.h"

namespace Image {

/**
 * Bitmap raw image decoder.
 *
 * Used by BMP/AVI.
 */
class BitmapRawDecoder : public Codec {
public:
	BitmapRawDecoder(int width, int height, int bitsPerPixel, bool ignoreAlpha, bool _flip = false);
	~BitmapRawDecoder();

	const Graphics::Surface *decodeFrame(Common::SeekableReadStream &stream);
	Graphics::PixelFormat getPixelFormat() const;

private:
	Graphics::Surface _surface;
	int _width, _height;
	int _bitsPerPixel;
	bool _ignoreAlpha;

	// this flag indicates whether bitmapRawDecoder is created to decode QTvideo or raw images.
	// because we need to flip the image when we are dealing with QTvideo
	bool _flip;
};

} // End of namespace Image

#endif
