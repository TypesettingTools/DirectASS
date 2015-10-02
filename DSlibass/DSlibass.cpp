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
#include "dslauids.h"
#include "DSUtils.h"
#include "registry.h"
#include "MatroskaParser.h"
#include "LibassWrapper.h"
#include "DSlibass.h"
#include "DSLARendererProp.h"
#include "SubInputPin.h"
#include <Shlwapi.h>

using namespace std; 

CDSlibass::CDSlibass(LPUNKNOWN punk,
                   HRESULT *phr) :
    CTransformFilter(NAME("DSlibass Subtitle Renderer"), punk, CLSID_DSlibass),
	m_pRenderer(nullptr),
	m_bInitialised(false),
	m_pSubInput(nullptr),
	m_cCurrentPixFmt(DSLAPixFmt_None),
	m_subtitleDelay(0),
	m_cCurrentColorMatrix(DSLAMatrixMode_BT601),
	m_cCurrentTrack(0)
{

	DbgSetModuleLevel(LOG_ERROR | LOG_LOCKING | LOG_TRACE | LOG_MEMORY | LOG_TIMING, 5);

	DbgLog((LOG_MEMORY, 3, L"CDSlibass: creating"));

	HRESULT hr;
	CRegistry reg(HKEY_CURRENT_USER, DSLA_REGISTRY_KEY, hr);
	if (SUCCEEDED(hr)) {
		for (int i = 0; i < DSLAPixFmts_NB; i++) {
			bool value = reg.ReadBool(RegistryMediaValuesNames[i], hr);
			if (hr == S_OK)
				m_cEnabledPixFmts[i] = value;
			else 
				m_cEnabledPixFmts[i] = true;
		}
		DWORD intVal = reg.ReadDWORD(L"SubtitleDelay", hr);
		if (hr == S_OK)
			m_subtitleDelay = intVal;
		intVal = reg.ReadDWORD(L"ColorMatrix", hr);
		if (hr == S_OK)
			m_cCurrentColorMatrix = (DSLAMatrixModes)intVal;
	} else {
		for(int i = 0; i < DSLAPixFmts_NB; i++) {
			m_cEnabledPixFmts[i] = true;
		}
	}

	for(int i = 0; i < DSLAPixFmts_NB; i++) {
		m_cSupportedPixFmts[i] = true;
	}

	m_subtitlePaths.push_back(L".\\");
	m_subtitlePaths.push_back(L"Subtitles\\");

	return;
}

CDSlibass::~CDSlibass(void)
{
	DbgLog((LOG_MEMORY, 3, L"CDSlibass: destroying"));
	SAFE_DELETE(m_pOutput);
	SAFE_DELETE(m_pInput);
}

STDMETHODIMP CDSlibass::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  return (riid == __uuidof(ISpecifyPropertyPages)) ? GetInterface((ISpecifyPropertyPages *)this, ppv) :
		(riid == __uuidof(IDSLASettings)) ? GetInterface((IDSLASettings *)this, ppv) :
    __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CDSlibass::GetPages(CAUUID *pPages) {

	CheckPointer(pPages, E_POINTER);

	pPages->cElems = 1;
	pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
	if (pPages->pElems == nullptr) 
	{
		return E_OUTOFMEMORY;
	}

	pPages->pElems[0] = CLSID_DSLARendererProp;
	return S_OK;

}

