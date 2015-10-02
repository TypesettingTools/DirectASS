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

#include "stdafx.h"
#include "DSlibassSettings.h"
#include "LibassWrapper.h"
#include "SubPicRenderer.h"
#include "memcpy_amd.h"


#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

#define rgba2y601(c)  ( (( 263*_r(c) + 516*_g(c) + 100*_b(c)) >> 10) + 16  )
#define rgba2u601(c)  ( ((-2419*_r(c) - 4749*_g(c) + 7168*_b(c)) >> 14) + 128 )
#define rgba2v601(c)  ( (( 448*_r(c) - 375*_g(c) -  73*_b(c)) >> 10) + 128 )

#define rgba2y709(c) ( (( 745*_r(c) + 2506*_g(c) + 253*_b(c) ) >> 12) + 16 )
#define rgba2u709(c) ( ((-41*_r(c) - 407*_g(c) + 448*_b(c) ) >> 10) + 128 )
#define rgba2v709(c) ( (( 14336*_r(c) - 11051*_g(c) - 3285*_b(c) ) >> 15) + 128 )


using namespace std;

static inline RECT RectCreate( LONG left, LONG top, LONG right, LONG bottom )
{
    RECT r = { left, top, right, bottom };
    return r;
}
static inline RECT RectFromImg( const ASS_Image *img )
{
    return RectCreate( img->dst_x, img->dst_y, img->dst_x + img->w, img->dst_y + img->h );
}
static inline void RectAdd( RECT & r, const RECT & n )
{
    r.left = min( r.left, n.left );
    r.top = min( r.top, n.top );
    r.right = max( r.right, n.right );
    r.bottom = max( r.bottom, n.bottom );
}
static inline LONG RectSurface( const RECT & r )
{
    return (r.right - r.left) * (r.bottom - r.top);
}
static inline bool RectOverlap( const RECT & a, const RECT & b, int i_dx, int i_dy )
{
    return  max(a.left - i_dx, b.left) < min( a.right + i_dx, b.right ) &&
            max(a.top - i_dy, b.top) < min( a.bottom + i_dy, b.bottom );
}

static inline DWORD ConvertRGBAPixelToAYUV(DWORD c, BYTE yuvMatrix) {

	BYTE y = 0;
	BYTE u = 0;
	BYTE v = 0;

	if(!yuvMatrix){
		y = rgba2y601 (c);
		u = rgba2u601 (c);
		v = rgba2v601 (c);
	} else {
		y = rgba2y709 (c);
		u = rgba2u709 (c);
		v = rgba2v709 (c);
	}

	return ((c & 0xFF) << 24) + (y << 16) + (u << 8) + v;

}

CSubPicRenderer::CSubPicRenderer() :
	m_pLibass(new CLibassWrapper()),
	m_cWidth(0),
	m_cHeight(0),
	m_cPixFmt(DSLAPixFmt_None),
	m_cColorMatrix(DSLAMatrixMode_BT601),
	m_pChromaBuf(nullptr)
{
	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer: creating"));
}

CSubPicRenderer::~CSubPicRenderer()
{
	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer: destroying"));
}

