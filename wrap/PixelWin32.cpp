#include "PixelWin32.h"
#include "ipicture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_END(name) }

NAMESPACE_BEGIN(Pixel)

BitmapDIB::~BitmapDIB()
{
	Release();
}

void BitmapDIB::Initialize()
{
	ownbits = false;
	reference = false;
	bitmap = NULL;
	hBitmap = NULL;
	info = NULL;
	palette = NULL;
	classcode = 1;
}

BitmapDIB::BitmapDIB()
{
	Initialize();
}

BitmapDIB::BitmapDIB(int width, int height, Format pixfmt)
{
	Initialize();
	int hr = Create(width, height, pixfmt);
	if (hr != 0) {
		char text[300];
		#ifndef _MSC_VER
		sprintf(text, "cannot create dib: %d (win32 error: %d)", hr, (int)GetLastError());
		#else
		sprintf_s(text, "cannot create dib: %d (win32 error: %d)", hr, (int)GetLastError());
		#endif
		throw new BitmapError(text);
	}
}

BitmapDIB::BitmapDIB(const char *filename, IRGB *pal)
{
	Initialize();
	int hr = Load(filename, pal);
	if (hr == 0) {
		char text[300];
		#ifndef _MSC_VER
		sprintf(text, "cannot load picture %s", filename);
		#else
		sprintf_s(text, "cannot load picture %s", filename);
		#endif
		throw new BitmapError(text);
	}
}

BitmapDIB::BitmapDIB(int width, int height, enum Format pixfmt, long pitch, void *bits)
{
	Initialize();
	throw new BitmapError("BitmapDIB doesn't support create from user data");
}

BitmapDIB::BitmapDIB(IBITMAP *bmp)
{
	Initialize();
	throw new BitmapError("BitmapDIB doesn't support create from another bitmap");
}



// 创建
int BitmapDIB::Create(int width, int height, enum Format pixfmt)
{
	int fmt = (int)pixfmt;
	int palsize = 0;
	int bitfield = 0;
	int bpp = 0;
	long pitch;
	int size;

	Release();

	bpp = ipixelfmt[fmt].bpp;

	if (bpp < 15) {
		palsize = (1 << bpp) * sizeof(PALETTEENTRY);
	}
	else if (bpp == 16 || bpp == 32 || bpp == 24) {
		bitfield = 3 * sizeof(RGBQUAD);
	}

	size = sizeof(BITMAPINFO) + bitfield + palsize + sizeof(RGBQUAD);
	info = (BITMAPINFO*)malloc(size);
	if (info == NULL) return -1;

	pitch = (ipixelfmt[fmt].pixelbyte * width + 3) & ~3;
	
	info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth = width;
	info->bmiHeader.biHeight = -height;
	info->bmiHeader.biPlanes = 1;
	info->bmiHeader.biBitCount = bpp;
	info->bmiHeader.biCompression = BI_RGB;
	info->bmiHeader.biSizeImage = pitch * height;
	info->bmiHeader.biXPelsPerMeter = 0;
	info->bmiHeader.biYPelsPerMeter = 0;
	info->bmiHeader.biClrUsed = 0;
	info->bmiHeader.biClrImportant = 0;

	if (fmt == IPIX_FMT_R8G8B8) {
		info->bmiHeader.biCompression = BI_RGB;
	}
	else if (bpp >= 15) {
		IUINT32 *data = (IUINT32*)info;
		data[10] = ipixelfmt[fmt].rmask;
		data[11] = ipixelfmt[fmt].gmask;
		data[12] = ipixelfmt[fmt].bmask;
		data[13] = ipixelfmt[fmt].amask;
		info->bmiHeader.biCompression = BI_BITFIELDS;
	}

	if (palsize > 0) {
		long offset = sizeof(BITMAPINFOHEADER) + bitfield;
		palette = (LPRGBQUAD)((char*)info + offset);
	}	else {
		palette = NULL;
	}

	if (palette) {
		for (int i = 0; i < 256; i++) {
			palette[i].rgbBlue = i;
			palette[i].rgbGreen = i;
			palette[i].rgbRed = i;
			palette[i].rgbReserved = 0;
		}
	}

	void *pixel;
	hBitmap = ::CreateDIBSection(NULL, (LPBITMAPINFO)info, 
		DIB_RGB_COLORS, &pixel, NULL, 0);

	bitmap = NULL;
	reference = false;
	ownbits = false;

	if (hBitmap == NULL) {
		Release();
		return -2;
	}

	bitmap = ibitmap_reference_new(pixel, pitch, width, height, fmt);

	if (bitmap == NULL) {
		Release();
	}
	
	bitmap->extra = hBitmap;
	this->pixfmt = fmt;
	SetClip(NULL);

	return 0;
}