HRESULT CDSlibass::Transform(IMediaSample *pIn, IMediaSample *pOut)
{
	CheckPointer(pIn,E_POINTER);
	CheckPointer(pOut,E_POINTER);

	BYTE *pInData;
	BYTE *pOutData;
	
	pIn->GetPointer(&pInData);
	pOut->GetPointer(&pOutData);

	CheckPointer(pInData,E_POINTER);
	CheckPointer(pOutData,E_POINTER);

	HRESULT hr;
	CMediaType *pInMT = nullptr;
    if (pIn->GetMediaType((AM_MEDIA_TYPE**)&pInMT) == S_OK)
    {
		DbgLog((LOG_TRACE, 3, L"CDSlibass::Transform: new media type attached to input sample, switching..."));
		DisplayType(L"Media type:", pInMT);
		hr = m_pInput->CheckMediaType(pInMT);
		if(SUCCEEDED(hr))
		{
			hr = m_pInput->SetMediaType(pInMT);
			if(FAILED(hr)){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: failed to switch input media type, SetMediaType returned %#x", hr));
				DeleteMediaType(pInMT);
				return E_FAIL;
			}
			DeleteMediaType(pInMT);
		} else {
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: failed to switch input media type, CheckInputType returned %#x", hr));
			DeleteMediaType(pInMT);
			return E_FAIL;
		}
	}
	
	CMediaType *pOutMT = nullptr;
    if (pOut->GetMediaType((AM_MEDIA_TYPE**)&pOutMT) == S_OK)
    {
		DbgLog((LOG_TRACE, 3, L"CDSlibass::Transform: new media type attached to output sample, switching..."));
		DisplayType(L"Media type:", pInMT);
		hr = m_pOutput->CheckMediaType(pOutMT);
		if(SUCCEEDED(hr))
		{
			hr = m_pOutput->SetMediaType(pOutMT);
			if(FAILED(hr)){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: failed to switch output media type, SetMediaType returned %#x", hr));
				DeleteMediaType(pInMT);
				return E_FAIL;
			}
			DeleteMediaType(pOutMT);
		} else {
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: failed to switch output media type, CheckInputType returned %#x", hr));
			DeleteMediaType(pOutMT);
			return E_FAIL;
		}
	}
	

	const CMediaType& pInType = m_pInput->CurrentMediaType();
	VIDEOINFOHEADER2 *viIn = (VIDEOINFOHEADER2 *) pInType.pbFormat;
	const CMediaType& pOutType = m_pOutput->CurrentMediaType();
	VIDEOINFOHEADER2 *viOut = (VIDEOINFOHEADER2 *) pOutType.pbFormat;

	CRefTime tStart, tStop ;
	pIn->GetTime((REFERENCE_TIME *) &tStart, (REFERENCE_TIME *) &tStop);

	CRefTime tSegStart;
	
	tSegStart = m_pInput->CurrentStartTime();

	DbgLog((LOG_TRACE, 3, L"CDSlibass::Transform: transforming frame at time %d", tStart.Millisecs() + tSegStart.Millisecs()));
	if(FAILED(m_pRenderer->RenderFrame(pInData, tStart.Millisecs() + tSegStart.Millisecs() - m_subtitleDelay, m_cCurrentTrack))) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: RenderFrame failed"));
		return E_FAIL;
	}

	DbgLog((LOG_TRACE, 3, L"CDSlibass::Transform: copying frame at time %d", tStart.Millisecs() + tSegStart.Millisecs()));
	if(FAILED(CopyFrame(pInData, pOutData, viIn, viOut))) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: CopyFrame failed"));
		return E_FAIL;
	}

	REFERENCE_TIME TimeStart, TimeEnd;
    if (NOERROR == pIn->GetTime(&TimeStart, &TimeEnd)) {
        pOut->SetTime(&TimeStart, &TimeEnd);
    }

    LONGLONG MediaStart, MediaEnd;
    if (pIn->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR) {
        pOut->SetMediaTime(&MediaStart,&MediaEnd);
    }

    hr = pIn->IsSyncPoint();
    switch(hr){
		case S_OK : pOut->SetSyncPoint(TRUE);
			break;
		case S_FALSE : pOut->SetSyncPoint(FALSE);
			break;
		default :  return E_UNEXPECTED;
	}	


	hr = pIn->IsPreroll();
    switch(hr){
		case S_OK : pOut->SetPreroll(TRUE);
			break;
		case S_FALSE : pOut->SetPreroll(FALSE);
			break;
		default :  return E_UNEXPECTED;
	}
    
    hr = pIn->IsDiscontinuity();
    switch(hr){
		case S_OK : pOut->SetDiscontinuity(TRUE);
			break;
		case S_FALSE : pOut->SetDiscontinuity(FALSE);
			break;
		default :  return E_UNEXPECTED;
	}
	
	return NOERROR;

}