void CSubPicRenderer::BuildRegions(ASS_Image * img) {

	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::BuildRegions: calculating dirty areas"));

	vector<ASS_Image *> imageList;

	for(ASS_Image *tmp = img; tmp != NULL; tmp = tmp->next) {
		if(tmp->w > 0 && tmp->h > 0) {
			imageList.push_back(tmp);
		}
	}

	const UINT MaxRegions = imageList.size();

	const int overlap_w = max((m_cWidth + 49) / 50, 32);
	const int overlap_h = max((m_cHeight + 99) / 100, 32);
	m_SubPictures.clear();

	for(UINT img_used = 0; img_used < imageList.size(); ) {

		UINT i;
		for(i = 0; i < imageList.size(); i++) {
			if(imageList[i])
				break;
		}
		SubPicture sp;
		sp.pos = RectFromImg(imageList[i]);
		m_SubPictures.emplace_back(move(sp));
		imageList[i] = nullptr;
		img_used++;

		BOOL ok;
		do {
			ok = false;
			for(i = 0; i < imageList.size(); i++) {

				if( !imageList[i])
					continue;

				RECT r = RectFromImg(imageList[i]);

				int best_reg = -1;
				LONG best_surf = LONG_MAX;
				for(UINT j = 0; j < m_SubPictures.size(); j++) {
					if(!RectOverlap(m_SubPictures[j].pos, r, overlap_w, overlap_h))
						continue;
					LONG s = RectSurface(r);
					if(s < best_surf) {
						best_reg = j;
						best_surf = s;
					}
				}
				if(best_reg >= 0){
					RectAdd(m_SubPictures[best_reg].pos, r);
					imageList[i] = nullptr;
					img_used++;
					ok = true;
				}
			}
		} while (ok);

		if(m_SubPictures.size() > MaxRegions) {

			int best_reg_i = -1;
			int best_reg_j = -1;
			LONG best_surf = LONG_MAX;
			
			for(UINT k = 0; k < m_SubPictures.size(); k++) {
				for(UINT j = 0; j < m_SubPictures.size(); j++) {
					RECT n = m_SubPictures[k].pos;
					RectAdd(n, m_SubPictures[j].pos);
					LONG ds = RectSurface(n) - RectSurface(m_SubPictures[k].pos) - RectSurface(m_SubPictures[j].pos);
					
					if(ds < best_surf) {
						best_reg_i = k;
						best_reg_j = j;
						best_surf = ds;
					}
				}
			}

			RectAdd(m_SubPictures[best_reg_i].pos, m_SubPictures[best_reg_j].pos);
			m_SubPictures.erase(m_SubPictures.begin() + best_reg_j);
		}

	}

	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::BuildRegions: %d region(s) generated", m_SubPictures.size()));

}

HRESULT CSubPicRenderer::SetFrameParameters(UINT Width, INT Height, DSLAPixFmts PixFmt) {

	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetFrameParameters: %dx%d, format %d", Width, Height, PixFmt));

	if(Width != m_cWidth || Height != m_cHeight || PixFmt != m_cPixFmt){
		switch(PixFmt) {
		case DSLAPixFmt_YV12:
		case DSLAPixFmt_NV12:
			DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetFrameParameters: reallocating buffers"));
			m_pChromaBuf.reset((BYTE *) _aligned_malloc(Width * abs(Height) * 2, 16));
			break;
		case DSLAPixFmt_P010:
		case DSLAPixFmt_P016:
			DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetFrameParameters: reallocating buffers"));
			m_pChromaBuf.reset((BYTE *) _aligned_malloc(Width * abs(Height) * 4, 16));
			break;
		default:
			break;
		}

		m_cWidth = Width;
		m_cHeight = Height;
		m_cPixFmt = PixFmt;
	}

	return S_OK;
}

HRESULT CSubPicRenderer::SetColorMatrix(DSLAMatrixModes ColorMatrix) {

	m_cColorMatrix = ColorMatrix;

	if(m_cColorMatrix == DSLAMatrixMode_AUTO) {
		DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetColorMatrix: automatic color matrix selection"));
		if(m_cWidth > 1024 || abs(m_cHeight) >= 600){
			m_cColorMatrix = DSLAMatrixMode_BT709;
		} else {
			m_cColorMatrix = DSLAMatrixMode_BT601;
		}
	}

#ifdef _DEBUG
	switch(m_cColorMatrix){
	case DSLAMatrixMode_BT601:
		DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetColorMatrix: setting color matrix: BT.601"));
		break;
	case DSLAMatrixMode_BT709:
		DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::SetColorMatrix: setting color matrix: BT.709"));
		break;
	default:
		break;
	}
#endif

	return S_OK;
}

HRESULT CSubPicRenderer::RenderFrame(BYTE *Buf, ULONGLONG tc, WORD TrackNum) {

	HRESULT hr = S_OK;

	DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::RenderFrame: rendering frame at %d ms", tc));

	m_pExtBuf = Buf;
	ASS_Image *img = NULL;
	int detect_change;

	hr = m_pLibass->ASSRenderFrame(tc, TrackNum, &img, &detect_change);
	if(FAILED(hr))
		return hr;
	
	if(img != NULL) {

		if(detect_change != 0){
			DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::RenderFrame: drawing new subpictures"));
			BuildRegions(img);
			DrawSubPictures(img);
		}
		DbgLog((LOG_TRACE, 3, L"CSubPicRenderer::RenderFrame: blending subpictures"));
		BlendSubPictures();
	}
	return hr;
}

