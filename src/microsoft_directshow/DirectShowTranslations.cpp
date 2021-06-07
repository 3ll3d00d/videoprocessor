/*
 * Copyright(C) 2021 Dennis Fleurbaaij <mail@dennisfleurbaaij.com>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program. If not, see < https://www.gnu.org/licenses/>.
 */


#include <stdafx.h>

#include <guid.h>
#include <microsoft_directshow/DirectShowDefines.h>

#include "DirectShowTranslations.h"


const GUID TranslateToMediaSubType(PixelFormat pixelFormat)
{
	switch (pixelFormat)
	{
	case PixelFormat::YUV_8BIT:
		// TODO: Can also be HDYC if color space is rec709, see https://www.fourcc.org/yuv.php
		return MEDIASUBTYPE_UYVY;

	case PixelFormat::YUV_10BIT:
		return MEDIASUBTYPE_v210;

	case PixelFormat::R210:
		return MEDIASUBTYPE_r210;
	}

	throw std::runtime_error("TranslateToMediaSubType cannot translate");
}


DXVA_NominalRange TranslatePixelValueRange(PixelValueRange pixelValueRange)
{
	switch (pixelValueRange)
	{
	case PixelValueRange::PIXELVALUERANGE_UNKNOWN:
		return DXVA_NominalRange::DXVA_NominalRange_Unknown;

	case PixelValueRange::PIXELVALUERANGE_0_255:
		return DXVA_NominalRange::DXVA_NominalRange_0_255;

	case PixelValueRange::PIXELVALUERANGE_16_235:
		return DXVA_NominalRange::DXVA_NominalRange_16_235;
	}

	throw std::runtime_error("TranslatePixelValueRange cannot translate");
}


DXVA_VideoTransferMatrix TranslateVideoTransferMatrix(ColorSpace colorSpace)
{
	switch (colorSpace)
	{
	case ColorSpace::REC_601_525:
		return DXVA_VideoTransferMatrix_BT601;

	case ColorSpace::REC_601_625:
		return DXVA_VideoTransferMatrix_BT601;

	case ColorSpace::REC_709:
		return DXVA_VideoTransferMatrix_BT709;

	case ColorSpace::BT_2020:
		return DIRECTSHOW_VIDEOTRANSFERMATRIX_BT2020_10;
	}

	throw std::runtime_error("TranslateVideoTransferMatrix cannot translate");
}


DXVA_VideoPrimaries TranslateVideoPrimaries(ColorSpace colorSpace)
{
	switch (colorSpace)
	{
	case ColorSpace::REC_601_525:
		return DXVA_VideoPrimaries_BT470_2_SysM;  // TODO: There is also a SysBG

	case ColorSpace::REC_709:
		return DXVA_VideoPrimaries_BT709;

	case ColorSpace::BT_2020:
		return DIRECTSHOW_VIDEOPRIMARIES_BT2020;
	}

	throw std::runtime_error("TranslateVideoTransferMatrix cannot translate");
}


DXVA_VideoTransferFunction TranslateVideoTranferFunction(EOTF eotf, ColorSpace colorSpace)
{
	switch (eotf)
	{
	case EOTF::SDR:
		if (colorSpace == ColorSpace::REC_709)
			return DXVA_VideoTransFunc_22_709;
		else
			throw std::runtime_error("Don't know video transfer function for SDR outside of REC 709");
		break;

	case EOTF::PQ:
		return DIRECTSHOW_VIDEOTRANSFUNC_2084;
		break;
	}

	throw std::runtime_error("TranslateVideoTranferFunction cannot translate");
}


DWORD TranslatePixelformatToBiCompression(PixelFormat pixelFormat)
{
	switch (pixelFormat)
	{
	case PixelFormat::ARGB_8BIT:
	case PixelFormat::BGRA_8BIT:
	case PixelFormat::R210:
	case PixelFormat::RGB_BE_10BIT:
	case PixelFormat::RGB_LE_10BIT:
	case PixelFormat::RGB_BE_12BIT:
	case PixelFormat::RGB_LE_12BIT:
		return BI_RGB;
	}

	return PixelFormatFourCC(pixelFormat);
}