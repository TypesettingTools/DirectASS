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

#include "ebml/WinIOCallback.h"


using namespace LIBEBML_NAMESPACE;
using namespace std;

typedef struct MatroskaAttachment {
public:
	wstring Description;
	wstring FileName;
	string MimeType;
	__int64 FileSize, ID;
	unique_ptr<BYTE> FileData;

	MatroskaAttachment() {}

	MatroskaAttachment(MatroskaAttachment&& att) {
		Description = std::move(att.Description);
		FileName = std::move(att.FileName);
		MimeType = std::move(att.MimeType);
		FileSize = att.FileSize;
		ID = att.ID;
		FileData = std::move(att.FileData);
	}

} MatroskaAttachment;

typedef struct MatroskaSubChunk {
public:
	UINT timecode;
	UINT duration;
	string subtitle;

	MatroskaSubChunk() {}

	MatroskaSubChunk(MatroskaSubChunk&& sc) {
		timecode = sc.timecode;
		duration = sc.duration;
		subtitle = std::move(sc.subtitle);
	}

} MatroskaSubChunk;

typedef struct MatroskaASSTrack {
public:
	wstring name;
	string header;
	vector<MatroskaSubChunk> events;

	MatroskaASSTrack() {};

	MatroskaASSTrack(MatroskaASSTrack&& t) {
		name = std::move(t.name);
		header = std::move(t.header);
		events = std::move(t.events);
	}

} MatroskaASSTrack;

class CMatroskaParser
{
public:
	CMatroskaParser(void);
	~CMatroskaParser(void);
	

	HRESULT Open(wstring FileName);

	HRESULT ParseAttachments(vector<MatroskaAttachment> & attachments);
	HRESULT ParseSubtitleTrack(MatroskaASSTrack & subtitles, UINT num);

	UINT GetSubtitleTracksNumber() {return m_cSubTracksFound.size();};

private:

	HRESULT LookForSubtitleTracks();

	std::unique_ptr<WinIOCallback> m_File;

	filepos_t m_AttsPosition;
	bool m_bAttsFound;

	filepos_t m_TracksPosition;
	bool m_bTracksFound;
	vector<UINT> m_cSubTracksFound;

	filepos_t m_ClustersPosition;
	bool m_bClustersFound;

	uint64 m_cTimecodeScale;

};