HRESULT CDSlibass::CopyFrame(BYTE *In, BYTE *Out, VIDEOINFOHEADER2 *viIn, VIDEOINFOHEADER2 *viOut) {

	CheckPointer(In, E_POINTER);
	CheckPointer(Out, E_POINTER);
	CheckPointer(viIn, E_POINTER);
	CheckPointer(viOut, E_POINTER);

	switch(m_cCurrentPixFmt) {

	case DSLAPixFmt_RGB32:
		if((viIn->bmiHeader.biHeight * viOut->bmiHeader.biHeight) < 0){
			if(FAILED(BitBltRGBWithFlip(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth, 4))){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: (BitBltRGBWithFlip failed"));
				return E_FAIL;
			}
		} else {
			if(FAILED(BitBltRGB(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth, 4))){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltRGB failed"));
				return E_FAIL;
			}
		}
		break;

	case DSLAPixFmt_RGB24:
		if((viIn->bmiHeader.biHeight * viOut->bmiHeader.biHeight) < 0){
			if(FAILED(BitBltRGBWithFlip(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth, 3))){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltRGBWithFlip failed"));
				return E_FAIL;
			}
		} else {
			if(FAILED(BitBltRGB(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth, 3))){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltRGB failed"));
				return E_FAIL;
			}
		}
		break;

	case DSLAPixFmt_YV12:
		if(FAILED(BitBltYV12(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth))){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltYV12 failed"));
			return E_FAIL;
		}
		break;

	case DSLAPixFmt_NV12:
		if(FAILED(BitBltNV12(In, Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth))){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltNV12 failed"));
			return E_FAIL;
		}
		break;

	case DSLAPixFmt_P010:
	case DSLAPixFmt_P016:
		if(FAILED(BitBltP01((WORD *)In, (WORD *)Out, viOut->rcTarget.bottom, viOut->rcTarget.right, viIn->bmiHeader.biWidth, viOut->bmiHeader.biWidth))){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Transform: BitBltP01 failed"));
			return E_FAIL;
		}
		break;
	}

	return S_OK;

}

HRESULT CDSlibass::LoadEmbeddedFonts(wstring FileName){

	DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadEmbeddedFonts: parsing %s", FileName.c_str()));
	CMatroskaParser parser;

	vector<MatroskaAttachment> atts;

	if(FAILED(parser.Open(FileName))){
		DbgLog((LOG_ERROR, 3, L"CDSlibass::LoadEmbeddedFonts: parser->Open failed"));
		return E_FAIL;	
	}

	if(FAILED(parser.ParseAttachments(atts))){
		DbgLog((LOG_ERROR, 3, L"CDSlibass::LoadEmbeddedFonts: parser->ParseAttachments failed"));
		return E_FAIL;
	}

	char temp[4096] = {0};

	for(auto i = atts.begin(); i < atts.end(); i++){

		if((*i).MimeType == string("application/x-truetype-font") ||
			(*i).MimeType == string("application/x-font") ||
			(*i).MimeType == string("application/vnd.ms-opentype")){

			DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadEmbeddedFonts: Installing font: %s", (*i).FileName.c_str()));

			WideCharToMultiByte(CP_UTF8, 0, (*i).FileName.c_str(), -1, temp, 4096, nullptr, nullptr);
			m_pRenderer->GetLibass()->AddFont(temp, (BYTE *) (*i).FileData.get(), (*i).FileSize);
		}

	}

	return S_OK;
}

HRESULT CDSlibass::LoadExternalFonts(wstring Path) {

	DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadExternalFonts: looking for fonts in %s", Path.c_str() ));

	WIN32_FIND_DATA findData;

	vector<wstring> font_ext;

	font_ext.push_back(L"*.ttf");
	font_ext.push_back(L"*.ttc");
	font_ext.push_back(L"*.otf");
	
	for(auto i = font_ext.begin(); i < font_ext.end(); i++) {
		wstring temp(Path + *i);
		auto hSearch = FindFirstFile(temp.c_str(), &findData);
		if(hSearch != INVALID_HANDLE_VALUE){
			do {
				temp = Path + wstring(findData.cFileName);
				DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadExternalFonts: loading font from %s", temp.c_str() ));
				ifstream font(temp, ios::binary);

				auto fontData = unique_ptr<BYTE>(new BYTE[findData.nFileSizeLow]);
				font.read((char *)fontData.get(), findData.nFileSizeLow);

				char mb_temp[4096] = {0};
				WideCharToMultiByte(CP_UTF8, 0, temp.c_str(), -1, mb_temp, 4096, nullptr, nullptr);
				m_pRenderer->GetLibass()->AddFont(mb_temp, fontData.get(), findData.nFileSizeLow);

			} while (FindNextFile(hSearch, &findData));
		}
		FindClose(hSearch);
	}

	return S_OK;
}

int CDSlibass::LookForExternalSubtitles(wstring Path, wstring FileName, wstring Extension){

	DbgLog((LOG_TRACE, 3, L"CDSlibass::LookForExternalSubtitles: looking in %s for %s", Path.c_str(), wstring(FileName + Extension).c_str()));

	WIN32_FIND_DATA findData;

	int trackCount = 0;

	auto temp = Path + FileName + Extension;

	if(!(Extension.find(L"ass") != wstring::npos || 
		Extension.find(L"ssa") != wstring::npos ||
		Extension.find(L"srt") != wstring::npos ||
		Extension.find(L"mks") != wstring::npos)) {
		return 0;
	}

	auto hSearch = FindFirstFile(temp.c_str(), &findData);
	if(hSearch != INVALID_HANDLE_VALUE){
		do {
			temp = Path + wstring(findData.cFileName);
			DbgLog((LOG_TRACE, 3, L"CDSlibass::LookForExternalSubtitles: adding file %s", temp.c_str()));
			if(Extension.find(L"mks") != wstring::npos) {
				
				DbgLog((LOG_TRACE, 3, L"CDSlibass::LookForExternalSubtitles: parsing MKS file"));

				LoadEmbeddedFonts(temp);
				
				CMatroskaParser parser;

				if(FAILED(parser.Open(temp))){
					DbgLog((LOG_ERROR, 3, L"CDSlibass::LookForExternalSubtitles: parser->Open failed"));
					continue;
				}

				UINT trackNum = parser.GetSubtitleTracksNumber();
				
				for(UINT i = 0; i < trackNum; i++) {
					DbgLog((LOG_TRACE, 3, L"CDSlibass::LookForExternalSubtitles: adding MKS track #%d", trackNum));
					MatroskaASSTrack track;
					if(FAILED(parser.ParseSubtitleTrack(track, i))){
						DbgLog((LOG_ERROR, 3, L"CDSlibass::LookForExternalSubtitles: parser->ParseSubtitleTrack failed"));
						continue;
					}

					m_pRenderer->GetLibass()->AddMKSTrack((BYTE *)track.header.c_str(), track.header.size());
					WCHAR TrackName[BUFSIZ] = {0};
					swprintf_s(TrackName, BUFSIZ, L"%s: %s", findData.cFileName, track.name.c_str());
					m_vTracks.push_back(TrackName);
					trackCount++;

					for(auto j = track.events.begin(); j < track.events.end(); j++) {
						m_pRenderer->GetLibass()->MKSProcessChunk((BYTE *)(*j).subtitle.c_str(), 
							(*j).subtitle.size(), (*j).timecode, (*j).duration, m_vTracks.size() - 1);
					}

				}

			} else {
				m_pRenderer->GetLibass()->AddFileTrack(temp);
				m_vTracks.push_back(findData.cFileName);
				trackCount++;
			}
		} while (FindNextFile(hSearch, &findData));
	}
	FindClose(hSearch);

	return trackCount;
}

HRESULT CDSlibass::LoadSubtitleSources(){

	DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: entering"));

	wstring FileName;

	BOOL embFlag = false;
	int trackCount = 0;

	DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: enumerating filters and pins"));

	IEnumFilters *pEnumFilters = nullptr;
	if(m_pGraph && SUCCEEDED(m_pGraph->EnumFilters(&pEnumFilters)))
	{
		for(IBaseFilter *pBaseFilter = nullptr; S_OK == pEnumFilters->Next(1, &pBaseFilter, nullptr);)
		{
			IFileSourceFilter *pFSF = nullptr;
			if(S_OK == pBaseFilter->QueryInterface(IID_IFileSourceFilter, (void **) &pFSF)) {

				LPOLESTR fnw = nullptr;
				if(!pFSF || FAILED(pFSF->GetCurFile(&fnw, nullptr)) || !fnw) {
					continue;
				}
				DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: IFileSourceFilter found, getting video file name"));
				FileName.assign(fnw);
				CoTaskMemFree(fnw);
				SafeRelease(&pFSF);
			}

			IEnumPins *pEnumPins = nullptr;
			if(pBaseFilter && SUCCEEDED(pBaseFilter->EnumPins(&pEnumPins)))
			{
				for(IPin *pPin = nullptr; S_OK == pEnumPins->Next(1, &pPin, 0); )
				{
					IEnumMediaTypes *pEnumMediaTypes;
					if(pPin && SUCCEEDED(pPin->EnumMediaTypes(&pEnumMediaTypes)))
					{
						AM_MEDIA_TYPE* pMediaType = nullptr;
						for(; S_OK == pEnumMediaTypes->Next(1, &pMediaType, nullptr); DeleteMediaType(pMediaType), pMediaType = nullptr)
						{
							if((pMediaType->majortype == MEDIATYPE_Subtitle) &&
								(pMediaType->subtype == MEDIASUBTYPE_SSA || pMediaType->subtype == MEDIASUBTYPE_ASS || pMediaType->subtype == MEDIASUBTYPE_ASS2
								|| pMediaType->subtype == MEDIASUBTYPE_UTF8)) {
								DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: subtitle pin found"));
								m_pRenderer->GetLibass()->AddPinTrack();
								if(m_vTracks.empty()){
									m_vTracks.push_back(L"Embedded");
								}
								embFlag = true;
								trackCount++;
								break;
							}
						}
						if(pMediaType) 
							DeleteMediaType(pMediaType);
					}
					SafeRelease(&pEnumMediaTypes);
					SafeRelease(&pPin);
				}
			}
			SafeRelease(&pEnumPins);
			SafeRelease(&pBaseFilter);
		}
	}
	SafeRelease(&pEnumFilters);

	if(embFlag)
		LoadEmbeddedFonts(FileName);

	//extracting path from the file name
	wstring Path = FileName;
	Path = Path.substr(0, Path.find_last_of('\\') + 1);

	//stripping path and extension from the file name
	wstring FileNameStripped = FileName;
	FileNameStripped = FileNameStripped.substr(FileNameStripped.find_last_of('\\') + 1);
	FileNameStripped = FileNameStripped.substr(0, FileNameStripped.find_last_of('.'));

	for(auto i = m_subtitlePaths.begin(); i < m_subtitlePaths.end(); i++){

		DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: looking for external subtitles in %s", wstring(Path + *i).c_str()));

		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".ass");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".ssa");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".srt");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".mks");

		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".*.ass");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".*.ssa");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".*.srt");
		trackCount += LookForExternalSubtitles(Path + *i, FileNameStripped, L".*.mks");

		LoadExternalFonts(Path + *i + L"Fonts\\");

	}

	if(trackCount == 0){
		DbgLog((LOG_TRACE, 3, L"CDSlibass::LoadSubtitleSources: no subtitle tracks found, rejecting connection"));
		return E_FAIL;
	}

	return S_OK;
}


