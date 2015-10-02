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
#include "LibassWrapper.h"

using namespace std;

CLibassWrapper::CLibassWrapper() :
	m_pRenderer(nullptr, ass_renderer_done),
	m_pLibrary(ass_library_init(), ass_library_done),
	m_bNextFlag(false),
	m_cStartTime((REFERENCE_TIME)INT64_MAX)
{
	DbgLog((LOG_TRACE, 3, L"CLibassWrapper: creating"));
};

CLibassWrapper::~CLibassWrapper()
{
	DbgLog((LOG_TRACE, 3, L"CLibassWrapper: destroying"));
	m_vASSTracks.clear();
	ass_clear_fonts(m_pLibrary.get());
}

HRESULT CLibassWrapper::InitRenderer(LibassSettings & Settings) {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::InitRenderer: initialising ASS_Renderer"));

	ass_set_extract_fonts(m_pLibrary.get(), 0);
	ass_set_style_overrides(m_pLibrary.get(), NULL);

	m_pRenderer = std::move(unique_ptr<ASS_Renderer, decltype(&ass_renderer_done)>(ass_renderer_init(m_pLibrary.get()), ass_renderer_done));

	if (m_pRenderer == nullptr) {
		DbgLog((LOG_ERROR, 3, L"CLibassWrapper::InitRenderer: ass_renderer_init failed"));
		return E_FAIL;
	}

	ass_set_frame_size(m_pRenderer.get(), Settings.w, Settings.h);
	ass_set_margins(m_pRenderer.get(), 0, 0, 0, 0);
	ass_set_use_margins(m_pRenderer.get(), 0);
	ass_set_shaper(m_pRenderer.get(), ASS_SHAPING_COMPLEX);
	ass_set_font_scale(m_pRenderer.get(), 1.0);

	if (Settings.dar && Settings.sar)
        ass_set_aspect_ratio (m_pRenderer.get(), Settings.dar, Settings.sar);

	return S_OK;

}

void CLibassWrapper::AddFileTrack(std::wstring filename) {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddFileTrack: adding external subtitle track from file %s", filename.c_str()));

	m_vASSTracks.push_back(std::move(unique_ptr<CASSTrack>(new CASSTrack(m_pLibrary.get(), filename))));

}

// the embedded track should always be the first one in the vector
void CLibassWrapper::AddPinTrack() {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddPinTrack: adding embedded subtitle track"));

	if(m_vASSTracks.size() != 0){

		if(ASS_EMB_TRACK != m_vASSTracks[0]->getType()){
			DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddPinTrack: inserting embedded track"));
			m_vASSTracks.insert(m_vASSTracks.begin(), std::move(unique_ptr<CASSTrack>(new CASSTrack(m_pLibrary.get()))));
			return;
		}
		DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddPinTrack: replacing already present embedded track"));
		m_vASSTracks[0].reset(new CASSTrack(m_pLibrary.get()));
		return;
	}

	m_vASSTracks.push_back(unique_ptr<CASSTrack>(new CASSTrack(m_pLibrary.get())));

}

void CLibassWrapper::AddMKSTrack(BYTE *buf, int size){

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddMKSTrack: adding external track from MKS file"));

	m_vASSTracks.push_back(std::move(unique_ptr<CASSTrack>(new CASSTrack(m_pLibrary.get(), buf, size))));

}

void CLibassWrapper::AddNextTrack(BYTE *buf, int size, ULONGLONG tc){

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddMKSTrack: adding embedded track from the next Matroska linked segment"));
	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddMKSTrack: which starts at the %d ms", tc));

	m_vASSTracks.push_back(std::move(unique_ptr<CASSTrack>(new CASSTrack(m_pLibrary.get()))));
	ass_process_codec_private(m_vASSTracks[m_vASSTracks.size() - 1]->getTrack(), (char *) buf, size);
	m_cStartTime = tc*10000;
	m_bNextFlag = true;

}


void CLibassWrapper::SetFonts() {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::SetFonts: initialising fonts"));

	ass_set_fonts(m_pRenderer.get(), NULL, NULL, 1, NULL, 1);

}

void CLibassWrapper::Flush() {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::Flush: flushing buffered events in the emdedded track"));

	m_vASSTracks[0]->flush();

}

HRESULT CLibassWrapper::ASSRenderFrame(ULONGLONG tc, WORD trackNum, ASS_Image **Image, int *DetectChange) {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::ASSRenderFrame: rendering subtitles from track %d at %d ms", trackNum, tc));

	if(trackNum > m_vASSTracks.size()) {
		DbgLog((LOG_ERROR, 3, L"CLibassWrapper::ASSRenderFrame: track number is too big"));
		return E_INVALIDARG;
	}

	if((m_bNextFlag) && (tc >= m_cStartTime.Millisecs())) {

		DbgLog((LOG_TRACE, 3, L"CLibassWrapper::ASSRenderFrame: switching embedded track to the one from the next linked segment", trackNum, tc));

		m_vASSTracks[0].swap(m_vASSTracks[m_vASSTracks.size() - 1]);
		m_vASSTracks.pop_back();
		m_bNextFlag = false;
		m_cStartTime = INT64_MAX;
	}

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::ASSRenderFrame: calling ass_render_frame", trackNum, tc));
	ASS_Image *img = ass_render_frame(m_pRenderer.get(), m_vASSTracks[trackNum]->getTrack(), tc, DetectChange);

	*Image = img;

	return S_OK;
}

void CLibassWrapper::AddFont(char *name, BYTE *data, int data_size) {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::AddFont: adding font"));

	ass_add_font(m_pLibrary.get(), name, (char *) data, data_size);

}

void CLibassWrapper::PinProcessChunk(BYTE *data, int size, UINT timecode, UINT duration) {

	if(m_bNextFlag) {
		DbgLog((LOG_TRACE, 3, L"CLibassWrapper::PinProcessChunk: adding subtitle chunk to the next linked embedded track"));
		ass_process_chunk(m_vASSTracks[m_vASSTracks.size() - 1]->getTrack(), (char *) data, size, timecode, duration);
	} else {
		DbgLog((LOG_TRACE, 3, L"CLibassWrapper::PinProcessChunk: adding subtitle chunk to the embedded track"));
		ass_process_chunk(m_vASSTracks[0]->getTrack(), (char *) data, size, timecode, duration);
	}

}

void CLibassWrapper::PinProcessCodecPrivate(BYTE *data, int size) {

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::PinProcessCodecPrivate: processing CodecPrivate section of the embedded track"));

	ass_process_codec_private(m_vASSTracks[0]->getTrack(), (char *) data, size);

}

void CLibassWrapper::MKSProcessChunk(BYTE *data, int size, UINT timecode, UINT duration, BYTE trackNum){

	DbgLog((LOG_TRACE, 3, L"CLibassWrapper::PinProcessCodecPrivate: adding subtitle chunk to the MKS file track"));

	ass_process_chunk(m_vASSTracks[trackNum]->getTrack(), (char *) data, size, timecode, duration);

}
