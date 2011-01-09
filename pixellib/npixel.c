#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ibitmap.h"
#include "ibmbits.h"
#include "ibmcols.h"
#include "iblit386.h"
#include "ipicture.h"
#include "ikitwin.h"
#include "ibmsse2.h"

#ifdef _WIN32
#include "mswindx.h"

#include <olectl.h>
#include <objidl.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

typedef struct { 
	char *name;
	void *func;
}	nexport_list_t;


#define IENTRYLIST(name) { ""#name, name }




//! flag: -O3, -Wall
//! int: obj
//! out: libpixel
//! mode: lib
//! src: ikitwin.c, mswindx.c, ibmfont.c, ibmsse2.c, ibmwink.c
//! src: ibitmap.c, ibmbits.c, iblit386.c, ibmcols.c, ipicture.c, ibmdata.c


//=====================================================================
// SCREEN
//=====================================================================
int iscreen_dx = 1;
int iscreen_bpp = 0;		// 屏幕的深度
int iscreen_fmt = 0;
int iscreen_inited = 0;
int iscreen_locked = 0;
int iscreen_mx = 0;
int iscreen_my = 0;
int iscreen_mb = 0;
int iscreen_in = 0;
int iscreen_closing = 0;
int iscreen_atexit = 0;

IBITMAP *iscreen = NULL;
IBITMAP *cscreen = NULL;

static iColorIndex *cscreen_index = NULL;
static iColorIndex *iscreen_index = NULL;

#ifdef _WIN32
CSURFACE *dxdesktop = NULL;
CSURFACE *dxscreen = NULL;
#endif


void *align_malloc(size_t size) {
	char *ptr = (char*)malloc(size + 16 + sizeof(char*));
	char *src;
	if (ptr == NULL) return NULL;
	src = ptr + sizeof(char*);
	src = src + ((16 - (((size_t)src) & 15)) & 15);
	*(char**)(src - sizeof(char*)) = ptr;
	return src;
}

void align_free(void *ptr) {
	char *src = (char*)ptr - sizeof(char*);
	free(*(char**)src);
}

extern void*(*icmalloc)(size_t size);
extern void (*icfree)(void *ptr);

void init_allocator()
{
	icmalloc = align_malloc;
	icfree = align_free;
}


IBITMAP *ibitmap_ref(void *ptr, long pitch, int w, int h, int bpp)
{
	IBITMAP *bitmap;
	int fmt;
	if (bpp == 8) fmt = IPIX_FMT_C8;
	else if (bpp == 15) fmt = IPIX_FMT_X1R5G5B5;
	else if (bpp == 16) fmt = IPIX_FMT_R5G6B5;
	else if (bpp == 24) fmt = IPIX_FMT_R8G8B8;
	else fmt = IPIX_FMT_X8R8G8B8;
	bitmap = ibitmap_reference_new(ptr, pitch, w, h, fmt);
	assert(bitmap);
	bitmap->bpp = bpp;
	return bitmap;
}

void ibitmap_unref(IBITMAP *bmp)
{
	ibitmap_reference_del(bmp);
}

void ibitmap_adjust(IBITMAP *bmp, void *ptr, long pitch)
{
	ibitmap_reference_adjust(bmp, ptr, pitch);
}

static void iscreen_atexit_hook(void);


static void iscreen_init_index(iColorIndex *index)
{
	int i;
	index->color = 256;
	for (i = 0; i < 256; i++) {
		index->rgba[i] = IRGBA_TO_A8R8G8B8(i, i, i, 255);
	}
	for (i = 0; i < 32768; i++) {
		int r, g, b, a;
		IRGBA_FROM_X1R5G5B5(i, r, g, b, a);
		a = _ipixel_to_gray(r, g, b);
		index->ent[i] = a;
	}
}

int iscreen_init_main(int w, int h, int bpp)
{
	#ifdef _WIN32
	int fmt = 0;
	iscreen_dx = (bpp < 0)? 0 : 1;
	bpp = (bpp < 0)? (-bpp) : bpp;
	if (iscreen_dx) {
		if (DDrawInit() != 0) iscreen_dx = 0;
	}
	if (ikitwin_init() != 0) return -1;
	if (bpp < 0 && iscreen_dx) {
		iscreen_dx = 0;
		bpp = -bpp;
		DDrawDestroy();
	}
	if (ikitwin_set_mode(w, h, bpp, 0) != 0) return -2;
	if (iscreen_dx) {
		dxdesktop = DDrawCreateScreen(mswin_hwnd, 0, 0, 0, 0);
		if (dxdesktop != NULL) {
			dxscreen = DDrawSurfaceCreate(w, h, PIXFMT_DEFAULT,
				DDSM_SYSTEMMEM, NULL);
			if (dxscreen != NULL) {
				void *bits;
				long pitch;
				DDrawSurfaceInfo(dxscreen, 0, 0, &iscreen_bpp, &iscreen_fmt);
				if (DDrawSurfaceLock(dxscreen, &bits, &pitch, 0) == 0) {
					int fmt;
					iscreen = ibitmap_ref(bits, pitch, w, h, iscreen_bpp);
					fmt = ibitmap_pixfmt_guess(iscreen);
					if (fmt == IPIX_FMT_A8R8G8B8) fmt = IPIX_FMT_X8R8G8B8;
					ibitmap_pixfmt_set(iscreen, fmt);
					DDrawSurfaceUnlock(dxscreen);
					iscreen_locked = 0;
				}	else {
					DDrawSurfaceRelease(dxscreen);
					DDrawSurfaceRelease(dxdesktop);
					dxscreen = NULL;
					dxdesktop = NULL;
					DDrawDestroy();
					iscreen_dx = 0;
				}
			}	else {
				iscreen_dx = 0;
				DDrawSurfaceRelease(dxdesktop);
				dxdesktop = NULL;
				DDrawDestroy();
			}
		}	else {
			iscreen_dx = 0;
			DDrawDestroy();
		}
	}
	if (iscreen_dx == 0) {
		void *bits;
		long pitch;
		int fmt;
		ikitwin_lock(&bits, &pitch);
		iscreen = ibitmap_ref(bits, pitch, w, h, bpp);
		ikitwin_unlock();
		fmt = ibitmap_pixfmt_guess(iscreen);
		if (fmt == IPIX_FMT_A8R8G8B8) fmt = IPIX_FMT_X8R8G8B8;
		ibitmap_pixfmt_set(iscreen, fmt);
		iscreen_bpp = bpp;
	}
	#else
	int rmask, gmask, bmask, i, fmt;
	void *bits;
	long pitch;
	bpp = (bpp < 0)? (-bpp) : bpp;
	if (ikitwin_init() != 0) return -1;
	if (ikitwin_set_mode(w, h, bpp < 0? -bpp : bpp, 0) != 0) return -2;
	iscreen_bpp = ikitwin_depth(&rmask, &gmask, &bmask);
	ikitwin_lock(&bits, &pitch);
	iscreen = ibitmap_ref(bits, pitch, w, h, iscreen_bpp);
	ikitwin_unlock();
	for (fmt = 0, i = 0; i < IPIX_FMT_COUNT; i++) {
		if (ipixelfmt[i].rmask == rmask &&
			ipixelfmt[i].gmask == gmask &&
			ipixelfmt[i].bmask == bmask &&
			ipixelfmt[i].alpha == 0 &&
			ipixelfmt[i].bpp == iscreen_bpp) {
			fmt = i;
			break;
		}
	}
	ibitmap_pixfmt_set(iscreen, fmt);
	#endif
	cscreen = ibitmap_create(w, h, bpp);
	fmt = ibitmap_pixfmt_guess(cscreen);
	if (fmt == IPIX_FMT_A8R8G8B8) fmt = IPIX_FMT_X8R8G8B8;
	ibitmap_pixfmt_set(cscreen, fmt);

	if (iscreen->bpp == 8) {
		iscreen_index = (iColorIndex*)malloc(sizeof(iColorIndex));
		iscreen->extra = iscreen_index;
		assert(iscreen_index);
		iscreen_init_index(iscreen_index);
	}
	if (cscreen->bpp == 8) {
		cscreen_index = (iColorIndex*)malloc(sizeof(iColorIndex));
		cscreen->extra = cscreen_index;
		assert(cscreen_index);
		iscreen_init_index(cscreen_index);
	}
	if (iscreen_atexit == 0) {
		iscreen_atexit = 1;
		atexit(iscreen_atexit_hook);
	}
	ikitwin_set_caption("ADI-DRAW");
	iscreen_inited = 1;
	return 0;
}

