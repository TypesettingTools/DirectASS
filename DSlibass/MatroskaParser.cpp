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

#include "StdAfx.h"

#include "ebml/EbmlHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "matroska/FileKax.h"
#include "matroska/KaxSegment.h"
#include "matroska/KaxSeekHead.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxInfoData.h"
#include "matroska/KaxCluster.h"
#include "matroska/KaxBlockData.h"
#include "MatroskaParser.h"


#define SAFE_DELETE(a) if( (a) != NULL ) { delete (a); (a) = NULL; }

#define FINDFIRST(p, c)   (static_cast<c *>(((EbmlMaster *)p)->FindFirstElt(EBML_INFO(c), false)))
#define FINDNEXT(p, c, e) (static_cast<c *>(((EbmlMaster *)p)->FindNextElt(*e, false)))

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;


CMatroskaParser::CMatroskaParser() :
	m_File(nullptr),
	m_AttsPosition(0),
	m_bAttsFound(false),
	m_TracksPosition(0),
	m_bTracksFound(false),
	m_bClustersFound(false),
	m_cTimecodeScale(1000000)
{
}

CMatroskaParser::~CMatroskaParser() {
}

HRESULT CMatroskaParser::Open(wstring FileName) {

	m_File = unique_ptr<WinIOCallback>(new WinIOCallback(FileName.c_str(), MODE_READ));
	auto estream = EbmlStream(*m_File);

	auto level0 = unique_ptr<EbmlElement>(estream.FindNextID(EBML_INFO(EbmlHead), UINT32_MAX));

	if(level0 == nullptr) {
		return E_FAIL;
	}

	level0->SkipData(estream, EBML_CONTEXT(level0));

	level0.reset(estream.FindNextID(EBML_INFO(KaxSegment), UINT32_MAX));

	if(level0 == nullptr) {
		return E_FAIL;
	}

	if ((EbmlId(*level0) != EBML_ID(KaxSegment))){

		return E_FAIL;
	}

    auto segment = static_cast<KaxSegment *>(level0.get());

	int UpperLvlElement = 0;

	auto level1 = unique_ptr<EbmlElement>(estream.FindNextElement(EBML_CONTEXT(segment), UpperLvlElement, UINT32_MAX, true, 1));
	while ((level1 != nullptr) && (UpperLvlElement <= 0)) {

		if(EbmlId(*level1) == EBML_ID(KaxSeekHead)){
			
			EbmlElement *level2 = nullptr;
			level1->Read(estream, EBML_INFO_CONTEXT(EBML_INFO(KaxSeekHead)), UpperLvlElement, level2, true);
			auto seekhead = dynamic_cast<KaxSeekHead *>(level1.get());

			for(size_t i = 0; i < seekhead->ListSize(); i++){

				auto *seek = dynamic_cast<KaxSeek *>((*seekhead)[i]);
				if(seek == nullptr) {
					return E_FAIL;
				}

				for(size_t k = 0; k < seek->ListSize(); k++){
					auto *e = (*seek)[k];

					if(EbmlId(*e) == EBML_ID(KaxSeekID)) {
						auto *seekid = static_cast<KaxSeekID *>(e);
						if(EbmlId(seekid->GetBuffer(), seekid->GetSize()) == EBML_ID(KaxAttachments)){
							if(!m_bAttsFound) {
								m_bAttsFound = true;
								m_AttsPosition = level0->GetElementPosition() + level0->HeadSize() + uint16(*static_cast<KaxSeekPosition *>((*seek)[(k^1)]));
							}
						}
						else if(EbmlId(seekid->GetBuffer(), seekid->GetSize()) == EBML_ID(KaxTracks)){
							if(!m_bTracksFound) {
								m_bTracksFound = true;
								m_TracksPosition = level0->GetElementPosition() + level0->HeadSize() + uint16(*static_cast<KaxSeekPosition *>((*seek)[k^1]));
							}
						}
					}
				}
				if(m_bAttsFound && m_bTracksFound) {
					break;
				}
			}


		}

		else if(EbmlId(*level1) == EBML_ID(KaxCluster)){
			if(!m_bClustersFound) {
				m_bClustersFound = true;
				m_ClustersPosition = level1->GetElementPosition();
			}
		}

		else if(EbmlId(*level1) == EBML_ID(KaxInfo)){
			auto *tcscale = FINDFIRST(level1.get(), KaxTimecodeScale);
			if(tcscale != nullptr){
				m_cTimecodeScale = uint64(*tcscale);
			}
		}
		
		level1->SkipData(estream, EBML_CONTEXT(level1));

		if(m_bTracksFound && m_bClustersFound) {
			break;
		}

		level1.reset(estream.FindNextElement(EBML_CONTEXT(segment), UpperLvlElement, UINT32_MAX, true));
	}

	if(FAILED(LookForSubtitleTracks())){
		return E_FAIL;
	}

	return S_OK;
	
}

