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
#include "registry.h"

bool CRegistry::CreateRegistryKey(HKEY hKeyRoot, LPCTSTR subKey) {
	HKEY hKey;
	auto hr = RegCreateKeyEx(hKeyRoot, subKey, 0, NULL,	REG_OPTION_NON_VOLATILE, KEY_WRITE,	NULL, &hKey, NULL);

	if(hr == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		hKey = (HKEY)NULL;
		return true;
	}

	SetLastError(hr);
	return false;
}

bool CRegistry::DeleteRegistryKey(HKEY hKeyRoot, LPCTSTR subKey) {
	auto hr = RegDeleteKey(hKeyRoot, subKey);
	return SUCCEEDED(hr);
}
CRegistry::CRegistry(HKEY hKeyRoot, LPCTSTR subKey, HRESULT &hr) : m_key(nullptr){
	hr = RegOpenKeyEx(hKeyRoot, subKey, 0, KEY_READ | KEY_WRITE, &m_key);
	if (hr == ERROR_FILE_NOT_FOUND) {
		if (!CreateRegistryKey(hKeyRoot, subKey))
			return;
		hr = RegOpenKeyEx(hKeyRoot, subKey, 0, KEY_READ | KEY_WRITE, &m_key);
	}
}

CRegistry::~CRegistry() {
	if (m_key != nullptr)
		RegCloseKey(m_key);
	m_key = nullptr;
}

HRESULT CRegistry::Write(LPCTSTR key, LPCTSTR value) {
	if (m_key == nullptr) { return E_UNEXPECTED; }

	return RegSetValueEx(m_key, key, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
}

HRESULT CRegistry::Write(LPCTSTR key, bool value) {
	return Write(key, (DWORD)value);
}

HRESULT CRegistry::Write(LPCTSTR key, DWORD value) {
	if (m_key == nullptr) { return E_UNEXPECTED; }

	return RegSetValueEx(m_key, key, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
}

std::wstring CRegistry::ReadString(LPCTSTR key, HRESULT &hr) {
	std::wstring result;

	if (m_key == NULL) { hr = E_UNEXPECTED;  return result; }

	DWORD size;
	hr = RegQueryValueEx(m_key, key, NULL, NULL, NULL, &size);

	if (hr == ERROR_SUCCESS) {
		WCHAR *buffer = (WCHAR *)calloc(size,1);
		hr = RegQueryValueEx(m_key, key, NULL, NULL, (LPBYTE)buffer, &size);
		result = std::wstring(buffer);
		free(buffer);
	}

	return result;
}

bool CRegistry::ReadBool(LPCTSTR key, HRESULT &hr) {
	return ReadDWORD(key, hr) ? true : false;
}

DWORD CRegistry::ReadDWORD(LPCTSTR key, HRESULT &hr) {
	if (m_key == nullptr) { 
		hr = E_UNEXPECTED; 
		return 0; 
	}
	DWORD dwSize = sizeof(DWORD);
	DWORD dwVal = 0;
	hr = RegQueryValueEx(m_key, key, 0, NULL, (LPBYTE)&dwVal, &dwSize);
	return dwVal;
}

HRESULT CRegistry::DeleteKey(LPCTSTR key) {
	if (m_key == nullptr) { return E_UNEXPECTED; }
	return RegDeleteValue(m_key, key);
}