int iscreen_init(int w, int h, int bpp)
{
	init_allocator();
#ifdef __x86__
	_x86_choose_blitter();
	pixellib_mmx_init();
#endif
	return iscreen_init_main(w, h, bpp);
}

void iscreen_quit(void)
{
	if (iscreen_inited == 0) return;
	if (iscreen) ibitmap_unref(iscreen);
	if (cscreen) ibitmap_release(cscreen);
	if (cscreen_index) free(cscreen_index);
	if (iscreen_index) free(iscreen_index);
	iscreen = NULL;
	cscreen = NULL;
	cscreen_index = NULL;
	iscreen_index = NULL;
	#ifndef __unix
	if (iscreen_dx) {
		if (dxscreen) DDrawSurfaceRelease(dxscreen);
		if (dxdesktop) DDrawSurfaceRelease(dxdesktop);
		dxscreen = NULL;
		dxdesktop = NULL;
		DDrawDestroy();
	}
	#endif
	ikitwin_release_mode();
	ikitwin_quit();
	iscreen_inited = 0;
}

static void iscreen_atexit_hook(void)
{
	iscreen_quit();
}


int iscreen_lock(void)
{
	if (iscreen_inited == 0) return -100;
	#ifdef _WIN32
	if (iscreen_dx) {
		void *bits;
		long pitch;
		if (iscreen_locked == 0) {
			if (DDrawSurfaceLock(dxscreen, &bits, &pitch, 0) == 0) {
				ibitmap_adjust(iscreen, bits, pitch);
			}	else {
				return -1;
			}
		}
	}
	#else
	if (iscreen_locked == 0) {
		ikitwin_lock(NULL, NULL);
	}
	#endif
	iscreen_locked++;
	return 0;
}

int iscreen_unlock(void)
{
	if (iscreen_inited == 0) return -100;
	iscreen_locked--;
	#ifdef _WIN32
	if (iscreen_dx) {
		if (iscreen_locked == 0) {
			DDrawSurfaceUnlock(dxscreen);
		}
	}
	#endif
	return 0;
}

int iscreen_update(int *rect, int n)
{
	int fullwindow[4];
	if (iscreen_inited == 0) return -100;
	fullwindow[0] = 0;
	fullwindow[1] = 0;
	fullwindow[2] = iscreen->w;
	fullwindow[3] = iscreen->h;
	if (rect == NULL) rect = fullwindow;
	#ifdef _WIN32
	if (iscreen_dx) {
		int i;
		while (iscreen_locked) iscreen_unlock();
		for (i = 0; i < n; i++) {
			int x = rect[i * 4 + 0];
			int y = rect[i * 4 + 1];
			int w = rect[i * 4 + 2];
			int h = rect[i * 4 + 3];
			DDrawSurfaceBlit(dxdesktop, x, y, w, h, dxscreen, 
				x, y, w, h, DDBLIT_NOWAIT);
		}
		return 0;
	}
	#endif
	ikitwin_update(rect, 1);
	return 0;
}

int iscreen_convert(int *rect, int n)
{
	int fullwindow[4];
	int i;
	if (iscreen_inited == 0) return -100;
	fullwindow[0] = 0;
	fullwindow[1] = 0;
	fullwindow[2] = iscreen->w;
	fullwindow[3] = iscreen->h;
	if (rect == NULL) rect = fullwindow;
	iscreen_lock();
	for (i = 0; i < n; i++, rect += 4) {
		int x = rect[0];
		int y = rect[1];
		int w = rect[2];
		int h = rect[3];
		ibitmap_convert(iscreen, x, y, cscreen, x, y, w, h, NULL, 0);
	}
	iscreen_unlock();
	return 0;
}

int iscreen_keyon(int keycode)
{
	if (iscreen_inited == 0) return 0;
	return ikitwin_keymap[keycode & 511];
}

int iscreen_dispatch(void)
{
	int retval;
	retval = ikitwin_dispatch();
	iscreen_mx = ikitwin_mouse_x;
	iscreen_my = ikitwin_mouse_y;
	iscreen_mb = ikitwin_mouse_btn;
	iscreen_in = ikitwin_mouse_in;
	iscreen_closing = ikitwin_closing;
	return retval;
}

int iscreen_tick(int fps)
{
	return ikitwin_tick(fps);
}

float iscreen_fps(void)
{
	return ikitwin_fps();
}

IINT64 iscreen_clock(void)
{
	return ikitwin_clock();
}

void iscreen_sleep(long millisecond)
{
	ikitwin_sleep(millisecond);
}

void iscreen_caption(const char *text)
{
	if (iscreen_inited == 0) return;
	ikitwin_set_caption(text);
}

void iscreen_vsync(int mode)
{
	if (iscreen_inited == 0) return;
#ifdef _WIN32
	if (iscreen_dx) {
		DDrawWaitForVerticalBlank(mode);
	}
#endif
}

void iblend_putpixel(IBITMAP *bmp, int x, int y, IUINT32 c)
{
	if (x >= 0 && y >= 0 && x < (int)bmp->w && y < (int)bmp->h)
		ibitmap_rectfill(bmp, x, y, 1, 1, c);
}


//=====================================================================
// MISC
//=====================================================================
#include <math.h>


void imisc_bitmap_demo(IBITMAP *bmp, int type)
{
	int w = (int)bmp->w;
	int h = (int)bmp->h;
	int x, y, i, j, k;
	int pixfmt;
	pixfmt = ibitmap_pixfmt_guess(bmp);
	if (type == 0) {
		int size = 12, dd = 0;
#if 1
		iStoreProc store;
		IUINT32 *card;
		card = (IUINT32*)malloc(sizeof(IUINT32) * (bmp->w + size));
		if (card == NULL) return;
		for (i = 0, dd = 0, k = 0; i < (int)bmp->w + size; i++) {
			card[i] = (k == 0)? 0xffcccccc : 0xffffffff;
			if (++dd >= size) dd = 0, k ^= 1;
		}
		store = ipixel_get_store(ibitmap_pixfmt_guess(bmp), 0);
		for (j = 0, dd = 0, k = 0; j < h; j++) {
			store(bmp->line[j], card + k * size, 0, w, 
				(iColorIndex*)bmp->extra);
			if (++dd >= size) dd = 0, k ^= 1;
		}
		free(card);
#else
		for (y = 0, k = 0; y < h; y += size) {
			k = dd;
			dd ^= 1;
			for (x = 0; x < w; x += size, k++) {
				IUINT32 col = (k & 1)? 0xffcccccc : 0xffffffff;
				ibitmap_rectfill(bmp, x, y, size, size, col);
			}
		}
#endif
	}
	else if (type == 1) {
		int size = 32, dd = 0;
		for (y = 0, k = 0; y < h; y += size) {
			k = dd;
			dd ^= 1;
			for (x = 0; x < w; x += size, k++) {
				IUINT32 col = (k & 1)? 0xff0000ff : 0xff00003f;
				ibitmap_rectfill(bmp, x, y, size, size, col);
			}
		}
	}
	else if (type == 2) {
		ibitmap_rectfill(bmp, 0, 0, w, h, 0xffff00ff);
		for (j = -w - h; j < w + h; j += 32) {
			for (y = 0; y < h; y++) {
				iblend_putpixel(bmp, j + y, y, 0xffaaaaaa);
				iblend_putpixel(bmp, j - y, y, 0xffaaaaaa);
			}
		}
		for (i = 0; i < w; i += 96) {
			h = (int)(sin(3.14 * i / 640.0) * bmp->h * 0.3 + bmp->h * 0.7);
			for (y = 0; y < h; y++) {
				for (x = 0; x < 40; x++) {
					j = (40 - x) * 255 / 40;
					iblend_putpixel(bmp, i + x, bmp->h - 1 - y, 
						(j << 16) | (j << 8) | j | 0xff000000);
				}
			}
		}
		bmp->mask = ipixel_assemble(pixfmt, 0xff, 0, 0xff, 0xff);
	}
}


