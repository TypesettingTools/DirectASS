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

class CRegistry {

public:
	CRegistry(HKEY hKeyRoot, LPCTSTR subKey, HRESULT &hr);

	~CRegistry();

	HRESULT Write(LPCTSTR key, bool value);
	HRESULT Write(LPCTSTR key, DWORD value);
	HRESULT Write(LPCTSTR key, LPCTSTR value);

	std::wstring ReadString(LPCTSTR key, HRESULT &hr);
	bool ReadBool(LPCTSTR key, HRESULT &hr);
	DWORD ReadDWORD(LPCTSTR key, HRESULT &hr);
	
	HRESULT DeleteKey(LPCTSTR key);

	static bool DeleteRegistryKey(HKEY hKeyRoot, LPCTSTR subKey);
	static bool CreateRegistryKey(HKEY hKeyRoot, LPCTSTR subKey);
private:
	HKEY m_key; 
};