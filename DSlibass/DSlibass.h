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

#include "DSlibassSettings.h"
#include "DSLAInputPin.h"
#include "SubPicRenderer.h"

#define DSLA_REGISTRY_KEY L"Software\\DSlibass"

inline DSLAPixFmts PixFmtResolve(const GUID& subtype) {

	if(subtype == MEDIASUBTYPE_YV12) {
		return DSLAPixFmt_YV12;
	} else
	if(subtype == MEDIASUBTYPE_NV12) {
		return DSLAPixFmt_NV12;
	} else
	if(subtype == MEDIASUBTYPE_P010) {
		return DSLAPixFmt_P010;
	} else
	if(subtype == MEDIASUBTYPE_P016) {
		return DSLAPixFmt_P016;
	} else
	if(subtype == MEDIASUBTYPE_RGB32) {
		return DSLAPixFmt_RGB32;
	} else
	if(subtype == MEDIASUBTYPE_RGB24) {
		return DSLAPixFmt_RGB24;
	} else {
		return DSLAPixFmt_None;
	}

}

inline const GUID PixFmtRResolve(const DSLAPixFmts subtype) {

	if(subtype == DSLAPixFmt_YV12) {
		return MEDIASUBTYPE_YV12;
	} else
	if(subtype == DSLAPixFmt_NV12) {
		return MEDIASUBTYPE_NV12;
	} else
	if(subtype == DSLAPixFmt_P010) {
		return MEDIASUBTYPE_P010;
	} else
	if(subtype == DSLAPixFmt_P016) {
		return MEDIASUBTYPE_P016;
	} else
	if(subtype == DSLAPixFmt_RGB32) {
		return MEDIASUBTYPE_RGB32;
	} else
	if(subtype == DSLAPixFmt_RGB24) {
		return MEDIASUBTYPE_RGB24;
	} else {
		return GUID_NULL;
	}
	
}


//{6D34B559-3E47-4FF6-A186-4EDA3391C36A}
DEFINE_GUID(CLSID_DSlibass, 
0x6d34b559, 0x3e47, 0x4ff6, 0xa1, 0x86, 0x4e, 0xda, 0x33, 0x91, 0xc3, 0x6a);
class CDSlibass : public CTransformFilter, public IDSLASettings, public ISpecifyPropertyPages {

	friend class CSubInputPin;

public:
	CDSlibass(LPUNKNOWN punk, HRESULT *phr);
	~CDSlibass(void);

	//IUnknown
    DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	//ISpecifyPropertyPages
	STDMETHODIMP GetPages(CAUUID *pPages);

	//IDSLASettings
	STDMETHODIMP_(WORD) GetActiveTrackNum();
	STDMETHODIMP SetActiveTrackNum(WORD trackNum);

	STDMETHODIMP_(WORD) GetTracksCount();
	STDMETHODIMP_(WCHAR*) GetTrackName(WORD trackNum);

	STDMETHODIMP_(WORD) GetSubtitlePathsCount();
	STDMETHODIMP_(WCHAR*) GetSubtitlePath(WORD pathNum);
	STDMETHODIMP AddSubtitlePath(WCHAR *path);

	STDMETHODIMP_(INT) GetSubtitleDelay();
	STDMETHODIMP SetSubtitleDelay(INT delay);

	STDMETHODIMP_(DSLAMatrixModes) GetColorMatrix();
	STDMETHODIMP SetColorMatrix(DSLAMatrixModes colorMatrix);

	STDMETHODIMP_(BOOL) GetPixFmts(DSLAPixFmts fmt);
	STDMETHODIMP SetPixFmts(DSLAPixFmts fmt, BOOL enabled);

	STDMETHODIMP SaveSettings();
	
	//CTransformFilter
	HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc,
                             ALLOCATOR_PROPERTIES *pProperties);
	HRESULT CompleteConnect(PIN_DIRECTION direction,IPin *pReceivePin);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
	HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt);
	CBasePin* GetPin(int n);
	int GetPinCount();

private:
	
	HRESULT CopyFrame(BYTE *In, BYTE *Out, VIDEOINFOHEADER2 *viIn, VIDEOINFOHEADER2 *viOut);

	HRESULT Initialise(VIDEOINFOHEADER2 *vi);
	HRESULT LoadSubtitleSources();
	int LookForExternalSubtitles(std::wstring Path, std::wstring FileName, std::wstring Extension);
	int LookForASS(std::wstring Path, std::wstring FileName);
	int LookForSRT(std::wstring Path, std::wstring FileName);
	int LookForMKS(std::wstring Path, std::wstring FileName);
	HRESULT LoadEmbeddedFonts(std::wstring FileName);
	HRESULT LoadExternalFonts(std::wstring Path);

	std::unique_ptr<CSubInputPin> m_pSubInput;
	bool m_bInitialised;
	std::unique_ptr<CSubPicRenderer> m_pRenderer;
	DSLAPixFmts m_cCurrentPixFmt;

	std::vector<std::wstring> m_subtitlePaths;
	std::vector<std::wstring> m_vTracks;
	BOOL m_cEnabledPixFmts[DSLAPixFmts_NB];
	BOOL m_cSupportedPixFmts[DSLAPixFmts_NB];
	int m_subtitleDelay;
	DSLAMatrixModes m_cCurrentColorMatrix;
	WORD m_cCurrentTrack;

};