void imisc_bitmap_systext(IBITMAP *dst, int x, int y, const char *str, 
	IUINT32 color, IUINT32 bk, const IRECT *clip, int additive)
{
	//ifont_glyph_builtin(dst, x, y, str, color, bk, clip, additive);
}

// 反响查找
int CPixelFormatInverse[IPIX_FMT_COUNT] = {
	4, 5, 6, 7, 0, 1, 2, 3, 4, -1, -1, -1, -1, 17, 18, 19, 20, 13, 14, 
	15, 16, 25, 26, 27, 28, 21, 22, 23, 24, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};


//=====================================================================
// WIN32 GDI/GDI+ 接口
//=====================================================================
#if defined(_WIN32) || defined(WIN32)
int CSetDIBitsToDevice(void *hDC, int x, int y, const IBITMAP *bitmap,
	int sx, int sy, int sw, int sh)
{
	char _buffer[sizeof(BITMAPINFO) + (256 + 4) * sizeof(RGBQUAD)];
	BITMAPINFO *info = (BITMAPINFO*)_buffer;
	int fmt = 0;
	int width = (int)bitmap->w;
	int height = (int)bitmap->h;
	int bpp = (int)bitmap->bpp;
	int bitfield = 0;
	int palsize = 0;
	LPRGBQUAD palette = NULL;
	IRECT rect;
	int hr;
	
	fmt = ibitmap_pixfmt_guess(bitmap);

	info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth = width;
	info->bmiHeader.biHeight = -height;
	info->bmiHeader.biPlanes = 1;
	info->bmiHeader.biBitCount = bpp;
	info->bmiHeader.biCompression = BI_RGB;
	info->bmiHeader.biSizeImage = bitmap->pitch * height;
	info->bmiHeader.biXPelsPerMeter = 0;
	info->bmiHeader.biYPelsPerMeter = 0;
	info->bmiHeader.biClrUsed = 0;
	info->bmiHeader.biClrImportant = 0;

	if (bpp < 15) {
		palsize = (1 << bpp) * sizeof(PALETTEENTRY);
	}
	else if (bpp == 16 || bpp == 32 || bpp == 24) {
		bitfield = 3 * sizeof(RGBQUAD);
	}

	if (fmt == IPIX_FMT_R8G8B8 || fmt == IPIX_FMT_B8G8R8) {
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
		const iColorIndex *index = (const iColorIndex*)bitmap->extra;
		int i;
		if (index == NULL) {
			for (i = 0; i < 256; i++) {
				palette[i].rgbBlue = i;
				palette[i].rgbGreen = i;
				palette[i].rgbRed = i;
				palette[i].rgbReserved = 0;
			}
		}	else {
			for (i = 0; i < 256; i++) {
				int r, g, b, a;
				IRGBA_FROM_A8R8G8B8(index->rgba[i], r, g, b, a);
				a = 0;
				palette[i].rgbBlue = b;
				palette[i].rgbGreen = g;
				palette[i].rgbRed = r;
				palette[i].rgbReserved = a;
			}
		}
	}

	rect.left = sx;
	rect.top = sy;
	rect.right = sx + sw;
	rect.bottom = sy + sh;

	if (rect.left < 0) x -= rect.left, rect.left = 0;
	if (rect.top < 0) y -= rect.top, rect.top = 0;
	if (rect.right > width) rect.right = width;
	if (rect.bottom > height) rect.bottom = height;

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	if (width <= 0 || height <= 0) return 0;

	hr = SetDIBitsToDevice((HDC)hDC, x, y, width, height, 
						rect.left, rect.top, 
						0, (int)bitmap->h, bitmap->pixel,
						(LPBITMAPINFO)info, DIB_RGB_COLORS);

	if (hr <= 0) return -1;
	return 0;
}

int CStretchDIBits(void *hDC, int dx, int dy, int dw, int dh, 
	const IBITMAP *bitmap, int sx, int sy, int sw, int sh)
{
	char _buffer[sizeof(BITMAPINFO) + (256 + 4) * sizeof(RGBQUAD)];
	BITMAPINFO *info = (BITMAPINFO*)_buffer;
	int fmt = 0;
	int width = (int)bitmap->w;
	int height = (int)bitmap->h;
	int bpp = (int)bitmap->bpp;
	int bitfield = 0;
	int palsize = 0;
	LPRGBQUAD palette = NULL;
	IRECT clipdst, clipsrc;
	IRECT bound_dst, bound_src;
	int hr;
	
	fmt = ibitmap_pixfmt_guess(bitmap);

	info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth = width;
	info->bmiHeader.biHeight = -height;
	info->bmiHeader.biPlanes = 1;
	info->bmiHeader.biBitCount = bpp;
	info->bmiHeader.biCompression = BI_RGB;
	info->bmiHeader.biSizeImage = bitmap->pitch * height;
	info->bmiHeader.biXPelsPerMeter = 0;
	info->bmiHeader.biYPelsPerMeter = 0;
	info->bmiHeader.biClrUsed = 0;
	info->bmiHeader.biClrImportant = 0;

	if (bpp < 15) {
		palsize = (1 << bpp) * sizeof(PALETTEENTRY);
	}
	else if (bpp == 16 || bpp == 32 || bpp == 24) {
		bitfield = 3 * sizeof(RGBQUAD);
	}

	if (fmt == IPIX_FMT_R8G8B8 || fmt == IPIX_FMT_B8G8R8) {
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
		const iColorIndex *index = (const iColorIndex*)bitmap->extra;
		int i;
		if (index == NULL) {
			for (i = 0; i < 256; i++) {
				palette[i].rgbBlue = i;
				palette[i].rgbGreen = i;
				palette[i].rgbRed = i;
				palette[i].rgbReserved = 0;
			}
		}	else {
			for (i = 0; i < 256; i++) {
				int r, g, b, a;
				IRGBA_FROM_A8R8G8B8(index->rgba[i], r, g, b, a);
				a = 0;
				palette[i].rgbBlue = b;
				palette[i].rgbGreen = g;
				palette[i].rgbRed = r;
				palette[i].rgbReserved = a;
			}
		}
	}

	clipdst.left = 0;
	clipdst.top = 0;
	clipdst.right = dx + dw;
	clipdst.bottom = dy + dh;
	clipsrc.left = 0;
	clipsrc.top = 0;
	clipsrc.right = (int)bitmap->w;
	clipsrc.bottom = (int)bitmap->h;

	bound_dst.left = dx;
	bound_dst.top = dy;
	bound_dst.right = dx + dw;
	bound_dst.bottom = dy + dh;
	bound_src.left = sx;
	bound_src.top = sy;
	bound_src.right = sx + sw;
	bound_src.bottom = sy + sh;

	if (ibitmap_clip_scale(&clipdst, &clipsrc, &bound_dst, &bound_src, 0))
		return -100;

	dx = bound_dst.left;
	dy = bound_dst.top;
	dw = bound_dst.right - bound_dst.left;
	dh = bound_dst.bottom - bound_dst.top;
	sx = bound_src.left;
	sy = bound_src.top;
	sw = bound_src.right - bound_src.left;
	sh = bound_src.bottom - bound_src.top;

	SetStretchBltMode((HDC)hDC, COLORONCOLOR);

	hr = (int)StretchDIBits((HDC)hDC, dx, dy, dw, dh, sx, sy, sw, sh,
				bitmap->pixel, info, DIB_RGB_COLORS, SRCCOPY);

	if (hr == (int)GDI_ERROR) return -200;

	return 0;
}


