﻿/*
 * Copyright(C) 2021 Dennis Fleurbaaij <mail@dennisfleurbaaij.com>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program. If not, see < https://www.gnu.org/licenses/>.
 */

#include <stdafx.h>

#include <atlstr.h>
#include <algorithm>
#include <vector>

#include <version.h>
#include <cie.h>
#include <VideoConversionOverride.h>
#include <resource.h>
#include <VideoProcessorApp.h>
#include <microsoft_directshow/video_renderers/DirectShowVideoRenderers.h>
#include <microsoft_directshow/video_renderers/DirectShowMPCVideoRenderer.h>
#include <microsoft_directshow/video_renderers/DirectShowEnhancedVideoRenderer.h>
#include <microsoft_directshow/video_renderers/DirectShowGenericVideoRenderer.h>
#include <microsoft_directshow/video_renderers/DirectShowGenericHDRVideoRenderer.h>
#include <microsoft_directshow/DirectShowRendererStartStopTimeMethod.h>
#include <microsoft_directshow/DirectShowDefines.h>
#include <guid.h>

#include "VideoProcessorDlg.h"


const static UINT_PTR TIMER_ID_1SECOND = 1;


BEGIN_MESSAGE_MAP(CVideoProcessorDlg, CDialog)

	// Pre-baked callbacks
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_QUERYDRAGICON()
	ON_WM_GETMINMAXINFO()
	ON_WM_SETFOCUS()
	ON_WM_CLOSE()
	ON_WM_TIMER()

	// UI element messages
	ON_CBN_SELCHANGE(IDC_CAPTURE_DEVICE_COMBO, &CVideoProcessorDlg::OnCaptureDeviceSelected)
	ON_CBN_SELCHANGE(IDC_CAPTURE_INPUT_COMBO, &CVideoProcessorDlg::OnCaptureInputSelected)
	ON_BN_CLICKED(IDC_TIMING_CLOCK_FRAME_OFFSET_AUTO_CHECK, &CVideoProcessorDlg::OnBnClickedTimingClockFrameOffsetAutoCheck)
	ON_CBN_SELCHANGE(IDC_COLORSPACE_CONTAINER_PRESET_COMBO, &CVideoProcessorDlg::OnColorSpaceContainerPresetSelected)
	ON_CBN_SELCHANGE(IDC_COLORSPACE_VIDEO_PRESET_COMBO, &CVideoProcessorDlg::OnColorSpaceVideoPresetSelected)
	ON_CBN_SELCHANGE(IDC_RENDERER_COMBO, &CVideoProcessorDlg::OnRendererSelected)
	ON_BN_CLICKED(IDC_RENDERER_RESTART_BUTTON, &CVideoProcessorDlg::OnBnClickedRendererRestart)
	ON_CBN_SELCHANGE(IDC_RENDERER_VIDEO_CONVERSION_COMBO, &CVideoProcessorDlg::OnRendererVideoConversionSelected)
	ON_BN_CLICKED(IDC_RENDERER_VIDEO_FRAME_USE_QUEUE_CHECK, &CVideoProcessorDlg::OnBnClickedRendererVideoFrameUseQueueCheck)
	ON_BN_CLICKED(IDC_RENDERER_RESET_BUTTON, &CVideoProcessorDlg::OnBnClickedRendererReset)
	ON_BN_CLICKED(IDC_RENDERER_RESET_AUTO_CHECK, &CVideoProcessorDlg::OnBnClickedRendererResetAutoCheck)
	ON_CBN_SELCHANGE(IDC_RENDERER_DIRECTSHOW_START_STOP_TIME_METHOD_COMBO, &CVideoProcessorDlg::OnRendererDirectShowStartStopTimeMethodSelected)
	ON_CBN_SELCHANGE(IDC_RENDERER_DIRECTSHOW_NOMINAL_RANGE_COMBO, &CVideoProcessorDlg::OnRendererDirectShowNominalRangeSelected)
	ON_CBN_SELCHANGE(IDC_RENDERER_DIRECTSHOW_TRANSFER_FUNCTION_COMBO, &CVideoProcessorDlg::OnRendererDirectShowTransferFunctionSelected)
	ON_CBN_SELCHANGE(IDC_RENDERER_DIRECTSHOW_TRANSFER_MATRIX_COMBO, &CVideoProcessorDlg::OnRendererDirectShowTransferMatrixSelected)
	ON_CBN_SELCHANGE(IDC_RENDERER_DIRECTSHOW_PRIMARIES_COMBO, &CVideoProcessorDlg::OnRendererDirectShowPrimariesSelected)
	ON_BN_CLICKED(IDC_RENDERER_FULL_SCREEN_CHECK, &CVideoProcessorDlg::OnBnClickedRendererFullScreenCheck)

	// Custom messages
	ON_MESSAGE(WM_MESSAGE_CAPTURE_DEVICE_FOUND, &CVideoProcessorDlg::OnMessageCaptureDeviceFound)
	ON_MESSAGE(WM_MESSAGE_CAPTURE_DEVICE_LOST, &CVideoProcessorDlg::OnMessageCaptureDeviceLost)
	ON_MESSAGE(WM_MESSAGE_CAPTURE_DEVICE_STATE_CHANGE, &CVideoProcessorDlg::OnMessageCaptureDeviceStateChange)
	ON_MESSAGE(WM_MESSAGE_CAPTURE_DEVICE_CARD_STATE_CHANGE, &CVideoProcessorDlg::OnMessageCaptureDeviceCardStateChange)
	ON_MESSAGE(WM_MESSAGE_CAPTURE_DEVICE_VIDEO_STATE_CHANGE, &CVideoProcessorDlg::OnMessageCaptureDeviceVideoStateChange)
	ON_MESSAGE(WM_MESSAGE_DIRECTSHOW_NOTIFICATION, &CVideoProcessorDlg::OnMessageDirectShowNotification)
	ON_MESSAGE(WM_MESSAGE_RENDERER_STATE_CHANGE, &CVideoProcessorDlg::OnMessageRendererStateChange)
	ON_MESSAGE(WM_MESSAGE_RENDERER_DETAIL_STRING, &CVideoProcessorDlg::OnMessageRendererDetailString)

	// Command handlers (from accelerator)
	ON_COMMAND(ID_COMMAND_FULLSCREEN_TOGGLE, &CVideoProcessorDlg::OnCommandFullScreenToggle)
	ON_COMMAND(ID_COMMAND_FULLSCREEN_EXIT, &CVideoProcessorDlg::OnCommandFullScreenExit)
	ON_COMMAND(ID_COMMAND_RENDERER_RESET, &CVideoProcessorDlg::OnCommandRendererReset)

END_MESSAGE_MAP()


static const std::vector<DirectShowStartStopTimeMethod> RENDERER_DIRECTSHOW_START_STOP_TIME_OPTIONS =
{
	// Sorted in preferred order
	DirectShowStartStopTimeMethod::DS_SSTM_CLOCK_SMART,
	DirectShowStartStopTimeMethod::DS_SSTM_CLOCK_THEO,
	DirectShowStartStopTimeMethod::DS_SSTM_CLOCK_CLOCK,
	DirectShowStartStopTimeMethod::DS_SSTM_THEO_THEO,
	DirectShowStartStopTimeMethod::DS_SSTM_CLOCK_NONE,
	DirectShowStartStopTimeMethod::DS_SSTM_THEO_NONE,
	DirectShowStartStopTimeMethod::DS_SSTM_NONE
};


static const std::vector<std::pair<LPCTSTR, DXVA_NominalRange>> DIRECTSHOW_NOMINAL_RANGE_OPTIONS =
{
	std::make_pair(TEXT("Auto"),             DXVA_NominalRange_Unknown),
	std::make_pair(TEXT("Full (0-255)"),     DXVA_NominalRange_0_255),
	std::make_pair(TEXT("Limited (16-235)"), DXVA_NominalRange_16_235),
	std::make_pair(TEXT("Small (48-208)"),   DXVA_NominalRange_48_208)
};


static const std::vector<std::pair<LPCTSTR, DXVA_VideoTransferFunction>> DIRECTSHOW_TRANSFER_FUNCTION_OPTIONS =
{
	std::make_pair(TEXT("Auto"),                      DXVA_VideoTransFunc_Unknown),
	std::make_pair(TEXT("PQ"),                        DIRECTSHOW_VIDEOTRANSFUNC_2084),
	std::make_pair(TEXT("Rec 709 (γ=2.2)"),           DXVA_VideoTransFunc_22_709),
	std::make_pair(TEXT("Bt.2020 constant"),          DIRECTSHOW_VIDEOTRANSFUNC_2020_const),

	std::make_pair(TEXT("True gamma 1.8"),            DXVA_VideoTransFunc_18),
	std::make_pair(TEXT("True gamma 2.0"),            DXVA_VideoTransFunc_20),
	std::make_pair(TEXT("True gamma 2.2"),            DXVA_VideoTransFunc_22),
	std::make_pair(TEXT("True gamma 2.6"),            DIRECTSHOW_VIDEOTRANSFUNC_26),
	std::make_pair(TEXT("True gamma 2.8"),            DXVA_VideoTransFunc_28),

	std::make_pair(TEXT("Linear RGB (γ=1.0)"),        DXVA_VideoTransFunc_10),
	std::make_pair(TEXT("204M (γ=2.2)"),              DXVA_VideoTransFunc_22_240M),
	std::make_pair(TEXT("8-bit gamma 2.2"),           DXVA_VideoTransFunc_22_8bit_sRGB),
	std::make_pair(TEXT("Log 100:1 H.264"),           DIRECTSHOW_VIDEOTRANSFUNC_Log_100),
	std::make_pair(TEXT("Log 316:1 H.264"),           DIRECTSHOW_VIDEOTRANSFUNC_Log_316),
	std::make_pair(TEXT("Rec 709 (γ=2.2) symmetric"), DIRECTSHOW_VIDEOTRANSFUNC_709_sym),
	std::make_pair(TEXT("Bt.2020 non-const"),         DIRECTSHOW_VIDEOTRANSFUNC_2020),
	std::make_pair(TEXT("Hybrid log"),                DIRECTSHOW_VIDEOTRANSFUNC_HLG)
};