HRESULT CDSlibass::Initialise(VIDEOINFOHEADER2 *vi){

	if(!m_bInitialised){

		DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialize: entering"));

		LibassSettings settings;
		settings.w = vi->rcTarget.right;
		settings.h = vi->rcTarget.bottom;
		settings.sar = (double) vi->bmiHeader.biWidth / (double) abs(vi->bmiHeader.biHeight);
		settings.dar = settings.sar; // this makes rendering on anamorphic video vsfilter-compatible

		DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: setting up CLibassWrapper"));
		DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: rendering target size: %dx%d", settings.w, settings.h));
		DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: SAR: %f", settings.sar));
		DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: DAR: %f", settings.dar));

		if(m_pRenderer == nullptr) {
			HRESULT hr = S_OK;
			DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: creating CLibassWrapper"));
			m_pRenderer = std::move(unique_ptr<CSubPicRenderer>(new CSubPicRenderer()));
			if(FAILED(hr)){
				DbgLog((LOG_TRACE, 3, L"CDSlibass::Initialise: CLibassWrapper creation failed"));
				return hr;
			}
		}

		if(FAILED(LoadSubtitleSources())){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Initialise: LoadSubtitle sources failed"));
			return E_FAIL;
		}

		if(FAILED(m_pRenderer->GetLibass()->InitRenderer(settings))){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::Initialise: InitRenderer failed"));
			return E_FAIL;
		}

		m_pRenderer->GetLibass()->SetFonts();

	}

	m_bInitialised = true;
	return NOERROR;
}

