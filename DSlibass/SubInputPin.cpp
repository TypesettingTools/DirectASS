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

#include "LibassWrapper.h"
#include "DSLibass.h"
#include "SubInputPin.h"

CSubInputPin::CSubInputPin(CDSlibass* pFilter, CCritSec* pLock, HRESULT* phr)
	: CBaseInputPin(NAME("CSubInputPin"), pFilter, pLock, phr, L"Subtitle"),
	m_pDSlibass(pFilter)
{
	DbgLog((LOG_TRACE, 3, L"CSubInputPin: creating"));
}


/* Obviously, we support only SRT and SSA/ASS, but the graph will stop
 * if we reject the connection here, so we pretend that we accept everything.
 */
HRESULT CSubInputPin::CheckMediaType(const CMediaType *pmt) {

	DbgLog((LOG_TRACE, 3, L"CSubInputPin::CheckMediaType: checking input media type"));
	DisplayType(L"Media type: ", pmt);

	if((pmt->majortype == MEDIATYPE_Subtitle && 
		(pmt->subtype == MEDIASUBTYPE_UTF8 || 
		pmt->subtype == MEDIASUBTYPE_SSA || 
		pmt->subtype == MEDIASUBTYPE_ASS || 
		pmt->subtype == MEDIASUBTYPE_ASS2 || 
		pmt->subtype == MEDIASUBTYPE_SSF || 
		pmt->subtype == MEDIASUBTYPE_USF || 
		pmt->subtype == MEDIASUBTYPE_VOBSUB || 
		pmt->subtype == MEDIASUBTYPE_HDMVSUB))){
			DbgLog((LOG_TRACE, 3, L"CSubInputPin::CheckMediaType: accepted"));
			return S_OK;
	}

	DbgLog((LOG_TRACE, 3, L"CSubInputPin::CheckMediaType: not accepted"));
	return VFW_E_TYPE_NOT_ACCEPTED;
		   
}

STDMETHODIMP CSubInputPin::EndFlush() {
	
	DbgLog((LOG_TRACE, 3, L"CSubInputPin::EndFlush: flushing subtitle input pin"));

	if (m_mt.subtype == MEDIASUBTYPE_SSA || 
			m_mt.subtype == MEDIASUBTYPE_ASS || 
			m_mt.subtype == MEDIASUBTYPE_ASS2 ||
			m_mt.subtype == MEDIASUBTYPE_UTF8) {

		m_pDSlibass->m_pRenderer->GetLibass()->Flush();

	}

	return __super::EndFlush();

}

HRESULT CSubInputPin::SetMediaType(const CMediaType *pmt) {

	DbgLog((LOG_TRACE, 3, L"CSubInputPin::SetMediaType: setting media type on the subtitle pin"));

	auto hr = __super::SetMediaType(pmt);
	if(FAILED(hr)) {
		return hr;
	}
	
	if((IsEqualGUID(*pmt->FormatType(), FORMAT_SubtitleInfo))) {

		if(!m_pDSlibass->m_pRenderer) {
			DbgLog((LOG_TRACE, 3, L"CSubInputPin::SetMediaType: CSubPicRenderer not yet created, creating one"));
			m_pDSlibass->m_pRenderer = std::move(std::unique_ptr<CSubPicRenderer>(new CSubPicRenderer()));
			if(FAILED(hr)){
				return hr;
			}

		}

		if(pmt->subtype == MEDIASUBTYPE_UTF8) {

			DbgLog((LOG_TRACE, 3, L"CSubInputPin::SetMediaType: adding new embedded SRT track"));

			char buf[BUFSIZ];
			m_pDSlibass->m_pRenderer->GetLibass()->AddPinTrack();
			auto *si = (SUBTITLEINFO *) pmt->Format();
			WCHAR TrackName[BUFSIZ] = {0};
			swprintf_s(TrackName, BUFSIZ, L"Embedded track: %s", si->TrackName);
			if(m_pDSlibass->m_vTracks.empty()){
				m_pDSlibass->m_vTracks.push_back(TrackName);
			} else {
				m_pDSlibass->m_vTracks[0].assign(TrackName);
			}
			sprintf_s (buf, BUFSIZ, "[V4+ Styles]\nStyle: Default,%s,20,&H1EFFFFFF,&H00FFFFFF,"
				"&H29000000,&H3C000000,0,0,0,0,100,100,0,0,1,1,1.2,2,10,10,"
				"12,1\n\n[Events]\n",
				"Arial");
			m_pDSlibass->m_pRenderer->GetLibass()->PinProcessCodecPrivate((BYTE *)buf, BUFSIZ);

		} else if (pmt->subtype == MEDIASUBTYPE_SSA || 
			pmt->subtype == MEDIASUBTYPE_ASS ||
			pmt->subtype == MEDIASUBTYPE_ASS2) {

			DbgLog((LOG_TRACE, 3, L"CSubInputPin::SetMediaType: adding new embedded ASS track"));

			m_pDSlibass->m_pRenderer->GetLibass()->AddPinTrack();
			auto *si = (SUBTITLEINFO *) pmt->Format();
			WCHAR TrackName[BUFSIZ] = {0};
			swprintf_s(TrackName, BUFSIZ, L"Embedded track: %s", si->TrackName);
			if(m_pDSlibass->m_vTracks.empty()){
				m_pDSlibass->m_vTracks.push_back(TrackName);
			} else {
				m_pDSlibass->m_vTracks[0].assign(TrackName);
			}
			m_pDSlibass->m_pRenderer->GetLibass()->PinProcessCodecPrivate((BYTE *) pmt->Format() + si->dwOffset, 
				pmt->FormatLength() - sizeof(SUBTITLEINFO));
		}

	}


	return __super::SetMediaType(pmt);

}