static const std::vector<std::pair<LPCTSTR, DXVA_VideoTransferMatrix>> DIRECTSHOW_TRANSFER_MATRIX_OPTIONS =
{
	std::make_pair(TEXT("Auto"),       DXVA_VideoTransferMatrix_Unknown),
	std::make_pair(TEXT("BT.2020 10"), DIRECTSHOW_VIDEOTRANSFERMATRIX_BT2020_10),
	std::make_pair(TEXT("BT.2020 12"), DIRECTSHOW_VIDEOTRANSFERMATRIX_BT2020_12),
	std::make_pair(TEXT("BT.709"),     DXVA_VideoTransferMatrix_BT709),
	std::make_pair(TEXT("BT.601"),     DXVA_VideoTransferMatrix_BT601),
	std::make_pair(TEXT("240M"),       DXVA_VideoTransferMatrix_SMPTE240M),
	std::make_pair(TEXT("FCC"),        DIRECTSHOW_VIDEOTRANSFERMATRIX_FCC),
	std::make_pair(TEXT("YCgCo"),      DIRECTSHOW_VIDEOTRANSFERMATRIX_YCgCo)
};


static const std::vector<std::pair<LPCTSTR, DXVA_VideoPrimaries>> DIRECTSHOW_PRIMARIES_OPTIONS =
{
	std::make_pair(TEXT("Auto"),         DXVA_VideoPrimaries_Unknown),
	std::make_pair(TEXT("BT.2020"),      DIRECTSHOW_VIDEOPRIMARIES_BT2020),
	std::make_pair(TEXT("DCI-P3"),       DIRECTSHOW_VIDEOPRIMARIES_DCI_P3),
	std::make_pair(TEXT("BT.709"),       DXVA_VideoPrimaries_BT709),

	std::make_pair(TEXT("NTSC SysM"),    DXVA_VideoPrimaries_BT470_2_SysM),
	std::make_pair(TEXT("NTSC SysBG"),   DXVA_VideoPrimaries_BT470_2_SysBG),
	std::make_pair(TEXT("CIE 1931 XYZ"), DIRECTSHOW_VIDEOPRIMARIES_XYZ),
	std::make_pair(TEXT("ACES"),         DIRECTSHOW_VIDEOPRIMARIES_ACES),
};


static const std::vector<VideoConversionOverride> RENDERER_VIDEO_CONVERSION =
{
	VideoConversionOverride::VIDEOCONVERSION_NONE,
	VideoConversionOverride::VIDEOCONVERSION_V210_TO_P010
};


//
// Constructor/destructor
//


CVideoProcessorDlg::CVideoProcessorDlg():
	CDialog(CVideoProcessorDlg::IDD, nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_blackMagicDeviceDiscoverer = new BlackMagicDeckLinkCaptureDeviceDiscoverer(*this);
}


CVideoProcessorDlg::~CVideoProcessorDlg()
{
	for (auto& captureDevice : m_captureDevices)
		(*captureDevice).Release();
}


//
// Option handlers
//


void CVideoProcessorDlg::StartFullScreen()
{
	m_rendererfullScreen = true;
}


void CVideoProcessorDlg::DefaultRendererName(const CString& rendererName)
{
	m_defaultRendererName = rendererName;
}


//
// UI-related handlers
//


void CVideoProcessorDlg::OnCaptureDeviceSelected()
{
	const int captureDeviceIndex = m_captureDeviceCombo.GetCurSel();
	if (captureDeviceIndex < 0)
		return;

	// Find input device based on IDeckLink* object
	for (auto& captureDevice : m_captureDevices)
	{
		if (m_captureDeviceCombo.GetItemDataPtr(captureDeviceIndex) == captureDevice.p)
		{
			m_desiredCaptureDevice = captureDevice;
			break;
		}
	}

	UpdateState();
}


void CVideoProcessorDlg::OnCaptureInputSelected()
{
	assert(m_captureDevice);

	const int captureInputIndex = m_captureInputCombo.GetCurSel();
	if (captureInputIndex < 0)
		return;

	const CaptureInputId selectedCaptureInputId = (CaptureInputId)m_captureInputCombo.GetItemData(captureInputIndex);
	assert(selectedCaptureInputId != INVALID_CAPTURE_INPUT_ID);

	m_desiredCaptureInputId = selectedCaptureInputId;

	UpdateState();
}


void CVideoProcessorDlg::OnBnClickedTimingClockFrameOffsetAutoCheck()
{
	const bool checked = m_timingClockFrameOffsetAutoCheck.GetCheck();

	m_timingClockFrameOffsetEdit.EnableWindow(!checked);
}


void CVideoProcessorDlg::OnColorSpaceContainerPresetSelected()
{
	// TODO
}


void CVideoProcessorDlg::OnColorSpaceVideoPresetSelected()
{
	// TODO
}


void CVideoProcessorDlg::OnRendererSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnBnClickedRendererRestart()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnBnClickedRendererRestart()")));

	if (m_rendererState == RendererState::RENDERSTATE_FAILED)
		m_rendererState = RendererState::RENDERSTATE_UNKNOWN;

	m_wantToRestartRenderer = true;
	UpdateState();
}


void CVideoProcessorDlg::OnRendererVideoConversionSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnBnClickedRendererVideoFrameUseQueueCheck()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnBnClickedRendererVideoFrameUseQueueCheck()")));

	const bool useQueue = m_rendererVideoFrameUseQeueueCheck.GetCheck();

	m_wantToRestartRenderer = true;
	UpdateState();
}


void CVideoProcessorDlg::OnBnClickedRendererReset()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnBnClickedRendererReset()")));

	if (!m_videoRenderer)
		return;

	m_videoRenderer->Reset();
}


void CVideoProcessorDlg::OnBnClickedRendererResetAutoCheck()
{
	const bool checked = m_rendererResetAutoCheck.GetCheck();
}


void CVideoProcessorDlg::OnRendererDirectShowStartStopTimeMethodSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnRendererDirectShowNominalRangeSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnRendererDirectShowTransferFunctionSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnRendererDirectShowTransferMatrixSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnRendererDirectShowPrimariesSelected()
{
	OnBnClickedRendererRestart();
}


void CVideoProcessorDlg::OnBnClickedRendererFullScreenCheck()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnBnClickedRendererFullScreenCheck()")));

	assert(!m_rendererfullScreen);  // Can happen in debug mode where the UI remains visible
	m_rendererfullScreen = true;

	m_wantToRestartRenderer = true;
	UpdateState();
}


//
// Custom message handlers
//


LRESULT CVideoProcessorDlg::OnMessageCaptureDeviceFound(WPARAM wParam, LPARAM lParam)
{
	ACaptureDeviceComPtr captureDevice;
	captureDevice.Attach((ACaptureDevice*)wParam);

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnMessageCaptureDeviceFound(): %s"), captureDevice->GetName()));

	m_captureDevices.insert(captureDevice);

	RefreshCaptureDeviceList();

	return 0;
}


LRESULT	CVideoProcessorDlg::OnMessageCaptureDeviceLost(WPARAM wParam, LPARAM lParam)
{
	ACaptureDeviceComPtr captureDevice;
	captureDevice.Attach((ACaptureDevice*)wParam);

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnMessageCaptureDeviceLost(): %s"), captureDevice->GetName()));

	auto it = m_captureDevices.find(captureDevice);
	if (it == m_captureDevices.end())
		FatalError(TEXT("Cannot find capture device to remove"));

	// Device being removed is the one we're using, let's stop using it
	if (m_captureDevice == captureDevice)
	{
		m_desiredCaptureDevice = nullptr;
		UpdateState();
	}

	m_captureDevices.erase(it);

	RefreshCaptureDeviceList();

	return 0;
}


LRESULT	CVideoProcessorDlg::OnMessageCaptureDeviceStateChange(WPARAM wParam, LPARAM lParam)
{
	const CaptureDeviceState newState = (CaptureDeviceState)wParam;

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnMessageCaptureDeviceStateChange(): %s"), ToString(newState)));

	if (!m_captureDevice)
		return 0;

	assert(newState != m_captureDeviceState);

	m_captureDeviceState = newState;

	switch (newState)
	{
	case CaptureDeviceState::CAPTUREDEVICESTATE_READY:
		m_captureDeviceStateText.SetWindowText(TEXT("Ready"));
		break;

	case CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING:
		m_captureDeviceStateText.SetWindowText(TEXT("Capturing"));
		m_timingClockDescriptionText.SetWindowText(m_captureDevice->GetTimingClock()->TimingClockDescription());
		break;

	default:
		assert(false);
	}

	UpdateState();

	return 0;
}