// 从已有的内存块创建
int BitmapDIB::Create(int width, int height, enum Format pixfmt, long pitch, void *bits)
{
	Release();
	return -100;
}

// 删除
void BitmapDIB::Release()
{
	if (bitmap) ibitmap_reference_del(bitmap);
	bitmap = NULL;
	if (hBitmap) DeleteObject(hBitmap);
	hBitmap = NULL;
	if (info) free(info);
	palette = NULL;
	info = NULL;
}

// 从文件读取
int BitmapDIB::Load(const char *filename, IRGB *pal)
{
	IBITMAP *bmp;
	bmp = ipic_load_file(filename, 0, pal);
	if (bmp == NULL) return -10;

	int hr = Create((int)bmp->w, (int)bmp->h, (Format)ibitmap_pixfmt_guess(bmp));
	if (hr != 0) {
		ibitmap_release(bmp);
		return hr;
	}

	ibitmap_blit(bitmap, 0, 0, bmp, 0, 0, (int)bmp->w, (int)bmp->h, 0);
	ibitmap_release(bmp);

	return 0;
}

// 从外部赋给一个IBITMAP
int BitmapDIB::Assign(IBITMAP *bmp)
{
	return -100;
}


HBITMAP BitmapDIB::GetDIB()
{
	return hBitmap;
}

LPRGBQUAD BitmapDIB::GetPalette()
{
	return palette;
}

int BitmapDIB::SetDIBitsToDevice(HDC hDC, int x, int y, const IRECT *bound)
{
	RECT rect;

	if (bitmap == NULL || hBitmap == NULL) 
		return -1;

	int w = (int)bitmap->w;
	int h = (int)bitmap->h;

	if (bound == NULL) {
		rect.left = 0;
		rect.top = 0;
		rect.right = w;
		rect.bottom = h;
	}	else {
		rect.left = bound->left;
		rect.top = bound->top;
		rect.right = bound->right;
		rect.bottom = bound->bottom;
	}

	if (rect.left < 0) {
		x -= rect.left;
		rect.left = 0;
	}
	if (rect.top < 0) {
		y -= rect.top;
		rect.top = 0;
	}

	if (rect.right > w) rect.right = w;
	if (rect.bottom > h) rect.bottom = h;
	if (rect.left >= rect.right || rect.top >= rect.bottom) 
		return -2;

	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	int hr = ::SetDIBitsToDevice(hDC, x, y, w, h, 
						rect.left, rect.top, 
						0, (int)bitmap->h, bitmap->pixel,
						(LPBITMAPINFO)info, DIB_RGB_COLORS);

	if (hr <= 0) return -3;

	return 0;
}

int BitmapDIB::StretchDIBits(HDC hDC, const IRECT *dest, const IRECT *bound)
{
	IRECT clipdst, bound_dst, bound_src;

	if (bitmap == NULL || hBitmap == NULL || dest == NULL) 
		return -100;

	bound_dst = *dest;
	clipdst = bound_dst;

	if (bound == NULL) {
		bound_src.left = 0;
		bound_src.top = 0;
		bound_src.right = (int)bitmap->w;
		bound_src.bottom = (int)bitmap->h;
	}	else {
		bound_src = *bound;
	}

	if (ibitmap_clip_scale(&clipdst, &clip, &bound_dst, &bound_src, 0))
		return -200;

	int dx = bound_dst.left;
	int dy = bound_dst.top;
	int dw = bound_dst.right - bound_dst.left;
	int dh = bound_dst.bottom - bound_dst.top;
	int sx = bound_src.left;
	int sy = bound_src.top;
	int sw = bound_src.right - bound_src.left;
	int sh = bound_src.bottom - bound_src.top;

	::SetStretchBltMode(hDC, COLORONCOLOR);

	int hr = ::StretchDIBits(hDC, dx, dy, dw, dh, sx, sy, sw, sh,
					bitmap->pixel, (LPBITMAPINFO)info, DIB_RGB_COLORS,
					SRCCOPY);

	if (hr == (int)GDI_ERROR) 
		return -300;

	return 0;
}