// 初始化 BITMAPINFO
int CInitDIBInfo(BITMAPINFO *info, int w, int h, int pixfmt, 
	const LPRGBQUAD pal)
{
	int palsize = 0;
	int bitfield = 0;
	DWORD pixelbytes = 0;
	DWORD pitch;
	int bpp;

	bpp = ipixelfmt[pixfmt].bpp;

	if (bpp < 15) palsize = (1 << bpp) * sizeof(PALETTEENTRY);

	if (bpp == 16 || bpp == 15 || bpp == 32 || bpp == 24) {
		bitfield = 3 * sizeof(RGBQUAD);
	}

	pixelbytes = (bpp + 7) >> 3;
	pitch = (pixelbytes * w + 3) & ~3;

	info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth = w;
	info->bmiHeader.biHeight = h;
	info->bmiHeader.biPlanes = 1;
	info->bmiHeader.biBitCount = bpp;
	info->bmiHeader.biCompression = BI_RGB;
	info->bmiHeader.biSizeImage = (h < 0)? (-h) * pitch : h * pitch;
	info->bmiHeader.biXPelsPerMeter = 0;
	info->bmiHeader.biYPelsPerMeter = 0;
	info->bmiHeader.biClrUsed = 0;
	info->bmiHeader.biClrImportant = 0;

	if (bpp == 24 && pixfmt == IPIX_FMT_R8G8B8) {
		info->bmiHeader.biCompression = BI_RGB;
	}	else {
		DWORD *data = (DWORD*)info;
		data[10] = ipixelfmt[pixfmt].rmask;
		data[11] = ipixelfmt[pixfmt].gmask;
		data[12] = ipixelfmt[pixfmt].bmask;
		data[13] = ipixelfmt[pixfmt].amask;
		info->bmiHeader.biCompression = BI_BITFIELDS;
	}

	if (palsize > 0) {
		int offset = sizeof(BITMAPINFOHEADER) + bitfield;
		LPRGBQUAD palette = (LPRGBQUAD)((char*)info + offset);
		int i;
		for (i = 0; i < palsize; i++) {
			palette[i].rgbBlue = pal[i].rgbBlue;
			palette[i].rgbGreen = pal[i].rgbGreen;
			palette[i].rgbRed = pal[i].rgbRed;
			palette[i].rgbReserved = 0;
		}
		return offset;
	}

	return 0;
}

// CreateDIBSection 
HBITMAP CCreateDIBSection(HDC hdc, int w, int h, int pixfmt,
	const LPRGBQUAD pal, LPVOID *bits)
{
	unsigned char buffer[4 * 4 + sizeof(BITMAPINFOHEADER) + 1024];
	BITMAPINFO *info = (BITMAPINFO*)buffer;
	HBITMAP hBitmap;
	LPVOID ptr;
	int mode = (ipixelfmt[pixfmt].bpp < 15)? DIB_PAL_COLORS: DIB_RGB_COLORS;
	CInitDIBInfo(info, w, h, pixfmt, pal);
	hBitmap = CreateDIBSection(hdc, (LPBITMAPINFO)info, mode, &ptr, NULL, 0);
	if (bits) *bits = ptr;
	return hBitmap;
}


// 预先乘法
void CPremultiplyAlpha(void *bits, long pitch, int w, int h)
{
	for (; h > 0; h--) {
		BYTE *src = (BYTE*)bits;
		int x, r1, g1, b1, r2, g2, b2;
		for (x = (w >> 1); x > 0; x--, src += 8) {
			r1 = ((int)src[0]) * src[3];
			g1 = ((int)src[1]) * src[3];
			b1 = ((int)src[2]) * src[3];
			r2 = ((int)src[4]) * src[7];
			g2 = ((int)src[5]) * src[7];
			b2 = ((int)src[6]) * src[7];
			src[0] = (r1 + (r1 >> 8) + 0x80) >> 8;
			src[1] = (g1 + (g1 >> 8) + 0x80) >> 8;
			src[2] = (b1 + (b1 >> 8) + 0x80) >> 8;
			src[4] = (r2 + (r2 >> 8) + 0x80) >> 8;
			src[5] = (g2 + (g2 >> 8) + 0x80) >> 8;
			src[6] = (b2 + (b2 >> 8) + 0x80) >> 8;
		}
		if (w & 1) {
			r1 = ((int)src[0]) * src[3];
			g1 = ((int)src[1]) * src[3];
			b1 = ((int)src[2]) * src[3];
			src[0] = (r1 + (r1 >> 8) + 0x80) >> 8;
			src[1] = (g1 + (g1 >> 8) + 0x80) >> 8;
			src[2] = (b1 + (b1 >> 8) + 0x80) >> 8;
		}
		bits = (char*)bits + pitch;
	}
}



