//=====================================================================
//
// mswindx.c - Microsoft Window DirectDraw7 Easy Interface
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#if (defined(_WIN32) || defined(WIN32))

#include <windows.h>
#include "ddraw.h"

#include "mswindx.h"

#if DIRECTDRAW_VERSION < 0x0700
#error DIRECTDRAW_VERSION must equal or abrove 0x0700
#endif

//---------------------------------------------------------------------
// CSURFACE
//---------------------------------------------------------------------
struct CSURFACE
{
	int w;
	int h;
	int bpp;
	int lock;
	int mode;
	int pixfmt;
	int primary;
	long pitch;
	HWND hWnd;
	unsigned long mask;
	unsigned char *bits;
	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWCLIPPER clip;
	LPDIRECTDRAWSURFACE7 lpDDS;
};


//---------------------------------------------------------------------
// IGUID
//---------------------------------------------------------------------
typedef struct _IGUID
{
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
}	IGUID;

#define IDEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
	const IGUID n = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}


IDEFINE_GUID( _IID_IDirectDraw7,                  
	0x15e65ec0,0x3b9c,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b );
IDEFINE_GUID( _IID_IDirectDrawSurface7,           
	0x06675a80,0x3b9b,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b );
IDEFINE_GUID( _IID_IDirectDrawPalette,            
	0x6C14DB84,0xA733,0x11CE,0xA5,0x21,0x00,0x20,0xAF,0x0B,0xE5,0x60 );
IDEFINE_GUID( _IID_IDirectDrawClipper,            
	0x6C14DB85,0xA733,0x11CE,0xA5,0x21,0x00,0x20,0xAF,0x0B,0xE5,0x60 );
IDEFINE_GUID( _IID_IDirectDrawColorControl,       
	0x4B9F0EE0,0x0D7E,0x11D0,0x9B,0x06,0x00,0xA0,0xC9,0x03,0xA3,0xB8 );
IDEFINE_GUID( _IID_IDirectDrawGammaControl,       
	0x69C11C3E,0xB46B,0x11D1,0xAD,0x7A,0x00,0xC0,0x4F,0xC2,0x9B,0x4E );


//---------------------------------------------------------------------
// local variable
//---------------------------------------------------------------------
typedef HRESULT (WINAPI * DIRECTDRAWCREATEEX)( GUID*, VOID**, REFIID, IUnknown*);

DIRECTDRAWCREATEEX DirectDrawCreateEx2 = NULL;
HINSTANCE hDDrawDLL = NULL;

LPDIRECTDRAW7 lpDirectDraw7 = NULL;
HWND hDirectDrawWnd = NULL;
int hFullScreenMode = 0;
int hSurfaceCounter = 0;
#include <tchar.h>
int DDrawInit(void)
{
	LPVOID lpDD;
	HRESULT hr;

	DDrawDestroy();

	hDDrawDLL = LoadLibrary(TEXT("ddraw.dll"));

	if (hDDrawDLL == NULL) {
		return -1;
	}

	DirectDrawCreateEx2 = (DIRECTDRAWCREATEEX)GetProcAddress(hDDrawDLL, 
		"DirectDrawCreateEx");

	if (DirectDrawCreateEx2 == NULL) {
		FreeLibrary(hDDrawDLL);
		DirectDrawCreateEx2 = NULL;
		hDDrawDLL = NULL;
		return -2;
	}

	hr = DirectDrawCreateEx2(NULL, (void**)&lpDD, 
		(GUID*)&_IID_IDirectDraw7, NULL);

	if (hr != DD_OK) {
		return -1;
	}

	lpDirectDraw7 = (LPDIRECTDRAW7)lpDD;

	hFullScreenMode = 0;
	hSurfaceCounter = 0;

	return 0;
}