//=====================================================================
// ThemeFont 定义
//=====================================================================
int ThemeFont::InitFont(const LOGFONTA *logfont)
{
	hFont = NULL;
	hFontOld = NULL;
	hDC = NULL;
	hBitmapOld = NULL;
	hFont = CreateFontIndirectA(logfont);
	if (hFont == NULL) return -1;
	return Initialize();
}

int ThemeFont::InitFont(const LOGFONTW *logfont)
{
	hFont = NULL;
	hFontOld = NULL;
	hDC = NULL;
	hBitmapOld = NULL;
	hFont = CreateFontIndirectW(logfont);
	if (hFont == NULL) return -1;
	return Initialize();
}

int ThemeFont::Initialize()
{
	hDC = CreateCompatibleDC(NULL);
	if (hDC == NULL) {
		if (hFont) DeleteObject(hFont);
		hFont = NULL;
		return -2;
	}
	hFontOld = (HFONT)SelectObject(hDC, hFont);
	if (DIB.Create(256, 64, A8R8G8B8) != 0) 
		return -3;
	hBitmapOld = (HBITMAP)SelectObject(hDC, DIB.GetDIB());
	ThemeEdge = 0;
	ThemeBlur = 0x80000000;
	ThemeBlurSize = 0;
	SampleDown = false;
	FontFormat = 0;
	lpFontParams = NULL;
	FontParams.cbSize = sizeof(FontParams);
	SetTextColor(hDC, RGB(255, 255, 255));
	SetBkColor(hDC, 0);
	SetBkMode(hDC, OPAQUE);
	if (GetTextMetricsA(hDC, &mta) == FALSE) return -4;
	if (GetTextMetricsW(hDC, &mtw) == FALSE) return -5;
	return 0;
}

int ThemeFont::Destroy()
{
	if (hDC) {
		if (hBitmapOld) SelectObject(hDC, hBitmapOld);
		if (hFontOld) SelectObject(hDC, hFontOld);
		DeleteDC(hDC);
		hDC = NULL;
		hFontOld = NULL;
		hBitmapOld = NULL;
	}
	if (hFont) {
		DeleteObject(hFont);
		hFont = NULL;
	}
	return 0;
}

ThemeFont::~ThemeFont()
{
	Destroy();
}

ThemeFont::ThemeFont(const LOGFONTA *logfont)
{
	if (InitFont(logfont)) 
		throw new BitmapError("cannot create font");
}

ThemeFont::ThemeFont(const LOGFONTW *logfont)
{
	if (InitFont(logfont)) 
		throw new BitmapError("cannot create font");
}

ThemeFont::ThemeFont(HFONT hFont)
{
	LOGFONTW logfont;
	if (GetObject(hFont, sizeof(LOGFONTW), &logfont) == 0) 
		throw new BitmapError("cannot get logfont");
	if (InitFont(&logfont)) 
		throw new BitmapError("cannot create font");
}

