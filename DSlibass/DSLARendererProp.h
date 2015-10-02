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

#include "DSlibassSettings.h"

// {D97BA6E8-A57D-44B1-9CC1-99CF36264510}
DEFINE_GUID(CLSID_DSLARendererProp, 
0xd97ba6e8, 0xa57d, 0x44b1, 0x9c, 0xc1, 0x99, 0xcf, 0x36, 0x26, 0x45, 0x10);
class CDSLARendererProp :
	public CBasePropertyPage
{
public:
	CDSLARendererProp(LPUNKNOWN pUnk, HRESULT* phr);
	~CDSLARendererProp(void);

	HRESULT OnActivate();
	HRESULT OnConnect(IUnknown *pUnk);
	HRESULT OnDisconnect();
	HRESULT OnApplyChanges();
	HRESULT OnDeactivate();
	INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:

	void SetDirty()
	{
		m_bDirty = TRUE;
		if (m_pPageSite)
		{
			m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
		}
	}

	HRESULT ApplyChanges();
	HRESULT GetSettings();

	IDSLASettings *m_pSettings;

	std::vector<std::wstring> m_subtitlePaths;
	std::vector<std::wstring> m_vTracks;
	BOOL m_cPixFmts[DSLAPixFmts_NB];
	int m_subtitleDelay;
	DSLAMatrixModes m_cCurrentColorMatrix;
	WORD m_cCurrentTrack;

};