LRESULT CVideoProcessorDlg::OnMessageCaptureDeviceCardStateChange(WPARAM wParam, LPARAM lParam)
{
	CaptureDeviceCardStateComPtr cardState;
	cardState.Attach((CaptureDeviceCardState*)wParam);
	assert(cardState);

	DbgLog((LOG_TRACE, 1,
		TEXT("CVideoProcessorDlg::OnMessageCaptureDeviceStateChange(): Lock=%d"),
		cardState->inputLocked));

	// Input fields
	m_inputLockedText.SetWindowText(ToString(cardState->inputLocked));

	if (cardState->inputDisplayMode)
		m_inputDisplayModeText.SetWindowText(cardState->inputDisplayMode->ToString());
	else
		m_inputDisplayModeText.SetWindowText(TEXT(""));

	if (cardState->inputEncoding != Encoding::UNKNOWN)
		m_inputEncodingText.SetWindowText(ToString(cardState->inputEncoding));
	else
		m_inputEncodingText.SetWindowText(TEXT(""));

	if (cardState->inputBitDepth != BitDepth::UNKNOWN)
		m_inputBitDepthText.SetWindowText(ToString(cardState->inputBitDepth));
	else
		m_inputBitDepthText.SetWindowText(TEXT(""));

	// Other
	m_captureDeviceOtherList.ResetContent();
	for (auto& str : cardState->other)
	{
		m_captureDeviceOtherList.AddString(str);
	}

	UpdateState();

	return 0;
}


LRESULT CVideoProcessorDlg::OnMessageCaptureDeviceVideoStateChange(WPARAM wParam, LPARAM lParam)
{
	VideoStateComPtr videoState;
	videoState.Attach((VideoState*)wParam);

	DbgLog((LOG_TRACE, 1,
		TEXT("CVideoProcessorDlg::OnMessageCaptureDeviceVideoStateChange(): Valid=%d"),
		videoState->valid));

	const bool rendererAcceptedState = (bool)lParam;

	m_captureDeviceVideoState = videoState;

	assert(videoState);
	assert(m_captureDevice);

	if (videoState->valid)
	{
		m_videoValidText.SetWindowText(_T("Yes"));
		m_videoDisplayModeText.SetWindowText(videoState->displayMode->ToString());
		m_videoPixelFormatText.SetWindowText(ToString(videoState->videoFrameEncoding));
		m_videoEotfText.SetWindowText(ToString(videoState->eotf));
		m_videoColorSpaceText.SetWindowText(ToString(videoState->colorspace));
	}
	else
	{
		m_videoValidText.SetWindowText(_T("No"));
		m_videoDisplayModeText.SetWindowText(_T(""));
		m_videoPixelFormatText.SetWindowText(_T(""));
		m_videoEotfText.SetWindowText(_T(""));
		m_videoColorSpaceText.SetWindowText(_T(""));
	}

	if (videoState->valid)
	{
		m_colorspaceCie1931xy.SetColorSpace(videoState->colorspace);

		m_colorspaceContainerREdit.SetWindowTextW(
			CieXYToString(
				ColorSpaceToCie1931RedX(videoState->colorspace),
				ColorSpaceToCie1931RedY(videoState->colorspace)));

		m_colorspaceContainerGEdit.SetWindowTextW(
			CieXYToString(
				ColorSpaceToCie1931GreenX(videoState->colorspace),
				ColorSpaceToCie1931GreenY(videoState->colorspace)));

		m_colorspaceContainerBEdit.SetWindowTextW(
			CieXYToString(
				ColorSpaceToCie1931BlueX(videoState->colorspace),
				ColorSpaceToCie1931BlueY(videoState->colorspace)));

		//cstring.Format(_T("%.01f"), ColorSpaceToCie1931RedX(videoState->colorspacel);
		//m_colorspaceContainerWPEdit.SetWindowTextW(_T(""));
	}
	else
	{
		m_colorspaceCie1931xy.SetColorSpace(ColorSpace::UNKNOWN);

		m_colorspaceContainerREdit.SetWindowTextW(_T(""));
		m_colorspaceContainerGEdit.SetWindowTextW(_T(""));
		m_colorspaceContainerBEdit.SetWindowTextW(_T(""));
		m_colorspaceContainerWPEdit.SetWindowTextW(_T(""));
	}

	m_colorspaceCie1931xy.SetHDRData(videoState->hdrData);

	if (videoState->valid && videoState->hdrData)
	{
		const HDRData hdrData = *(videoState->hdrData);

		CString cstring;
		cstring.Format(_T("%.01f"), hdrData.maxCll);
		m_luminanceMaxCll.SetWindowText(cstring);

		cstring.Format(_T("%.01f"), hdrData.maxFall);
		m_luminanceMaxFall.SetWindowText(cstring);

		cstring.Format(_T("%.05f"), hdrData.masteringDisplayMinLuminance);
		m_luminanceMasterMin.SetWindowText(cstring);

		cstring.Format(_T("%.01f"), hdrData.masteringDisplayMaxLuminance);
		m_luminanceMasterMax.SetWindowText(cstring);

		m_colorspaceVideoREdit.SetWindowTextW(
			CieXYToString(hdrData.displayPrimaryRedX, hdrData.displayPrimaryRedY));

		m_colorspaceVideoGEdit.SetWindowTextW(
			CieXYToString(hdrData.displayPrimaryGreenX, hdrData.displayPrimaryGreenY));

		m_colorspaceVideoBEdit.SetWindowTextW(
			CieXYToString(hdrData.displayPrimaryBlueX, hdrData.displayPrimaryBlueY));

		m_colorspaceVideoWPEdit.SetWindowTextW(
			CieXYToString(hdrData.whitePointX, hdrData.whitePointX));
	}
	else
	{
		m_luminanceMaxCll.SetWindowText(_T(""));
		m_luminanceMaxFall.SetWindowText(_T(""));
		m_luminanceMasterMin.SetWindowText(_T(""));
		m_luminanceMasterMax.SetWindowText(_T(""));

		m_colorspaceVideoREdit.SetWindowTextW(_T(""));
		m_colorspaceVideoGEdit.SetWindowTextW(_T(""));
		m_colorspaceVideoBEdit.SetWindowTextW(_T(""));
		m_colorspaceVideoWPEdit.SetWindowTextW(_T(""));

	}

	// If the renderer did not accept the new state we need to restart the renderer
	if (!rendererAcceptedState)
	{
		m_wantToRestartRenderer = true;
	}

	// New round, new chances, reset state here
	if (m_rendererState == RendererState::RENDERSTATE_FAILED)
	{
		m_rendererState = RendererState::RENDERSTATE_UNKNOWN;
	}

	UpdateState();

	return 0;
}


// This is a handler for the DirectShow graph in the renderer,
// it works by using the GUI's message queue.
LRESULT CVideoProcessorDlg::OnMessageDirectShowNotification(WPARAM wParam, LPARAM lParam)
{
	if (m_videoRenderer)
		if (FAILED(m_videoRenderer->OnWindowsEvent(wParam, lParam)))
			FatalError(TEXT("Failed to handle windows event in renderer"));

	return 0;
}


LRESULT CVideoProcessorDlg::OnMessageRendererStateChange(WPARAM wParam, LPARAM lParam)
{
	const RendererState newRendererState = (RendererState)wParam;

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnMessageRendererStateChange(): %s"), ToString(newRendererState)));

	assert(newRendererState != RendererState::RENDERSTATE_UNKNOWN);

	bool enableButtons = false;

	assert(m_videoRenderer);
	assert(m_rendererState != newRendererState);
	m_rendererState = newRendererState;

	switch (m_rendererState)
	{
	// Renderer ready, can be started if wanted
	case RendererState::RENDERSTATE_READY:
		m_rendererStateText.SetWindowText(TEXT("Ready"));
		break;

	// Renderer running, ready for frames
	case RendererState::RENDERSTATE_RENDERING:
		m_deliverCaptureDataToRenderer.store(true, std::memory_order_release);
		enableButtons = true;
		m_windowedVideoWindow.ShowLogo(false);
		m_rendererStateText.SetWindowText(TEXT("Rendering"));
		break;

	// Stopped rendering, can be cleaned up
	case RendererState::RENDERSTATE_STOPPED:
		RenderRemove();
		RenderGUIClear();
		m_rendererStateText.SetWindowText(TEXT(""));
		break;

	default:
		assert(false);
	}

	m_rendererFullscreenCheck.EnableWindow(enableButtons);
	m_rendererRestartButton.EnableWindow(enableButtons);
	m_rendererResetButton.EnableWindow(enableButtons);

	UpdateState();

	return 0;
}


LRESULT CVideoProcessorDlg::OnMessageRendererDetailString(WPARAM wParam, LPARAM lParam)
{
	CString* pDetailString = (CString*)wParam;

	m_rendererDetailStringStatic.SetWindowText(*pDetailString);

	delete pDetailString;
	return 0;
}


//
// Command handlers
//


void CVideoProcessorDlg::OnCommandFullScreenToggle()
{
	m_rendererfullScreen = !m_rendererfullScreen;

	m_wantToRestartRenderer = true;
	UpdateState();
}


