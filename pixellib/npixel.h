/**********************************************************************
 *
 * npixia.h - pixia main header file
 *
 * NOTE: arch. independence header,
 * for more information, please see the readme file
 *
 **********************************************************************/

#ifndef __NPIXEL_H__
#define __NPIXEL_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ibitmap.h"
#include "ibmbits.h"
#include "ibmcols.h"
#include "ibmwink.h"
#include "iblit386.h"
#include "ipicture.h"
#include "ikitwin.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(WIN32) && (!defined(_WIN32))
#define _WIN32
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#endif


/**********************************************************************
 * GLOBAL MACRO
 **********************************************************************/


#if (defined(WIN32) || defined(_WIN32))
#define IEXPORTING __declspec (dllexport)
#else
#define IEXPORTING 
#endif

#ifdef __cplusplus
extern "C" {
#endif


int ipicture_save(const char *filename, const IBITMAP *bmp, const IRGB *pal);

//=====================================================================
// SCREEN
//=====================================================================
extern IBITMAP *iscreen;	// 和屏幕色深一样的位图
extern IBITMAP *cscreen;	// 和初始化色深一样的位图

extern int iscreen_mx;			// 鼠标X
extern int iscreen_my;			// 鼠标Y
extern int iscreen_mb;			// 鼠标按钮
extern int iscreen_in;			// 鼠标焦点
extern int iscreen_closing;		// 是否关闭

int iscreen_init(int w, int h, int bpp);

void iscreen_quit(void);

int iscreen_lock(void);

int iscreen_unlock(void);

int iscreen_update(int *rect, int n);

int iscreen_convert(int *rect, int n);

int iscreen_keyon(int keycode);

int iscreen_dispatch(void);

int iscreen_tick(int fps);

float iscreen_fps(void);

IINT64 iscreen_clock(void);

void iscreen_sleep(long millisecond);

void iscreen_caption(const char *text);

void iscreen_vsync(int mode);


//=====================================================================
// MISC
//=====================================================================
void imisc_bitmap_demo(IBITMAP *bmp, int type);

void imisc_bitmap_systext(IBITMAP *dst, int x, int y, const char *string, 
	IUINT32 color, IUINT32 bk, const IRECT *clip, int additive);

// 反响查找
extern int CPixelFormatInverse[];


#ifdef _WIN32
//=====================================================================
// WIN32 GDI/GDI+ 接口
//=====================================================================

// 初始化 BITMAPINFO
int CInitDIBInfo(BITMAPINFO *info, int w, int h, int pixfmt, 
	const LPRGBQUAD pal);

// DIB - 传送到 Device
int CSetDIBitsToDevice(void *hDC, int x, int y, const IBITMAP *bitmap,
	int sx, int sy, int sw, int sh);

// DIB - 缩放到 Device
int CStretchDIBits(void *hDC, int dx, int dy, int dw, int dh, 
	const IBITMAP *bitmap, int sx, int sy, int sw, int sh);

// CreateDIBSection 
HBITMAP CCreateDIBSection(HDC hdc, int w, int h, int pixfmt,
	const LPRGBQUAD pal, LPVOID *bits);

// 预先乘法
void CPremultiplyAlpha(void *bits, long pitch, int w, int h);

// user32.dll 导出：UpdateLayeredWindow
BOOL CUpdateLayeredWindow(HWND hWnd, HDC hdcDst, POINT *pptDst,
	SIZE *psize, HDC hdcSrc, POINT *pptSrc, COLORREF crKey, 
	BLENDFUNCTION *pblend, DWORD dwFlags);

// user32.dll 导出：SetLayeredWindowAttr
BOOL CSetLayeredWindowAttr(HWND hWnd, COLORREF crKey,
	BYTE bAlpha, DWORD dwFlags);

// msimg32.dll AlphaBlend
BOOL CAlphaBlend(HDC hdcDst, int nXOriginDest, int nYOriginDest,
	int nWidthDest, int nHeightDest, HDC hdcSrc, int nXOriginSrc, 
	int nYOriginSrc, int nWidthSrc, int nHeightSrc, 
	BLENDFUNCTION blendFunction);

// msimg32.dll 导出：TransparentBlt
BOOL CTransparentBlt(HDC hdcDst, int nXOriginDest, int nYOriginDest,
	int nWidthDest, int nHeightDest, HDC hdcSrc, int nXOriginSrc, 
	int nYOriginSrc, int nWidthSrc, int nHeightSrc, UINT crTransparent);

// msimg32.dll 导出：GradientFill
BOOL CGradientFill(HDC hdc, PTRIVERTEX pVertex, ULONG dwNumVertex,
	PVOID pMesh, ULONG dwNumMesh, ULONG dwMode);

// comctl32.dll 导出：DrawShadowText
int CDrawShadowText(HDC hdc, LPCWSTR pszText, UINT cch, const RECT *pRect,
	DWORD dwFlags, COLORREF crText, COLORREF crShadow, int ixOffset, 
	int iyOffset);


// GDI+ 开始1，结束0
int CGdiPlusInit(int startup);

// GDI+ 读取内存中的图片
void *CGdiPlusLoadMemory(const void *data, long size, int *cx, int *cy, 
	long *pitch, int *pfmt, int *bpp, int *errcode);

// GDI+ 读取内存图片到 IBITMAP
IBITMAP *CGdiPlusLoadBitmap(const void *data, long size, int *errcode);

// GDI+ 从文件读取 IBITMAP
IBITMAP *CGdiPlusLoadFile(const char *fname, int *errcode);

// 初始化GDI+ 的解码器
void CGdiPlusDecoderInit(int GdiPlusStartup);



//---------------------------------------------------------------------
// 高级 WIN32功能
//---------------------------------------------------------------------

// 创建 DIB的 IBITMAP，HBITMAP保存在 IBITMAP::extra
IBITMAP *CGdiCreateDIB(HDC hdc, int w, int h, int pixfmt, const LPRGBQUAD p);

// 删除 DIB的 IBITMAP，IBITMAP::extra 需要指向 DIB的 HBITMAP
void CGdiReleaseDIB(IBITMAP *bmp);

// 取得 DIB的 pixel format
int CGdiDIBFormat(const DIBSECTION *sect);

// 将字体转换为 IBITMAP: IPIX_FMT_A8
IBITMAP* CCreateTextW(HFONT hFont, const wchar_t *text, 
	int ncount, UINT format, LPDRAWTEXTPARAMS param);

// 将字体转换为 IBITMAP: IPIX_FMT_A8
IBITMAP* CCreateTextA(HFONT hFont, const char *text, 
	int ncount, UINT format, LPDRAWTEXTPARAMS param);



#endif


#ifdef __cplusplus
}
#endif


#endif