void CSubPicRenderer::DrawSubPictures(ASS_Image *img) {

	for(UINT i = 0; i < m_SubPictures.size(); i++) {

		const UINT bufsize = (m_SubPictures[i].pos.right - m_SubPictures[i].pos.left) * (m_SubPictures[i].pos.bottom - m_SubPictures[i].pos.top) * 4;
		const UINT bufstride = (m_SubPictures[i].pos.right - m_SubPictures[i].pos.left) * 4;

		m_SubPictures[i].pixels.reset((BYTE *) _aligned_malloc(bufsize, 16));
		memset(m_SubPictures[i].pixels.get(), 0, bufsize);

		for(ASS_Image *tmp = img; tmp != nullptr; tmp = tmp->next) {

			if( tmp->dst_x < m_SubPictures[i].pos.left || tmp->dst_x + tmp->w > m_SubPictures[i].pos.right ||
				tmp->dst_y < m_SubPictures[i].pos.top || tmp->dst_y + tmp->h > m_SubPictures[i].pos.bottom)
				continue;

			BYTE colors[4];

			if(m_cPixFmt == DSLAPixFmt_RGB32 || m_cPixFmt == DSLAPixFmt_RGB24) {
				//convert RGBA ordering to BGRA
				colors[0] = ( tmp->color >> 8 ) & 0xFF;
				colors[1] = ( tmp->color >> 16) & 0xFF;
				colors[2] = ( tmp->color >> 24);
				colors[3] = 255 - (( tmp->color) & 0xFF);
			} else {
				//convert RGBA to YUVA
				DWORD color = ConvertRGBAPixelToAYUV(tmp->color, m_cColorMatrix);
				colors[0] = (color >> 16) & 0xFF;
				colors[1] = (color >> 8 ) & 0xFF;
				colors[2] = (color      ) & 0xFF;
				colors[3] = 255 - (color >> 24);
			}

			BYTE *buf = m_SubPictures[i].pixels.get();
			BYTE *src = tmp->bitmap;

			buf += bufstride * (tmp->dst_y - m_SubPictures[i].pos.top) + (tmp->dst_x - m_SubPictures[i].pos.left) * 4;

			for(int y = 0; y < tmp->h; y++) {
				BYTE *c = buf;
				for(int x = 0; x < tmp->w; x++) {
					const BYTE an = (src[x] * colors[3] + 255) >> 8;
					const BYTE ao = c[3];
					if(ao == 0 || an == 255){
                        *(int *)c = *(int *)colors;
						c[3] = an;
					} else {
						c[3] = (an * 255 + (c[3] * (255 - an)) + 255) >> 8;
						if(c[3]) {
							c[0] = ((an * 255 * colors[0] + (c[0] * ao * (255 - an))) / c[3] + 255) >> 8;
							c[1] = ((an * 255 * colors[1] + (c[1] * ao * (255 - an))) / c[3] + 255) >> 8;
							c[2] = ((an * 255 * colors[2] + (c[2] * ao * (255 - an))) / c[3] + 255) >> 8;
						}
					}
					c += 4;
				}
				buf += bufstride;
				src += tmp->stride;
			}
		}
	}
}

void CSubPicRenderer::BlendSubPictures() {

	for(UINT i = 0; i < m_SubPictures.size(); i++) {
		switch(m_cPixFmt) {
		case DSLAPixFmt_RGB32:
		case DSLAPixFmt_RGB24:
			if(m_cHeight > 0) {
				BlendRGBFlipped(m_SubPictures[i]);
			} else {
				BlendRGB(m_SubPictures[i]);
			}
			break;
		case DSLAPixFmt_YV12:
			BlendYV12(m_SubPictures[i]);
			break;
		case DSLAPixFmt_NV12:
			BlendNV12(m_SubPictures[i]);
			break;
		case DSLAPixFmt_P010:
		case DSLAPixFmt_P016:
			BlendP01(m_SubPictures[i]);
			break;
		default:
			break;
		}
	}
}

