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

const LPCTSTR RegistryMediaValuesNames[] = {
	{L"AllowP010"},
	{L"AllowP016"},
	{L"AllowNV12"},
	{L"AllowYV12"},
	{L"AllowRGB32"},
	{L"AllowRGB24"},
};

typedef enum DSLAPixFmts {
	DSLAPixFmt_None = -1,
	DSLAPixFmt_P010,
	DSLAPixFmt_P016,
	DSLAPixFmt_NV12,
	DSLAPixFmt_YV12,
	DSLAPixFmt_RGB32,
	DSLAPixFmt_RGB24,
	DSLAPixFmts_NB
};

typedef enum DSLAMatrixModes {
	DSLAMatrixMode_BT601,
	DSLAMatrixMode_BT709,
	DSLAMatrixMode_AUTO
};

typedef struct {

	const GUID *subtype;
	DWORD biCompression;
	WORD biBitCount;
	WORD biPlanes;

} DSLAOutMediaType;

const DSLAOutMediaType OutMediaTypes[] = {
	{&MEDIASUBTYPE_P010, '010P', 24, 2},
	{&MEDIASUBTYPE_P016, '610P', 24, 2},
	{&MEDIASUBTYPE_NV12, '21VN', 12, 3},
	{&MEDIASUBTYPE_YV12, '21VY', 12, 2},
	{&MEDIASUBTYPE_RGB32, BI_RGB, 32, 1},
	{&MEDIASUBTYPE_RGB24, BI_RGB, 24, 1}
};


[uuid("13A45F53-F9DC-4F56-A22F-DD63383E1684")]
interface IDSLASettings : public IUnknown {

	// Active subtitle track number
	STDMETHOD_(WORD,GetActiveTrackNum)() = 0;
	STDMETHOD(SetActiveTrackNum)(WORD trackNum) = 0;

	// Subtitle tracks' names are read-only
	STDMETHOD_(WORD,GetTracksCount)() = 0;
	STDMETHOD_(WCHAR*,GetTrackName)(WORD trackNum) = 0;

	// External subtitles search paths
	STDMETHOD_(WORD,GetSubtitlePathsCount)() = 0;
	STDMETHOD_(WCHAR*,GetSubtitlePath)(WORD pathNum) = 0;
	STDMETHOD(AddSubtitlePath)(WCHAR *path) = 0;

	// Subtitle delay in ms
	STDMETHOD_(INT,GetSubtitleDelay)() = 0;
	STDMETHOD(SetSubtitleDelay)(INT delay) = 0;

	// RGB->YUV color matrix. 
	// 0 - BT.601
	// 1 - BT.709
	// 2 - automatic selection
	STDMETHOD_(DSLAMatrixModes,GetColorMatrix)() = 0;
	STDMETHOD(SetColorMatrix)(DSLAMatrixModes colorMatrix) = 0;

	// List of enabled colorspaces
	// Count is DSLAPixFmts_MAX
	STDMETHOD_(BOOL,GetPixFmts)(DSLAPixFmts fmt) = 0;
	STDMETHOD(SetPixFmts)(DSLAPixFmts fmt, BOOL enabled) = 0;

	// Save current settings
	STDMETHOD(SaveSettings)() = 0;
};