ThemeFont::ThemeFont(const char *facename, int height, int width, int weight,
	bool italic, bool underline, bool antialising)
{
	LOGFONTA logfont;
	size_t size;
	for (size = 0; facename[size]; size++);
	if (size + 1 > LF_FACESIZE) size = LF_FACESIZE - 1;
	memcpy(logfont.lfFaceName, facename, (size + 1) * sizeof(char));
	logfont.lfHeight = height;
	logfont.lfWidth = width;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfWeight = weight;
	logfont.lfItalic = italic? TRUE : FALSE;
	logfont.lfUnderline = underline? TRUE : FALSE;
	logfont.lfStrikeOut = 0;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfOutPrecision = OUT_TT_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	if (antialising) {
		logfont.lfQuality = ANTIALIASED_QUALITY;
	#if 0
		if (GetVersionNumber() >= 5001)
			logfont.lfQuality = 6; // CLEARTYPE_NATURAL_QUALITY;.
	#endif
	}
	if (InitFont(&logfont)) 
		throw new BitmapError("cannot create font");
}

ThemeFont::ThemeFont(const wchar_t *facename, int height, int width, int weight,
	bool italic, bool underline, bool antialising)
{
	LOGFONTW logfont;
	size_t size;
	for (size = 0; facename[size]; size++);
	if (size + 1 > LF_FACESIZE) size = LF_FACESIZE - 1;
	memcpy(logfont.lfFaceName, facename, (size + 1) * sizeof(wchar_t));
	logfont.lfHeight = height;
	logfont.lfWidth = width;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfWeight = weight;
	logfont.lfItalic = italic? TRUE : FALSE;
	logfont.lfUnderline = underline? TRUE : FALSE;
	logfont.lfStrikeOut = 0;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfOutPrecision = OUT_TT_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	if (antialising) {
		logfont.lfQuality = ANTIALIASED_QUALITY;
	#if 0
		if (GetVersionNumber() >= 5001)
			logfont.lfQuality = 6; // CLEARTYPE_NATURAL_QUALITY;.
	#endif
	}
	if (InitFont(&logfont)) 
		throw new BitmapError("cannot create font");
}

int ThemeFont::Resize(int width, int height)
{
	int neww, newh;
	if (width <= DIB.GetW() && height <= DIB.GetH()) return 0;
	if (hDC == NULL) return -1;
	for (neww = 1; neww < width; ) neww <<= 1;
	for (newh = 1; newh < newh; ) newh <<= 1;
	width = neww;
	height = newh;
	if (width < DIB.GetW()) width = DIB.GetW();
	if (height < DIB.GetH()) height = DIB.GetH();
	if (hBitmapOld) SelectObject(hDC, hBitmapOld);
	if (DIB.Create(width, height, A8R8G8B8)) return -2;
	hBitmapOld = (HBITMAP)SelectObject(hDC, DIB.GetDIB());
	return 0;
}

int ThemeFont::GdiGetSize(const char *text, int size, int *width, int *height)
{
	UINT format = FontFormat;
	int retval = 0;
	RECT rect;
	if (size < 0) { for (size = 0; text[size]; ) size++; }
	format &= ~(DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);
	format |= DT_CALCRECT;
	retval = DrawTextExA(hDC, (char*)text, size, &rect, format, lpFontParams);
	if (width) width[0] = 0;
	if (height) height[0] = 0;
	if (retval <= 0) return -1;
	if (width) width[0] = rect.right - rect.left;
	if (height) height[0] = rect.bottom - rect.top;
	return 0;
}

int ThemeFont::GdiGetSize(const wchar_t *text, int size, int *width, int *height)
{
	UINT format = FontFormat;
	int retval = 0;
	RECT rect;
	if (size < 0) { for (size = 0; text[size]; ) size++; }
	format &= ~(DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);
	format |= DT_CALCRECT;
	retval = DrawTextExW(hDC, (wchar_t*)text, size, &rect, format, lpFontParams);
	if (width) width[0] = 0;
	if (height) height[0] = 0;
	if (retval <= 0) return -1;
	if (width) width[0] = rect.right - rect.left;
	if (height) height[0] = rect.bottom - rect.top;
	return 0;
}


IBITMAP* ThemeFont::GdiCreateBitmap(const char *text, int size)
{
	UINT format = FontFormat;
	RECT rect;
	int width;
	int height;
	if (size == 0) return NULL;
	format &= ~(DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);
	if (GdiGetSize(text, size, &width, &height) != 0) return NULL;
	if (Resize(width, height) != 0) return NULL;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	if (width <= 0 || height <= 0) return NULL;
	DIB.Fill(0, 0, 0, width, height);
	int retval = DrawTextExA(hDC, (char*)text, size, &rect, format, lpFontParams);
	if (retval <= 0) return NULL;
	return GdiCreateBitmap(rect.right - rect.left, rect.bottom - rect.top);
}