// Destroy Surface
void DDrawDestroy(void)
{
	if (hSurfaceCounter > 0) {
		fprintf(stderr, "Destroy DirectDraw before release the surfaces\n");
		fflush(stderr);
	}

	if (lpDirectDraw7 != NULL) {
		IDirectDraw7_Release(lpDirectDraw7);
		lpDirectDraw7 = NULL;
	}

	hDirectDrawWnd = NULL;

	if (hDDrawDLL) {
		FreeLibrary(hDDrawDLL);
		hDDrawDLL = NULL;
	}
}


// create surface from DDSURFACEDESC2
CSURFACE *DDrawCreateFromDesc(const DDSURFACEDESC2 *desc, HRESULT *hr)
{
	LPDIRECTDRAWSURFACE7 lpDDS;
	DDSURFACEDESC2 ddsd;
	CSURFACE *surface;
	HRESULT result;

	if (lpDirectDraw7 == NULL) {
		if (DDrawInit() != 0) return NULL;
	}

	surface = (CSURFACE*)malloc(sizeof(CSURFACE));
	assert(surface);

	ddsd = *desc;

	result = IDirectDraw7_CreateSurface(lpDirectDraw7, &ddsd, &lpDDS, NULL);
	if (hr) *hr = result;

	if (result != DD_OK) {
		free(surface);
		return NULL;
	}
	
	result = IDirectDrawSurface7_GetSurfaceDesc(lpDDS, &ddsd);
	if (hr) *hr = result;

	if (result != DD_OK) {
		IDirectDrawSurface7_Release(lpDDS);
		free(surface);
		return NULL;
	}

	surface->lpDDS = lpDDS;
	surface->ddsd = ddsd;
	surface->w = ddsd.dwWidth;
	surface->h = ddsd.dwHeight;
	surface->bpp = ddsd.ddpfPixelFormat.dwRGBBitCount;

	if (surface->bpp == 8) surface->pixfmt = PIXFMT_8;
	else if (surface->bpp == 15) surface->pixfmt = PIXFMT_RGB15;
	else if (surface->bpp == 16) surface->pixfmt = PIXFMT_RGB16;
	else if (surface->bpp == 24) surface->pixfmt = PIXFMT_RGB24;
	else if (surface->bpp == 32) {
		if (ddsd.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) {
			surface->pixfmt = PIXFMT_ARGB32;
		}	else {
			surface->pixfmt = PIXFMT_RGB32;
		}
	}

	surface->pitch = ddsd.dwLinearSize;
	surface->bits = (unsigned char*)ddsd.lpSurface;

	surface->mask = 0;
	surface->lock = 0;
	surface->clip = NULL;
	surface->primary = 0;
	surface->hWnd = NULL;
	
	hSurfaceCounter++;

	return surface;
}


// 删除表面
void DDrawSurfaceRelease(CSURFACE *surface)
{
	assert(surface);

	if (surface->clip) {
		IDirectDrawClipper_Release(surface->clip);
		surface->clip = NULL;
	}

	if (surface->lpDDS) {
		if (surface->lock) {
			IDirectDrawSurface7_Unlock(surface->lpDDS, NULL);
			surface->lock = 0;
		}
		if (IDirectDrawSurface7_IsLost(surface->lpDDS) != DD_OK) {
			IDirectDrawSurface7_Restore(surface->lpDDS);
		}
		IDirectDrawSurface7_Release(surface->lpDDS);
		surface->lpDDS = NULL;
		hSurfaceCounter--;
	}

	if (surface->primary) {
		if (lpDirectDraw7 && hFullScreenMode) {
			IDirectDraw7_RestoreDisplayMode(lpDirectDraw7);
		}
		hFullScreenMode = 0;
	}

	memset(surface, 0, sizeof(CSURFACE));
	free(surface);

	assert(hSurfaceCounter >= 0);
}