HRESULT CDSlibass::CompleteConnect(PIN_DIRECTION direction,IPin *pReceivePin)
{
	if(direction == PINDIR_OUTPUT) {

		DbgLog((LOG_TRACE, 3, L"CDSlibass::CompleteConnect: entering on PINDIR_OUTPUT"));

		CMediaType pmt = m_pOutput->CurrentMediaType();
		if(!(PixFmtResolve(pmt.subtype) == m_cCurrentPixFmt)
			&& SUCCEEDED(m_pInput->GetConnected()->QueryAccept(&pmt))) {
			DbgLog((LOG_TRACE, 3, L"CDSlibass::CompleteConnect: reconnecting input pin with new mediatype"));
			if(FAILED(ReconnectPin(m_pInput, &pmt))){
				DbgLog((LOG_ERROR, 3, L"CDSlibass::CompleteConnect: reconnecting failed"));
				return E_FAIL;
			}
			m_pInput->SetMediaType(&m_pInput->CurrentMediaType());
		}

	}

	return __super::CompleteConnect(direction, pReceivePin);
}


HRESULT CDSlibass::CheckInputType(const CMediaType *mtIn)
{
	DbgLog((LOG_TRACE, 3, L"CDSlibass::CheckInputType: checking"));
	DisplayType(L"Media type: ", mtIn);

    CheckPointer(mtIn,E_POINTER);
	
    if (*mtIn->FormatType() != FORMAT_VideoInfo2) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::CheckInputType: no VIDEOINFOHEADER2 in media type"));
        return E_INVALIDARG;
    }
	
    if(mtIn->majortype == MEDIATYPE_Video){
		GUID t = mtIn->subtype;

		if(PixFmtResolve(t) != -1 && m_cSupportedPixFmts[PixFmtResolve(t)]) {
			DbgLog((LOG_TRACE, 3, L"CDSlibass::CheckInputType: media type accepted"));
			return NOERROR;
		}
	}
	DbgLog((LOG_TRACE, 3, L"CDSlibass::CheckInputType: media type not accepted"));
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CDSlibass::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	DbgLog((LOG_TRACE, 3, L"CDSlibass::GetMediaType: media type #%d requested", iPosition));

	if (m_pInput->IsConnected() == FALSE) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::GetMediaType: input pin isn't connected", iPosition));
		return E_UNEXPECTED;
	}

	if (iPosition < 0) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::GetMediaType: iPosition < 0", iPosition));
		return E_INVALIDARG;
	}

	if(iPosition > DSLAPixFmts_NB) {
		DbgLog((LOG_TRACE, 3, L"CDSlibass::GetMediaType: iPosition too big", iPosition));
		return VFW_S_NO_MORE_ITEMS;
	}

	CheckPointer(pMediaType,E_POINTER);
	
	CMediaType &mtIn = m_pInput->CurrentMediaType();

	*pMediaType = mtIn;

	int i = 0;

	for(int j = 0; j < DSLAPixFmts_NB; j++) {

		if(m_cSupportedPixFmts[j] && (i++ == iPosition)) {

			pMediaType->subtype = *OutMediaTypes[j].subtype;
			VIDEOINFOHEADER2 *vi = (VIDEOINFOHEADER2 *) pMediaType->Format();
			vi->bmiHeader.biCompression = OutMediaTypes[j].biCompression;
			vi->bmiHeader.biBitCount = OutMediaTypes[j].biBitCount;
			vi->bmiHeader.biPlanes = OutMediaTypes[j].biPlanes;
			vi->bmiHeader.biSizeImage = (abs(vi->bmiHeader.biHeight) * vi->bmiHeader.biWidth) * vi->bmiHeader.biBitCount >> 3;
			pMediaType->SetSampleSize(vi->bmiHeader.biSizeImage);

			DbgLog((LOG_TRACE, 3, L"CDSlibass::GetMediaType: returning media type #%d", iPosition));	
			DisplayType(L"Media type: ", pMediaType);

			return NOERROR;

		}

	}
    
    return VFW_S_NO_MORE_ITEMS;
}