IBITMAP* ThemeFont::GdiCreateBitmap(const wchar_t *text, int size)
{
	UINT format = FontFormat;
	RECT rect;
	int width;
	int height;
	if (size == 0) return NULL;
	format &= ~(DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);
	if (GdiGetSize(text, size, &width, &height) != 0) return NULL;
	if (Resize(width, height) != 0) return NULL;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	if (width <= 0 || height <= 0) return NULL;
	DIB.Fill(0, 0, 0, width, height);
	int retval = DrawTextExW(hDC, (wchar_t*)text, size, &rect, format, lpFontParams);
	if (retval <= 0) return NULL;
	return GdiCreateBitmap(rect.right - rect.left, rect.bottom - rect.top);
}

IBITMAP* ThemeFont::GdiCreateBitmap(int width, int height)
{
	IBITMAP *bitmap;
	bitmap = ibitmap_create(width, height, 8);
	if (bitmap == NULL) return NULL;
	ibitmap_pixfmt_set(bitmap, IPIX_FMT_A8);
	for (int j = 0; j < height; j++) {
		const IUINT32 *src = (const IUINT32*)DIB.GetBitmap()->line[j];
		IUINT8 *dst = (IUINT8*)bitmap->line[j];
		IUINT32 sr, sg, sb, sa;
		for (int i = width; i > 0; dst++, src++, i--) {
			_ipixel_load_card(src, sr, sg, sb, sa);
			sa = sr + sg + sg + sb;
			dst[0] = (IUINT8)((sa >> 2) & 0xff);
		}
	}
	return bitmap;
}


IBITMAP* ThemeFont::Render(const char *text, int size, IUINT32 color)
{
	IBITMAP *mask, *bitmap;
	if (size < 0) { for (size = 0; text[size]; size++); }
	if (size == 0) return NULL;
	mask = GdiCreateBitmap(text, size);
	if (mask == NULL) return NULL;
	bitmap = RenderBitmap(mask, color);
	ibitmap_release(mask);
	return bitmap;
}

IBITMAP* ThemeFont::Render(const wchar_t *text, int size, IUINT32 color)
{
	IBITMAP *mask, *bitmap;
	if (size < 0) { for (size = 0; text[size]; size++); }
	if (size == 0) return NULL;
	mask = GdiCreateBitmap(text, size);
	if (mask == NULL) return NULL;
	bitmap = RenderBitmap(mask, color);
	ibitmap_release(mask);
	return bitmap;
}