void CVideoProcessorDlg::OnCommandFullScreenExit()
{
	// If fullscreen toggle off, else do nothing
	if (m_rendererfullScreen)
		OnCommandFullScreenToggle();
}


void CVideoProcessorDlg::OnCommandRendererReset()
{
	if (m_videoRenderer)
	{
		m_videoRenderer->Reset();
	}
}



//
// ICaptureDeviceDiscovererCallback
//


void CVideoProcessorDlg::OnCaptureDeviceFound(ACaptureDeviceComPtr& captureDevice)
{
	// WARNING: Most likely to be called from some internal capture card thread!

	assert(captureDevice);

	PostMessage(
		WM_MESSAGE_CAPTURE_DEVICE_FOUND,
		(WPARAM)captureDevice.Detach(),
		0);
}


void CVideoProcessorDlg::OnCaptureDeviceLost(ACaptureDeviceComPtr& captureDevice)
{
	// WARNING: Most likely to be called from some internal capture card thread!

	assert(captureDevice);

	PostMessage(
		WM_MESSAGE_CAPTURE_DEVICE_LOST,
		(WPARAM)captureDevice.Detach(),
		0);
}


//
// ICaptureDeviceCallback
//


void CVideoProcessorDlg::OnCaptureDeviceState(CaptureDeviceState state)
{
	// WARNING: Often, but not always, called from some internal capture card thread!

	PostMessage(
		WM_MESSAGE_CAPTURE_DEVICE_STATE_CHANGE,
		state,
		0);
}


void CVideoProcessorDlg::OnCaptureDeviceCardStateChange(CaptureDeviceCardStateComPtr cardState)
{
	// WARNING: Most likely to be called from some internal capture card thread!

	assert(cardState);

	PostMessage(
		WM_MESSAGE_CAPTURE_DEVICE_CARD_STATE_CHANGE,
		(WPARAM)cardState.Detach(),
		0);
}


void CVideoProcessorDlg::OnCaptureDeviceVideoStateChange(VideoStateComPtr videoState)
{
	// WARNING: Most likely to be called from some internal capture card thread!

	assert(videoState);
	bool rendererAcceptedState = true;  // If no renderer it'll have no problems with the new state

	// This is an atomic bool which is set by the main thread and used in context of the
	// capture thread which will deliver frames.
	if (m_deliverCaptureDataToRenderer.load(std::memory_order_acquire))
	{
		assert(m_captureDevice);
		assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING);
		assert(m_videoRenderer);
		assert(m_rendererState == RendererState::RENDERSTATE_RENDERING);

		rendererAcceptedState = m_videoRenderer->OnVideoState(videoState);
	}

	PostMessage(
		WM_MESSAGE_CAPTURE_DEVICE_VIDEO_STATE_CHANGE,
		(WPARAM)videoState.Detach(),
		rendererAcceptedState);
}


void CVideoProcessorDlg::OnCaptureDeviceVideoFrame(VideoFrame& videoFrame)
{
	// WARNING: Most likely to be called from some internal capture card thread!

	// This is an atomic bool which is set by the main thread and used in context of the
	// capture thread which will deliver frames.
	if (m_deliverCaptureDataToRenderer.load(std::memory_order_acquire))
	{
		assert(m_captureDevice);
		assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING);
		assert(m_videoRenderer);
		assert(m_rendererState == RendererState::RENDERSTATE_RENDERING);

		m_videoRenderer->OnVideoFrame(videoFrame);
	}
}


void CVideoProcessorDlg::OnRendererState(RendererState rendererState)
{
	// Will be called synchronous as a response to our calls and hence does
	// not need posting messages, we still do so to de-couple actions.

	PostMessage(
		WM_MESSAGE_RENDERER_STATE_CHANGE,
		rendererState,
		0);
}


void CVideoProcessorDlg::OnRendererDetailString(const CString& details)
{
	// Will be called synchronous as a response to our calls and hence does
	// not need posting messages, we still do so to de-couple actions.

	CString* pDetailString = new CString(details);

	PostMessage(
		WM_MESSAGE_RENDERER_DETAIL_STRING,
		(WPARAM)pDetailString,
		0);
}


void CVideoProcessorDlg::UpdateState()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::UpdateState()")));

	// Want to change cards
	if (m_desiredCaptureDevice != m_captureDevice)
	{
		m_captureInputCombo.EnableWindow(FALSE);

		// Have a render and it's rendering, stop it
		if (m_videoRenderer &&
			m_rendererState == RendererState::RENDERSTATE_RENDERING)
			RenderStop();

		// Waiting for render to go away
		// (This has to come before stopping the capture as the renderer might be
		//  using the capture card as a clock.)
		if (m_videoRenderer)
			return;

		// Have a capture and it's capturing, stop it
		if (m_captureDevice &&
			m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING)
			CaptureStop();

		// If capture device is stopped we're happy to remove it
		if (m_captureDevice && m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_READY)
			CaptureRemove();

		// We're waiting for the capture device to go away
		if (m_captureDevice)
			return;

		// From this point on we should be clean, set up new card if so desired
		assert(!m_videoRenderer);
		assert(!m_captureDevice);
		assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN ||
			   m_rendererState == RendererState::RENDERSTATE_FAILED);
		assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN);

		if (m_desiredCaptureDevice)
		{
			m_captureDevice = m_desiredCaptureDevice;
			m_captureDevice->SetCallbackHandler(this);
			m_captureDevice->SetFrameOffsetMs(GetTimingClockFrameOffsetMs());

			RefreshInputConnectionCombo();

			m_captureInputCombo.EnableWindow(TRUE);
		}
	}

	// Capture card gone, but still have renderer, can't live for much longer
	if (!m_captureDevice && m_videoRenderer)
	{
		assert(m_rendererState != RendererState::RENDERSTATE_RENDERING);
		return;
	}

	// If we want to terminate at this point we should be good to do so
	if (m_wantToTerminate)
	{
		assert(!m_captureDevice);
		assert(!m_desiredCaptureDevice);
		assert(!m_videoRenderer);
		assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN);
		assert(
			m_rendererState == RendererState::RENDERSTATE_UNKNOWN ||
			m_rendererState == RendererState::RENDERSTATE_FAILED ||
			m_rendererState == RendererState::RENDERSTATE_STOPPED);

		CDialog::EndDialog(S_OK);
		return;
	}

	// If we don't have a capture card here we don't want to.
	if (!m_captureDevice)
	{
		assert(!m_desiredCaptureDevice);
		return;
	}

	assert(m_desiredCaptureDevice == m_captureDevice);

	// Capture device still starting up
	if (m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN)
		return;

	assert(m_captureDeviceState != CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN);

	// Have the right capture device, but want different input
	if (m_desiredCaptureInputId != INVALID_CAPTURE_INPUT_ID &&
		m_desiredCaptureInputId != m_currentCaptureInputId)
	{
		// Have a render and it's rendering, stop it
		if (m_videoRenderer &&
			m_rendererState == RendererState::RENDERSTATE_RENDERING)
			RenderStop();

		// Waiting for render to go away
		// (This has to come before stopping the capture as the renderer might be
		//  using the capture card as a clock.)
		if (m_videoRenderer)
			return;

		// Have a capture and it's capturing, stop it
		if (m_captureDevice &&
			m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING)
			CaptureStop();

		// Waiting for the capture to be stopped
		if (m_captureDeviceState != CaptureDeviceState::CAPTUREDEVICESTATE_READY)
			return;

		// From this point on we should be clean, set up new card
		assert(!m_videoRenderer);
		assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN);
		assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_READY);

		m_captureDevice->SetCaptureInput(m_desiredCaptureInputId);
		m_currentCaptureInputId = m_desiredCaptureInputId;

		CaptureStart();
		return;
	}

	// Have the right card and the right input
	assert(m_captureDevice);
	assert(m_desiredCaptureDevice == m_captureDevice);
	assert(m_desiredCaptureInputId == m_currentCaptureInputId);

	// Only continue if we're actually capturing
	if (m_captureDeviceState != CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING)
		return;

	//
	// Capture is good from here on out
	//

	// No render, start one if the current state is not failed and we have a valid video state
	if (!m_videoRenderer)
	{
		// If we still have a full screen window and don't want to be full screen anymore clean it up
		if (!m_rendererfullScreen && m_fullScreenVideoWindow)
		{
			FullScreenVideoWindowDestroy();

			m_fullScreenVideoWindow = nullptr;
		}

		// If the renderer failed we don't auto-start it again but wait for something to happen
		if (m_rendererState == RendererState::RENDERSTATE_FAILED)
			return;

		if (m_captureDeviceVideoState &&
			m_captureDeviceVideoState->valid)
			RenderStart();

		return;
	}

	assert(m_videoRenderer);

	// If we have a renderer but the video state is invalid stop if rendering
	if (m_rendererState == RendererState::RENDERSTATE_RENDERING &&
		(!m_captureDeviceVideoState ||
	  	 !m_captureDeviceVideoState->valid))
	{
		RenderStop();
		return;
	}

	// Somebody wants to restart rendering, allrighty then
	if (m_rendererState == RendererState::RENDERSTATE_RENDERING &&
		m_wantToRestartRenderer)
	{
		m_wantToRestartRenderer = false;

		RenderStop();
		return;
	}

	// We have a renderer and a valid video state, relax and enjoy the show
	assert(m_captureDeviceVideoState);
	//assert(m_captureDeviceVideoState->valid);
}


