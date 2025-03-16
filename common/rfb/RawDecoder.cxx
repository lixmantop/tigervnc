/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include <rdr/OutStream.h>
#include <rfb/ServerParams.h>
#include <rfb/PixelBuffer.h>
#include <rfb/RawDecoder.h>

using namespace rfb;
// neticar
RawDecoder::RawDecoder(int _jpeg) : Decoder(DecoderPlain)
{
	jpeg=_jpeg;
}

RawDecoder::~RawDecoder()
{
}

bool RawDecoder::readRect(const core::Rect& r, rdr::InStream* is,
                          const ServerParams& server, rdr::OutStream* os)
{
	int size;
	if(jpeg==1) {
	  if(is->hasData(4) ){
		  is->setRestorePoint();
		  size=is->readU32();
	  } else {
		  return false;
	  }

	  if (!is->hasDataOrRestore(size))
	     return false;

	   is->clearRestorePoint();


	  os->writeU32(size);
	  os->copyBytes(is, size);
	  return true;

	} else {
	  if (!is->hasData(r.area() * (server.pf().bpp/8)))
		return false;
	  os->copyBytes(is, r.area() * (server.pf().bpp/8));
	  return true;
	}

}

void RawDecoder::decodeRect(const core::Rect& r, const uint8_t* buffer,
                            size_t buflen, const ServerParams& server,
                            ModifiablePixelBuffer* pb)
{
	if(jpeg==0) {
		assert(buflen >= (size_t)r.area() * (server.pf().bpp/8));
		pb->imageRect(server.pf(), r, buffer);
	} else {
		const void* bufptr = (const void*)buffer;

	    int stride;
	    void *buf;

	    JpegDecompressor jd;
	    int size;
	    memcpy(&size, buffer, 4);
	    //bufptr = bufptr + 4;
	    buf = pb->getBufferRW(r, &stride);

	    jd.decompress((uint8_t*)bufptr +4, buflen, (uint8_t *)buf, stride, r, pb->getPF());
	    pb->commitBufferRW(r);
	}
}