IBITMAP* ThemeFont::RenderBitmap(IBITMAP *mask, IUINT32 color)
{
	IBITMAP *savemask = mask;
	int width = (int)mask->w;
	int height = (int)mask->h;
	int imgw, imgh;
	int i, j, inc;
	if (SampleDown) {
		#if 1
		int limity = (height + 1) / 2;
		IUINT8 zero[2] = { 0, 0 };
		int step = 2;
		for (j = 0; j < limity; j++) {
			int startline = j * 2;
			const IUINT8 *s1, *s2;
			IUINT8 *dst = (IUINT8*)mask->line[j];
			IUINT32 c1, c2, c3, c4;
			s1 = (const IUINT8*)mask->line[startline + 0];
			if (startline < height - 1) {
				step = 2;
				s2 = (const IUINT8*)mask->line[startline + 1];
			}	else {
				step = 0;
				s2 = zero;
			}
			for (i = 0; i < width - 1; s1 += 2, s2 += step, dst++, i += 2) {
				c1 = s1[0];
				c2 = s1[1];
				c3 = s2[0];
				c4 = s2[1];
				dst[0] = (IUINT8)((c1 + c2 + c3 + c4) >> 2);
			}
			if (i == width - 1) {
				dst[0] = (IUINT8)(( ((IUINT32)s1[0]) + ((IUINT32)s2[0]) ) >> 2);
			}
		}
		width = (width + 1) / 2;
		height = (height + 1) / 2;
		mask->w = (unsigned)width;
		mask->h = (unsigned)height;
		#else
		width = (width + 1) / 2;
		height = (height + 1) / 2;
		mask = ibitmap_resample(mask, NULL, width, height, 0);
		if (mask == NULL) return NULL;
		#endif
	}

	inc = ThemeEdge? 1 : 0;
	if (ThemeBlurSize > inc) inc = ThemeBlurSize;
	imgw = width + inc * 2;
	imgh = height + inc * 2;

	IBITMAP *bitmap = ibitmap_create(imgw, imgh, 32);
	if (bitmap == NULL) return NULL;

	ibitmap_pixfmt_set(bitmap, IPIX_FMT_A8R8G8B8);
	ibitmap_fill(bitmap, 0, 0, imgw, imgh, 0, 0);

	if (ThemeBlurSize) {
		IBITMAP *src = ibitmap_create(imgw, imgh, 8);
		if (src != NULL) {
			ibitmap_pixfmt_set(src, IPIX_FMT_A8);
			ibitmap_blit(src, inc, inc, mask, 0, 0, width, height, 0);
			for (i = ThemeBlurSize; i > 0; i = i / 2) {
				IBITMAP *tmp = ibitmap_drop_shadow(mask, i, i);
				if (tmp == NULL) break;
				ibitmap_maskfill(bitmap, inc - i, inc - i, tmp, 0, 0, width, height, ThemeBlur, NULL);
				ibitmap_release(tmp);
				break;
			}
			ibitmap_release(src);
		}
	}

	if (ThemeEdge) {
		for (j = 0; j <= 2; j++) {
			for (i = 0; i <= 2; i++) {
				if (i == 1 && j == 1) continue;
				ibitmap_maskfill(bitmap, inc - 1 + i, inc - 1 + j, 
					mask, 0, 0, width, height, ThemeEdge, NULL);
			}
		}
	}

	ibitmap_maskfill(bitmap, inc, inc, mask, 0, 0, width, height, color, NULL);

	if (savemask != mask) ibitmap_release(mask);

	return bitmap;
}

int ThemeFont::GetSize(const char *text, int size, int *width, int *height)
{
	int imgw, imgh, inc = 0;
	if (size < 0) { for (size = 0; text[size]; size++); }
	if (width) width[0] = 0;
	if (height) height[0] = 0;
	if (GdiGetSize(text, size, &imgw, &imgh) != 0) return -1;
	if (SampleDown) imgw = (imgw + 1) / 2, imgh = (imgh + 1) / 2;
	if (ThemeEdge) inc = 1;
	if (ThemeBlurSize > inc) inc = ThemeBlurSize;
	imgw += inc * 2;
	imgh += inc * 2;
	if (width) width[0] = imgw;
	if (height) height[0] = imgh;
	return 0;
}

int ThemeFont::GetSize(const wchar_t *text, int size, int *width, int *height)
{
	int imgw, imgh, inc = 0;
	if (size < 0) { for (size = 0; text[size]; size++); }
	if (width) width[0] = 0;
	if (height) height[0] = 0;
	if (GdiGetSize(text, size, &imgw, &imgh) != 0) return -1;
	if (SampleDown) imgw = (imgw + 1) / 2, imgh = (imgh + 1) / 2;
	if (ThemeEdge) inc = 1;
	if (ThemeBlurSize > inc) inc = ThemeBlurSize;
	imgw += inc * 2;
	imgh += inc * 2;
	if (width) width[0] = imgw;
	if (height) height[0] = imgh;
	return 0;
}


NAMESPACE_END(Pixel)





//! flag: -O3, -Wall
//! int: obj
//! out: libpixel
//! mode: lib
//! src: PixelBitmap.cpp
//! src: ikitwin.c, mswindx.c, ibmfont.c, ibmsse2.c, ibmwink.c, npixel.c
//! src: ibitmap.c, ibmbits.c, iblit386.c, ibmcols.c, ipicture.c, ibmdata.c


