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

typedef enum ASS_TRACK_TYPE {
	ASS_EMB_TRACK,
	ASS_FILE_TRACK,
	ASS_MKS_TRACK
} ASS_TRACK_TYPE;

class CASSTrack
{
public:

	CASSTrack(ASS_Library *lib);
	CASSTrack(ASS_Library *lib, std::wstring filename);
	CASSTrack(ASS_Library *lib, BYTE *buf, int data_size);
	~CASSTrack(void) {};

	void flush();

	ASS_Track *getTrack() { return m_pTrack.get(); };
	ASS_TRACK_TYPE getType() { return m_cType; };
	
private:

	void parseSRT(std::wstring filename);
	std::unique_ptr<ASS_Track, decltype(&ass_free_track)> m_pTrack;
	ASS_TRACK_TYPE m_cType;

};