HRESULT CMatroskaParser::ParseAttachments(vector<MatroskaAttachment> & attachments){

	if(!m_bAttsFound) {
		return E_UNEXPECTED;
	}

	m_File->setFilePointer(m_AttsPosition);

	auto estream = EbmlStream(*m_File);
		
	//look for the attachments section
	int UpperLvlElement = 0;

	auto level1 = unique_ptr<EbmlElement>(estream.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), UpperLvlElement, UINT32_MAX, true, 1));
	
	if(EbmlId(*level1) != EBML_ID(KaxAttachments)){
		return E_FAIL;
	}
	
	//read the attachments
	EbmlElement *level2 = nullptr;
	level1->Read(estream, EBML_INFO_CONTEXT(EBML_INFO(KaxAttachments)), UpperLvlElement, level2, true);

	KaxAttachments *AttachedFiles = dynamic_cast<KaxAttachments *>(level1.get());

	for(size_t i = 0; i < AttachedFiles->ListSize(); i++){

		MatroskaAttachment attachment;
		auto *att = dynamic_cast<KaxAttached *>((*AttachedFiles)[i]);
		if(att == nullptr) {
			return E_FAIL;
		}
		for (size_t k = 0; k < att->ListSize(); k++) {
			auto *e = (*att)[k];

			if (EbmlId(*e) == EBML_ID(KaxFileName))
				attachment.FileName = wstring(UTFstring(*static_cast<KaxFileName *>(e)).c_str());

			else if (EbmlId(*e) == EBML_ID(KaxMimeType))
				attachment.MimeType = string(*static_cast<KaxMimeType *>(e));

			else if (EbmlId(*e) == EBML_ID(KaxFileUID))
				attachment.ID = uint32(*static_cast<KaxFileUID *>(e));

			else if (EbmlId(*e) == EBML_ID(KaxFileData)) {
				attachment.FileSize = static_cast<KaxFileData *>(e)->GetSize();
				attachment.FileData = unique_ptr<BYTE>(new BYTE[attachment.FileSize]);
				memcpy(attachment.FileData.get(), (void *) static_cast<KaxFileData *>(e)->GetBuffer(), attachment.FileSize);
			}
		}

		if((attachment.ID == -1) || (attachment.FileSize == -1) || (attachment.MimeType.empty()))
			continue;

		attachments.push_back(std::move(attachment));

	}
	return S_OK;
	
}