HRESULT CDSlibass::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	DbgLog((LOG_TRACE, 3, L"CDSlibass::CheckTransform: checking transform"));
	DisplayType(L"Input media type: ", mtIn);
	DisplayType(L"Output media type: ", mtOut);
	
    CheckPointer(mtIn,E_POINTER);
    CheckPointer(mtOut,E_POINTER);

	if(mtOut->majortype == MEDIATYPE_Video){
		GUID t = mtOut->subtype;

		if(PixFmtResolve(t) != -1 && m_cSupportedPixFmts[PixFmtResolve(t)]) {
			DbgLog((LOG_TRACE, 3, L"CDSlibass::CheckTransform: can transform"));
			return NOERROR;
		}
	}

	DbgLog((LOG_ERROR, 3, L"CDSlibass::CheckTransform: can not transform"));
	
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CDSlibass::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt){

	HRESULT hr = __super::SetMediaType(dir, pmt);
	if(FAILED(hr)) {
		return hr;
	}
	
	if(dir == PINDIR_INPUT) {

		DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: setting media type on input pin"));
		DisplayType(L"Media type: ", pmt);

		VIDEOINFOHEADER2 *pvi = (VIDEOINFOHEADER2 *) pmt->Format();

		if(!m_bInitialised){
			DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: calling Initialise"));
			hr = Initialise(pvi);
			if(FAILED(hr)) {
				DbgLog((LOG_ERROR, 3, L"CDSlibass::SetMediaType: Initialise failed"));
				return hr;
			}
		}
				
		IEnumMediaTypes *pEnum = nullptr;
		IPin *pUpstreamPin = nullptr;
		CMediaType *pMediaType = nullptr;
		BOOL cSupportedPixFmts[DSLAPixFmts_NB] = {false, false, false, false, false, false};

		DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: retrieving the upstream pin"));
		hr = m_pInput->ConnectedTo(&pUpstreamPin);
		if(FAILED(hr)) {
			DbgLog((LOG_ERROR, 3, L"CDSlibass::SetMediaType: ConnectedTo failed"));
			return hr;
		}

		if(SUCCEEDED(pUpstreamPin->EnumMediaTypes(&pEnum))){

			DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: enumerating media types on the upstream pin"));

			for(;S_OK == pEnum->Next(1, (AM_MEDIA_TYPE **)&pMediaType, nullptr);) {

				if(PixFmtResolve(pMediaType->subtype) != -1)
					cSupportedPixFmts[PixFmtResolve(pMediaType->subtype)] = true;
							
				if(pMediaType) {
					DeleteMediaType(pMediaType);
					pMediaType = nullptr;
				}
			}
			for(int i = 0; i < DSLAPixFmts_NB; i++) {
				m_cSupportedPixFmts[i] &= cSupportedPixFmts[i];
			}
		}

		if(!(PixFmtResolve(pmt->subtype) == m_cCurrentPixFmt)){
			m_cCurrentPixFmt = PixFmtResolve(pmt->subtype);
		}

		DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: setting up the renderer"));
		m_pRenderer->SetFrameParameters(pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight, m_cCurrentPixFmt);
		m_pRenderer->SetColorMatrix(m_cCurrentColorMatrix);

	} else if(dir == PINDIR_OUTPUT){

		DbgLog((LOG_TRACE, 3, L"CDSlibass::SetMediaType: setting media type on output pin"));
		DisplayType(L"Media type: ", pmt);

	}

	return __super::SetMediaType(dir, pmt);
}