//
// Helpers
//


void CVideoProcessorDlg::RefreshCaptureDeviceList()
{
	// Rebuild combo box with all devices which can capture
	m_captureDeviceCombo.ResetContent();

	for (auto& captureDevice : m_captureDevices)
	{
		if (!captureDevice->CanCapture())
			continue;

		const int index = m_captureDeviceCombo.AddString(captureDevice->GetName());
		m_captureDeviceCombo.SetItemDataPtr(index, (void*)captureDevice.p);

		// Retain selected device even if combo box position has changed
		if (captureDevice == m_captureDevice)
			m_captureDeviceCombo.SetCurSel(index);
	}

	if (m_captureDeviceCombo.GetCount() > 0)
	{
		m_captureDeviceCombo.EnableWindow(TRUE);

		// Select first capture device if none is selected yet
		const int index = m_captureDeviceCombo.GetCurSel();
		if (index == CB_ERR)
		{
			m_captureDeviceCombo.SetCurSel(0);
			OnCaptureDeviceSelected();
		}
	}
	else
	{
		m_captureDeviceCombo.EnableWindow(FALSE);

		if (m_captureDevice)
		{
			m_desiredCaptureDevice = nullptr;
			UpdateState();
		}
	}
}


void CVideoProcessorDlg::RefreshInputConnectionCombo()
{
	assert(m_captureDevice);

	const CaptureInputs captureInputs = m_captureDevice->SupportedCaptureInputs();
	const CaptureInputId currentCaptureInputId = m_captureDevice->CurrentCaptureInputId();

	m_captureInputCombo.ResetContent();

	int index;
	for (auto& captureInput : captureInputs)
	{
		index = m_captureInputCombo.AddString(captureInput.name);
		m_captureInputCombo.SetItemData(index, captureInput.id);

		// If we're in a known state keep current selection
		if(m_captureDeviceState != CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN &&
			captureInput.id == currentCaptureInputId)
		{
			m_captureInputCombo.SetCurSel(index);
			OnCaptureInputSelected();
		}
	}

	// If no input connection has been selected, select first index
	if (m_captureInputCombo.GetCount() > 0)
	{
		index = m_captureInputCombo.GetCurSel();

		// Nothing selected yet
		if (index == CB_ERR)
		{
			// Iterate all options
			bool found = false;
			for (int i = 0; i < m_captureInputCombo.GetCount(); i++)
			{
				const int n = m_captureInputCombo.GetLBTextLen(i);

				CString str;
				m_captureInputCombo.GetLBText(i, str.GetBuffer(n));
				str.ReleaseBuffer();

				if (str == TEXT("HDMI"))
				{
					m_captureInputCombo.SetCurSel(i);
					found = true;
					break;
				}
			}

			// Nothing found, just take first
			if(!found)
				m_captureInputCombo.SetCurSel(0);

			OnCaptureInputSelected();
		}
	}
}


void CVideoProcessorDlg::CaptureStart()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::CaptureStart()")));

	assert(m_captureDevice);
	assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_READY);
	assert(!m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN);

	// Update internal state before call to StartCapture as that might be synchronous
	m_captureDeviceState = CaptureDeviceState::CAPTUREDEVICESTATE_STARTING;

	m_captureDevice->StartCapture();

	// Update GUI
	m_captureDeviceStateText.SetWindowText(TEXT("Starting"));
}


void CVideoProcessorDlg::CaptureStop()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::CaptureStop(): Begin")));

	assert(m_captureDevice);
	assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING);
	assert(!m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN ||
		   m_rendererState == RendererState::RENDERSTATE_FAILED);

	// Update internal state before call to StartCapture as that might be synchronous
	m_captureDeviceState = CaptureDeviceState::CAPTUREDEVICESTATE_STOPPING;

	m_captureDevice->StopCapture();

	m_captureDeviceVideoState = nullptr;

	// Update GUI
	CaptureGUIClear();
	m_captureDeviceStateText.SetWindowText(TEXT("Stopping"));

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::CaptureStop(): End")));
}


void CVideoProcessorDlg::CaptureRemove()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::CaptureRemove(): Begin")));

	assert(m_captureDevice);
	assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_READY);
	assert(!m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN ||
		   m_rendererState == RendererState::RENDERSTATE_FAILED);

	m_captureDeviceState = CaptureDeviceState::CAPTUREDEVICESTATE_UNKNOWN;
	m_captureDevice->SetCallbackHandler(nullptr);
	m_captureDevice.Release();
	m_captureDevice = nullptr;

	m_desiredCaptureInputId = INVALID_CAPTURE_INPUT_ID;
	m_currentCaptureInputId = INVALID_CAPTURE_INPUT_ID;

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::CaptureRemove(): End")));
}


void CVideoProcessorDlg::CaptureGUIClear()
{
	// Capture device group
	m_captureDeviceStateText.SetWindowText(TEXT(""));
	m_captureDeviceOtherList.ResetContent();

	// Input group
	m_inputLockedText.SetWindowText(TEXT(""));
	m_inputDisplayModeText.SetWindowText(TEXT(""));
	m_inputEncodingText.SetWindowText(TEXT(""));
	m_inputBitDepthText.SetWindowText(TEXT(""));
	m_inputVideoFrameCountText.SetWindowText(TEXT(""));
	m_inputVideoFrameMissedText.SetWindowText(TEXT(""));

	// Captured video group
	m_videoValidText.SetWindowText(TEXT(""));
	m_videoDisplayModeText.SetWindowText(TEXT(""));
	m_videoPixelFormatText.SetWindowText(TEXT(""));
	m_videoEotfText.SetWindowText(TEXT(""));
	m_videoColorSpaceText.SetWindowText(TEXT(""));

	// Timing clock
	m_timingClockDescriptionText.SetWindowText(TEXT(""));

	// ColorSpace group
	m_colorspaceCie1931xy.SetColorSpace(ColorSpace::UNKNOWN);
	m_colorspaceCie1931xy.SetHDRData(nullptr);

	// HDR group
	m_luminanceMaxCll.SetWindowText(TEXT(""));
	m_luminanceMaxFall.SetWindowText(TEXT(""));
	m_luminanceMasterMin.SetWindowText(TEXT(""));
	m_luminanceMasterMax.SetWindowText(TEXT(""));
}