void CSubPicRenderer::BlendRGB(SubPicture& sp) {

	const UINT stride = m_cWidth;
	BYTE *dst = m_pExtBuf;
	BYTE *src = sp.pixels.get();

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;

	if(m_cPixFmt == DSLAPixFmt_RGB32){

		dst += (sp.pos.top * stride + sp.pos.left) * 4;

		for(UINT y = 0; y < srcheight; y++) {
			BYTE *d = dst;
			BYTE *s = src;
			for(UINT x = 0; x < srcwidth; x++) {
				const BYTE an = s[3];
				const BYTE ao = d[3];
				if(ao == 0){
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
					d[3] = an;
				} else {
					d[3] = (an * 255 + (d[3] * (255 - an)) + 255) >> 8;
					if(d[3]) {
						d[0] = ((an * 255 * s[0] + (d[0] * ao * (255 - an))) / d[3] + 255) >> 8;
						d[1] = ((an * 255 * s[1] + (d[1] * ao * (255 - an))) / d[3] + 255) >> 8;
						d[2] = ((an * 255 * s[2] + (d[2] * ao * (255 - an))) / d[3] + 255) >> 8;
					}
				}
				d += 4;
				s += 4;
			}
			src += srcstride;
			dst += stride;
		}

	} else if(m_cPixFmt == DSLAPixFmt_RGB24){

		dst += (sp.pos.top * stride + sp.pos.left) * 3;

		for(UINT y = 0; y < srcheight; y++) {
			BYTE *d = dst;
			BYTE *s = src;
			for(UINT x = 0; x < srcwidth; x++) {
				const BYTE an = s[3];
				if(an == 0){
					continue;
				} else if(an == 255) {
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
				} else {
					d[0] = (an * s[0] + (255 - an) * d[0] + 255) >> 8;
					d[1] = (an * s[1] + (255 - an) * d[1] + 255) >> 8;
					d[2] = (an * s[2] + (255 - an) * d[2] + 255) >> 8;
				}
				d += 3;
				s += 4;
			}
			src += srcstride;
			dst += stride;
		}
	}
}

void CSubPicRenderer::BlendRGBFlipped(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	BYTE *dst = m_pExtBuf;
	BYTE *src = sp.pixels.get();

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;

	if(m_cPixFmt == DSLAPixFmt_RGB32){

		dst += ( (height - sp.pos.top - 1) * stride + sp.pos.left) * 4;

		for(UINT y = 0; y < srcheight; y++) {
			BYTE *d = dst;
			BYTE *s = src;
			for(UINT x = 0; x < srcwidth; x++) {
				const BYTE an = s[3];
				const BYTE ao = d[3];
				if(ao == 0){
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
					d[3] = an;
				} else {
					d[3] = (an * 255 + (d[3] * (255 - an)) + 255) >> 8;
					if(d[3]) {
						d[0] = ((an * 255 * s[0] + (d[0] * ao * (255 - an))) / d[3] + 255) >> 8;
						d[1] = ((an * 255 * s[1] + (d[1] * ao * (255 - an))) / d[3] + 255) >> 8;
						d[2] = ((an * 255 * s[2] + (d[2] * ao * (255 - an))) / d[3] + 255) >> 8;
					}
				}
				d += 4;
				s += 4;
			}
			src += srcstride;
			dst -= stride * 4;
		}

	} else if(m_cPixFmt == DSLAPixFmt_RGB24){

		dst += ( (height - sp.pos.top - 1) * stride + sp.pos.left) * 3;

		for(UINT y = 0; y < srcheight; y++) {
			BYTE *d = dst;
			BYTE *s = src;
			for(UINT x = 0; x < srcwidth; x++) {
				const BYTE an = s[3];
				if(an == 0){
					continue;
				} else if(an == 255) {
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
				} else {
					d[0] = (an * s[0] + (255 - an) * d[0] + 255) >> 8;
					d[1] = (an * s[1] + (255 - an) * d[1] + 255) >> 8;
					d[2] = (an * s[2] + (255 - an) * d[2] + 255) >> 8;
				}
				d += 3;
				s += 4;
			}
			src += srcstride;
			dst -= stride * 3;
		}
	}
}