// 创建 DirectDraw屏幕
// 如果 bpp为0的话则创建窗口模式，w,h失效，否则全屏幕
CSURFACE *DDrawCreateScreen(HWND hWnd, int w, int h, int bpp, HRESULT *HR)
{
	DDSURFACEDESC2 ddsd;
	CSURFACE *surface;
	HRESULT hr;

	if (lpDirectDraw7 == NULL) {
		if (DDrawInit() != 0) return NULL;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	// 创建窗口模式
	if (bpp == 0) {
		hr = IDirectDraw7_SetCooperativeLevel(lpDirectDraw7, hWnd,
			DDSCL_NORMAL);

		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: SetCooperativeLevel failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			return NULL;
		}

		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		surface = DDrawCreateFromDesc(&ddsd, &hr);

		if (surface == NULL) {
			fprintf(stderr, "DirectDraw: Create Primary Surface failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			return NULL;
		}

		hr = IDirectDraw7_CreateClipper(lpDirectDraw7, 0, &surface->clip, 0);
		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: Create Clipper failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			surface->clip = NULL;
			DDrawSurfaceRelease(surface);
			return NULL;
		}

		hr = IDirectDrawClipper_SetHWnd(surface->clip, 0, hWnd);
		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: SetHWnd failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			DDrawSurfaceRelease(surface);
			return NULL;
		}

		hr = IDirectDrawSurface7_SetClipper(surface->lpDDS, surface->clip);
		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: SetClipper failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			DDrawSurfaceRelease(surface);
			return NULL;
		}
	}	
	else {	// 创建全屏模式
		DWORD dwFlags;
		
		if (hFullScreenMode) {
			fprintf(stderr, "DirectDraw: Already in Full Screen Mode\n");
			fflush(stderr);
			return NULL;
		}

		dwFlags = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT | 
			DDSCL_ALLOWMODEX;

		hr = IDirectDraw7_SetCooperativeLevel(lpDirectDraw7, hWnd, dwFlags);
		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: SetCooperativeLevel failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			return NULL;
		}

		hr = IDirectDraw7_SetDisplayMode(lpDirectDraw7, w, h, bpp, 0,
			DDSDM_STANDARDVGAMODE);

		if (hr != DD_OK) {
			fprintf(stderr, "DirectDraw: SetDisplayMode failed\n");
			fflush(stderr);
			if (HR) *HR = hr;
			return NULL;
		}

		ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | 
			DDSCAPS_COMPLEX;
		ddsd.dwBackBufferCount = 1;

		surface = DDrawCreateFromDesc(&ddsd, &hr);

		if (surface == NULL) {
			fprintf(stderr, "DirectDraw: Create Primary Surface failed\n");
			fflush(stderr);
			IDirectDraw7_RestoreDisplayMode(lpDirectDraw7);
			if (HR) *HR = hr;
			return NULL;
		}

		hFullScreenMode = 1;
	}

	IDirectDraw7_GetDisplayMode(lpDirectDraw7, &ddsd);
	surface->primary = 1;
	surface->hWnd = hWnd;

	return surface;
}


// 创建表面
// BPP为零时创建和主表面相同的像素格式，否则创建指定格式。
// mode为 DDSM_SYSTEMMEM或者 DDSM_VIDEOMEM
CSURFACE *DDrawSurfaceCreate(int w, int h, int fmt, int mode, HRESULT *hr)
{
	DDSURFACEDESC2 ddsd;
	CSURFACE *surface;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	ddsd.dwFlags =  DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = w;
	ddsd.dwHeight = h;

	if (mode == DDSM_SYSTEMMEM) 
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

	switch (fmt)
	{
	case PIXFMT_DEFAULT:
		break;
	case PIXFMT_8:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB|DDPF_PALETTEINDEXED8;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
		break;
	case PIXFMT_RGB15:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
		ddsd.ddpfPixelFormat.dwRBitMask = 0xfc00;
		ddsd.ddpfPixelFormat.dwGBitMask = 0x03e0;
		ddsd.ddpfPixelFormat.dwBBitMask = 0x001f;
		break;
	case PIXFMT_RGB16:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
		ddsd.ddpfPixelFormat.dwRBitMask = 0xf800;
		ddsd.ddpfPixelFormat.dwGBitMask = 0x07e0;
		ddsd.ddpfPixelFormat.dwBBitMask = 0x001f;
		break;
	case PIXFMT_RGB24:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 24;
		ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
		ddsd.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		ddsd.ddpfPixelFormat.dwBBitMask = 0x0000ff;
		break;
	case PIXFMT_RGB32:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
		ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
		ddsd.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		ddsd.ddpfPixelFormat.dwBBitMask = 0x0000ff;
		break;
	case PIXFMT_ARGB32:
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
		ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
		ddsd.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		ddsd.ddpfPixelFormat.dwBBitMask = 0x0000ff;
		ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	default:
		fprintf(stderr, "DDrawSurfaceCreate: unsupported pixfmt\n");
		fflush(stderr);
		return NULL;
		break;
	}

	surface = DDrawCreateFromDesc(&ddsd, hr); 

	if (surface == NULL) {
		fprintf(stderr, "DDrawSurfaceCreate failed\n");
		fflush(stderr);
		return NULL;
	}

	return surface;
}


