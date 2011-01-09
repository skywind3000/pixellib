#ifndef __PIXEL_WIN32_H__
#define __PIXEL_WIN32_H__



#if defined(WIN32) && (!defined(_WIN32))
	#define _WIN32
#endif

#if defined(_WIN32) && (!defined(WIN32))
	#define WIN32
#endif

#ifdef _WIN32

#include "PixelBitmap.h"
#include <windows.h>


namespace Pixel
{

//=====================================================================
// BitmapDIB 定义
//=====================================================================
class BitmapDIB : public Bitmap
{
public:
	virtual ~BitmapDIB();
	BitmapDIB();
	BitmapDIB(int width, int height, enum Format pixfmt = A8R8G8B8);
	BitmapDIB(const char *filename, IRGB *pal = NULL);
	BitmapDIB(int width, int height, enum Format pixfmt, long pitch, void *bits);
	BitmapDIB(IBITMAP *bmp);

	//////////////////////// 基础接口 ////////////////////////

	// 创建
	virtual int Create(int width, int height, enum Format pixfmt = A8R8G8B8);

	// 从已有的内存块创建
	virtual int Create(int width, int height, enum Format pixfmt, long pitch, void *bits);

	// 删除
	virtual void Release();

	// 从文件读取
	virtual int Load(const char *filename, IRGB *pal = NULL);

	// 从外部赋给一个IBITMAP
	int Assign(IBITMAP *bmp);

	//////////////////////// WIN32操作 ////////////////////////
	HBITMAP GetDIB();
	LPRGBQUAD GetPalette();

	int SetDIBitsToDevice(HDC hDC, int x, int y, const IRECT *bound = NULL);

	int StretchDIBits(HDC hDC, const IRECT *dest, const IRECT *bound = NULL);

protected:
	void Initialize();


protected:
	HBITMAP hBitmap;
	LPRGBQUAD palette;
	BITMAPINFO *info;
};


//=====================================================================
// ThemeFont 定义
//=====================================================================
class ThemeFont
{
public:
	virtual ~ThemeFont();
	ThemeFont(const LOGFONTA *logfont);
	ThemeFont(const LOGFONTW *logfont);
	ThemeFont(HFONT hFont);
	ThemeFont(const char *facename, int height, int width, int weight = 0,
		bool italic = false, bool underline = false, bool antialising = true);
	ThemeFont(const wchar_t *facename, int height, int width, int weight = 0,
		bool italic = false, bool underline = false, bool antialising = true);

public:
	IBITMAP *Render(const char *text, int size = -1, IUINT32 color = 0xff000000);
	IBITMAP *Render(const wchar_t *text, int size = -1, IUINT32 color = 0xff000000);
	int GetSize(const char *text, int size = -1, int *width = NULL, int *height = NULL);
	int GetSize(const wchar_t *text, int size = -1, int *width = NULL, int *height = NULL);

protected:
	int InitFont(const LOGFONTA *logfont);
	int InitFont(const LOGFONTW *logfont);
	int Initialize(); 
	int Destroy();
	int Resize(int width, int height);
	int GdiGetSize(const char *text, int size, int *width, int *height);
	int GdiGetSize(const wchar_t *text, int size, int *width, int *height);
	IBITMAP *GdiCreateBitmap(const char *text, int size);
	IBITMAP *GdiCreateBitmap(const wchar_t *text, int size);
	IBITMAP *GdiCreateBitmap(int width, int height);
	IBITMAP *RenderBitmap(IBITMAP *mask, IUINT32 color = 0xff000000);

public:
	IUINT32 ThemeEdge;
	IUINT32 ThemeBlur;
	int ThemeBlurSize;
	bool SampleDown;
	UINT32 FontFormat;

protected:
	DRAWTEXTPARAMS FontParams;
	LPDRAWTEXTPARAMS lpFontParams;
	TEXTMETRICA mta;
	TEXTMETRICW mtw;
	BitmapDIB DIB;
	HFONT hFont;
	HFONT hFontOld;
	HBITMAP hBitmapOld;
	HDC hDC;
};

};


#endif


#endif