void CSubPicRenderer::BlendChroma(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	UINT i, j;
	BYTE *dataU = m_pChromaBuf.get();
	BYTE *dataV = dataU + stride * height;
	BYTE *src = sp.pixels.get();

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;

	dataU += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			dataU[j] = (s[3] * s[1] + (255 - s[3]) * dataU[j] + 255) >> 8;
			s += 4;
		}
		src += srcstride;
		dataU += stride;
	}

	src = sp.pixels.get();

	dataV += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			dataV[j] = (s[3] * s[2] + (255 - s[3]) * dataV[j] + 255) >> 8;
			s += 4;
		}
		src += srcstride;
		dataV += stride;
	}

}

void CSubPicRenderer::BlendChroma_16(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	UINT i, j;
	WORD *dataU = (WORD *) m_pChromaBuf.get();
	WORD *dataV = dataU + stride * height;
	BYTE *src = sp.pixels.get();

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;

	dataU += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			const WORD an = s[3] << 8;
			const WORD un = s[1] << 8;
			dataU[j] = (an * un + (65535 - an) * dataU[j] + 65535) >> 16;
			s += 4;
		}
		src += srcstride;
		dataU += stride;
	}

	src = sp.pixels.get();

	dataV += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			const WORD an = s[3] << 8;
			const WORD vn = s[2] << 8;
			dataV[j] = (an * vn + (65535 - an) * dataV[j] + 65535) >> 16;
			s += 4;
		}
		src += srcstride;
		dataV += stride;
	}

}

void CSubPicRenderer::BlendYV12(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	UINT i, j;
	BYTE *dataY = m_pExtBuf;
	BYTE *dataV = dataY + stride * height;
	BYTE *dataU = dataV + (stride >> 1) * (height >> 1);
	BYTE *chrU = m_pChromaBuf.get();
	BYTE *chrV = chrU + stride * height;
	BYTE *src = sp.pixels.get();

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;
	const UINT srcwidth_m2 = ((sp.pos.right + 1) >> 1) - (sp.pos.left >> 1);
	const UINT srcheight_m2 = ((sp.pos.bottom + 1) >> 1) - (sp.pos.top >> 1);
	const UINT dst_offset = ((sp.pos.top >> 1) * (stride >> 1)) + (sp.pos.left >> 1);
	const UINT chr_offset = (sp.pos.top - (sp.pos.top & 1)) * stride + (sp.pos.left - (sp.pos.left & 1));
	
	dataY += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			dataY[j] = (s[3] * s[0] + (255 - s[3]) * dataY[j] + 255) >> 8;
			s += 4;
		}
		src += srcstride;
		dataY += stride;
	}

	dataV += dst_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chr = chrV;
		BYTE *chr_next = chrV + stride;
		for(j = 0; j < srcwidth_m2; j++) {
			chr[j << 1] = chr[(j << 1) + 1] = chr_next[j << 1] = chr_next[(j << 1) + 1] = dataV[j];
		}
		dataV += stride >> 1;
		chrV += stride * 2;
	}

	dataU += dst_offset;
	chrU += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chr = chrU;
		BYTE *chr_next = chrU + stride;
		for(j = 0; j < srcwidth_m2; j++) {
			chr[j << 1] = chr[(j << 1) + 1] = chr_next[j << 1] = chr_next[(j << 1) + 1] = dataU[j];
		}
		dataU += stride >> 1;
		chrU += stride * 2;
	}

	BlendChroma(sp);

	dataV = m_pExtBuf + stride * height;
	chrV = m_pChromaBuf.get() + stride * height;
	dataV += dst_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chr = chrV;
		BYTE *chr_next = chrV + stride;
		for(j = 0; j < srcwidth_m2; j++) {
			 dataV[j] = (chr[j << 1] + chr[(j << 1) + 1] + chr_next[j << 1] + chr_next[(j << 1) + 1]) >> 2;
		}
		dataV += stride >> 1;
		chrV += stride * 2;
	}

	dataU = m_pExtBuf + stride * height + (stride >> 1) * (height >> 1);
	chrU = m_pChromaBuf.get();
	dataU += dst_offset;
	chrU += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chr = chrU;
		BYTE *chr_next = chrU + stride;
		for(j = 0; j < srcwidth_m2; j++) {
			dataU[j] = (chr[j << 1] + chr[(j << 1) + 1] + chr_next[j << 1] + chr_next[(j << 1) + 1]) >> 2;
		}
		dataU += stride >> 1;
		chrU += stride * 2;
	}
}