int DDrawSurfaceInfo(const CSURFACE *s, int *w, int *h, int *bpp, int *fmt)
{
	assert(s);

	if (bpp) *bpp = s->bpp;
	if (fmt) *fmt = s->pixfmt;

	if (s->primary == 0 || hFullScreenMode) {
		if (w) *w = s->w;
		if (h) *h = s->h;
	}	else {
		RECT rect;
		GetClientRect(s->hWnd, &rect);
		if (w) *w = rect.right - rect.left;
		if (h) *h = rect.bottom - rect.top;
	}

	return 0;
}

// Clip Rectangle
int DDrawScaleClip(int DBW, int DBH, int *dx, int *dy, int *dw, 
	int *dh, int SBW, int SBH, int *sx, int *sy, int *sw, int *sh)
{
	int x1 = *dx;
	int y1 = *dy;
	int w1 = *dw;
	int h1 = *dh;
	int x2 = *sx;
	int y2 = *sy;
	int w2 = *sw;
	int h2 = *sh;
	int cl, ct, cr, cb;
	int cw, ch;
	cl = 0; ct = 0; cr = DBW; cb = DBH;
	cw = cr - cl;
	ch = cb - ct;
	if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0) return -1;
	if (x1 >= cl && y1 >= ct && w1 <= cw && h1 <= ch &&
		x2 >= 0 && y2 >= 0 && w2 <= SBW && h2 <= SBH) {
		return 0;
	}	else {
		float fx = ((float)w2) / w1;
		float fy = ((float)h2) / h1;
		float ix = 1.0f / fx;
		float iy = 1.0f / fy;
		int ds, d;
		if (x1 < cl) {
			d = cl - x1;
			x2 += (int)(d * fx);
			w2 -= (int)(d * fx);
			w1 -= d, x1 = cl;
		}
		if (y1 < ct) {
			d = ct - y1;
			y2 += (int)(d * fx);
			h2 -= (int)(d * fx);
			h1 -= d;
			y1 = ct;
		}
		if (x2 < 0) {
			x1 += (int)(-x2 * ix), w1 += (int)(x2 * ix), w2 += x2, x2 = 0;
		}
		if (y2 < 0) {
			y1 += (int)(-y2 * iy), h2 += (int)(y2 * iy), h2 += y2, y2 = 0;
		}
		if (x1 >= cr || y1 >= cb) return -2;
		if (x2 >= SBW || y2 >= SBH) return -3;
		if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0) return -4;
		if (x1 + w1 > cr) 
			ds = x1 + w1 - cr, w1 -= ds, w2 -= (int)(ds * fx);
		if (y1 + h1 > cb)
			ds = y1 + h1 - cb, h1 -= ds, h2 -= (int)(ds * fx);
		if (x2 + w2 > SBW)
			ds = x2 + w2 - SBW, w1 -= (int)(ds * ix), w2 -= ds;
		if (y2 + h2 > SBH)
			ds = y2 + h2 - SBH, h1 -= (int)(ds * iy), h2 -= ds;
		if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0) 
			return -5;
	}
	*dx = x1, *dy = y1, *dw = w1, *dh = h1;
	*sx = x2, *sy = y2, *sw = w2, *sh = h2;
	return 0;
}


