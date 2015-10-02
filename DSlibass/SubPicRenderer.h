/*
 *      Copyright (C) 2012 Evgeny Marchenkov
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#pragma once

struct aligned_deleter {
	void operator()(void* buf) const
	{
		_aligned_free(buf);
	}
};

typedef std::unique_ptr<BYTE, aligned_deleter> AlignedBuffer;

struct SubPicture {
public:
	RECT pos;
	AlignedBuffer pixels;

	SubPicture() : pixels(nullptr) {}

	//dummy operator to shut up the compiler error
	SubPicture & operator =(const SubPicture & sp)
	{
		SubPicture p;
		return p;
	}

	SubPicture(SubPicture&& sp)
	{
		pos = std::move(sp.pos);
		pixels = std::move(sp.pixels);
	}

};


class CSubPicRenderer
{
public:
	CSubPicRenderer();
	~CSubPicRenderer();

	HRESULT SetFrameParameters(UINT Width, INT Height, DSLAPixFmts PixFmt);
	HRESULT SetColorMatrix(DSLAMatrixModes ColorMatrix);

	HRESULT RenderFrame(BYTE *Buf, ULONGLONG tc, WORD TrackNum);
	
	CLibassWrapper* GetLibass() { return m_pLibass.get(); };

private:

	void BuildRegions(ASS_Image *img);
	void DrawSubPictures(ASS_Image *img);
	void BlendSubPictures();

	void BlendRGB(SubPicture& sp);
	void BlendRGBFlipped(SubPicture& sp);
	void BlendYV12(SubPicture& sp);
	void BlendNV12(SubPicture& sp);
	void BlendP01(SubPicture& sp);
	void BlendChroma(SubPicture& sp);
	void BlendChroma_16(SubPicture& sp);

	std::unique_ptr<CLibassWrapper> m_pLibass;

	std::vector<SubPicture> m_SubPictures;
	AlignedBuffer m_pChromaBuf;
	BYTE *m_pExtBuf;
	UINT m_cWidth;
	INT m_cHeight;
	DSLAPixFmts m_cPixFmt;
	DSLAMatrixModes m_cColorMatrix;

};

//Stride is in pixels
HRESULT BitBltRGB(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut, BYTE bpp);
HRESULT BitBltRGBWithFlip(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut, BYTE bpp);
HRESULT BitBltYV12(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut);
HRESULT BitBltNV12(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut);
HRESULT BitBltP01(WORD *pBufIn, WORD *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut);