CBasePin* CDSlibass::GetPin(int n) {

	HRESULT hr = S_OK;

	DbgLog((LOG_TRACE, 3, L"CDSlibass::GetPin: requested pin #%d", n));

	if (m_pInput == nullptr) {

		DbgLog((LOG_MEMORY, 3, L"CDSlibass::GetPin: creating CDSLAInputPin"));

		m_pInput = new CDSLAInputPin(this, &hr);
		ASSERT(SUCCEEDED(hr));
		if (m_pInput == nullptr) {
			DbgLog((LOG_ERROR, 3, L"CDSlibass::GetPin: CDSLAInputPin creation failed"));
			return nullptr;
		}

		DbgLog((LOG_MEMORY, 3, L"CDSlibass::GetPin: creating CTransformOutputPin"));

		m_pOutput = new CTransformOutputPin(NAME("Transform output pin"), this, &hr, L"XForm Out");
		ASSERT(SUCCEEDED(hr));
		if (m_pInput == nullptr) {
			SAFE_DELETE(m_pInput);
			DbgLog((LOG_ERROR, 3, L"CDSlibass::GetPin: CTransformOutputPin creation failed"));
			return nullptr;
		}

		DbgLog((LOG_MEMORY, 3, L"CDSlibass::GetPin: creating CSubInputPin"));

		m_pSubInput = std::move(unique_ptr<CSubInputPin>(new CSubInputPin(this, m_pLock, &hr)));
		ASSERT(SUCCEEDED(hr));
        if (m_pSubInput == nullptr) {
			SAFE_DELETE(m_pInput);
			SAFE_DELETE(m_pOutput);
			DbgLog((LOG_ERROR, 3, L"CDSlibass::GetPin: CSubInputPin creation failed"));
			return nullptr;
        }

    }

    // Return the appropriate pin

	if (n == 0) {
		return m_pInput;
	} else
	if (n == 1) {
		return m_pOutput;
	} else
	if (n == 2) {
		return m_pSubInput.get();
	} else {
        return nullptr;
    }
}

int CDSlibass::GetPinCount() {

	DbgLog((LOG_TRACE, 3, L"CDSlibass::GetPinCount: requested pin count"));

	return 3;
}