// BLIT: 
// (dx, dy, dw, dh)  -  目标表面的区域
// (sx, sy, sw, sh)  -  源表面的区域
// flags的设置在上面那些 DDBLIT_* 开头的宏
// 返回 0表示成功，其他值表示 DirectDrawSurface7::Blt的返回
int DDrawSurfaceBlit(CSURFACE *dst, int dx, int dy, int dw, int dh, 
	CSURFACE *src, int sx, int sy, int sw, int sh, int flags)
{
	LPDIRECTDRAWSURFACE7 lpDDS1;
	LPDIRECTDRAWSURFACE7 lpDDS2;
	RECT RectDst;
	RECT RectSrc;
	DWORD dwFlags;
	HRESULT hr;
	int DBW;
	int DBH;

	// get destination screen size
	if (dst->primary && hFullScreenMode == 0) {
		DDrawSurfaceInfo(dst, &DBW, &DBH, 0, 0);
	}	else {
		DBW = dst->w;
		DBH = dst->h;
	}

	if ((flags & DDBLIT_NOCLIP) == 0) {
		if (DDrawScaleClip(DBW, DBH, &dx, &dy, &dw, &dh,
			src->w, src->h, &sx, &sy, &sw, &sh) != 0) {
			return -1;
		}
	}

	// calculate destination rectangle (window & fullscreen)
	if (dst->primary && hFullScreenMode == 0) {
		RECT Window;
		POINT pt;
		GetClientRect(dst->hWnd, &Window);
		pt.x = pt.y = 0;
		ClientToScreen(dst->hWnd, &pt);
		OffsetRect(&Window, pt.x, pt.y);
		RectDst.left = Window.left + dx;
		RectDst.top = Window.top + dy;
		RectDst.right = RectDst.left + dw;
		RectDst.bottom = RectDst.top + dh;
	}	else {
		RectDst.left = dx;
		RectDst.top = dy;
		RectDst.right = RectDst.left + dw;
		RectDst.bottom = RectDst.top + dh;
	}

	// calculate source rectangle (window & fullscreen)
	if (src->primary && hFullScreenMode == 0) {
		RECT Window;
		POINT pt;
		GetClientRect(src->hWnd, &Window);
		pt.x = pt.y = 0;
		ClientToScreen(src->hWnd, &pt);
		OffsetRect(&Window, pt.x, pt.y);
		RectSrc.left = Window.left + sx;
		RectSrc.top = Window.top + sy;
		RectSrc.right = RectSrc.left + sw;
		RectSrc.bottom = RectSrc.top + sh;
	}	else {
		RectSrc.left = sx;
		RectSrc.top = sy;
		RectSrc.right = RectSrc.left + sw;
		RectSrc.bottom = RectSrc.top + sh;
	}

	lpDDS1 = dst->lpDDS;
	lpDDS2 = src->lpDDS;

	dwFlags = 0;

	if (flags & DDBLIT_MASK) dwFlags |= DDBLT_KEYSRC;
	if (flags & DDBLIT_NOWAIT) dwFlags |= DDBLT_DONOTWAIT;
	else dwFlags |= DDBLT_WAIT;

	hr = IDirectDrawSurface7_Blt(lpDDS1, &RectDst, lpDDS2, &RectSrc, 
		dwFlags, NULL);

	if (hr == DDERR_SURFACELOST) {
		if (IDirectDrawSurface7_IsLost(lpDDS1) == DDERR_SURFACELOST) {
			IDirectDrawSurface7_Restore(lpDDS1);
		}
		if (IDirectDrawSurface7_IsLost(lpDDS2) == DDERR_SURFACELOST) {
			IDirectDrawSurface7_Restore(lpDDS2);
		}
		hr = IDirectDrawSurface7_Blt(lpDDS1, &RectDst, lpDDS2, &RectSrc, 
			dwFlags, NULL);
	}

	if (hr != DD_OK) return (int)hr;

	return 0;
}