void CVideoProcessorDlg::RenderStart()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderStart(): Begin")));

	assert(!m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_UNKNOWN ||
		   m_rendererState == RendererState::RENDERSTATE_STOPPED);

	assert(m_captureDevice);
	assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING);

	int i;

	i = m_rendererCombo.GetCurSel();

	// No renderer picked yet, ignore
	if (i < 0)
		return;

	GUID* rendererClSID = (GUID*)m_rendererCombo.GetItemData(i);

	// Get directshow start-stop method
	i = m_rendererDirectShowStartStopTimeMethodCombo.GetCurSel();
	assert(i >= 0);
	DirectShowStartStopTimeMethod directShowStartStopTimeMethod =
		(DirectShowStartStopTimeMethod)m_rendererDirectShowStartStopTimeMethodCombo.GetItemData(i);

	// Get forced video conversion
	i = m_rendererVideoConversionCombo.GetCurSel();
	assert(i >= 0);
	VideoConversionOverride videoConversionOverride =
		(VideoConversionOverride)m_rendererVideoConversionCombo.GetItemData(i);

	// Capture card always provides the clock
	ITimingClock* timingClock = m_captureDevice->GetTimingClock();
	if (!timingClock)
		FatalError(TEXT("Failed to get timing clock from capture card"));

	m_windowedVideoWindow.SetWindowTextW(TEXT("Starting..."));
	m_rendererState = RendererState::RENDERSTATE_STARTING;

	//
	// Construct renderer
	//

	try
	{
		i = m_rendererNominalRangeCombo.GetCurSel();
		assert(i >= 0);
		DXVA_NominalRange forceNominalRange =
			(DXVA_NominalRange)m_rendererNominalRangeCombo.GetItemData(i);

		i = m_rendererTransferFunctionCombo.GetCurSel();
		assert(i >= 0);
		DXVA_VideoTransferFunction forceVideoTransferFunction =
			(DXVA_VideoTransferFunction)m_rendererTransferFunctionCombo.GetItemData(i);

		i = m_rendererTransferMatrixCombo.GetCurSel();
		assert(i >= 0);
		DXVA_VideoTransferMatrix forceVideoTransferMatrix =
			(DXVA_VideoTransferMatrix)m_rendererTransferMatrixCombo.GetItemData(i);

		i = m_rendererPrimariesCombo.GetCurSel();
		assert(i >= 0);
		DXVA_VideoPrimaries forceVideoPrimaries =
			(DXVA_VideoPrimaries)m_rendererPrimariesCombo.GetItemData(i);

		m_videoRenderer = new DirectShowGenericHDRVideoRenderer(
			*rendererClSID,
			*this,
			GetRenderWindow(),
			this->GetSafeHwnd(),
			WM_MESSAGE_DIRECTSHOW_NOTIFICATION,
			timingClock,
			directShowStartStopTimeMethod,
			GetRendererVideoFrameUseQueue(),
			GetRendererVideoFrameQueueSizeMax(),
			videoConversionOverride,
			forceNominalRange,
			forceVideoTransferFunction,
			forceVideoTransferMatrix,
			forceVideoPrimaries);

		if (m_captureDeviceVideoState)
			m_videoRenderer->OnVideoState(m_captureDeviceVideoState);

		m_videoRenderer->Build();
		m_videoRenderer->Start();

		m_rendererStateText.SetWindowText(TEXT("Started HDR renderer, waiting for image..."));

	}
	catch (std::runtime_error e)
	{
		if (m_videoRenderer)
		{
			delete m_videoRenderer;
			m_videoRenderer = nullptr;
		}

		try
		{
			if (IsEqualCLSID(*rendererClSID, CLSID_MPCVR))
			{
				m_videoRenderer = new DirectShowMPCVideoRenderer(
					*this,
					GetRenderWindow(),
					this->GetSafeHwnd(),
					WM_MESSAGE_DIRECTSHOW_NOTIFICATION,
					timingClock,
					directShowStartStopTimeMethod,
					GetRendererVideoFrameUseQueue(),
					GetRendererVideoFrameQueueSizeMax(),
					videoConversionOverride);
			}
			else if (IsEqualCLSID(*rendererClSID, CLSID_EnhancedVideoRenderer))
			{
				m_videoRenderer = new DirectShowEnhancedVideoRenderer(
					*this,
					GetRenderWindow(),
					this->GetSafeHwnd(),
					WM_MESSAGE_DIRECTSHOW_NOTIFICATION,
					timingClock,
					directShowStartStopTimeMethod,
					GetRendererVideoFrameUseQueue(),
					GetRendererVideoFrameQueueSizeMax(),
					videoConversionOverride);
			}
			else
				m_videoRenderer = new DirectShowGenericVideoRenderer(
					*rendererClSID,
					*this,
					GetRenderWindow(),
					this->GetSafeHwnd(),
					WM_MESSAGE_DIRECTSHOW_NOTIFICATION,
					timingClock,
					directShowStartStopTimeMethod,
					GetRendererVideoFrameUseQueue(),
					GetRendererVideoFrameQueueSizeMax(),
					videoConversionOverride);

			if (!m_videoRenderer)
				FatalError(TEXT("Failed to build DirectShow Video Renderer"));

			if (m_captureDeviceVideoState)
				m_videoRenderer->OnVideoState(m_captureDeviceVideoState);

			m_videoRenderer->Build();
			m_videoRenderer->Start();

			m_rendererStateText.SetWindowText(TEXT("Started, waiting for image..."));
		}
		catch (std::runtime_error e)
		{
			delete m_videoRenderer;
			m_videoRenderer = nullptr;

			m_rendererState = RendererState::RENDERSTATE_FAILED;
			m_rendererStateText.SetWindowText(TEXT("Failed"));

			// Show error in renderer box
			size_t size = strlen(e.what()) + 1;
			wchar_t* wtext = new wchar_t[size];
			size_t outSize;
			mbstowcs_s(&outSize, wtext, size, e.what(), size - 1);
			m_windowedVideoWindow.SetWindowText(wtext);
			delete[] wtext;

			// Ensure we're not full screen anymore and update state
			m_rendererfullScreen = false;
			UpdateState();

			// Give the user a chance to try again
			m_rendererRestartButton.EnableWindow(true);
		}
	}

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderStart(): End")));
}


void CVideoProcessorDlg::RenderStop()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderStop(): Begin")));

	assert(m_captureDevice);
	assert(m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING);

	assert(m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_RENDERING);
	assert(m_deliverCaptureDataToRenderer.load(std::memory_order_acquire));

	// After this call no frames will ever go through to the renderer
	m_deliverCaptureDataToRenderer.store(false, std::memory_order_release);

	// Update internal state before call to StartCapture as that might be synchronous
	m_rendererState = RendererState::RENDERSTATE_STOPPING;

	m_videoRenderer->Stop();

	m_rendererStateText.SetWindowText(TEXT("Stopping"));

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderStop(): End")));
}


void CVideoProcessorDlg::RenderRemove()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderRemove(): Begin")));

	assert(m_videoRenderer);
	assert(m_rendererState == RendererState::RENDERSTATE_STOPPED);
	assert(!m_deliverCaptureDataToRenderer);

	delete m_videoRenderer;
	m_videoRenderer = nullptr;

	m_rendererState = RendererState::RENDERSTATE_UNKNOWN;

	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::RenderRemove(): End")));
}


void CVideoProcessorDlg::RenderGUIClear()
{
	m_rendererDetailStringStatic.SetWindowText(TEXT(""));
	m_rendererVideoFrameQueueSizeText.SetWindowText(TEXT(""));
	m_rendererDroppedFrameCountText.SetWindowText(TEXT(""));

	m_windowedVideoWindow.ShowLogo(true);
}


void CVideoProcessorDlg::FullScreenVideoWindowConstruct()
{
	assert(!m_fullScreenVideoWindow);

	HMONITOR hmon = MonitorFromWindow(this->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST);

	m_fullScreenVideoWindow = new FullscreenVideoWindow();
	if (!m_fullScreenVideoWindow)
		FatalError(TEXT("Failed to create full screen renderer window"));

	m_fullScreenVideoWindow->Create(hmon, this->GetSafeHwnd());
}


void CVideoProcessorDlg::FullScreenVideoWindowDestroy()
{
	assert(m_fullScreenVideoWindow);
	delete m_fullScreenVideoWindow;
	m_fullScreenVideoWindow = nullptr;
}


HWND CVideoProcessorDlg::GetRenderWindow()
{
	if (m_rendererfullScreen)
	{
		// If we don't have a full screen window yet make one
		if (!m_fullScreenVideoWindow)
			FullScreenVideoWindowConstruct();

		assert(IsWindow(m_fullScreenVideoWindow->GetHWND()));
		return m_fullScreenVideoWindow->GetHWND();
	}

	assert(!m_fullScreenVideoWindow);
	assert(IsWindow(m_windowedVideoWindow));
	return m_windowedVideoWindow;
}


size_t CVideoProcessorDlg::GetRendererVideoFrameQueueSizeMax()
{
	// Note that this field is marked as numbers only so guaranteed to convert corrrectly

	CString text;
	m_rendererVideoFrameQueueSizeMaxEdit.GetWindowText(text);
	return _ttoi(text);
}


bool CVideoProcessorDlg::GetRendererVideoFrameUseQueue()
{
	return m_rendererVideoFrameUseQeueueCheck.GetCheck();
}


int CVideoProcessorDlg::GetTimingClockFrameOffsetMs()
{
	CString text;
	m_timingClockFrameOffsetEdit.GetWindowText(text);

	// ttoi throws non-parsed stuff away so in case there is crap set the output to the
	// used value, this way the user always knows what's going on.
	const int frameOffsetMs = _ttoi(text);
	SetTimingClockFrameOffsetMs(frameOffsetMs);

	return frameOffsetMs;
}


void CVideoProcessorDlg::SetTimingClockFrameOffsetMs(int timingClockFrameOffsetMs)
{
	CString cstring;
	cstring.Format(_T("%i"), timingClockFrameOffsetMs);
	m_timingClockFrameOffsetEdit.SetWindowText(cstring);
}


void CVideoProcessorDlg::UpdateTimingClockFrameOffset()
{
	if (m_captureDevice)
		m_captureDevice->SetFrameOffsetMs(GetTimingClockFrameOffsetMs());

	if (m_videoRenderer)
		m_videoRenderer->Reset();
}


void CVideoProcessorDlg::RebuildRendererCombo()
{
	ClearRendererCombo();

	std::vector<RendererId> rendererIds;

	//
	// Get all supported renderer ids
	//

	DirectShowVideoRendererIds(rendererIds);

	//
	// Populate selection box, sorted
	//

	std::sort(rendererIds.begin(), rendererIds.end());
	for (const auto& rendererEntry : rendererIds)
	{
		GUID* clsid = new GUID(rendererEntry.guid);

		int comboIndex = m_rendererCombo.AddString(rendererEntry.name);
		m_rendererCombo.SetItemData(comboIndex, (DWORD_PTR)clsid);

		if (rendererEntry.name.CompareNoCase(m_defaultRendererName) == 0)
		{
			m_rendererCombo.SetCurSel(comboIndex);
		}
	}
}


void CVideoProcessorDlg::ClearRendererCombo()
{
	for (int i = 0; i < m_rendererCombo.GetCount(); i++)
	{
		delete (void *)m_rendererCombo.GetItemData(i);
	}

	m_rendererCombo.ResetContent();
}


void CVideoProcessorDlg::_FatalError(int line, const std::string& functionName, const CString& error)
{
	CString s;
	s.Format(
		_T("%s\r\n\rFile: VideoProcessorDlg.cpp:%i\r\nFunction: %s"),
		error, line, CString(functionName.c_str()));

	::MessageBox(nullptr, s, TEXT("Fatal error"), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);

	CDialog::EndDialog(S_FALSE);
}


//
// CDialog
//


void CVideoProcessorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	// Capture device group
	DDX_Control(pDX, IDC_CAPTURE_DEVICE_COMBO, m_captureDeviceCombo);
	DDX_Control(pDX, IDC_CAPTURE_INPUT_COMBO, m_captureInputCombo);
	DDX_Control(pDX, IDC_CAPTURE_STATE_STATIC, m_captureDeviceStateText);
	DDX_Control(pDX, IDC_CAPTURE_DEVICE_OTHER_LIST, m_captureDeviceOtherList);

	// Input group
	DDX_Control(pDX, IDC_INPUT_LOCKED_STATIC, m_inputLockedText);
	DDX_Control(pDX, IDC_INPUT_DISPLAY_MODE_STATIC, m_inputDisplayModeText);
	DDX_Control(pDX, IDC_INPUT_ENCODING_STATIC, m_inputEncodingText);
	DDX_Control(pDX, IDC_INPUT_BIT_DEPTH_STATIC, m_inputBitDepthText);
	DDX_Control(pDX, IDC_INPUT_VIDEO_FRAME_COUNT_STATIC, m_inputVideoFrameCountText);
	DDX_Control(pDX, IDC_INPUT_VIDEO_FRAME_MISSED_STATIC, m_inputVideoFrameMissedText);
	DDX_Control(pDX, IDC_INPUT_LATENCY_MS_STATIC, m_inputLatencyMsText);

	// Captured video group
	DDX_Control(pDX, IDC_VIDEO_VALID_STATIC, m_videoValidText);
	DDX_Control(pDX, IDC_VIDEO_DISPLAY_MODE_STATIC, m_videoDisplayModeText);
	DDX_Control(pDX, IDC_VIDEO_PIXEL_FORMAT_STATIC, m_videoPixelFormatText);
	DDX_Control(pDX, IDC_VIDEO_EOTF_STATIC, m_videoEotfText);
	DDX_Control(pDX, IDC_VIDEO_COLORSPACE_STATIC, m_videoColorSpaceText);

	// Timing clock group
	DDX_Control(pDX, IDC_TIMING_CLOCK_DESCRIPTION_STATIC, m_timingClockDescriptionText);
	DDX_Control(pDX, IDC_TIMING_CLOCK_FRAME_OFFSET_EDIT, m_timingClockFrameOffsetEdit);
	DDX_Control(pDX, IDC_TIMING_CLOCK_FRAME_OFFSET_AUTO_CHECK, m_timingClockFrameOffsetAutoCheck);

	// ColorSpace group
	DDX_Control(pDX, IDC_CIE1931XY_GRAPH, m_colorspaceCie1931xy);

	DDX_Control(pDX, IDC_COLORSPACE_CONTAINER_PRESET_COMBO, m_colorspaceContainerPresetCompbo);
	DDX_Control(pDX, IDC_COLORSPACE_CONTAINER_R_EDIT, m_colorspaceContainerREdit);
	DDX_Control(pDX, IDC_COLORSPACE_CONTAINER_G_EDIT, m_colorspaceContainerGEdit);
	DDX_Control(pDX, IDC_COLORSPACE_CONTAINER_B_EDIT, m_colorspaceContainerBEdit);
	DDX_Control(pDX, IDC_COLORSPACE_CONTAINER_WP_EDIT, m_colorspaceContainerWPEdit);
	DDX_Control(pDX, IDC_COLORSPACE_VIDEO_PRESET_COMBO, m_colorspaceVideoPresetCompbo);
	DDX_Control(pDX, IDC_COLORSPACE_VIDEO_R_EDIT, m_colorspaceVideoREdit);
	DDX_Control(pDX, IDC_COLORSPACE_VIDEO_G_EDIT, m_colorspaceVideoGEdit);
	DDX_Control(pDX, IDC_COLORSPACE_VIDEO_B_EDIT, m_colorspaceVideoBEdit);
	DDX_Control(pDX, IDC_COLORSPACE_VIDEO_WP_EDIT, m_colorspaceVideoWPEdit);
	DDX_Control(pDX, IDC_COLORSPACE_RESET_BUTTON, m_colorspaceResetButton);
	DDX_Control(pDX, IDC_COLORSPACE_C_TO_V_BUTTON, m_colorspaceContainerToVideoButton);

	//  (HDR) luminance group
	DDX_Control(pDX, IDC_LUMINANCE_MAXCLL_EDIT, m_luminanceMaxCll);
	DDX_Control(pDX, IDC_LUMINANCE_MAXFALL_EDIT, m_luminanceMaxFall);
	DDX_Control(pDX, IDC_LUMINANCE_MASTER_MIN_EDIT, m_luminanceMasterMin);
	DDX_Control(pDX, IDC_LUMINANCE_MASTER_MAX_EDIT, m_luminanceMasterMax);

	// Renderer group
	DDX_Control(pDX, IDC_RENDERER_COMBO, m_rendererCombo);
	DDX_Control(pDX, IDC_RENDERER_DETAIL_STRING_STATIC, m_rendererDetailStringStatic);
	DDX_Control(pDX, IDC_RENDERER_RESTART_BUTTON, m_rendererRestartButton);
	DDX_Control(pDX, IDC_RENDERER_STATE_STATIC, m_rendererStateText);
	DDX_Control(pDX, IDC_RENDERER_WINDOWED_VIDEO_WINDOW, m_windowedVideoWindow);

	// Renderer Queue group
	DDX_Control(pDX, IDC_RENDERER_VIDEO_FRAME_USE_QUEUE_CHECK, m_rendererVideoFrameUseQeueueCheck);
	DDX_Control(pDX, IDC_RENDERER_VIDEO_FRAME_QUEUE_SIZE_STATIC, m_rendererVideoFrameQueueSizeText);
	DDX_Control(pDX, IDC_RENDERER_VIDEO_FRAME_QUEUE_SIZE_MAX_EDIT, m_rendererVideoFrameQueueSizeMaxEdit);
	DDX_Control(pDX, IDC_RENDERER_DROPPED_FRAME_COUNT_STATIC, m_rendererDroppedFrameCountText);
	DDX_Control(pDX, IDC_RENDERER_RESET_BUTTON, m_rendererResetButton);
	DDX_Control(pDX, IDC_RENDERER_RESET_AUTO_CHECK, m_rendererResetAutoCheck);

	// Renderer Video conversion group
	DDX_Control(pDX, IDC_RENDERER_VIDEO_CONVERSION_COMBO, m_rendererVideoConversionCombo);

	// Renderer DirectShow override group
	DDX_Control(pDX, IDC_RENDERER_DIRECTSHOW_START_STOP_TIME_METHOD_COMBO, m_rendererDirectShowStartStopTimeMethodCombo);
	DDX_Control(pDX, IDC_RENDERER_DIRECTSHOW_NOMINAL_RANGE_COMBO, m_rendererNominalRangeCombo);
	DDX_Control(pDX, IDC_RENDERER_DIRECTSHOW_TRANSFER_FUNCTION_COMBO, m_rendererTransferFunctionCombo);
	DDX_Control(pDX, IDC_RENDERER_DIRECTSHOW_TRANSFER_MATRIX_COMBO, m_rendererTransferMatrixCombo);
	DDX_Control(pDX, IDC_RENDERER_DIRECTSHOW_PRIMARIES_COMBO, m_rendererPrimariesCombo);

	// Renderer latency (ms) group
	DDX_Control(pDX, IDC_RENDERER_LATENCY_TO_VP_STATIC, m_rendererLatencyToVPText);
	DDX_Control(pDX, IDC_RENDERER_LATENCY_TO_DS_STATIC, m_rendererLatencyToDSText);

	// Renderer output group
	DDX_Control(pDX, IDC_RENDERER_FULL_SCREEN_CHECK, m_rendererFullscreenCheck);
}



// Called when the dialog box is initialized
BOOL CVideoProcessorDlg::OnInitDialog()
{
	if (!CDialog::OnInitDialog())
		return FALSE;

	CString title;
	title.Format(_T("VideoProcessor (%s)"), VERSION_DESCRIBE);
	SetWindowText(title.GetBuffer());

	SetIcon(m_hIcon, FALSE);

	// Set initial dialog size as minimum size
	CRect rectWindow;
	GetWindowRect(rectWindow);
	m_minDialogSize = rectWindow.Size();

	// Empty popup menus
	m_captureDeviceCombo.ResetContent();

	// Disable the interface
	m_captureDeviceCombo.EnableWindow(FALSE);
	m_captureInputCombo.EnableWindow(FALSE);

	// Get all renderers
	RebuildRendererCombo();

	// Fill renderer selection boxes
	for (auto p : RENDERER_DIRECTSHOW_START_STOP_TIME_OPTIONS)
	{
		int index = m_rendererDirectShowStartStopTimeMethodCombo.AddString(ToString(p));
		m_rendererDirectShowStartStopTimeMethodCombo.SetItemData(index, (int)p);
	}
	m_rendererDirectShowStartStopTimeMethodCombo.SetCurSel(0);

	for (const auto& p : DIRECTSHOW_NOMINAL_RANGE_OPTIONS)
	{
		int index = m_rendererNominalRangeCombo.AddString(p.first);
		m_rendererNominalRangeCombo.SetItemData(index, p.second);
	}
	m_rendererNominalRangeCombo.SetCurSel(0);

	for (const auto& p : DIRECTSHOW_TRANSFER_FUNCTION_OPTIONS)
	{
		int index = m_rendererTransferFunctionCombo.AddString(p.first);
		m_rendererTransferFunctionCombo.SetItemData(index, (int)p.second);
	}
	m_rendererTransferFunctionCombo.SetCurSel(0);

	for (const auto& p : DIRECTSHOW_TRANSFER_MATRIX_OPTIONS)
	{
		int index = m_rendererTransferMatrixCombo.AddString(p.first);
		m_rendererTransferMatrixCombo.SetItemData(index, (int)p.second);
	}
	m_rendererTransferMatrixCombo.SetCurSel(0);

	for (const auto& p : DIRECTSHOW_PRIMARIES_OPTIONS)
	{
		int index = m_rendererPrimariesCombo.AddString(p.first);
		m_rendererPrimariesCombo.SetItemData(index, (int)p.second);
	}
	m_rendererPrimariesCombo.SetCurSel(0);

	for (const auto& p : RENDERER_VIDEO_CONVERSION)
	{
		int index = m_rendererVideoConversionCombo.AddString(ToString(p));
		m_rendererVideoConversionCombo.SetItemData(index, (int)p);
	}
	m_rendererVideoConversionCombo.SetCurSel(0);

	// Start discovery services
	m_blackMagicDeviceDiscoverer->Start();

	m_accelerator = LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1));
	if (!m_accelerator)
		FatalError(TEXT("Failed to load accelerator"));

	CaptureGUIClear();
	RenderGUIClear();

	m_rendererVideoFrameQueueSizeMaxEdit.SetWindowText(TEXT("32"));
	m_timingClockFrameOffsetEdit.SetWindowText(TEXT("90"));
	m_rendererVideoFrameUseQeueueCheck.SetCheck(true);
	m_rendererResetAutoCheck.SetCheck(true);

	// Start timers
	SetTimer(TIMER_ID_1SECOND, 1000, nullptr);

	return TRUE;
}