HRESULT CDSlibass::DecideBufferSize(IMemAllocator *pAlloc,ALLOCATOR_PROPERTIES *pProperties)
{
	DbgLog((LOG_TRACE, 3, L"CDSlibass::DecideBufferSize: negotiating allocators with downstream filter"));

    if (m_pInput->IsConnected() == FALSE) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::DecideBufferSize: input pin not connected"));
        return E_UNEXPECTED;
    }

    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);
    HRESULT hr = NOERROR;

	//first we try cbAlign = 16
	pProperties->cBuffers = 1;
    pProperties->cbBuffer = m_pOutput->CurrentMediaType().GetSampleSize();
	pProperties->cbAlign = 16;
    ASSERT(pProperties->cbBuffer);

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);
    if (FAILED(hr)) {
		//if we fail, then we try cbAlign = 1
		DbgLog((LOG_TRACE, 3, L"CDSlibass::DecideBufferSize: first attempt failed, trying cbAlign = 1"));
		pProperties->cbAlign = 1;
		hr = pAlloc->SetProperties(pProperties,&Actual);
		if(FAILED(hr)){
			DbgLog((LOG_ERROR, 3, L"CDSlibass::DecideBufferSize: SetProperties failed"));
			return hr;
		}
    }

    ASSERT( Actual.cBuffers == 1 );

	if (pProperties->cbAlign != Actual.cbAlign) {
		DbgLog((LOG_ERROR, 3, L"CDSlibass::DecideBufferSize: actual alignment is not the same as the requested one"));
	}

    if (pProperties->cBuffers > Actual.cBuffers ||
            pProperties->cbBuffer > Actual.cbBuffer) {
				DbgLog((LOG_ERROR, 3, L"CDSlibass::DecideBufferSize: actual buffer count and/or buffer size is less than requested"));
                return E_FAIL;
    }
    return NOERROR;
}

STDMETHODIMP_(WORD) CDSlibass::GetActiveTrackNum() {

	return m_cCurrentTrack;

}

STDMETHODIMP CDSlibass::SetActiveTrackNum(WORD trackNum) {

	m_cCurrentTrack = trackNum;
	return S_OK;

}

STDMETHODIMP_(WORD) CDSlibass::GetTracksCount() {

	return m_vTracks.size();

}

STDMETHODIMP_(WCHAR*) CDSlibass::GetTrackName(WORD trackNum){

	return trackNum < m_vTracks.size() ? (WCHAR*) m_vTracks[trackNum].c_str() : NULL;

}

STDMETHODIMP_(WORD) CDSlibass::GetSubtitlePathsCount() {

	return m_subtitlePaths.size();

}

STDMETHODIMP_(WCHAR*) CDSlibass::GetSubtitlePath(WORD pathNum) {

	return pathNum < m_subtitlePaths.size() ? (WCHAR*) m_subtitlePaths[pathNum].c_str() : NULL;

}

STDMETHODIMP CDSlibass::AddSubtitlePath(WCHAR *path) {

	if(find(m_subtitlePaths.begin(), m_subtitlePaths.end(), wstring(path))!=m_subtitlePaths.end()) {
		m_subtitlePaths.push_back(path);
	}
	
	return S_OK;

}

STDMETHODIMP_(INT) CDSlibass::GetSubtitleDelay() {

	return m_subtitleDelay;

}

STDMETHODIMP CDSlibass::SetSubtitleDelay(INT delay) {

	m_subtitleDelay = delay;

	return S_OK;

}

STDMETHODIMP_(DSLAMatrixModes) CDSlibass::GetColorMatrix() {

	return m_cCurrentColorMatrix;

}

STDMETHODIMP CDSlibass::SetColorMatrix(DSLAMatrixModes colorMatrix) {

	m_cCurrentColorMatrix = colorMatrix;
	m_pRenderer->SetColorMatrix(colorMatrix);

	return S_OK;

}

STDMETHODIMP_(BOOL) CDSlibass::GetPixFmts(DSLAPixFmts fmt) {

	return m_cEnabledPixFmts[fmt];

}

STDMETHODIMP CDSlibass::SetPixFmts(DSLAPixFmts fmt, BOOL enabled) {

	m_cEnabledPixFmts[fmt] = enabled;

	return S_OK;

}

STDMETHODIMP CDSlibass::SaveSettings() {
	DbgLog((LOG_TRACE, 3, L"CDSlibass::SaveSettings: trying to save settings to the registry."));
	HRESULT hr = S_OK;
	CRegistry registry(HKEY_CURRENT_USER, DSLA_REGISTRY_KEY, hr);
	if (hr == S_OK) {
		DbgLog((LOG_TRACE, 3, L"CDSlibass::SaveSettings: writing settings."));
		registry.Write(L"SubtitleDelay", (DWORD)m_subtitleDelay);
		registry.Write(L"ColorMatrix", (DWORD)m_cCurrentColorMatrix);
		for (auto type = DSLAPixFmt_None+1; type < DSLAPixFmts_NB; type++)
			registry.Write(RegistryMediaValuesNames[type], (bool)m_cEnabledPixFmts[type]);
	}
	return S_OK;
}