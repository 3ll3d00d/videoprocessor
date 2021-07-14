/*
 * Copyright(C) 2021 Dennis Fleurbaaij <mail@dennisfleurbaaij.com>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program. If not, see < https://www.gnu.org/licenses/>.
 */

#include "stdafx.h"

#include <dvdmedia.h>

#include <guid.h>
#include <CNoopVideoFrameFormatter.h>
#include <microsoft_directshow/DirectShowTranslations.h>
#include <ffmpeg/CFFMpegDecoderVideoFrameFormatter.h>

#include "DirectShowGenericHDRVideoRenderer.h"


DirectShowGenericHDRVideoRenderer::DirectShowGenericHDRVideoRenderer(
	GUID rendererCLSID,
	IRendererCallback& callback,
	HWND videoHwnd,
	HWND eventHwnd,
	UINT eventMsg,
	ITimingClock* timingClock,
	DirectShowStartStopTimeMethod timestamp,
	bool useFrameQueue,
	size_t frameQueueMaxSize,
	DXVA_NominalRange forceNominalRange,
	DXVA_VideoTransferFunction forceVideoTransferFunction,
	DXVA_VideoTransferMatrix forceVideoTransferMatrix,
	DXVA_VideoPrimaries forceVideoPrimaries):
	DirectShowVideoRenderer(
		callback,
		videoHwnd,
		eventHwnd,
		eventMsg,
		timingClock,
		timestamp,
		useFrameQueue,
		frameQueueMaxSize),
	m_rendererCLSID(rendererCLSID),
	m_forceNominalRange(forceNominalRange),
	m_forceVideoTransferFunction(forceVideoTransferFunction),
	m_forceVideoTransferMatrix(forceVideoTransferMatrix),
	m_forceVideoPrimaries(forceVideoPrimaries)
{
	callback.OnRendererDetailString(TEXT("DirectShow HDR renderer"));
}


//
// IRenderer
//

bool DirectShowGenericHDRVideoRenderer::OnVideoState(VideoStateComPtr& videoState)
{
	if (!DirectShowVideoRenderer::OnVideoState(videoState))
		return false;

	// Handle HDR data
	if (videoState->hdrData)
	{
		if (!m_videoState->hdrData || *videoState->hdrData != *(m_videoState->hdrData))
		{
			if (FAILED(m_liveSource->OnHDRData(videoState->hdrData)))
				throw std::runtime_error("Failed to set HDR data");

			// Update the HDR in our local videostate copy
			m_videoState->hdrData = videoState->hdrData;
		}
	}

	return true;
}


//
// DirectShowVideoRenderer
//


void DirectShowGenericHDRVideoRenderer::RendererBuild()
{
	if (FAILED(CoCreateInstance(
		m_rendererCLSID,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IBaseFilter,
		(void**)&m_pRenderer)))
		throw std::runtime_error("Failed to create renderer instance");
}


