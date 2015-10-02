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


extern "C" {
#include "ass/ass.h"
}

#include "stdafx.h"
#include "ASSTrack.h"
#include "DSlibassSettings.h"

typedef struct {

	int w;
	int h;
	double dar;
	double sar;

} LibassSettings;


class CLibassWrapper
{

private:

	std::unique_ptr<ASS_Renderer, decltype(&ass_renderer_done)> m_pRenderer;
	std::unique_ptr<ASS_Library, decltype(&ass_library_done)> m_pLibrary;
	std::vector<std::unique_ptr<CASSTrack>> m_vASSTracks;

	bool m_bNextFlag;
	CRefTime m_cStartTime; //

public:

	HRESULT ASSRenderFrame(ULONGLONG tc, WORD TrackNum, ASS_Image **Image, int *DetectChange);
	HRESULT InitRenderer(LibassSettings & Settings);
	void AddFileTrack(std::wstring filename);
	void AddPinTrack();
	void AddMKSTrack(BYTE *buf, int size);
	void AddNextTrack(BYTE *buf, int size, ULONGLONG tc);
	void PinProcessChunk(BYTE *data, int size, UINT timecode, UINT duration);
	void PinProcessCodecPrivate(BYTE *data, int size);
	void MKSProcessChunk(BYTE *data, int size, UINT timecode, UINT duration, BYTE trackNum);
	void Flush();
	void AddFont(char *name, BYTE *data, int data_size);
	void SetFonts();
	
	CLibassWrapper();
	~CLibassWrapper();

};

