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
#include "ASSTrack.h"

using namespace std;

static inline int lrint (double const x) {
	int n; 
	__asm fld qword ptr x; 
	__asm fistp dword ptr n; 
	return n;
}


CASSTrack::CASSTrack(ASS_Library *lib) :
	m_cType(ASS_EMB_TRACK),
	m_pTrack(ass_new_track(lib), ass_free_track)
{
	DbgLog((LOG_TRACE, 3, L"CASSTrack: creating new empty track"));
}

CASSTrack::CASSTrack(ASS_Library *lib, std::wstring filename) :
	m_cType(ASS_FILE_TRACK),
	m_pTrack(nullptr, ass_free_track)
{

	DbgLog((LOG_TRACE, 3, L"CASSTrack: creating new track from file %s", filename.c_str()));

	ifstream inputfile = ifstream(filename, std::ios_base::in);

	string input;

	inputfile.seekg(0, std::ios::end);   
	input.reserve(inputfile.tellg());
	inputfile.seekg(0, std::ios::beg);

	input.assign((std::istreambuf_iterator<char>(inputfile)),
				std::istreambuf_iterator<char>());

	inputfile.close();

	m_pTrack = std::move(unique_ptr<ASS_Track, decltype(&ass_free_track)>(ass_read_memory(lib, (char *)input.c_str(), input.size(), "UTF-8"), ass_free_track));

	if(m_pTrack == nullptr) {
		DbgLog((LOG_TRACE, 3, L"CASSTrack: ASS parsing failed, trying to parse the file as SRT"));
		m_pTrack = std::move(unique_ptr<ASS_Track, decltype(&ass_free_track)>(ass_new_track(lib), ass_free_track));
		parseSRT(filename);
	}

}

CASSTrack::CASSTrack(ASS_Library *lib, BYTE *buf, int data_size) :
	m_cType(ASS_MKS_TRACK),
	m_pTrack(nullptr, ass_free_track)
{
	DbgLog((LOG_TRACE, 3, L"CASSTrack: creating new track from a CodecPrivate section"));
	m_pTrack = std::move(unique_ptr<ASS_Track, decltype(&ass_free_track)>(ass_new_track(lib), ass_free_track));
	ass_process_codec_private(m_pTrack.get(), (char *) buf, data_size);

}

void CASSTrack::parseSRT(std::wstring filename) {

	char l[BUFSIZ], buf[BUFSIZ];
    int start[4], end[4], isn;

	FILE *fh;
	_wfopen_s (&fh, filename.c_str(), L"r");

    if (fh) {
        

    sprintf_s (buf, BUFSIZ, "[V4+ Styles]\nStyle: Default,%s,20,&H1EFFFFFF,&H00FFFFFF,"
             "&H29000000,&H3C000000,0,0,0,0,100,100,0,0,1,1,1.2,2,10,10,"
             "12,1\n\n[Events]\n",
             "Arial");

    ass_process_data (m_pTrack.get(), buf, BUFSIZ - 1);

    while (fgets (l, BUFSIZ - 1, fh) != NULL) {
        if (l[0] == 0 || l[0] == '\n' || l[0] == '\r')
            continue;

        if (sscanf_s (l, "%d:%d:%d,%d --> %d:%d:%d,%d", &start[0], &start[1],
                    &start[2], &start[3], &end[0], &end[1], &end[2],
                    &end[3]) == 8) {
            sprintf_s (buf, BUFSIZ - 1, "Dialogue: 0,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,"
                     "Default,,0,0,0,,{\\blur0.7}",
                     start[0], start[1], start[2],
                     (int) lrint ( (double) start[3] / 10.0), end[0], end[1],
                     end[2], (int) lrint ( (double) end[3] / 10.0));
            isn = 0;

            while (fgets (l, BUFSIZ - 1, fh) != NULL) {
                if (l[0] == 0 || l[0] == '\n' || l[0] == '\r')
                    break;

                if (l[strlen (l) - 1] == '\n' || l[strlen (l) - 1] == '\r')
                    l[strlen (l) - 1] = 0;

                if (isn) {
                    strcat_s (buf, BUFSIZ - 1, "\\N");
                }

                strncat_s (buf, BUFSIZ - 1, l, BUFSIZ - 1);
                isn = 1;
            }

            ass_process_data (m_pTrack.get(), buf, BUFSIZ - 1);
        }
    }
	fclose(fh);
	}

}


void CASSTrack::flush() {

	DbgLog((LOG_TRACE, 3, L"CASSTrack::flush: flushing track"));

	if(m_pTrack){
		ass_flush_events(m_pTrack.get());
	}

}