void DirectShowGenericHDRVideoRenderer::MediaTypeGenerate()
{
	GUID mediaSubType;
	int bitCount;
	LONG heightMultiplier = 1;

	switch (m_videoState->videoFrameEncoding)
	{
		/* TODO: FIXME
		// v210 (YUV422)
		case VideoFrameEncoding::YUV_10BIT:

			// Fails:
			//  - P010/P010: Solid green
			//  - YV12/YUV420P: Solid green
			//  - MEDIASUBTYPE_P216/16/AV_CODEC_ID_V210X/AV_PIX_FMT_YUV422P16 : some moving artifacts
			//  - MEDIASUBTYPE_P210/10/AV_CODEC_ID_V210/AV_PIX_FMT_YUV422P10 : Solid green
			//  - MEDIASUBTYPE_P010/10/AV_CODEC_ID_V210/AV_PIX_FMT_P010 : Solid green
			//  - MEDIASUBTYPE_RGB48LE/48/AV_CODEC_ID_V210/AV_PIX_FMT_RGB48LE : Solid green (=decoder problem)
			//  - MEDIASUBTYPE_RGB48LE/48/AV_CODEC_ID_V210X/AV_PIX_FMT_RGB48LE : Moving artifacts which look like expected video
			//  - MEDIASUBTYPE_P210/10/AV_CODEC_ID_V210/AV_PIX_FMT_YUV422P10 : Solid green
			//
			// AV_CODEC_ID_V210 outputs AV_PIX_FMT_YUV422P10
			// AV_CODEC_ID_V210X outputs AV_PIX_FMT_YUV422P16

			mediaSubType = MEDIASUBTYPE_P010;
			bitCount = 10;

			m_videoFramFormatter = new CFFMpegDecoderVideoFrameFormatter(
				AV_CODEC_ID_V210,
				AV_PIX_FMT_P010);
			break;
			*/
		// r210 to RGB48
	case VideoFrameEncoding::R210:

		mediaSubType = MEDIASUBTYPE_RGB48LE;
		bitCount = 48;
		heightMultiplier = -1;

		m_videoFramFormatter = new CFFMpegDecoderVideoFrameFormatter(
			AV_CODEC_ID_R210,
			AV_PIX_FMT_RGB48LE);
		break;

		// RGB 12-bit to RGB48
	case VideoFrameEncoding::R12B:

		mediaSubType = MEDIASUBTYPE_RGB48LE;
		bitCount = 48;
		heightMultiplier = -1;

		m_videoFramFormatter = new CFFMpegDecoderVideoFrameFormatter(
			AV_CODEC_ID_R12B,
			AV_PIX_FMT_RGB48LE);
		break;

		// No conversion needed
	default:
		mediaSubType = TranslateToMediaSubType(m_videoState->videoFrameEncoding);
		bitCount = VideoFrameEncodingBitsPerPixel(m_videoState->videoFrameEncoding);;

		m_videoFramFormatter = new CNoopVideoFrameFormatter();
	}

	m_videoFramFormatter->OnVideoState(m_videoState);

	// Build pmt
	assert(!m_pmt.pbFormat);
	ZeroMemory(&m_pmt, sizeof(AM_MEDIA_TYPE));

	m_pmt.formattype = FORMAT_VIDEOINFO2;
	m_pmt.cbFormat = sizeof(VIDEOINFOHEADER2);
	m_pmt.majortype = MEDIATYPE_Video;
	m_pmt.subtype = mediaSubType;
	m_pmt.bFixedSizeSamples = TRUE;
	m_pmt.bTemporalCompression = FALSE;

	assert(!m_pmt.pbFormat);
	m_pmt.pbFormat = (BYTE*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER2));
	if (!m_pmt.pbFormat)
		throw std::runtime_error("Out of mem");

	VIDEOINFOHEADER2* pvi2 = (VIDEOINFOHEADER2*)m_pmt.pbFormat;
	ZeroMemory(pvi2, sizeof(VIDEOINFOHEADER2));

	// Populate bitmap info header
	// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader

	pvi2->bmiHeader.biSizeImage = m_videoFramFormatter->GetOutFrameSize();
	pvi2->bmiHeader.biBitCount = bitCount;
	pvi2->bmiHeader.biCompression = m_pmt.subtype.Data1;
	pvi2->bmiHeader.biWidth = m_videoState->displayMode->FrameWidth();
	pvi2->bmiHeader.biHeight = ((long)m_videoState->displayMode->FrameHeight()) * heightMultiplier;
	pvi2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pvi2->bmiHeader.biPlanes = 1;
	pvi2->bmiHeader.biClrImportant = 0;
	pvi2->bmiHeader.biClrUsed = 0;

	pvi2->AvgTimePerFrame = (REFERENCE_TIME)(UNITS / m_videoState->displayMode->RefreshRateHz());

	DXVA_ExtendedFormat* colorimetry = (DXVA_ExtendedFormat*)&(pvi2->dwControlFlags);

	colorimetry->VideoPrimaries =
		(m_forceVideoPrimaries != DXVA_VideoPrimaries::DXVA_VideoPrimaries_Unknown) ?
		m_forceVideoPrimaries :
		TranslateVideoPrimaries(m_videoState->colorspace);

	colorimetry->VideoTransferMatrix =
		(m_forceVideoTransferMatrix != DXVA_VideoTransferMatrix::DXVA_VideoTransferMatrix_Unknown) ?
		m_forceVideoTransferMatrix :
		TranslateVideoTransferMatrix(m_videoState->colorspace);

	colorimetry->VideoTransferFunction =
		(m_forceVideoTransferFunction != DXVA_VideoTransferFunction::DXVA_VideoTransFunc_Unknown) ?
		m_forceVideoTransferFunction :
		TranslateVideoTranferFunction(m_videoState->eotf, m_videoState->colorspace);

	colorimetry->NominalRange =
		(m_forceNominalRange != DXVA_NominalRange::DXVA_NominalRange_Unknown) ?
		m_forceNominalRange :
		DXVA_NominalRange::DXVA_NominalRange_Unknown;  // = Let renderer guess

	pvi2->dwControlFlags += AMCONTROL_USED;
	pvi2->dwControlFlags += AMCONTROL_COLORINFO_PRESENT;

	m_pmt.lSampleSize = DIBSIZE(pvi2->bmiHeader);
}


void DirectShowGenericHDRVideoRenderer::RendererConnect()
{
	if (FAILED(m_pGraph->AddFilter(m_pRenderer, L"Renderer")))
		throw std::runtime_error("Failed to add renderer to the graph");

	IEnumPins* pEnum = nullptr;
	IPin* pLiveSourceOutputPin = nullptr;
	IPin* pRendererInputPin = nullptr;

	if (FAILED(m_liveSource->EnumPins(&pEnum)))
		throw std::runtime_error("Failed to get livesource pin enumerator");

	if (pEnum->Next(1, &pLiveSourceOutputPin, nullptr) != S_OK)
	{
		pEnum->Release();

		throw std::runtime_error("Failed to run next on livesource pin");
	}

	pEnum->Release();
	pEnum = nullptr;

	if (FAILED(m_pRenderer->EnumPins(&pEnum)))
	{
		pLiveSourceOutputPin->Release();
		pRendererInputPin->Release();

		throw std::runtime_error("Failed to get livesource pin enumerator");
	}

	if (pEnum->Next(1, &pRendererInputPin, nullptr) != S_OK)
	{
		pLiveSourceOutputPin->Release();
		pRendererInputPin->Release();
		pEnum->Release();

		throw std::runtime_error("Failed to get livesource pin enumerator");
	}

	pEnum->Release();
	pEnum = nullptr;

	// Directly connect
	if (FAILED(m_pGraph->ConnectDirect(pLiveSourceOutputPin, pRendererInputPin, &m_pmt)))
	{
		pLiveSourceOutputPin->Release();
		pRendererInputPin->Release();

		throw std::runtime_error("Failed to connect pins");
	}

	pLiveSourceOutputPin->Release();
	pRendererInputPin->Release();
}


void DirectShowGenericHDRVideoRenderer::LiveSourceBuildAndConnect()
{
	DirectShowVideoRenderer::LiveSourceBuildAndConnect();

	if (m_videoState->hdrData)
		m_liveSource->OnHDRData(m_videoState->hdrData);
}