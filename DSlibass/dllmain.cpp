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
#include "LibassWrapper.h"
#include "DSLARendererProp.h"
#include "DSlibass.h"
#include "registry.h"
#include "SubInputPin.h"


static const WCHAR g_wszName[] = L"DSlibass";



const REGPINTYPES sudInPinTypes1[] =
{
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_P010
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_P016
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_NV12
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_YV12
	},
	{
		&MEDIATYPE_Video,
		&MEDIASUBTYPE_RGB32
	},
	{
		&MEDIATYPE_Video,
		&MEDIASUBTYPE_RGB24
	}
};

const REGPINTYPES sudInPinTypes2 =
{
		&MEDIATYPE_Subtitle,
		&MEDIASUBTYPE_NULL
};


const REGPINTYPES sudOutPinTypes[] =
{
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_P010
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_P016
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_NV12
	},
	{	&MEDIATYPE_Video,
		&MEDIASUBTYPE_YV12
	},
	{
		&MEDIATYPE_Video,
		&MEDIASUBTYPE_RGB32
	},
	{
		&MEDIATYPE_Video,
		&MEDIASUBTYPE_RGB24
	}
};

const REGFILTERPINS sudpPins[] =
{
    {
		L"Video Input",		// Pins string name
		FALSE,				// Is it rendered
		FALSE,				// Is it an output
		FALSE,				// Are we allowed none
		FALSE,				// And allowed many
		&CLSID_NULL,			// Connects to filter
		NULL,					// Connects to pin
		6,					// Number of types
		sudInPinTypes1		// Pin information
    },
	{
		L"Subtitle Input",
		TRUE,
		FALSE,
		FALSE,
		FALSE,
		&CLSID_NULL,
		NULL,
		1,
		&sudInPinTypes2
	},
    {
		L"Output",			// Pins string name
		FALSE,				// Is it rendered
		TRUE,					// Is it an output
		FALSE,				// Are we allowed none
		FALSE,				// And allowed many
		&CLSID_NULL,			// Connects to filter
		NULL,					// Connects to pin
		6,					// Number of types
		sudOutPinTypes		// Pin information
	}
};

const AMOVIESETUP_FILTER sudDSlibass =
{
	&CLSID_DSlibass,				//filter clsid
	L"DSlibass Subtitle Renderer",	//filter name
	MERIT_PREFERRED + 2,			//filter merit
	3,								//pins count
	sudpPins,						//pins array
};


CFactoryTemplate g_Templates[] = {
	{
		sudDSlibass.strName,
		sudDSlibass.clsID,
		CreateInstance<CDSlibass>,
		NULL,
		&sudDSlibass
	},
	{
		L"DSlibass Renderer Settings",
		&CLSID_DSLARendererProp,
		CreateInstance<CDSLARendererProp>,
		NULL,
		NULL
	}
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );

}


STDAPI DllUnregisterServer()
{
	//CRegistry::DeleteRegistryKey(HKEY_CURRENT_USER, DSLA_REGISTRY_KEY);
    return AMovieDllRegisterServer2( FALSE );

}


extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint(reinterpret_cast<HINSTANCE>(hModule), dwReason, lpReserved);
}