STDMETHODIMP CSubInputPin::Receive(IMediaSample *pSample) {

	static UINT srtLines = 0;

	DbgLog((LOG_TRACE, 3, L"CSubInputPin::Receive: receiving subtitle chunk"));

	BYTE* pData = nullptr;
	auto hr = pSample->GetPointer(&pData);
	if(FAILED(hr) || pData == nullptr) {
		return hr;
	}

	CRefTime tStart, tStop ;
	pSample->GetTime((REFERENCE_TIME *) &tStart, (REFERENCE_TIME *) &tStop);

	CMediaType *pOutMT = nullptr;
    hr = pSample->GetMediaType((AM_MEDIA_TYPE**)&pOutMT);
    if (hr == S_OK)
    {
		DbgLog((LOG_TRACE, 3, L"CSubInputPin::Receive: media type attached: new Matroska linked segment detected"));
		hr = CheckMediaType(pOutMT);
        if(FAILED(hr))
        {
            DeleteMediaType(pOutMT);
            return E_FAIL;
        }

		if((IsEqualGUID(*pOutMT->FormatType(), FORMAT_SubtitleInfo))) {
			DbgLog((LOG_TRACE, 3, L"CSubInputPin::Receive: adding embedded track from the next linked segment"));
			SUBTITLEINFO *si = (SUBTITLEINFO *) pOutMT->Format();
			m_pDSlibass->m_pRenderer->GetLibass()->AddNextTrack((BYTE *) pOutMT->Format() + si->dwOffset, 
				pOutMT->FormatLength() - sizeof(SUBTITLEINFO), tStart.Millisecs() + m_tStart.Millisecs());


		}
		
        DeleteMediaType(pOutMT);
    }

	int len = pSample->GetActualDataLength();

	if(m_mt.subtype == MEDIASUBTYPE_UTF8) {

		DbgLog((LOG_TRACE, 3, L"CSubInputPin::Receive: reparsing SRT chunk to ASS and sending it to the renderer"));

		char buf[BUFSIZ];
		std::string l;
		l.append((char *) pData, len);
		char newline[3] = "\\N";
		std::string::size_type pos;
		while((pos = l.find("\r\n")) && (pos != std::string::npos)){
			l.replace(pos, 2, newline);
		}
		sprintf_s(buf, BUFSIZ, "%d,0,Default,,0,0,0,,{\\blur0.7}%s", srtLines, l.c_str());
		m_pDSlibass->m_pRenderer->GetLibass()->PinProcessChunk((BYTE *)buf, strlen(buf), tStart.Millisecs() + m_tStart.Millisecs(), tStop.Millisecs() - tStart.Millisecs());
		srtLines++;
	} else if (m_mt.subtype == MEDIASUBTYPE_SSA ||
		m_mt.subtype == MEDIASUBTYPE_ASS ||
		m_mt.subtype == MEDIASUBTYPE_ASS2) {
		DbgLog((LOG_TRACE, 3, L"CSubInputPin::Receive: sending ASS chunk to the renderer"));
		m_pDSlibass->m_pRenderer->GetLibass()->PinProcessChunk(pData, len, tStart.Millisecs() + m_tStart.Millisecs(), tStop.Millisecs() - tStart.Millisecs());
	}

	return S_OK;


}