BOOL CVideoProcessorDlg::PreTranslateMessage(MSG* pMsg)
{
	// Handle accelerator combinations
	if (m_accelerator)
	{
		if (::TranslateAccelerator(m_hWnd, m_accelerator, pMsg))
			return TRUE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void CVideoProcessorDlg::OnOK()
{
	// Called if the user presses enter somewhere

	CWnd* pwndCtrl = GetFocus();
	int ctrl_ID = pwndCtrl->GetDlgCtrlID();

	switch (ctrl_ID)
	{
		case IDC_TIMING_CLOCK_FRAME_OFFSET_EDIT:
			UpdateTimingClockFrameOffset();
			break;

		case IDC_RENDERER_VIDEO_FRAME_QUEUE_SIZE_MAX_EDIT:
			if (m_videoRenderer)
			{
				m_videoRenderer->SetFrameQueueMaxSize(GetRendererVideoFrameQueueSizeMax());
			}
			break;

		default:
			m_wantToRestartRenderer = true;
			UpdateState();
	}

	// Don't call the super implmenetation as that will close the window
}


void CVideoProcessorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}


void CVideoProcessorDlg::OnSize(UINT nType, int cx, int cy)
{
	if (m_videoRenderer)
		m_videoRenderer->OnSize();

	CDialog::OnSize(nType, cx, cy);
}


void CVideoProcessorDlg::OnSetFocus(CWnd* pOldWnd)
{
	CDialog::OnSetFocus(pOldWnd);
}


void CVideoProcessorDlg::OnClose()
{
	DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnClose()")));

	if (m_wantToTerminate)
		return;

	// Set intent first, stopping the discoverer will lead to state update calls
	m_desiredCaptureDevice = nullptr;
	m_wantToTerminate = true;

	// Stop discovery
	if (m_blackMagicDeviceDiscoverer)
	{
		m_blackMagicDeviceDiscoverer->Stop();
		m_blackMagicDeviceDiscoverer.Release();
	}

	UpdateState();

	// Remove all renderers
	ClearRendererCombo();
}


void CVideoProcessorDlg::OnTimer(UINT_PTR nIDEvent)
{
	CString cstring;

	if (m_rendererState == RendererState::RENDERSTATE_RENDERING)
	{
		cstring.Format(_T("%lu"), m_videoRenderer->GetFrameQueueSize());
		m_rendererVideoFrameQueueSizeText.SetWindowText(cstring);

		cstring.Format(_T("%.01f"), m_videoRenderer->EntryLatencyMs());
		m_rendererLatencyToVPText.SetWindowText(cstring);

		cstring.Format(_T("%.01f"), m_videoRenderer->ExitLatencyMs());
		m_rendererLatencyToDSText.SetWindowText(cstring);

		const double frameMs = 1000.0 / m_captureDeviceVideoState->displayMode->RefreshRateHz();

		if (m_videoRenderer->ExitLatencyMs() < (-3*frameMs) ||
			m_videoRenderer->ExitLatencyMs() > 10)
			m_rendererLatencyToDSText.SetTextColor(CColorStatic::RED);
		else if (m_videoRenderer->ExitLatencyMs() < (-2*frameMs) ||
			     m_videoRenderer->ExitLatencyMs() > -5)
			m_rendererLatencyToDSText.SetTextColor(CColorStatic::ORANGE);
		else
			m_rendererLatencyToDSText.SetTextColor(CColorStatic::GREEN);

		cstring.Format(_T("%lu"), m_videoRenderer->DroppedFrameCount());
		m_rendererDroppedFrameCountText.SetWindowText(cstring);
	}
	else
	{
		m_rendererVideoFrameQueueSizeText.SetWindowText(_T(""));
		m_rendererLatencyToVPText.SetWindowText(_T(""));
		m_rendererLatencyToDSText.SetWindowText(_T(""));
		m_rendererDroppedFrameCountText.SetWindowText(TEXT(""));
	}

	if (m_captureDeviceState == CaptureDeviceState::CAPTUREDEVICESTATE_CAPTURING)
	{
		cstring.Format(_T("%lu"), m_captureDevice->VideoFrameCapturedCount());
		m_inputVideoFrameCountText.SetWindowText(cstring);

		cstring.Format(_T("%lu"), m_captureDevice->VideoFrameMissedCount());
		m_inputVideoFrameMissedText.SetWindowText(cstring);

		cstring.Format(_T("%.01f"), m_captureDevice->HardwareLatencyMs());
		m_inputLatencyMsText.SetWindowText(cstring);

		if (m_captureDevice->HardwareLatencyMs() < 10)
			m_inputLatencyMsText.SetTextColor(CColorStatic::GREEN);
		else if (m_captureDevice->HardwareLatencyMs() < 15)
			m_inputLatencyMsText.SetTextColor(CColorStatic::ORANGE);
		else
			m_inputLatencyMsText.SetTextColor(CColorStatic::RED);
	}
	else
	{
		m_inputVideoFrameCountText.SetWindowText(TEXT(""));
		m_inputVideoFrameMissedText.SetWindowText(TEXT(""));
		m_inputLatencyMsText.SetWindowText(_T(""));
	}

	// Prevent screensaver, this should be called "periodically" for whatever that means
	if (m_timerSeconds % 60 == 0)
	{
		SetThreadExecutionState(ES_DISPLAY_REQUIRED);
	}


	// Auto adjust
	if (m_timerSeconds % 5 == 0 &&
		m_rendererState == RendererState::RENDERSTATE_RENDERING)
	{
		assert(m_videoRenderer);
		assert(m_captureDevice);

		bool queueOk = true;

		// Auto-click reset on renderer if requested
		const bool rendererResetAuto = m_rendererResetAutoCheck.GetCheck();
		if (rendererResetAuto)
		{
			const bool needsReset = m_videoRenderer->GetFrameQueueSize() >= 3;
			queueOk = !needsReset;

			if (needsReset)
			{
				DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnTimer(): Resetting renderer")));
				m_videoRenderer->Reset();
			}
		}

		// Auto update the clock frame offset to get just over zero
		// only do this if the queue is ok as it will have a major impact on the offset(or unmanaged)
		const bool timingClockFrameOffsetAuto = m_timingClockFrameOffsetAutoCheck.GetCheck();
		if (queueOk && timingClockFrameOffsetAuto)
		{
			const double videoFrameLead = -(m_videoRenderer->ExitLatencyMs());
			const double frameDurationMs = 1000.0 / m_captureDeviceVideoState->displayMode->RefreshRateHz();

			const bool needsAdjusting =
				videoFrameLead < 0 ||
				videoFrameLead >(frameDurationMs * 2);

			if (needsAdjusting)
			{
				DbgLog((LOG_TRACE, 1, TEXT("CVideoProcessorDlg::OnTimer(): Adjusting clock frame offset + reset")));

				const int delta = (int)round(-videoFrameLead);
				const int newOffset = GetTimingClockFrameOffsetMs() + delta;

				SetTimingClockFrameOffsetMs(newOffset);
				UpdateTimingClockFrameOffset();
			}
		}
	}

	++m_timerSeconds;
}


HCURSOR CVideoProcessorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CVideoProcessorDlg::OnGetMinMaxInfo(MINMAXINFO* minMaxInfo)
{
	CDialog::OnGetMinMaxInfo(minMaxInfo);

	// Guarantee minimum size of window
	minMaxInfo->ptMinTrackSize.x = std::max(minMaxInfo->ptMinTrackSize.x, m_minDialogSize.cx);
	minMaxInfo->ptMinTrackSize.y = std::max(minMaxInfo->ptMinTrackSize.y, m_minDialogSize.cy);
}