// 锁定表面：锁定后才能写数据，flags值如上面三个的组合
// 返回    0 成功，
// 返回    1 表示还在绘制，无法锁定(没有NOWAIT时)
// 返回   -1 表示已经锁定过了，无法再次锁定
// 返回   -2 锁定时候碰到的其他错误 
int DDrawSurfaceLock(CSURFACE *dst, void **bits, long *pitch, int flags)
{
	DDSURFACEDESC2 ddsd;
	DWORD dwFlags;
	HRESULT hr;

	dwFlags = 0;

	if (dst->lock) {
		return -1;
	}

	if ((flags & DDLOCK_NOWAIT) == 0) dwFlags |= DDLOCK_WAIT;
	if ((flags & DDLOCK_OREAD) != 0) dwFlags |= DDLOCK_READONLY;
	if ((flags & DDLOCK_OWRITE) != 0) dwFlags |= DDLOCK_WRITEONLY;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	hr = IDirectDrawSurface7_Lock(dst->lpDDS, NULL, &ddsd, dwFlags, NULL);

	if (hr == DDERR_SURFACELOST) {
		IDirectDrawSurface7_Restore(dst->lpDDS);
		hr = IDirectDrawSurface7_Lock(dst->lpDDS, NULL, &ddsd, dwFlags, 0);
	}

	if (hr == DDERR_WASSTILLDRAWING) 
		return 1;
	
	if (hr != DD_OK)
		return -2;

	dst->lock = 1;
	dst->bits = (unsigned char*)ddsd.lpSurface;
	dst->pitch = (long)ddsd.dwLinearSize;

	if (bits) *bits = (void*)ddsd.lpSurface;
	if (pitch) *pitch = (long)ddsd.dwLinearSize;

	return 0;
}


// 解锁表面：解锁后才能调用BLIT
// 返回    0 成功，
// 返回   -1 表示已经锁定过了，无法再次锁定
// 返回   -2 锁定时候碰到的其他错误 
int DDrawSurfaceUnlock(CSURFACE *dst)
{
	HRESULT hr;

	if (dst->lock == 0) {
		return -1;
	}

	hr = IDirectDrawSurface7_Unlock(dst->lpDDS, NULL);

	if (hr == DDERR_SURFACELOST) {
		IDirectDrawSurface7_Restore(dst->lpDDS);
		hr = IDirectDrawSurface7_Unlock(dst->lpDDS, NULL);
	}

	if (hr != DD_OK) 
		return -2;

	dst->lock = 0;

	return 0;
}


// 设置关键色
int DDrawSurfaceSetMask(CSURFACE *dst, unsigned long mask)
{
	DDCOLORKEY ddck;
	HRESULT hr;

	ddck.dwColorSpaceLowValue = mask;
	ddck.dwColorSpaceHighValue = mask;

	if (dst->lock) {
		return -1;
	}

	hr = IDirectDrawSurface7_SetColorKey(dst->lpDDS, DDCKEY_SRCBLT, &ddck);

	if (hr != DD_OK) {
		return -2;
	}

	dst->mask = mask;

	return 0;
}


// 垂直同步: 0对应DDWAITVB_BLOCKBEGIN, 其他为DDWAITVB_BLOCKEND 
int DDrawWaitForVerticalBlank(int mode)
{
	DWORD dwFlag = (mode == 0)? DDWAITVB_BLOCKBEGIN : DDWAITVB_BLOCKEND;
	HRESULT hr;

	if (lpDirectDraw7 == NULL) {
		if (DDrawInit() != 0) return -1;
	}

	hr = IDirectDraw7_WaitForVerticalBlank(lpDirectDraw7, dwFlag, NULL);

	if (hr != DD_OK)
		return -1;

	return 0;
}



#endif