void CSubPicRenderer::BlendNV12(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	UINT i, j;
	BYTE *dataY = m_pExtBuf;
	BYTE *dataC = dataY + stride * height;
	BYTE *src = sp.pixels.get();
	BYTE *chrU = m_pChromaBuf.get();
	BYTE *chrV = chrU + stride * height;

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;
	const UINT srcwidth_m2 = ((sp.pos.right + 1) >> 1) - (sp.pos.left >> 1);
	const UINT srcheight_m2 = ((sp.pos.bottom + 1) >> 1) - (sp.pos.top >> 1);
	const UINT dst_offset = (sp.pos.top >> 1) * stride + (sp.pos.left - (sp.pos.left & 1));
	const UINT chr_offset = (sp.pos.top - (sp.pos.top & 1)) * stride + (sp.pos.left - (sp.pos.left & 1));
	
	dataY += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			dataY[j] = (s[3] * s[0] + (255 - s[3]) * dataY[j] + 255) >> 8;
			s += 4;
		}
		src += srcstride;
		dataY += stride;
	}

	dataC += dst_offset;
	chrU += chr_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chU = chrU;
		BYTE *chU_next = chrU + stride;
		BYTE *chV = chrV;
		BYTE *chV_next = chrV + stride;
		for(j = 0; j < srcwidth_m2 << 1; j+=2) {
			chU[j] = chU[j + 1] = chU_next[j] = chU_next[j + 1] = dataC[j];
			chV[j] = chV[j + 1] = chV_next[j] = chV_next[j + 1] = dataC[j + 1];
		}
		dataC += stride;
		chrU += stride * 2;
		chrV += stride * 2;
	}

	BlendChroma(sp);

	dataC = m_pExtBuf + stride * height;
	dataC += dst_offset;
	chrU = m_pChromaBuf.get();
	chrV = chrU + stride * height;
	chrU += chr_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		BYTE *chU = chrU;
		BYTE *chU_next = chrU + stride;
		BYTE *chV = chrV;
		BYTE *chV_next = chrV + stride;
		for(j = 0; j < srcwidth_m2 << 1; j+=2) {
			dataC[j] = (chU[j] + chU[j + 1] + chU_next[j] + chU_next[j + 1]) >> 2;
			dataC[j + 1] = (chV[j] + chV[j + 1] + chV_next[j] + chV_next[j + 1]) >> 2;
		}
		dataC += stride;
		chrU += stride * 2;
		chrV += stride * 2;
	}
}

