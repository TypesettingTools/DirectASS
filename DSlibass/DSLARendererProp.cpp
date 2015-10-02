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
#include "DSUtils.h"
#include "DSLARendererProp.h"
#include "resource.h"

#include <Commctrl.h>


CDSLARendererProp::CDSLARendererProp(LPUNKNOWN pUnk, HRESULT* phr) :
	CBasePropertyPage(NAME("DSLARendererProp"), pUnk, IDD_PROPPAGE_RENDERER, IDS_RENDERER_SETTINGS),
	m_pSettings(nullptr)
{
}


CDSLARendererProp::~CDSLARendererProp(void)
{
}

HRESULT CDSLARendererProp::OnConnect(IUnknown *pUnk) {

	CheckPointer(pUnk, E_POINTER);

	ASSERT(m_pSettings == nullptr);
	return pUnk->QueryInterface(&m_pSettings);

}

HRESULT CDSLARendererProp::OnActivate() {

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
	if (InitCommonControlsEx(&icc) == FALSE)
	{
		return E_FAIL;
	}

	ASSERT(m_pSettings != nullptr);

	GetSettings();

	WCHAR buf[4096] = {0};

	_itow_s(m_subtitleDelay, buf, 4096, 10);

	Edit_SetText(GetDlgItem(m_Dlg, IDC_SUBDELAY), buf);

	buf[0] = 0;

	Button_SetCheck(GetDlgItem(m_Dlg, IDC_BT601), (m_cCurrentColorMatrix == DSLAMatrixMode_BT601));
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_BT709), (m_cCurrentColorMatrix == DSLAMatrixMode_BT709));
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_BTAUTO), (m_cCurrentColorMatrix == DSLAMatrixMode_AUTO));

	Button_SetCheck(GetDlgItem(m_Dlg, IDC_YV12), m_cPixFmts[DSLAPixFmt_YV12]);
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_NV12), m_cPixFmts[DSLAPixFmt_NV12]);
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_P010), m_cPixFmts[DSLAPixFmt_P010]);
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_P016), m_cPixFmts[DSLAPixFmt_P016]);
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_RGB32), m_cPixFmts[DSLAPixFmt_RGB32]);
	Button_SetCheck(GetDlgItem(m_Dlg, IDC_RGB24), m_cPixFmts[DSLAPixFmt_RGB24]);

	SendDlgItemMessage(m_Dlg, IDC_SUBTRACK, LB_RESETCONTENT, 0, 0);

	for(auto i = m_vTracks.begin(); i < m_vTracks.end(); i++) {
		SendDlgItemMessage(m_Dlg, IDC_SUBTRACK, LB_ADDSTRING, 0, (LPARAM)(*i).c_str());
	}

	SendDlgItemMessage(m_Dlg, IDC_SUBTRACK, LB_SETCURSEL, m_cCurrentTrack, 0);

	return S_OK;
}

HRESULT CDSLARendererProp::GetSettings() {

	m_subtitleDelay = m_pSettings->GetSubtitleDelay();
	m_cCurrentColorMatrix = m_pSettings->GetColorMatrix();
	m_cCurrentTrack = m_pSettings->GetActiveTrackNum();

	for(int i = 0; i < DSLAPixFmts_NB; i++) {
		m_cPixFmts[i] = m_pSettings->GetPixFmts((DSLAPixFmts)i);
	}

	WORD subTracks = m_pSettings->GetTracksCount();

	for(int i = 0; i < subTracks; i++) {
		m_vTracks.push_back(m_pSettings->GetTrackName(i));
	}

	WORD paths = m_pSettings->GetSubtitlePathsCount();

	for(int i = 0; i < subTracks; i++) {
		m_subtitlePaths.push_back(m_pSettings->GetSubtitlePath(i));
	}

	return S_OK;

}

HRESULT CDSLARendererProp::OnDisconnect() {
	SafeRelease(&m_pSettings);
	return S_OK;
}

INT_PTR CDSLARendererProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg != WM_COMMAND)
		return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);

	LRESULT lValue;
	BOOL bValue;

	if(LOWORD(wParam) == IDC_SUBDELAY && HIWORD(wParam) == EN_CHANGE){
		WCHAR buf[4096] = {0};
		Edit_GetText(GetDlgItem(m_Dlg, LOWORD(wParam)), buf, 4096);
		lValue = _wtoi(buf);
		if(lValue != m_subtitleDelay) {
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_BT601 && HIWORD(wParam) == BN_CLICKED){
		lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(lValue != (m_cCurrentColorMatrix == DSLAMatrixMode_BT601)){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_BT709 && HIWORD(wParam) == BN_CLICKED){
		lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(lValue != (m_cCurrentColorMatrix == DSLAMatrixMode_BT709)){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_BTAUTO && HIWORD(wParam) == BN_CLICKED){
		lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(lValue != (m_cCurrentColorMatrix == DSLAMatrixMode_AUTO)){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_YV12 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_YV12]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_NV12 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_NV12]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_P010 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_P010]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_P016 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_P016]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_RGB32 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_RGB32]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_RGB24 && HIWORD(wParam) == BN_CLICKED){
		bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
		if(bValue != m_cPixFmts[DSLAPixFmt_RGB24]){
			SetDirty();
		}
	} else if(LOWORD(wParam) == IDC_SUBTRACK && HIWORD(wParam) == LBN_SELCHANGE){
		lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), LB_GETCURSEL, 0, 0);
		if(lValue != m_cCurrentTrack) {
			SetDirty();
		}
	}
	return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

HRESULT CDSLARendererProp::ApplyChanges() {
	ASSERT(m_pSettings != nullptr);

	WCHAR buf[4096] = {0};

	Edit_GetText(GetDlgItem(m_Dlg, IDC_SUBDELAY), buf, 4096);

	int delay = _wtoi(buf);

	m_pSettings->SetSubtitleDelay(delay);

	BOOL bBT601 = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_BT601));
	BOOL bBT709 = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_BT709));
	BOOL bBTAUTO = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_BTAUTO));

	m_pSettings->SetColorMatrix(bBT601 ? DSLAMatrixMode_BT601 : bBT709 ? DSLAMatrixMode_BT709 : DSLAMatrixMode_AUTO);

	WORD track = SendDlgItemMessage(m_Dlg, IDC_SUBTRACK, LB_GETCURSEL, 0, 0);

	m_pSettings->SetActiveTrackNum(track);

	BOOL bPixFmts[DSLAPixFmts_NB] = {0};

	bPixFmts[DSLAPixFmt_YV12] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_YV12));
	bPixFmts[DSLAPixFmt_NV12] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_NV12));
	bPixFmts[DSLAPixFmt_P010] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_P010));
	bPixFmts[DSLAPixFmt_P016] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_P016));
	bPixFmts[DSLAPixFmt_RGB32] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_RGB32));
	bPixFmts[DSLAPixFmt_RGB24] = (BOOL)Button_GetCheck(GetDlgItem(m_Dlg, IDC_RGB24));

	for(int i = 0; i < DSLAPixFmts_NB; i++) {
		m_pSettings->SetPixFmts((DSLAPixFmts)i, bPixFmts[i]);
	}
	m_pSettings->SaveSettings();

	return S_OK;
}

HRESULT CDSLARendererProp::OnDeactivate() {
	return ApplyChanges();
}

HRESULT CDSLARendererProp::OnApplyChanges() {
	HRESULT hr = ApplyChanges();
	GetSettings();
	return hr;
}