HRESULT CMatroskaParser::ParseSubtitleTrack(MatroskaASSTrack & subtitles, UINT num){

	if(!m_bTracksFound || m_cSubTracksFound.empty() || !m_bClustersFound){
		return E_UNEXPECTED;
	}

	uint64 default_duration = 0;

	m_File->setFilePointer(m_TracksPosition);

	auto estream = EbmlStream(*m_File);

	int UpperLvlElement = 0;
	
	auto level1 = unique_ptr<EbmlElement>(estream.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), UpperLvlElement, UINT32_MAX, true, 1));

	if(EbmlId(*level1) != EBML_ID(KaxTracks)){
		return E_FAIL;
	}

	EbmlElement *level2 = nullptr;
	level1->Read(estream, EBML_INFO_CONTEXT(EBML_INFO(KaxTracks)), UpperLvlElement, level2, true);

	KaxTracks *tracks = dynamic_cast<KaxTracks *>(level1.get());

	for(size_t i = 0; i < tracks->ListSize(); i++) {

		auto *TrackEntry = dynamic_cast<KaxTrackEntry *>((*tracks)[i]);
		if(TrackEntry == nullptr){
			return E_FAIL;
		}

		uint16 tracknum;
		uint16 tracktype;
		string codecid;
		string codecprivatedata;
		uint64 codecprivatesize;
		uint64 trackdefaultduration = 0;
		wstring trackname;

		for (size_t k = 0; k < TrackEntry->ListSize(); k++) {
			auto *e = (*TrackEntry)[k];

			if(EbmlId(*e) == EBML_ID(KaxTrackType)){
				tracktype = uint16(*static_cast<KaxTrackType *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxCodecID)){
				codecid = string(*static_cast<KaxCodecID *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxTrackNumber)){
				tracknum = uint16(*static_cast<KaxTrackNumber *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxTrackDefaultDuration)){
				trackdefaultduration = uint64(*static_cast<KaxTrackDefaultDuration *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxTrackName)){
				trackname = wstring(UTFstring(*static_cast<KaxTrackName *>(e)).c_str());
			}
			else if(EbmlId(*e) == EBML_ID(KaxCodecPrivate)){
				codecprivatesize = (*static_cast<KaxCodecPrivate *>(e)).GetSize();
				codecprivatedata.reserve(codecprivatesize);
				codecprivatedata.assign((char *)(*static_cast<KaxCodecPrivate *>(e)).GetBuffer(), codecprivatesize);
			}

		}

		if(tracknum == m_cSubTracksFound[num]){
			subtitles.name = trackname;
			subtitles.header = move(codecprivatedata);
			default_duration = trackdefaultduration;
		}
	}

	m_File->setFilePointer(m_ClustersPosition);

	UpperLvlElement = 0;
	
	level1.reset(estream.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), UpperLvlElement, UINT32_MAX, true, 1));

	while((level1 != nullptr) && (UpperLvlElement <= 0)) {
				
		if (EbmlId(*level1) == EBML_ID(KaxCluster)) {

			EbmlElement *level2 = nullptr;

			level1->Read(estream, EBML_INFO_CONTEXT(EBML_INFO(KaxCluster)), UpperLvlElement, level2, true);

			auto *cluster = static_cast<KaxCluster *>(level1.get());

			auto *ctc = FINDFIRST(level1.get(), KaxClusterTimecode);

			if(ctc != nullptr){
				cluster->InitTimecode(uint64(*ctc), m_cTimecodeScale);
			} else {
				cluster->InitTimecode(0, m_cTimecodeScale);
			}

			for (size_t i = 0; i < cluster->ListSize(); i++) {

				auto *e = (*cluster)[i];

				if (EbmlId(*e) == EBML_ID(KaxBlockGroup)) {
					auto *bg = static_cast<KaxBlockGroup *>(e);
					auto *block = FINDFIRST(bg, KaxBlock);
					if(block == nullptr || block->NumberFrames() == 0)
						continue;
					block->SetParent(*cluster);
					if(block->TrackNum() != m_cSubTracksFound[num])
						continue;
					auto *kduration = FINDFIRST(bg, KaxBlockDuration);
					uint64 duration;
					if(kduration != nullptr) {
						duration = uint64(*kduration) * m_cTimecodeScale / 1000000;
					} else {
						duration = default_duration * m_cTimecodeScale / 1000000;
					}
					for (size_t j = 0; j < block->NumberFrames(); j++) {
						MatroskaSubChunk chunk;
						chunk.duration = duration;
						chunk.timecode = (block->GlobalTimecode() / 1000000) + j * duration;

						DataBuffer buf = block->GetBuffer(j);
						chunk.subtitle = string((char *)buf.Buffer(), buf.Size());
						subtitles.events.push_back(std::move(chunk));
					}
				} else if (EbmlId(*e) == EBML_ID(KaxSimpleBlock)) {
					auto *sb = static_cast<KaxSimpleBlock *>(e);
					if(sb->NumberFrames() == 0)
						continue;
					sb->SetParent(*cluster);
					if(sb->TrackNum() != m_cSubTracksFound[num])
						continue;
					for (size_t j = 0; j< sb->NumberFrames(); j++) {
						MatroskaSubChunk chunk;
						chunk.duration = default_duration;
						chunk.timecode = (sb->GlobalTimecode() + j * default_duration) * m_cTimecodeScale / 1000000;
						
						auto buf = sb->GetBuffer(j);
						chunk.subtitle = string((char *)buf.Buffer(), buf.Size());
						subtitles.events.push_back(std::move(chunk));
					}

				}
			
			}

		}

		level1->SkipData(estream, EBML_CONTEXT(level1));

		level1.reset(estream.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), UpperLvlElement, UINT32_MAX, true, 1));
	}
	
	return S_OK;
}

HRESULT CMatroskaParser::LookForSubtitleTracks() {

	m_File->setFilePointer(m_TracksPosition);

	auto estream = EbmlStream(*m_File);

	int UpperLvlElement = 0;
	
	auto level1 = unique_ptr<EbmlElement>(estream.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), UpperLvlElement, UINT32_MAX, true, 1));

	if(EbmlId(*level1) != EBML_ID(KaxTracks)){
		return E_FAIL;
	}

	EbmlElement *level2 = nullptr;
	level1->Read(estream, EBML_INFO_CONTEXT(EBML_INFO(KaxTracks)), UpperLvlElement, level2, true);

	auto *tracks = dynamic_cast<KaxTracks *>(level1.get());

	for(size_t i = 0; i < tracks->ListSize(); i++) {

		auto *TrackEntry = dynamic_cast<KaxTrackEntry *>((*tracks)[i]);
		if(TrackEntry == nullptr){
			return E_FAIL;
		}

		uint16 tracknum;
		uint16 tracktype;
		string codecid;

		for (size_t k = 0; k < TrackEntry->ListSize(); k++) {
			auto *e = (*TrackEntry)[k];

			if(EbmlId(*e) == EBML_ID(KaxTrackType)){
				tracktype = uint16(*static_cast<KaxTrackType *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxCodecID)){
				codecid = string(*static_cast<KaxCodecID *>(e));
			}
			else if(EbmlId(*e) == EBML_ID(KaxTrackNumber)){
				tracknum = uint16(*static_cast<KaxTrackNumber *>(e));
			}

		}

		if(tracktype == 0x11 
			&& ((codecid == "S_TEXT/SSA")
			|| (codecid == "S_TEXT/ASS"))){
				m_cSubTracksFound.push_back(tracknum);
		}

	}

	return S_OK;

}