void CSubPicRenderer::BlendP01(SubPicture& sp) {

	const UINT stride = m_cWidth;
	const UINT height = abs(m_cHeight);
	UINT i, j;
	WORD *dataY = (WORD *) m_pExtBuf;
	WORD *dataC = dataY + stride * height;
	BYTE *src = sp.pixels.get();
	WORD *chrU = (WORD *) m_pChromaBuf.get();
	WORD *chrV = chrU + stride * height;

	const UINT srcwidth = sp.pos.right - sp.pos.left;
	const UINT srcstride = srcwidth * 4;
	const UINT srcheight = sp.pos.bottom - sp.pos.top;
	const UINT srcwidth_m2 = ((sp.pos.right + 1) >> 1) - (sp.pos.left >> 1);
	const UINT srcheight_m2 = ((sp.pos.bottom + 1) >> 1) - (sp.pos.top >> 1);
	const UINT dst_offset = (sp.pos.top >> 1) * stride + (sp.pos.left - (sp.pos.left & 1));
	const UINT chr_offset = (sp.pos.top - (sp.pos.top & 1)) * stride + (sp.pos.left - (sp.pos.left & 1));
	
	dataY += sp.pos.top * stride + sp.pos.left;

	for(i = 0; i < srcheight; i++) {
		BYTE *s = src;
		for (j = 0; j < srcwidth; j++) {
			const WORD yn = s[0] << 8;
			const WORD an = s[3] << 8;
			dataY[j] = (an * yn + (65535 - an) * dataY[j] + 65535) >> 16;
			s += 4;
		}
		src += srcstride;
		dataY += stride;
	}

	dataC += dst_offset;
	chrU += chr_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		WORD *chU = chrU;
		WORD *chU_next = chrU + stride;
		WORD *chV = chrV;
		WORD *chV_next = chrV + stride;
		for(j = 0; j < srcwidth_m2 << 1; j+=2) {
			chU[j] = chU[j + 1] = chU_next[j] = chU_next[j + 1] = dataC[j];
			chV[j] = chV[j + 1] = chV_next[j] = chV_next[j + 1] = dataC[j + 1];
		}
		dataC += stride;
		chrU += stride * 2;
		chrV += stride * 2;
	}

	BlendChroma_16(sp);

	dataC = (WORD *) m_pExtBuf + stride * height;
	dataC += dst_offset;
	chrU = (WORD *) m_pChromaBuf.get();
	chrV = chrU + stride * height;
	chrU += chr_offset;
	chrV += chr_offset;

	for(i = 0; i < srcheight_m2; i++) {
		WORD *chU = chrU;
		WORD *chU_next = chrU + stride;
		WORD *chV = chrV;
		WORD *chV_next = chrV + stride;
		for(j = 0; j < srcwidth_m2 << 1; j+=2) {
			dataC[j] = (chU[j] + chU[j + 1] + chU_next[j] + chU_next[j + 1]) >> 2;
			dataC[j + 1] = (chV[j] + chV[j + 1] + chV_next[j] + chV_next[j + 1]) >> 2;
		}
		dataC += stride;
		chrU += stride * 2;
		chrV += stride * 2;
	}
}

HRESULT BitBltRGB(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut, BYTE bpp) {

	UINT y = 0;

	if((strideIn < width) || (strideOut < width))
		return E_FAIL;

	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width * bpp);
		pBufIn += strideIn * bpp;
		pBufOut += strideOut * bpp;
	}
	return NOERROR;

}

HRESULT BitBltRGBWithFlip(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut, BYTE bpp) {

	UINT y = 0;

	if((strideIn < width) || (strideOut < width))
		return E_FAIL;

	pBufIn += strideIn * bpp * (height - 1);

	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width * bpp);
		pBufIn -= strideIn * bpp;
		pBufOut += strideOut * bpp;
	}
	return NOERROR;

}

HRESULT BitBltYV12(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut) {

	UINT y = 0;

	if((strideIn < width) || (strideOut < width))
		return E_FAIL;

	//the Y plane
	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width);
		pBufIn += strideIn;
		pBufOut += strideOut;
	}

	//the V and U planes
	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width >> 1);
		pBufIn += ((strideIn + 1) >> 1);
		pBufOut += ((strideOut + 1) >> 1);
	}

	return NOERROR;

}

HRESULT BitBltNV12(BYTE *pBufIn, BYTE *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut) {

	UINT y = 0;

	if((strideIn < width) || (strideOut < width))
		return E_FAIL;

	//the Y plane
	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width);
		pBufIn += strideIn;
		pBufOut += strideOut;
	}

	//U and V planes
	for(y = 0; y < (height + 1) >> 1; y++) {
		memcpy_amd(pBufOut, pBufIn, width);
		pBufIn += strideIn;
		pBufOut += strideOut;
	}
	return NOERROR;

}

HRESULT BitBltP01(WORD *pBufIn, WORD *pBufOut, UINT height, UINT width, UINT strideIn, UINT strideOut) {

	UINT y = 0;

	if((strideIn < width) || (strideOut < width))
		return E_FAIL;

	//the Y plane
	for(y = 0; y < height; y++) {
		memcpy_amd(pBufOut, pBufIn, width << 1);
		pBufIn += strideIn;
		pBufOut += strideOut;
	}

	//U and V planes
	for(y = 0; y < (height + 1) >> 1; y++) {
		memcpy_amd(pBufOut, pBufIn, width << 1);
		pBufIn += strideIn;
		pBufOut += strideOut;
	}
	return NOERROR;

}