// user32.dll 导出：UpdateLayeredWindow
BOOL CUpdateLayeredWindow(HWND hWnd, HDC hdcDst, POINT *pptDst,
	SIZE *psize, HDC hdcSrc, POINT *pptSrc, COLORREF crKey, 
	BLENDFUNCTION *pblend, DWORD dwFlags)
{
	typedef BOOL (WINAPI *UpdateLayeredWindow_t)(HWND, HDC, POINT*,
		SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD);
	static UpdateLayeredWindow_t UpdateLayeredWindow_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (UpdateLayeredWindow_o == NULL) {
		hDLL = LoadLibraryA("user32.dll");
		if (hDLL == NULL) return FALSE;
		UpdateLayeredWindow_o = (UpdateLayeredWindow_t)
			GetProcAddress(hDLL, "UpdateLayeredWindow");
		if (UpdateLayeredWindow_o == NULL) return FALSE;
	}
	return UpdateLayeredWindow_o(hWnd, hdcDst, pptDst,
		psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
}


// user32.dll 导出：SetLayeredWindowAttr
BOOL CSetLayeredWindowAttr(HWND hWnd, COLORREF crKey,
	BYTE bAlpha, DWORD dwFlags)
{
	typedef BOOL (WINAPI *SetLayeredWindowAttributes_t)(HWND, COLORREF,
		BYTE, DWORD);
	static SetLayeredWindowAttributes_t SetLayeredWindowAttributes_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (SetLayeredWindowAttributes_o == NULL) {
		hDLL = LoadLibraryA("user32.dll");
		if (hDLL == NULL) return FALSE;
		SetLayeredWindowAttributes_o = (SetLayeredWindowAttributes_t)
			GetProcAddress(hDLL, "SetLayeredWindowAttributes");
		if (SetLayeredWindowAttributes_o == NULL) return FALSE;
	}
	return SetLayeredWindowAttributes_o(hWnd, crKey, bAlpha, dwFlags);
}


// msimg32.dll 导出：AlphaBlend
BOOL CAlphaBlend(HDC hdcDst, int nXOriginDest, int nYOriginDest,
	int nWidthDest, int nHeightDest, HDC hdcSrc, int nXOriginSrc, 
	int nYOriginSrc, int nWidthSrc, int nHeightSrc, 
	BLENDFUNCTION blendFunction)
{
	typedef BOOL (WINAPI *AlphaBlend_t)(HDC, int, int, int, int, HDC,
		int, int, int, int, BLENDFUNCTION);
	static AlphaBlend_t AlphaBlend_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (AlphaBlend_o == NULL) {
		hDLL = LoadLibraryA("msimg32.dll");
		if (hDLL == NULL) return FALSE;
		AlphaBlend_o = (AlphaBlend_t)GetProcAddress(hDLL, "AlphaBlend");
		if (AlphaBlend_o == NULL) return FALSE;
	}
	return AlphaBlend_o(hdcDst, nXOriginDest, nYOriginDest, nWidthDest,
		nHeightDest, hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, 
		nHeightSrc, blendFunction);
}


// msimg32.dll 导出：TransparentBlt
BOOL CTransparentBlt(HDC hdcDst, int nXOriginDest, int nYOriginDest,
	int nWidthDest, int nHeightDest, HDC hdcSrc, int nXOriginSrc, 
	int nYOriginSrc, int nWidthSrc, int nHeightSrc, UINT crTransparent)
{
	typedef BOOL (WINAPI *TransparentBlt_t)(HDC, int, int, int, int, HDC,
		int, int, int, int, UINT);
	static TransparentBlt_t TransparentBlt_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (TransparentBlt_o == NULL) {
		hDLL = LoadLibraryA("msimg32.dll");
		if (hDLL == NULL) return FALSE;
		TransparentBlt_o = (TransparentBlt_t)
			GetProcAddress(hDLL, "TransparentBlt");
		if (TransparentBlt_o == NULL) return FALSE;
	}
	return TransparentBlt_o(hdcDst, nXOriginDest, nYOriginDest, nWidthDest,
		nHeightDest, hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, 
		nHeightSrc, crTransparent);
}


// msimg32.dll 导出：GradientFill
BOOL CGradientFill(HDC hdc, PTRIVERTEX pVertex, ULONG dwNumVertex,
	PVOID pMesh, ULONG dwNumMesh, ULONG dwMode)
{
	typedef BOOL (WINAPI *GradientFill_t)(HDC, PTRIVERTEX, ULONG, PVOID, 
		ULONG, ULONG);
	static GradientFill_t GradientFill_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (GradientFill_o == NULL) {
		hDLL = LoadLibraryA("msimg32.dll");
		if (hDLL == NULL) return FALSE;
		GradientFill_o = (GradientFill_t)
			GetProcAddress(hDLL, "GradientFill");
		if (GradientFill_o == NULL) return FALSE;
	}
	return GradientFill_o(hdc, pVertex, dwNumVertex, pMesh, dwNumMesh,
		dwMode);
}

// comctl32.dll 导出：DrawShadowText
int CDrawShadowText(HDC hdc, LPCWSTR pszText, UINT cch, const RECT *pRect,
	DWORD dwFlags, COLORREF crText, COLORREF crShadow, int ixOffset, 
	int iyOffset)
{
	typedef int (WINAPI *DrawShadowText_t)(HDC, LPCWSTR, UINT, const RECT *,
		DWORD, COLORREF, COLORREF, int, int);
	static DrawShadowText_t DrawShadowText_o = NULL;
	static HINSTANCE hDLL = NULL;
	if (DrawShadowText_o == NULL) {
		hDLL = LoadLibraryA("comctl32.dll");
		if (hDLL == NULL) return -1000;
		DrawShadowText_o = (DrawShadowText_t)
			GetProcAddress(hDLL, "DrawShadowText");
		if (DrawShadowText_o == NULL) return -2000;
	}
	return DrawShadowText_o(hdc, pszText, cch, pRect, dwFlags, crText,
		crShadow, ixOffset, iyOffset);
}

// GDI+ 开始1，结束0
int CGdiPlusInit(int startup)
{
	static HINSTANCE hDLL = NULL;
	static ULONG token = 0;
	static int inited = 0;
	int retval;

	if (hDLL == NULL) {
		hDLL = LoadLibraryA("gdiplus.dll");
		if (hDLL == NULL) return -1;
	}

	if (startup) {
		struct {
			unsigned int version;
			void *callback;
			BOOL SuppressBackgroundThread;
			BOOL SuppressExternalCodecs;
		}	GStartupInput;
		struct {
			void *hook;
			void *unhook;
		}	GStartupOutput;

		typedef int (WINAPI *GdiPlusStartup_t)(ULONG*, LPVOID, LPVOID);
		static GdiPlusStartup_t GdiPlusStartup_o = NULL;

		if (inited) return 0;

		if (GdiPlusStartup_o == NULL) {
			GdiPlusStartup_o = (GdiPlusStartup_t)GetProcAddress(
				hDLL, "GdiplusStartup");
			if (GdiPlusStartup_o == NULL) {
				return -2;
			}
		}

		GStartupInput.version = 1;
		GStartupInput.callback = NULL;
		GStartupInput.SuppressBackgroundThread = 0;
		GStartupInput.SuppressExternalCodecs = 0;

		retval = GdiPlusStartup_o(&token, &GStartupInput, &GStartupOutput);

		if (retval != 0) {
			return -3;
		}

		inited = 1;
	}
	else {
		typedef int (WINAPI *GdiPlusShutdown_t)(ULONG*);
		static GdiPlusShutdown_t GdiPlusShutdown_o = NULL;

		if (inited == 0) return 0;

		if (GdiPlusShutdown_o == NULL) {
			GdiPlusShutdown_o = (GdiPlusShutdown_t)GetProcAddress(
				hDLL, "GdiplusShutdown");
			if (GdiPlusShutdown_o == NULL) {
				return -4;
			}
		}

		GdiPlusShutdown_o(&token);

		inited = 0;
	}

	return 0;
}


// GDI+ 读取内存中的图片
void *CGdiPlusLoadCore(const void *data, long size, int *cx, int *cy, 
	long *pitch, int *pfmt, int *bpp, int *errcode)
{
	typedef HRESULT (WINAPI *CreateStream_t)(HGLOBAL, BOOL, LPSTREAM*);
	typedef int (WINAPI *GdipCreateBitmap_t)(LPSTREAM, LPVOID*); 
	typedef int (WINAPI *GdipGetPixelFormat_t)(LPVOID, int*);
	typedef int (WINAPI *GdipDisposeImage_t)(LPVOID);
	typedef int (WINAPI *GdipGetImageWidth_t)(LPVOID, UINT*);
	typedef int (WINAPI *GdipGetImageHeight_t)(LPVOID, UINT*);
	typedef int (WINAPI *GdipLockBits_t)(LPVOID, LPVOID, UINT, int, LPVOID);
	typedef int (WINAPI *GdipUnlockBits_t)(LPVOID, LPVOID);

	static CreateStream_t CreateStream_o = NULL;
	static GdipCreateBitmap_t GdipCreateBitmap_o = NULL;
	static GdipGetPixelFormat_t GdipGetPixelFormat_o = NULL;
	static GdipDisposeImage_t GdipDisposeImage_o = NULL;
	static GdipGetImageWidth_t GdipGetImageWidth_o = NULL;
	static GdipGetImageHeight_t GdipGetImageHeight_o = NULL;
	static GdipLockBits_t GdipLockBits_o = NULL;
	static GdipUnlockBits_t GdipUnlockBits_o = NULL;

	static HINSTANCE hGdiPlusDLL = NULL;
	LPSTREAM ppstm = NULL;
	HGLOBAL hg = NULL;
	LPVOID pg = NULL;
	LPVOID bitmap = NULL;
	void *bits = NULL;
	int retval = 0;
	int gpixfmt = 0;
	int fmt = 0;
	int nbytes = 0;
	UINT width;
	UINT height;
	long stride;
	int GRECT[4];
	int i;

	struct { 
		unsigned int width;
		unsigned int height;
		int pitch;
		int format;
		void *scan0;
		int *reserved;
	}	GBitmapData;

	GBitmapData.width = 0;
	GBitmapData.height = 0;
	GBitmapData.pitch = 0;
	GBitmapData.scan0 = NULL;

	if (CreateStream_o == NULL) {
		static HINSTANCE hOleDLL = NULL;
		if (hOleDLL == NULL) {
			hOleDLL = LoadLibraryA("ole32.dll");
			if (hOleDLL == NULL) {
				if (errcode) errcode[0] = -20;
				return NULL;
			}
		}
		CreateStream_o = (CreateStream_t)GetProcAddress(
			hOleDLL, "CreateStreamOnHGlobal");
		if (CreateStream_o == NULL) {
			if (errcode) errcode[0] = -21;
			return NULL;
		}
	}

	if (hGdiPlusDLL == NULL) {
		hGdiPlusDLL = LoadLibraryA("gdiplus.dll");
		if (hGdiPlusDLL == NULL) {
			if (errcode) errcode[0] = -22;
			return NULL;
		}
	}

	if (GdipCreateBitmap_o == NULL) {
		GdipCreateBitmap_o = (GdipCreateBitmap_t)GetProcAddress(
			hGdiPlusDLL, "GdipCreateBitmapFromStream");
		if (GdipCreateBitmap_o == NULL) {
			if (errcode) errcode[0] = -23;
			return NULL;
		}
	}

	if (GdipGetPixelFormat_o == NULL) {
		GdipGetPixelFormat_o = (GdipGetPixelFormat_t)GetProcAddress(
			hGdiPlusDLL, "GdipGetImagePixelFormat");
		if (GdipGetPixelFormat_o == NULL) {
			if (errcode) errcode[0] = -24;
			return NULL;
		}
	}

	if (GdipDisposeImage_o == NULL) {
		GdipDisposeImage_o = (GdipDisposeImage_t)GetProcAddress(
			hGdiPlusDLL, "GdipDisposeImage");
		if (GdipDisposeImage_o == NULL) {
			if (errcode) errcode[0] = -25;
			return NULL;
		}
	}

	if (GdipGetImageWidth_o == NULL) {
		GdipGetImageWidth_o = (GdipGetImageWidth_t)GetProcAddress(
			hGdiPlusDLL, "GdipGetImageWidth");
		if (GdipGetImageWidth_o == NULL) {
			if (errcode) errcode[0] = -26;
			return NULL;
		}
	}

	if (GdipGetImageHeight_o == NULL) {
		GdipGetImageHeight_o = (GdipGetImageHeight_t)GetProcAddress(
			hGdiPlusDLL, "GdipGetImageHeight");
		if (GdipGetImageHeight_o == NULL) {
			if (errcode) errcode[0] = -27;
			return NULL;
		}
	}

	if (GdipLockBits_o == NULL) {
		GdipLockBits_o = (GdipLockBits_t)GetProcAddress(
			hGdiPlusDLL, "GdipBitmapLockBits");
		if (GdipLockBits_o == NULL) {
			if (errcode) errcode[0] = -28;
			return NULL;
		}
	}

	if (GdipUnlockBits_o == NULL) {
		GdipUnlockBits_o = (GdipUnlockBits_t)GetProcAddress(
			hGdiPlusDLL, "GdipBitmapUnlockBits");
		if (GdipUnlockBits_o == NULL) {
			if (errcode) errcode[0] = -29;
			return NULL;
		}
	}

	if (errcode) errcode[0] = 0;

	hg = GlobalAlloc(GMEM_MOVEABLE, size);

	if (hg == NULL) {
		if (errcode) errcode[0] = -1;
		return NULL;
	}

	pg = GlobalLock(hg);

	if (pg == NULL) {
		GlobalFree(hg);
		if (errcode) errcode[0] = -2;
		return NULL;
	}

	memcpy(pg, data, size);

	GlobalUnlock(hg);

	if (CreateStream_o(hg, 0, &ppstm) != S_OK) {
		GlobalFree(hg);
		if (errcode) errcode[0] = -3;
		return NULL;
	}

	retval = GdipCreateBitmap_o(ppstm, &bitmap);

	if (retval != 0) {
		retval = -4;
		bitmap = NULL;
		goto finalizing;
	}


	#define GPixelFormat1bppIndexed     196865
	#define GPixelFormat4bppIndexed     197634
	#define GPixelFormat8bppIndexed     198659
	#define GPixelFormat16bppGrayScale  1052676
	#define GPixelFormat16bppRGB555     135173
	#define GPixelFormat16bppRGB565     135174
	#define GPixelFormat16bppARGB1555   397319
	#define GPixelFormat24bppRGB        137224
	#define GPixelFormat32bppRGB        139273
	#define GPixelFormat32bppARGB       2498570
	#define GPixelFormat32bppPARGB      925707
	#define GPixelFormat48bppRGB        1060876
	#define GPixelFormat64bppARGB       3424269
	#define GPixelFormat64bppPARGB      29622286
	#define GPixelFormatMax             15
	
	if (GdipGetPixelFormat_o(bitmap, &gpixfmt) != 0) {
		retval = -5;
		goto finalizing;
	}
	
	if (gpixfmt == GPixelFormat8bppIndexed)
		gpixfmt = GPixelFormat8bppIndexed,
		fmt = 8, 
		nbytes = 1;
	else if (gpixfmt == GPixelFormat16bppRGB555)
		gpixfmt = GPixelFormat16bppRGB555,
		fmt = 555,
		nbytes = 2;
	else if (gpixfmt == GPixelFormat16bppRGB565)
		gpixfmt = GPixelFormat16bppRGB565,
		fmt = 565,
		nbytes = 2;
	else if (gpixfmt == GPixelFormat16bppARGB1555)
		gpixfmt = GPixelFormat16bppARGB1555,
		fmt = 1555,
		nbytes = 2;
	else if (gpixfmt == GPixelFormat24bppRGB)
		gpixfmt = GPixelFormat24bppRGB,
		fmt = 888,
		nbytes = 3;
	else if (gpixfmt == GPixelFormat32bppRGB)
		gpixfmt = GPixelFormat32bppRGB,
		fmt = 888,
		nbytes = 4;
	else if (gpixfmt == GPixelFormat32bppARGB)
		gpixfmt = GPixelFormat32bppARGB,
		fmt = 8888,
		nbytes = 4;
	else if (gpixfmt == GPixelFormat64bppARGB) 
		gpixfmt = GPixelFormat32bppARGB,
		fmt = 8888,
		nbytes = 4;
	else if (gpixfmt == GPixelFormat64bppPARGB)
		gpixfmt = GPixelFormat32bppARGB,
		fmt = 8888,
		nbytes = 4;
	else if (gpixfmt == GPixelFormat32bppPARGB)
		gpixfmt = GPixelFormat32bppARGB,
		fmt = 8888,
		nbytes = 4;
	else 
		gpixfmt = GPixelFormat32bppRGB,
		fmt = 8888,
		nbytes = 4;

	if (bpp) bpp[0] = nbytes * 8;
	if (pfmt) pfmt[0] = fmt;

	if (GdipGetImageWidth_o(bitmap, &width) != 0) {
		retval = -6;
		goto finalizing;
	}

	if (cx) cx[0] = (int)width;

	if (GdipGetImageHeight_o(bitmap, &height) != 0) {
		retval = -7;
		goto finalizing;
	}

	if (cy) cy[0] = (int)height;

	stride = (nbytes * width + 3) & ~3;
	if (pitch) pitch[0] = stride;

	GRECT[0] = 0;
	GRECT[1] = 0;
	GRECT[2] = (int)width;
	GRECT[3] = (int)height;

	if (GdipLockBits_o(bitmap, GRECT, 1, gpixfmt, &GBitmapData) != 0) {
		GBitmapData.scan0 = NULL;
		retval = -8;
		goto finalizing;
	}

	if (GBitmapData.format != gpixfmt) {
		retval = -9;
		goto finalizing;
	}

	bits = (char*)malloc(stride * height);

	if (bits == NULL) {
		retval = -10;
		goto finalizing;
	}

	for (i = 0; i < (int)height; i++) {
		char *src = (char*)GBitmapData.scan0 + GBitmapData.pitch * i;
		char *dst = (char*)bits + stride * i;
		memcpy(dst, src, stride);
	}

	retval = 0;
	
finalizing:
	if (GBitmapData.scan0 != NULL) {
		GdipUnlockBits_o(bitmap, &GBitmapData);
		GBitmapData.scan0 = NULL;
	}

	if (bitmap) {
		GdipDisposeImage_o(bitmap);
		bitmap = NULL;
	}

	if (ppstm) {
		#ifndef __cplusplus
		ppstm->lpVtbl->Release(ppstm);
		#else
		ppstm->Release();
		#endif
		ppstm = NULL;
	}

	if (hg) {
		GlobalFree(hg);
		hg = NULL;
	}

	if (errcode) errcode[0] = retval;

	return bits;
}


// 解压32U
static unsigned long iDecode32u(const void *data)
{
	const unsigned char *ptr = (const unsigned char*)data;
	unsigned long c1 = ptr[0];
	unsigned long c2 = ptr[1];
	unsigned long c3 = ptr[2];
	unsigned long c4 = ptr[3];
	return (c4 << 24) | (c3 << 16) | (c2 << 8) | c1;
}

// 读取Jng文件
void *CGdiPlusLoadMemory(const void *data, long size, int *cx, int *cy, 
	long *pitch, int *pfmt, int *bpp, int *errcode)
{
	const unsigned char *ptr = (const unsigned char*)data;
	unsigned long picw, pich, len1, len2;
	unsigned char *img1, *img2;
	long pitch1, pitch2;
	int icx, icy, ierror;
	int fmt1, fmt2, bpp1, bpp2; 
	int i, j;

	if (size < 20) {
		return CGdiPlusLoadCore(data, size, cx, cy, pitch, pfmt, bpp,
			errcode);
	}
	if (ptr[0] != 'N' || ptr[1] != 'J' || ptr[2] != 'P' || ptr[3] != 'G') {
		return CGdiPlusLoadCore(data, size, cx, cy, pitch, pfmt, bpp,
			errcode);
	}

	picw = iDecode32u(ptr + 4);
	pich = iDecode32u(ptr + 8);
	len1 = iDecode32u(ptr + 12);
	len2 = iDecode32u(ptr + 16);

	if (len1 == 0 && len2 == 0) {
		if (errcode) errcode[0] = -15;
		return NULL;
	}

	img1 = NULL;
	img2 = NULL;

	if (len1 > 0) {
		img1 = (unsigned char*)CGdiPlusLoadCore(ptr + 20, len1, &icx, &icy, 
			&pitch1, &fmt1, &bpp1, &ierror);

		if (img1 == NULL) {
			if (errcode) errcode[0] = -1000 + ierror;
			return NULL;
		}

		if (icx != (int)picw || icy != (int)pich) {
			free(img1);
			if (errcode) errcode[0] = -20;
			return NULL;
		}
	}

	if (len2 > 0) {
		img2 = (unsigned char*)CGdiPlusLoadCore(ptr + 20 + len1, 
			len2, &icx, &icy, &pitch2, &fmt2, &bpp2, &ierror);

		if (img2 == NULL) {
			if (img1) free(img1);
			if (errcode) errcode[0] = -2000 + ierror;
			return NULL;
		}

		if (icx != (int)picw || icy != (int)pich) {
			if (img1) free(img1);
			free(img2);
			if (errcode) errcode[0] = -30;
			return NULL;
		}
	}

	if (cx) cx[0] = (int)picw;
	if (cy) cy[0] = (int)pich;
	if (pitch) pitch[0] = pitch2;
	if (pfmt) pfmt[0] = 8888;
	if (bpp) bpp[0] = 32;
	if (errcode) errcode[0] = 0;

	if (img1 != NULL && img2 == NULL) {
		if (pfmt) pfmt[0] = fmt1;
		if (bpp) bpp[0] = bpp1;
		return img1;
	}

	if (img1 == NULL && img2 != NULL) {
		if (pfmt) pfmt[0] = fmt2;
		if (bpp) bpp[0] = bpp2;
		return img2;
	}

	if (bpp1 != 24 && bpp2 != 32) {
		if (errcode) errcode[0] = -40;
		free(img1);
		free(img2);
		return NULL;
	}

	for (j = 0; j < (int)pich; j++) {
		unsigned char *src = img1 + pitch1 * j;
		unsigned char *dst = img2 + pitch2 * j;
		unsigned long endian = 0x11223344;
		if (*((unsigned char*)&endian) == 0x44) {
			for (i = (int)picw; i > 0; src += 3, dst += 4, i--) {
				dst[3] = dst[1];
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
			}
		}
		else {
			for (i = (int)picw; i > 0; src += 3, dst += 4, i--) {
				dst[0] = dst[1];
				dst[1] = src[0];
				dst[2] = src[1];
				dst[3] = src[2];
			}
		}
	}

	free(img1);

	return img2;
}


// GDI+ 读取内存图片到 IBITMAP
IBITMAP *CGdiPlusLoadBitmap(const void *data, long size, int *errcode)
{
	int cx, cy, fmt, bpp, cc, pixfmt;
	void *bits;
	long pitch;
	IBITMAP *bmp;
	int i;

	bits = CGdiPlusLoadMemory(data, size, &cx, &cy, &pitch, &fmt, &bpp, &cc);

	if (bits == NULL) {
		if (errcode) errcode[0] = cc;
		return NULL;
	}

	pixfmt = -1;

	if (bpp == 8) {
		pixfmt = IPIX_FMT_C8;
	}
	else if (bpp == 16) {
		if (fmt == 555) pixfmt = IPIX_FMT_X1R5G5B5;
		else if (fmt == 565) pixfmt = IPIX_FMT_R5G6B5;
		else if (fmt == 1555) pixfmt = IPIX_FMT_A1R5G5B5;
	}
	else if (bpp == 24) {
		pixfmt = IPIX_FMT_R8G8B8;
	}
	else if (bpp == 32) {
		if (fmt == 888) pixfmt = IPIX_FMT_X8R8G8B8;
		else if (fmt == 8888) pixfmt = IPIX_FMT_A8R8G8B8;
	}

	if (pixfmt < 0) {
		free(bits);
		if (errcode) errcode[0] = -40;
		return NULL;
	}

	bmp = ibitmap_create(cx, cy, ipixelfmt[pixfmt].bpp);

	if (bmp == NULL) {
		free(bits);
		if (errcode) errcode[0] = -41;
		return NULL;
	}

	ibitmap_imode(bmp, pixfmt) = pixfmt;

	for (i = 0; i < cy; i++) {
		char *src = (char*)bits + pitch * i;
		char *dst = (char*)bmp->pixel + (long)bmp->pitch * i;
		memcpy(dst, src, (bpp / 8) * cx);
	}

	free(bits);

	return bmp;
}


// 读取文件内容
static void *iload_file_content(const char *fname, long *size)
{
	size_t length, remain;
	char *data, *lptr;
	FILE *fp;

	fp = fopen(fname, "rb");

	if (fp == NULL) {
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	data = (char*)malloc(length);

	if (data == NULL) {
		fclose(fp);
		return NULL;
	}

	for (lptr = data, remain = length; ; ) {
		size_t readed;
		if (feof(fp) || remain == 0) break;
		readed = fread(lptr, 1, remain, fp);
		remain -= readed;
		lptr += readed;
	}

	fclose(fp);

	if (size) size[0] = (long)length;

	return data;
}


// GDI+ 从文件读取 IBITMAP
IBITMAP *CGdiPlusLoadFile(const char *fname, int *errcode)
{
	long size;
	char *data;
	IBITMAP *bmp;

	data = (char*)iload_file_content(fname, &size);

	if (data == NULL) {
		if (errcode) errcode[0] = -50;
		return NULL;
	}

	bmp = CGdiPlusLoadBitmap(data, size, errcode);
	free(data);

	return bmp;
}

static struct IBITMAP *CGdiPlus_Load_Jpg(IMDIO *stream, IRGB *pal)
{
	static const unsigned char mark[2] = { 0xff, 0xd9 };
	char *buffer;
	size_t size;
	int error;
	IBITMAP *bmp;
	buffer = (char*)is_read_marked_block(stream, mark, 2, &size);
	if (buffer == NULL || size == 0) return NULL;
	bmp = CGdiPlusLoadBitmap(buffer, (long)size, &error);
	free(buffer);
	return bmp;
}

static struct IBITMAP *CGdiPlus_Load_Png(IMDIO *stream, IRGB *pal)
{
	static const unsigned char mark[12] = { 0, 0, 0, 0, 
		0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };
	char *buffer;
	size_t size;
	int error;
	IBITMAP *bmp;
	buffer = (char*)is_read_marked_block(stream, mark, 12, &size);
	if (buffer == NULL || size == 0) return NULL;
	bmp = CGdiPlusLoadBitmap(buffer, (long)size, &error);
	free(buffer);
	return bmp;
}

struct IBITMAP *CGdiPlus_Load_Tiff(IMDIO *stream, IRGB *pal)
{
	static const unsigned char mark[2] = { 0xff, 0xd9 };
	char *buffer;
	size_t size;
	int error;
	IBITMAP *bmp;
	buffer = (char*)is_read_marked_block(stream, mark, 0, &size);
	if (buffer == NULL || size == 0) return NULL;
	bmp = CGdiPlusLoadBitmap(buffer, (long)size, &error);
	free(buffer);
	return bmp;
}

struct IBITMAP *CGdiPlus_Load_Jng(IMDIO *stream, IRGB *pal)
{
	unsigned char head[20];
	unsigned char *ptr;
	unsigned long s1, s2;
	IBITMAP *bmp;
	int error;
	if (is_reader(stream, head, 20) != 20) {
		return NULL;
	}
	if (head[0] != 'N' || head[1] != 'J' || head[2] != 'P' || head[3] != 'G')
		return NULL;
	s1 = iDecode32u(head + 12);
	s2 = iDecode32u(head + 16);
	ptr = (unsigned char*)malloc(20 + s1 + s2);
	if (ptr == NULL) return NULL;
	memcpy(ptr, head, 20);
	if (is_reader(stream, ptr + 20, s1 + s2) != s1 + s2) {
		free(ptr);
		return NULL;
	}
	bmp = CGdiPlusLoadBitmap(ptr, 20 + s1 + s2, &error);
	free(ptr);
	return bmp;
}

// 初始化GDI+ 的解码器
void CGdiPlusDecoderInit(int GdiPlusStartup)
{
	static int inited = 0;
	if (inited) return;
	if (GdiPlusStartup) {
		CGdiPlusInit(1);
	}
	ipic_loader(0x89, CGdiPlus_Load_Png);
	//ipic_loader(0x49, CGdiPlus_Load_Tiff);
	//ipic_loader(0x4d, CGdiPlus_Load_Tiff);
	ipic_loader(0xff, CGdiPlus_Load_Jpg);
	ipic_loader(0x4e, CGdiPlus_Load_Jng);
	inited = 1;
}


//---------------------------------------------------------------------
// 高级 WIN32功能
//---------------------------------------------------------------------

// 创建 DIB的 IBITMAP，HBITMAP保存在 IBITMAP::extra
IBITMAP *CGdiCreateDIB(HDC hdc, int w, int h, int pixfmt, const LPRGBQUAD p)
{
	IBITMAP *bitmap;
	HBITMAP dib;
	void *bits;
	long pitch;
	dib = CCreateDIBSection(hdc, w, h, pixfmt, p, &bits);
	if (dib == NULL) return NULL;
	pitch = ((long)ipixelfmt[pixfmt].pixelbyte * (long)w + 3) & ~3;
	bitmap = ibitmap_reference_new(bits, pitch, w, h, pixfmt);
	if (bitmap == NULL) {
		DeleteObject(dib);
		return NULL;
	}
	bitmap->extra = dib;
	return bitmap;
}

// 删除 DIB的 IBITMAP，IBITMAP::extra 需要指向 DIB的 HBITMAP
void CGdiReleaseDIB(IBITMAP *bmp)
{
	assert(bmp);
	assert(bmp->extra);
	DeleteObject((HBITMAP)bmp->extra);
	bmp->extra = NULL;
	ibitmap_reference_del(bmp);
}


// 取得 DIB的 pixel format
int CGdiDIBFormat(const DIBSECTION *sect)
{
	const BITMAPINFOHEADER *header = &(sect->dsBmih);
	DWORD rmask = sect->dsBitfields[0];
	DWORD gmask = sect->dsBitfields[1];
	DWORD bmask = sect->dsBitfields[2];
	int compress = header->biCompression;
	int bpp = header->biBitCount;

	if (compress == BI_RGB) {
		if (bpp == 24) return IPIX_FMT_R8G8B8;
		else if (bpp == 32) {
			return IPIX_FMT_X8R8G8B8;
		}
		else {
			return -1;
		}
	}

	if (compress != BI_BITFIELDS) {
		return -2;
	}

	if (bpp == 32) {
		if (rmask == 0xff0000) return IPIX_FMT_X8R8G8B8;
		if (rmask == 0xff) return IPIX_FMT_X8B8G8R8;
		if (rmask == 0xff000000) return IPIX_FMT_R8G8B8X8;
		if (rmask == 0xff00) return IPIX_FMT_B8G8R8X8;
		return -3;
	}

	if (bpp == 24) {
		return (rmask == 0xff0000)? IPIX_FMT_R8G8B8 : IPIX_FMT_B8G8R8;
	}

	if (bpp == 16) {
		int i;
		for (i = IPIX_FMT_R5G6B5; i <= IPIX_FMT_B4G4R4A4; i++) {
			if (ipixelfmt[i].rmask == rmask && ipixelfmt[i].bmask == bmask) {
				if (ipixelfmt[i].alpha == 0) return i;
			}
		}
	}

	if (bpp == 15) {
		if (rmask == ipixelfmt[IPIX_FMT_X1R5G5B5].rmask)
			return IPIX_FMT_X1R5G5B5;
		if (rmask == ipixelfmt[IPIX_FMT_X1B5G5R5].rmask)
			return IPIX_FMT_X1B5G5R5;
		return -4;
	}

	if (bpp == 8) {
		return IPIX_FMT_C8;
	}

	gmask = gmask;

	return -5;
}


// 将字体转换为 IBITMAP: IPIX_FMT_A8
IBITMAP* CCreateTextCore(HFONT hFont, const wchar_t *textw, 
	const char *textc, int ncount, UINT format, LPDRAWTEXTPARAMS param)
{
	RECT rect = { 0, 0, 0, 0 };
	HFONT hFontSaved;
	HBITMAP hBitmap;
	HBITMAP hBitmapSaved;
	IBITMAP *bitmap;
	int width, height, retval, i;
	void *pixel;
	HDC hDC;

	hDC = CreateCompatibleDC(NULL);
	if (hDC == NULL) return NULL;

	hFontSaved = (HFONT)SelectObject(hDC, hFont);

	format &= ~(DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);

	if (textw != NULL) {
		retval = DrawTextExW(hDC, (wchar_t*)textw, ncount, &rect, 
			format | DT_CALCRECT, param);
	}
	else if (textc != NULL) {
		retval = DrawTextExA(hDC, (char*)textc, ncount, &rect, 
			format | DT_CALCRECT, param);
	}
	else {
		return NULL;
	}
	if (retval <= 0) return NULL;

	width = (int)rect.right - (int)rect.left;
	height = (int)rect.bottom - (int)rect.top;

	hBitmap = CCreateDIBSection(hDC, width, -height, IPIX_FMT_X8R8G8B8,
		NULL, &pixel);

	if (hBitmap == NULL) {
		SelectObject(hDC, hFontSaved);
		DeleteDC(hDC);
		return NULL;
	}

	bitmap = ibitmap_create(width, height, 8);

	if (bitmap == NULL) {
		DeleteObject(hBitmap);
		SelectObject(hDC, hFontSaved);
		DeleteDC(hDC);
		return NULL;		
	}

	ibitmap_pixfmt_set(bitmap, IPIX_FMT_A8);

	hBitmapSaved = (HBITMAP)SelectObject(hDC, hBitmap);

	SetTextColor(hDC, RGB(255, 255, 255));
	SetBkColor(hDC, 0);
	SetBkMode(hDC, OPAQUE);

	format &= ~((DWORD)(DT_CALCRECT));

	if (textw != NULL) {
		retval = DrawTextExW(hDC, (wchar_t*)textw, ncount, &rect, 
			format, param);
	}	else {
		retval = DrawTextExA(hDC, (char*)textc, ncount, &rect, 
			format, param);
	}

	if (retval <= 0) {
		SelectObject(hDC, hBitmapSaved);
		SelectObject(hDC, hFontSaved);
		DeleteObject(hBitmap);
		DeleteDC(hDC);
		ibitmap_release(bitmap);
		return NULL;
	}

	for (i = 0; i < height; i++) {
		const IUINT32 *src = (const IUINT32*)pixel + width * i;
		IUINT8 *dst = (IUINT8*)bitmap->line[i];
		IUINT32 sr, sg, sb, sa;
		int k;
		for (k = width; k > 0; src++, dst++, k--) {
			_ipixel_load_card(src, sr, sg, sb, sa);
			sa = sr + sg + sg + sb;
			dst[0] = (IUINT8)((sa >> 2) & 0xff);
		}
	}
	
	SelectObject(hDC, hBitmapSaved);
	SelectObject(hDC, hFontSaved);
	DeleteObject(hBitmap);
	DeleteDC(hDC);

	return bitmap;
}

// 将字体转换为 IBITMAP: IPIX_FMT_A8
IBITMAP* CCreateTextW(HFONT hFont, const wchar_t *text, 
	int ncount, UINT format, LPDRAWTEXTPARAMS param)
{
	return CCreateTextCore(hFont, text, NULL, ncount, format, param);
}

// 将字体转换为 IBITMAP: IPIX_FMT_A8
IBITMAP* CCreateTextA(HFONT hFont, const char *text, 
	int ncount, UINT format, LPDRAWTEXTPARAMS param)
{
	return CCreateTextCore(hFont, NULL, text, ncount, format, param);
}


#endif


