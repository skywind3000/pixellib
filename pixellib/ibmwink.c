//=====================================================================
//
// ibmwink.c - animation
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#include "ibmwink.h"
#include "iblit386.h"
#include "ibmfont.h"
#include "ibmdata.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>


#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

//=====================================================================
// 滤波器与通道操作
//=====================================================================

static void ibitmap_fetch_8(const IBITMAP *src, IUINT8 *card8, int line)
{
	IUINT8 mask;
	int overflow;
	int size;
	int h;

	overflow = ibitmap_imode_const(src, overflow);

	if (ibitmap_pixfmt_guess(src) == IPIX_FMT_G8) {
		IUINT32 r, g, b, a;
		IRGBA_FROM_A8R8G8B8(src->mask, r, g, b, a);
		mask = (IUINT8)_ipixel_to_gray(r, g, b);
	}	else {
		IUINT32 r, g, b, a;
		IRGBA_FROM_A8R8G8B8(src->mask, r, g, b, a);
		mask = (IUINT8)a;
	}

	size = (int)src->w + 2;
	h = (int)src->h;

	switch (overflow) 
	{
	case IBOM_TRANSPARENT:
		if (line < 0 || line >= (int)src->h) {
			memset(card8, mask, src->w + 2);
			return;
		}
		card8[0] = card8[size - 1] = mask;
		break;
	case IBOM_REPEAT:
		if (line < 0) line = 0;
		else if (line >= (int)src->h) line = (int)src->h - 1;
		card8[0] = ((IUINT8*)src->line[line])[0];
		card8[size - 1] = ((IUINT8*)src->line[line])[(int)src->w - 1];
		break;
	case IBOM_WRAP:
		line = line % h;
		if (line < 0) line += h;
		card8[size - 1] = ((IUINT8*)src->line[line])[0];
		card8[0] = ((IUINT8*)src->line[line])[(int)src->w - 1];
		break;
	case IBOM_MIRROR:
		if (line < 0) line = (-line) % h;
		else if (line >= h) line = h - 1 - (line % h);
		card8[0] = ((IUINT8*)src->line[line])[0];
		card8[size - 1] = ((IUINT8*)src->line[line])[(int)src->w - 1];
		break;
	}
	memcpy(card8 + 1, src->line[line], size - 2);
}

static void ibitmap_fetch_32(const IBITMAP *src, IUINT32 *card, int line)
{
	const iColorIndex *index = (const iColorIndex*)src->extra;
	iFetchProc fetch;
	IUINT32 mask;
	int overflow;
	int size;
	int h;

	overflow = ibitmap_imode_const(src, overflow);
	fetch = ipixel_get_fetch(ibitmap_pixfmt_guess(src), 0);
	mask = (IUINT32)src->mask;

	if (index == NULL) index = _ipixel_src_index;

	size = (int)src->w + 2;
	h = (int)src->h;

	switch (overflow)
	{
	case IBOM_TRANSPARENT:
		if (line < 0 || line >= (int)src->h) {
			int i;
			for (i = 0; i < size; i++) *card++ = mask;
			return;
		}
		card[0] = card[size - 1] = mask;
		break;
	case IBOM_REPEAT:
		if (line < 0) line = 0;
		else if (line >= (int)src->h) line = (int)src->h - 1;
		fetch(src->line[line], 0, 1, card, index);
		fetch(src->line[line], (int)src->w - 1, 1, card + size - 1, index);
		break;
	case IBOM_WRAP:
		line = line % h;
		if (line < 0) line += h;
		fetch(src->line[line], (int)src->w - 1, 1, card, index);
		fetch(src->line[line], 0, 1, card + size - 1, index);
		break;
	case IBOM_MIRROR:
		if (line < 0) line = (-line) % h;
		else if (line >= h) line = h - 1 - (line % h);
		fetch(src->line[line], 0, 1, card, index);
		fetch(src->line[line], (int)src->w - 1, 1, card + size - 1, index);
		break;
	}

	fetch(src->line[line], 0, size - 2, card + 1, index);
}

static inline IUINT8 
ibitmap_filter_pass_8(const IUINT8 *pixel, const short *filter)
{
	int result;

#ifdef __x86__
	if (X86_FEATURE(X86_FEATURE_MMX)) {
	}
#endif

	result =	(int)(pixel[0]) * (int)(filter[0]) + 
				(int)(pixel[1]) * (int)(filter[1]) +
				(int)(pixel[2]) * (int)(filter[2]) +
				(int)(pixel[3]) * (int)(filter[3]) +
				(int)(pixel[4]) * (int)(filter[4]) +
				(int)(pixel[5]) * (int)(filter[5]) +
				(int)(pixel[6]) * (int)(filter[6]) +
				(int)(pixel[7]) * (int)(filter[7]) +
				(int)(pixel[8]) * (int)(filter[8]);

	result = (result) >> 8;
	if (result > 255) result = 255;
	else if (result < 0) result = 0;

	return (IUINT8)result;
}

static inline IUINT32
ibitmap_filter_pass_32(const IUINT32 *pixel, const short *filter)
{
	int r1 = 0, g1 = 0, b1 = 0, a1 = 0;
	int r2 = 0, g2 = 0, b2 = 0, a2 = 0;
	int f, i;

#ifdef __x86__
	if (X86_FEATURE(X86_FEATURE_MMX)) {
	}
#endif

	for (i = 9; i > 0; pixel++, filter++, i--) {
		IRGBA_FROM_A8R8G8B8(pixel[0], r2, g2, b2, a2);
		f = filter[0];
		r1 += r2 * f;
		g1 += g2 * f;
		b1 += b2 * f;
		a1 += a2 * f;
	}

	r1 = (r1) >> 8;
	g1 = (g1) >> 8;
	b1 = (b1) >> 8;
	a1 = (a1) >> 8;

	if (r1 > 255) r1 = 255;
	else if (r1 < 0) r1 = 0;
	if (g1 > 255) g1 = 255;
	else if (g1 < 0) g1 = 0;
	if (b1 > 255) b1 = 255;
	else if (b1 < 0) b1 = 0;
	if (a1 > 255) a1 = 255;
	else if (a1 < 0) a1 = 0;

	return IRGBA_TO_A8R8G8B8(r1, g1, b1, a1);
}

int ibitmap_filter_8(IBITMAP *dst, const IBITMAP *src, const short *filter)
{
	IUINT8 *buffer, *p1, *p2, *p3, *p4, *card;
	IUINT8 pixel[9];
	int line, i;

	buffer = (IUINT8*)malloc((src->w + 2) * 3);
	if (buffer == NULL) return -10;
	
	for (line = 0; line < (int)src->h; line++) {
		p1 = buffer;
		p2 = p1 + src->w + 2;
		p3 = p2 + src->w + 2;
		p4 = (IUINT8*)dst->line[line];
		card = p4;
		ibitmap_fetch_8(src, p1, line - 1);
		ibitmap_fetch_8(src, p2, line - 0);
		ibitmap_fetch_8(src, p3, line + 1);
		p1++; p2++; p3++;
		for (i = (int)src->w; i > 0; i--, p1++, p2++, p3++, card++) {
			pixel[0] = p1[-1]; pixel[1] = p1[0]; pixel[2] = p1[1];
			pixel[3] = p2[-1]; pixel[4] = p2[0]; pixel[5] = p2[1];
			pixel[6] = p3[-1]; pixel[7] = p3[0]; pixel[8] = p3[1];
			card[0] = ibitmap_filter_pass_8(pixel, filter);
		}
	}

	free(buffer);

	return 0;
}

int ibitmap_filter_32(IBITMAP *dst, const IBITMAP *src, const short *filter)
{
	iColorIndex *index = (iColorIndex*)dst->extra;
	IUINT32 *buffer, *p1, *p2, *p3, *p4, *card;
	IUINT32 pixel[9];
	iStoreProc store;
	int line, i;

	buffer = (IUINT32*)malloc((src->w + 2) * 4 * 4);
	if (buffer == NULL) return -10;

	store = ipixel_get_store(ibitmap_pixfmt_guess(dst), 0);

	for (line = 0; line < (int)src->h; line++) {
		p1 = buffer;
		p2 = p1 + src->w + 2;
		p3 = p2 + src->w + 2;
		p4 = p3 + src->w + 2;
		card = p4;
		ibitmap_fetch_32(src, p1, line - 1);
		ibitmap_fetch_32(src, p2, line - 0);
		ibitmap_fetch_32(src, p3, line + 1);
		p1++; p2++; p3++;
		for (i = (int)src->w; i > 0; i--, p1++, p2++, p3++, card++) {
			pixel[0] = p1[-1]; pixel[1] = p1[0]; pixel[2] = p1[1];
			pixel[3] = p2[-1]; pixel[4] = p2[0]; pixel[5] = p2[1];
			pixel[6] = p3[-1]; pixel[7] = p3[0]; pixel[8] = p3[1];
			card[0] = ibitmap_filter_pass_32(pixel, filter);
			//card[0] = pixel[4];
		}
		store(dst->line[line], p4, 0, (int)src->w, index);
	}
	free(buffer);
	return 0;
}


//---------------------------------------------------------------------
// 使用滤波器
// filter是一个长为9的数组，该函数将以每个像素为中心3x3的9个点中的各个
// 分量乘以filter中对应的值后相加，再除以256，作为该点的颜色保存
//---------------------------------------------------------------------
int ibitmap_filter(IBITMAP *dst, const short *filter)
{
	IBITMAP *src;
	int retval;
	int pixfmt;

	assert(dst);

	src = ibitmap_create((int)dst->w, (int)dst->h, (int)dst->bpp);
	if (src == NULL) return -1;

	src->mode = dst->mode;
	src->mask = dst->mask;
	src->extra = dst->extra;
	pixfmt = ibitmap_pixfmt_guess(src);

	memcpy(src->pixel, dst->pixel, dst->pitch * dst->h);

	if (pixfmt == IPIX_FMT_G8 || pixfmt == IPIX_FMT_A8) {
		retval = ibitmap_filter_8(dst, src, filter);
	}	else {
		retval = ibitmap_filter_32(dst, src, filter);
	}

	ibitmap_release(src);

#ifdef __x86__
	if (X86_FEATURE(X86_FEATURE_MMX)) {
		immx_emms();
	}
#endif

	return retval;
}


//---------------------------------------------------------------------
// 取得channel
// dst必须是8位的位图，filter的0,1,2,3代表取得src中的r,g,b,a分量
//---------------------------------------------------------------------
int ibitmap_channel_get(IBITMAP *dst, int dx, int dy, const IBITMAP *src,
	int sx, int sy, int sw, int sh, int channel)
{
	char _buffer[IBITMAP_STACK_BUFFER];
	char *buffer = _buffer;
	IUINT32 *card;
	int pixfmt;
	long size;
#if IWORDS_BIG_ENDIAN
	static const int table[4] = { 1, 2, 3, 0 };
#else
	static const int table[4] = { 2, 1, 0, 3 };
#endif

	if (ibitmap_clipex(dst, &dx, &dy, src, &sx, &sy, &sw, &sh, NULL, 0)) 
		return -1;

	if (dst->bpp != 8) 
		return -2;

	pixfmt = ibitmap_pixfmt_guess(src);
	channel = table[channel & 3];

	if (pixfmt == IPIX_FMT_A8R8G8B8) {
		int y, x;
		for (y = 0; y < sh; y++) {
			IUINT8 *ss = (IUINT8*)(src->line[sy + y]) + 4 * sx;
			IUINT8 *dd = (IUINT8*)(dst->line[dy + y]) + 1 * dx;
			for (x = sw; x > 0; ss += 4, dd++, x--) {
				dd[0] = ss[channel];
			}
		}
	}	
	else {
		const iColorIndex *index = (const iColorIndex*)src->extra;
		iFetchProc fetch;
		int y, x;
		size = sizeof(IUINT32) * sw;
		if (size > IBITMAP_STACK_BUFFER) {
			buffer = (char*)malloc(size);
			if (buffer == NULL) return -3;
		}
		card = (IUINT32*)buffer;
		fetch = ipixel_get_fetch(pixfmt, 0);
		for (y = 0; y < sh; y++) {
			IUINT8 *ss = (IUINT8*)card;
			IUINT8 *dd = (IUINT8*)(dst->line[dy + y]) + 1 * dx;
			fetch(src->line[sy + y], sx, sw, card, index);
			for (x = sw; x > 0; ss += 4, dd++, x--) {
				dd[0] = ss[channel];
			}
		}
		if (buffer != _buffer) free(buffer);
	}

	return 0;
}


//---------------------------------------------------------------------
// 设置channel
// src必须是8位的位图，filter的0,1,2,3代表设置dst中的r,g,b,a分量
//---------------------------------------------------------------------
int ibitmap_channel_set(IBITMAP *dst, int dx, int dy, const IBITMAP *src,
	int sx, int sy, int sw, int sh, int channel)
{
	char _buffer[IBITMAP_STACK_BUFFER];
	char *buffer = _buffer;
	IUINT32 *card;
	int pixfmt;
	long size;
#if IWORDS_BIG_ENDIAN
	static const int table[4] = { 1, 2, 3, 0 };
#else
	static const int table[4] = { 2, 1, 0, 3 };
#endif

	if (ibitmap_clipex(dst, &dx, &dy, src, &sx, &sy, &sw, &sh, NULL, 0)) 
		return -1;

	if (src->bpp != 8) 
		return -2;

	pixfmt = ibitmap_pixfmt_guess(dst);
	channel = table[channel & 3];

	if (pixfmt == IPIX_FMT_A8R8G8B8) {
		int y, x;
		for (y = 0; y < sh; y++) {
			IUINT8 *ss = (IUINT8*)(src->line[sy + y]) + 1 * sx;
			IUINT8 *dd = (IUINT8*)(dst->line[dy + y]) + 4 * dx;
			for (x = sw; x > 0; ss++, dd += 4, x--) {
				dd[channel] = ss[0];
			}
		}
	}	
	else {
		const iColorIndex *index = (const iColorIndex*)src->extra;
		iFetchProc fetch;
		iStoreProc store;
		int x, y;
		if (index == NULL) index = _ipixel_src_index;
		size = sizeof(IUINT32) * sw;
		if (size > IBITMAP_STACK_BUFFER) {
			buffer = (char*)malloc(size);
			if (buffer == NULL) return -3;
		}
		card = (IUINT32*)buffer;
		fetch = ipixel_get_fetch(pixfmt, 0);
		store = ipixel_get_store(pixfmt, 0);
		for (y = 0; y < sh; y++) {
			IUINT8 *ss = (IUINT8*)(src->line[sy + y]) + 1 * sx;
			IUINT8 *dd = (IUINT8*)card;
			fetch(dst->line[dy + y], dx, sw, card, index);
			for (x = sw; x > 0; ss++, dd += 4, x--) {
				dd[channel] = ss[0];
			}
			store(dst->line[dy + y], card, dx, sw, index);
		}
		if (buffer != _buffer) free(buffer);
	}

	return 0;
}


//---------------------------------------------------------------------
// 图像更新：按照从上到下顺序，每条扫描线调用一次updater
//---------------------------------------------------------------------
int ibitmap_update(IBITMAP *dst, const IRECT *bound, 
	iBitmapUpdate updater, int readonly, void *user)
{
	unsigned char _buffer[IBITMAP_STACK_BUFFER];
	unsigned char *buffer = _buffer;
	int cl, ct, cr, cb, cw, ch;
	int fmt, j;
	long size;

	if (bound == NULL) {
		cl = ct = 0;
		cr = (int)dst->w;
		cb = (int)dst->h;
	}	else {
		cl = bound->left;
		ct = bound->top;
		cr = bound->right;
		cb = bound->bottom;
		if (cl < 0) cl = 0;
		if (ct < 0) ct = 0;
		if (cr > (int)dst->w) cr = (int)dst->w;
		if (cb > (int)dst->h) cb = (int)dst->h;
	}

	cw = cr - cl;
	ch = cb - ct;
	size = cw * 4;

	if (cw <= 0 || ch <= 0)
		return -1;

	fmt = ibitmap_pixfmt_guess(dst);

	if (size > IBITMAP_STACK_BUFFER && fmt != IPIX_FMT_A8R8G8B8) {
		buffer = (unsigned char*)malloc(size);
	}

	if (buffer) {
		iFetchProc fetch;
		iStoreProc store;
		iColorIndex *index;
		fetch = ipixel_get_fetch(fmt, 0);
		store = ipixel_get_store(fmt, 0);
	
		index = (iColorIndex*)(dst->extra);
		if (index == NULL) index = _ipixel_src_index;

		for (j = 0; j < ch; j++) {
			int y = j + ct;
			if (fmt == IPIX_FMT_A8R8G8B8) {
				IUINT32 *card = (IUINT32*)dst->line[y] + cl;
				int retval;
				retval = updater(cl, y, cw, card, user);
				if (retval < 0) return retval;
			}	else {
				IUINT32 *card = (IUINT32*)buffer;
				int retval;
				fetch(dst->line[y], cl, cw, card, index);
				retval = updater(cl, y, cw, card, user);
				if (retval < 0) return retval;
				if (readonly == 0) {
					store(dst->line[y], card, cl, cw, index);
				}
			}
		}
	}

	if (buffer != _buffer) {
		free(buffer);
	}

	return 0;
}



//=====================================================================
// 基础特效
//=====================================================================
IBITMAP *ibitmap_effect_drop_shadow(const IBITMAP *src, int dir, int level)
{
	static short f0[9] = { 16, 16, 16,  16, 128, 16,  16, 16, 16 };
	IBITMAP *alpha;
	short *filter;
	int i;

	alpha = ibitmap_create((int)src->w, (int)src->h, 8);
	if (alpha == NULL) return NULL;

	ibitmap_pixfmt_set(alpha, IPIX_FMT_A8);
	ibitmap_channel_get(alpha, 0, 0, src, 0, 0, (int)src->w, (int)src->h, 3);

	filter = f0;
	
	for (i = 0; i < level; i++) 
		ibitmap_filter(alpha, filter);

	return alpha;
}


static const IINT32 g_stack_blur8_mul[255] = {
        512,512,456,512,328,456,335,512,405,328,271,456,388,335,292,512,
        454,405,364,328,298,271,496,456,420,388,360,335,312,292,273,512,
        482,454,428,405,383,364,345,328,312,298,284,271,259,496,475,456,
        437,420,404,388,374,360,347,335,323,312,302,292,282,273,265,512,
        497,482,468,454,441,428,417,405,394,383,373,364,354,345,337,328,
        320,312,305,298,291,284,278,271,265,259,507,496,485,475,465,456,
        446,437,428,420,412,404,396,388,381,374,367,360,354,347,341,335,
        329,323,318,312,307,302,297,292,287,282,278,273,269,265,261,512,
        505,497,489,482,475,468,461,454,447,441,435,428,422,417,411,405,
        399,394,389,383,378,373,368,364,359,354,350,345,341,337,332,328,
        324,320,316,312,309,305,301,298,294,291,287,284,281,278,274,271,
        268,265,262,259,257,507,501,496,491,485,480,475,470,465,460,456,
        451,446,442,437,433,428,424,420,416,412,408,404,400,396,392,388,
        385,381,377,374,370,367,363,360,357,354,350,347,344,341,338,335,
        332,329,326,323,320,318,315,312,310,307,304,302,299,297,294,292,
        289,287,285,282,280,278,275,273,271,269,267,265,263,261,259
};

static const IINT32 g_stack_blur8_shr[255] = {
          9, 11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 
         17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 
         19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20,
         20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21,
         21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
         21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 
         22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
         22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 
         23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
         23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
         23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 
         23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 
         24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
         24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
         24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
         24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24
};


void ipixel_stackblur_4(void *src, long pitch, int w, int h, int rx, int ry)
{
	unsigned x, y, xp, yp, i;
	unsigned stack_ptr;
	unsigned stack_start;

	const unsigned char * src_pix_ptr;
	unsigned char * dst_pix_ptr;
	unsigned char * stack_pix_ptr;

	IUINT32 sum_r;
	IUINT32 sum_g;
	IUINT32 sum_b;
	IUINT32 sum_a;
	IUINT32 sum_in_r;
	IUINT32 sum_in_g;
	IUINT32 sum_in_b;
	IUINT32 sum_in_a;
	IUINT32 sum_out_r;
	IUINT32 sum_out_g;
	IUINT32 sum_out_b;
	IUINT32 sum_out_a;

	IUINT32 wm  = (IUINT32)w - 1;
	IUINT32 hm  = (IUINT32)h - 1;

	IUINT32 div;
	IUINT32 mul_sum;
	IUINT32 shr_sum;

	IUINT32 stack[512];

	if (rx > 0) {
		if (rx > 254) rx = 254;
		div = rx * 2 + 1;
		mul_sum = g_stack_blur8_mul[rx];
		shr_sum = g_stack_blur8_shr[rx];

		for (y = 0; y < (IUINT32)h; y++) {
			sum_r = 
			sum_g = 
			sum_b = 
			sum_a = 
			sum_in_r = 
			sum_in_g = 
			sum_in_b = 
			sum_in_a = 
			sum_out_r = 
			sum_out_g = 
			sum_out_b = 
			sum_out_a = 0;

			src_pix_ptr = (unsigned char*)src + y * pitch;

			for (i = 0; i <= (IUINT32)rx; i++) {
				stack_pix_ptr    = (unsigned char*)&stack[i];
				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];
				sum_r           += src_pix_ptr[0] * (i + 1);
				sum_g           += src_pix_ptr[1] * (i + 1);
				sum_b           += src_pix_ptr[2] * (i + 1);
				sum_a           += src_pix_ptr[3] * (i + 1);
				sum_out_r       += src_pix_ptr[0];
				sum_out_g       += src_pix_ptr[1];
				sum_out_b       += src_pix_ptr[2];
				sum_out_a       += src_pix_ptr[3];
			}
			for (i = 1; i <= (IUINT32)rx; i++) {
				if (i <= wm) src_pix_ptr += 4;
				stack_pix_ptr = (unsigned char*)&stack[i + rx];
				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];
				sum_r           += src_pix_ptr[0] * (rx + 1 - i);
				sum_g           += src_pix_ptr[1] * (rx + 1 - i);
				sum_b           += src_pix_ptr[2] * (rx + 1 - i);
				sum_a           += src_pix_ptr[3] * (rx + 1 - i);
				sum_in_r        += src_pix_ptr[0];
				sum_in_g        += src_pix_ptr[1];
				sum_in_b        += src_pix_ptr[2];
				sum_in_a        += src_pix_ptr[3];
			}

			stack_ptr = rx;
			xp = rx;
			if (xp > wm) xp = wm;

			src_pix_ptr = (unsigned char*)src + y * pitch + xp * 4;
			dst_pix_ptr = (unsigned char*)src + y * pitch;

			for (x = 0; x < (IUINT32)w; x++) {
				dst_pix_ptr[0] = (IUINT8)((sum_r * mul_sum) >> shr_sum);
				dst_pix_ptr[1] = (IUINT8)((sum_g * mul_sum) >> shr_sum);
				dst_pix_ptr[2] = (IUINT8)((sum_b * mul_sum) >> shr_sum);
				dst_pix_ptr[3] = (IUINT8)((sum_a * mul_sum) >> shr_sum);
				dst_pix_ptr += 4;

				sum_r -= sum_out_r;
				sum_g -= sum_out_g;
				sum_b -= sum_out_b;
				sum_a -= sum_out_a;

				stack_start = stack_ptr + div - rx;
				if (stack_start >= div) stack_start -= div;
				stack_pix_ptr = (unsigned char*)&stack[stack_start];

				sum_out_r -= stack_pix_ptr[0];
				sum_out_g -= stack_pix_ptr[1];
				sum_out_b -= stack_pix_ptr[2];
				sum_out_a -= stack_pix_ptr[3];

				if(xp < wm) {
					src_pix_ptr += 4;
					++xp;
				}

				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];

				sum_in_r += src_pix_ptr[0];
				sum_in_g += src_pix_ptr[1];
				sum_in_b += src_pix_ptr[2];
				sum_in_a += src_pix_ptr[3];
				sum_r    += sum_in_r;
				sum_g    += sum_in_g;
				sum_b    += sum_in_b;
				sum_a    += sum_in_a;

				++stack_ptr;
				if (stack_ptr >= div) stack_ptr = 0;
				stack_pix_ptr = (unsigned char*)&stack[stack_ptr];

				sum_out_r += stack_pix_ptr[0];
				sum_out_g += stack_pix_ptr[1];
				sum_out_b += stack_pix_ptr[2];
				sum_out_a += stack_pix_ptr[3];
				sum_in_r  -= stack_pix_ptr[0];
				sum_in_g  -= stack_pix_ptr[1];
				sum_in_b  -= stack_pix_ptr[2];
				sum_in_a  -= stack_pix_ptr[3];
			}
		}
	}

	if (ry > 0) {
		if (ry > 254) ry = 254;
		div = ry * 2 + 1;
		mul_sum = g_stack_blur8_mul[ry];
		shr_sum = g_stack_blur8_shr[ry];

		for (x = 0; x < (IUINT32)w; x++) {
			sum_r = 
			sum_g = 
			sum_b = 
			sum_a = 
			sum_in_r = 
			sum_in_g = 
			sum_in_b = 
			sum_in_a = 
			sum_out_r = 
			sum_out_g = 
			sum_out_b = 
			sum_out_a = 0;

			src_pix_ptr = (unsigned char*)src + x * 4;

			for (i = 0; i <= (IUINT32)ry; i++) {
				stack_pix_ptr    = (unsigned char*)&stack[i];
				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];
				sum_r           += src_pix_ptr[0] * (i + 1);
				sum_g           += src_pix_ptr[1] * (i + 1);
				sum_b           += src_pix_ptr[2] * (i + 1);
				sum_a           += src_pix_ptr[3] * (i + 1);
				sum_out_r       += src_pix_ptr[0];
				sum_out_g       += src_pix_ptr[1];
				sum_out_b       += src_pix_ptr[2];
				sum_out_a       += src_pix_ptr[3];
			}
			for (i = 1; i <= (IUINT32)ry; i++) {
				if (i <= hm) src_pix_ptr += pitch; 
				stack_pix_ptr = (unsigned char*)&stack[i + ry];
				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];
				sum_r           += src_pix_ptr[0] * (ry + 1 - i);
				sum_g           += src_pix_ptr[1] * (ry + 1 - i);
				sum_b           += src_pix_ptr[2] * (ry + 1 - i);
				sum_a           += src_pix_ptr[3] * (ry + 1 - i);
				sum_in_r        += src_pix_ptr[0];
				sum_in_g        += src_pix_ptr[1];
				sum_in_b        += src_pix_ptr[2];
				sum_in_a        += src_pix_ptr[3];
			}

			stack_ptr = ry;
			yp = ry;
			if(yp > hm) yp = hm;

			src_pix_ptr = (unsigned char*)src + yp * pitch + x * 4;
			dst_pix_ptr = (unsigned char*)src + x * 4;

			for (y = 0; y < (IUINT32)h; y++) {
				dst_pix_ptr[0] = (IUINT8)((sum_r * mul_sum) >> shr_sum);
				dst_pix_ptr[1] = (IUINT8)((sum_g * mul_sum) >> shr_sum);
				dst_pix_ptr[2] = (IUINT8)((sum_b * mul_sum) >> shr_sum);
				dst_pix_ptr[3] = (IUINT8)((sum_a * mul_sum) >> shr_sum);
				dst_pix_ptr += pitch;

				sum_r -= sum_out_r;
				sum_g -= sum_out_g;
				sum_b -= sum_out_b;
				sum_a -= sum_out_a;

				stack_start = stack_ptr + div - ry;
				if (stack_start >= div) stack_start -= div;

				stack_pix_ptr = (unsigned char*)&stack[stack_start];
				sum_out_r -= stack_pix_ptr[0];
				sum_out_g -= stack_pix_ptr[1];
				sum_out_b -= stack_pix_ptr[2];
				sum_out_a -= stack_pix_ptr[3];

				if (yp < hm) {
					src_pix_ptr += pitch;
					++yp;
				}

				stack_pix_ptr[0] = src_pix_ptr[0];
				stack_pix_ptr[1] = src_pix_ptr[1];
				stack_pix_ptr[2] = src_pix_ptr[2];
				stack_pix_ptr[3] = src_pix_ptr[3];

				sum_in_r += src_pix_ptr[0];
				sum_in_g += src_pix_ptr[1];
				sum_in_b += src_pix_ptr[2];
				sum_in_a += src_pix_ptr[3];
				sum_r    += sum_in_r;
				sum_g    += sum_in_g;
				sum_b    += sum_in_b;
				sum_a    += sum_in_a;

				++stack_ptr;
				if (stack_ptr >= div) stack_ptr = 0;
				stack_pix_ptr = (unsigned char*)&stack[stack_ptr];

				sum_out_r += stack_pix_ptr[0];
				sum_out_g += stack_pix_ptr[1];
				sum_out_b += stack_pix_ptr[2];
				sum_out_a += stack_pix_ptr[3];
				sum_in_r  -= stack_pix_ptr[0];
				sum_in_g  -= stack_pix_ptr[1];
				sum_in_b  -= stack_pix_ptr[2];
				sum_in_a  -= stack_pix_ptr[3];
			}
		}
	}
}

// 图像模糊
void ibitmap_stackblur(IBITMAP *src, int rx, int ry, const IRECT *bound)
{
	int x, y, w, h;
	IRECT rect;

	if (bound == NULL) {
		bound = &rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = (int)src->w;
		rect.bottom = (int)src->h;
	}

	x = bound->left;
	y = bound->top;
	w = bound->right - bound->left;
	h = bound->bottom - bound->top;

	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x + w >= (int)src->w) w = src->w - x;
	if (y + h >= (int)src->h) h = src->h - y;
	if (w <= 0 || h <= 0) return;

	if (src->bpp != 32) {
		IBITMAP *newbmp = ibitmap_create(w, h, 32);
		if (newbmp == NULL) return;
		ibitmap_pixfmt_set(newbmp, IPIX_FMT_A8R8G8B8);
		ibitmap_convert(newbmp, 0, 0, src, x, y, w, h, NULL, 0);
		ipixel_stackblur_4(newbmp->pixel, (long)newbmp->pitch,
			w, h, rx, ry);
		ibitmap_convert(src, x, y, newbmp, 0, 0, w, h, NULL, 0);
		ibitmap_release(newbmp);
		return;
	}

	ipixel_stackblur_4((char*)src->line[y] + x * 4, (long)src->pitch,
		w, h, rx, ry);
}

// 生成阴影
IBITMAP *ibitmap_drop_shadow(const IBITMAP *src, int rx, int ry)
{
	IBITMAP *alpha, *beta;
	alpha = ibitmap_create((int)src->w, (int)src->h, 8);
	if (alpha == NULL) return NULL;
	ibitmap_pixfmt_set(alpha, IPIX_FMT_A8);
	ibitmap_channel_get(alpha, 0, 0, src, 0, 0, (int)src->w, (int)src->h, 3);
	beta = ibitmap_create((int)src->w + rx * 2, (int)src->h + ry * 2, 8);
	if (beta == NULL) {
		ibitmap_release(alpha);
		return NULL;
	}
	ibitmap_pixfmt_set(beta, IPIX_FMT_A8);
	memset(beta->pixel, 0, beta->h * beta->pitch);
	ibitmap_blit(beta, rx, ry, alpha, 0, 0, (int)alpha->w, (int)alpha->h, 0);
	ibitmap_release(alpha);
	ibitmap_stackblur(beta, rx, ry, NULL);
	return beta;
}


// 生成圆角
IBITMAP *ibitmap_round_rect(const IBITMAP *src, int radius, int style)
{
	IBITMAP *alpha, *beta;
	int w = (int)src->w;
	int h = (int)src->h;
	int r = radius * 4;
	int w4 = w * 4;
	int h4 = h * 4;
	int x1 = r;
	int y1 = r;
	int x2 = w4 - r - 1;
	int y2 = h4 - r - 1;
	int i, j;

	alpha = ibitmap_resample(src, NULL, w4, h4, 1);
	if (alpha == NULL) return NULL;

	beta = ibitmap_create(w4, h4, 32);

	if (beta == NULL) {
		ibitmap_release(alpha);
		return NULL;
	}

	ibitmap_pixfmt_set(beta, IPIX_FMT_A8R8G8B8);
	ibitmap_convert(beta, 0, 0, alpha, 0, 0, w4, h4, NULL, 0);
	ibitmap_release(alpha);

	alpha = ibitmap_create(w4, h4 * 2 + 1, 32);

	if (alpha == NULL) {
		ibitmap_release(beta);
		return NULL;
	}

	ibitmap_pixfmt_set(alpha, IPIX_FMT_A8R8G8B8);
	memset(alpha->pixel, 0, alpha->pitch * alpha->h);

	ibitmap_put_circle(alpha, x1, y1, r, 1, NULL, 0xffffffff, 0);
	ibitmap_put_circle(alpha, x1, y2, r, 1, NULL, 0xffffffff, 0);
	ibitmap_put_circle(alpha, x2, y1, r, 1, NULL, 0xffffffff, 0);
	ibitmap_put_circle(alpha, x2, y2, r, 1, NULL, 0xffffffff, 0);

	ibitmap_fill(alpha, 0, y1, w4, h4 - 2 * r, 0xffffffff, 0);
	ibitmap_fill(alpha, x1, 0, w4 - 2 * r, h4, 0xffffffff, 0);

	for (j = 0; j < h4; j++) {
		IUINT8 *srcpix = (IUINT8*)alpha->line[j];
		IUINT8 *dstpix = (IUINT8*)beta->line[j];
		for (i = w4; i > 0; i--, srcpix += 4, dstpix += 4) {
		#if IWORDS_BIG_ENDIAN
			if (srcpix[0] == 0) dstpix[0] = 0;
		#else
			if (srcpix[3] == 0) dstpix[3] = 0;
		#endif
		}
	}

	if (style == 0) {
	}
	else if (style == 1 || style == 2) {
		ipixel_gradient_stop_t stops[3] = {
			{ 0, 0xff606060 }, 
			{ 0x8000, 0xff404040 },
			{ 0xffff, 0xff101010 },
		};
		ipixel_gradient_t gradient = { IBOM_TRANSPARENT, 0, 3, stops };
		ipixel_gradient_walker_t walker;
		if (style == 1) y2 = h4 / 2 - r - 1;
		else y2 = h4 - r - 1;
		memset(alpha->pixel, 0, alpha->pitch * alpha->h);
		ibitmap_put_circle(alpha, x1, y1, r, 1, NULL, 0xffffffff, 0);
		ibitmap_put_circle(alpha, x1, y2, r, 1, NULL, 0xffffffff, 0);
		ibitmap_put_circle(alpha, x2, y1, r, 1, NULL, 0xffffffff, 0);
		ibitmap_put_circle(alpha, x2, y2, r, 1, NULL, 0xffffffff, 0);
		if (style == 1) {
			ibitmap_fill(alpha, 0, y1, w4, h4 / 2 - 2 * r, 0xffffffff, 0);
			ibitmap_fill(alpha, x1, 0, w4 - 2 * r, h4 / 2, 0xffffffff, 0);
		}	else {
			ibitmap_fill(alpha, 0, y1, w4, h4 - 2 * r, 0xffffffff, 0);
			ibitmap_fill(alpha, x1, 0, w4 - 2 * r, h4, 0xffffffff, 0);
		}

		if (style == 2) {
			stops[0].color = 0xff404040;
			stops[1].color = 0xff101010;
			stops[2].color = 0xff000000;
		}
		ipixel_gradient_walker_init(&walker, &gradient);

		for (j = 0; j < h4; j++) {
			IUINT8 *srcpix = (IUINT8*)alpha->line[j];
			IUINT8 *dstpix = (IUINT8*)beta->line[j];
			IUINT32 limit = (style == 1)? (h4 / 2) : (h4);
			IINT64 pos = ((IINT64)j * 0xffff) / limit;
			IUINT32 color = ipixel_gradient_walker_pixel(&walker, pos);
			IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, save;
			IRGBA_FROM_A8R8G8B8(color, r1, g1, b1, a1);
			for (i = w4; i > 0; i--, srcpix += 4, dstpix += 4) {
				if (srcpix[_ipixel_card_alpha] && dstpix[_ipixel_card_alpha]) {
					cc = *(IUINT32*)dstpix;
					IRGBA_FROM_A8R8G8B8(cc, r2, g2, b2, a2);
					save = a2;
					IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2);
					*(IUINT32*)dstpix = IRGBA_TO_A8R8G8B8(r2, g2, b2, save);
				}
			}
		}
	}
	else if (style == 3) {
		ipixel_gradient_stop_t stops[3] = {
			{ 0, 0xff555555 }, 
			{ 0x6000, 0xff303030 },
			{ 0xffff, 0xff101010 },
		};
		ipixel_source_t source;
		ipixel_point_fixed_t p1;
		ipixel_point_fixed_t p2;
		cfixed r1, r2;
		int rr = (w4 + h4) / 2;
		iSpanDrawProc span;
		IUINT32 *card;

		p1.x = cfixed_from_int(w4 / 2);
		p1.y = cfixed_from_int(-h4 * 2 / 4);
		p2 = p1;
		r1 = cfixed_from_int(rr);
		r2 = r1 / 2;
		ipixel_source_init_gradient_radial(&source, &p1, &p2, r2, r1,
			stops, 3);
		ipixel_source_set_overflow(&source, IBOM_TRANSPARENT, 0);
		span = ipixel_get_span_proc(IPIX_FMT_A8R8G8B8, 1, 0);
		card = (IUINT32*)alpha->pixel;

		for (j = 0; j < h4; j++) {
			IUINT8 *dstpix = (IUINT8*)beta->line[j];
			IUINT8 *srcpix = (IUINT8*)card;
			IUINT32 c1, c2, r1, g1, b1, a1, r2, g2, b2, a2;
			ipixel_source_fetch(&source, 0, j, w4, card, NULL);
			for (i = w4; i > 0; srcpix += 4, dstpix += 4, i--) {
				if (dstpix[_ipixel_card_alpha]) {
					c1 = *(IUINT32*)srcpix;
					c2 = *(IUINT32*)dstpix;
					IRGBA_FROM_A8R8G8B8(c1, r1, g1, b1, a1);
					IRGBA_FROM_A8R8G8B8(c2, r2, g2, b2, a2);
					IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2);
					*(IUINT32*)dstpix = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
				}
			}
		}
	}

	ibitmap_release(alpha);

	alpha = ibitmap_resample(beta, NULL, w, h, 0);

	if (alpha) {
		ibitmap_pixfmt_set(alpha, IPIX_FMT_A8R8G8B8);
	}

	ibitmap_release(beta);

	return alpha;
}


static int i_update_trans(int x, int y, int w, IUINT32 *card, void *user)
{
	cfixed *mat = (cfixed*)user;
	cfixed mm[20];
	IINT32 cc, r, g, b, a;
	cfixed r2, g2, b2, a2;
	int i;
	for (i = 0; i < 20; i++) mm[i] = mat[i] >> 6;
	for (; w > 0; w--, card++) {
		cc = card[0];
		IRGBA_FROM_A8R8G8B8(cc, r, g, b, a);
		r2 = r * mm[ 0] + g * mm[ 1] + b * mm[ 2] + a * mm[ 3] + mm[ 4];
		g2 = r * mm[ 5] + g * mm[ 6] + b * mm[ 7] + a * mm[ 8] + mm[ 9];
		b2 = r * mm[10] + g * mm[11] + b * mm[12] + a * mm[13] + mm[14];
		a2 = r * mm[15] + g * mm[16] + b * mm[17] + a * mm[18] + mm[19];
		r = (IINT32)(r2 >> 10);
		g = (IINT32)(g2 >> 10);
		b = (IINT32)(b2 >> 10);
		a = (IINT32)(a2 >> 10);
		if (r < 0) r = 0;
		else if (r > 255) r = 255;
		if (g < 0) g = 0;
		else if (g > 255) g = 255;
		if (b < 0) b = 0;
		else if (b > 255) b = 255;
		if (a < 0) a = 0;
		else if (a > 255) a = 255;
		card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
	}
	return 0;
}

static int i_update_add(int x, int y, int w, IUINT32 *card, void *user)
{
	IUINT32 cc = *(IUINT32*)user;
	IINT32 cr, cg, cb, ca;
	IINT32 c, r, g, b, a;
	IRGBA_FROM_A8R8G8B8(cc, cr, cg, cb, ca);
	for (; w > 0; w--, card++) {
		c = card[0];
		IRGBA_FROM_A8R8G8B8(c, r, g, b, a);
		r = ICLIP_256(r + cr);
		g = ICLIP_256(g + cg);
		b = ICLIP_256(b + cb);
		a = ICLIP_256(a + ca);
		card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
	}
	return 0;
}

static int i_update_sub(int x, int y, int w, IUINT32 *card, void *user)
{
	IUINT32 cc = *(IUINT32*)user;
	IINT32 cr, cg, cb, ca;
	IINT32 c, r, g, b, a;
	IRGBA_FROM_A8R8G8B8(cc, cr, cg, cb, ca);
	for (; w > 0; w--, card++) {
		c = card[0];
		IRGBA_FROM_A8R8G8B8(c, r, g, b, a);
		r = ICLIP_256(r - cr);
		g = ICLIP_256(g - cg);
		b = ICLIP_256(b - cb);
		a = ICLIP_256(a - ca);
		card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
	}
	return 0;
}

static int i_update_mul(int x, int y, int w, IUINT32 *card, void *user)
{
	IUINT32 cc = *(IUINT32*)user;
	IINT32 cr, cg, cb, ca;
	IINT32 c, r, g, b, a;
	IRGBA_FROM_A8R8G8B8(cc, cr, cg, cb, ca);
	for (; w > 0; w--, card++) {
		c = card[0];
		IRGBA_FROM_A8R8G8B8(c, r, g, b, a);
		r = r * cr;
		g = g * cg;
		b = b * cb;
		a = a * ca;
		r = _ipixel_fast_div_255(r);
		g = _ipixel_fast_div_255(g);
		b = _ipixel_fast_div_255(b);
		a = _ipixel_fast_div_255(a);
		card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
	}
	return 0;
}


// 色彩变幻：输入 5 x 5矩阵
void ibitmap_color_transform(IBITMAP *dst, const IRECT *b, const float *t)
{
	cfixed transform[20];
	int i;
	for (i = 0; i < 20; i += 4) {
		cfixed *dst = transform + i;
		const float *src = t + i;
		dst[0] = cfixed_from_float(src[0]);
		dst[1] = cfixed_from_float(src[1]);
		dst[2] = cfixed_from_float(src[2]);
		dst[3] = cfixed_from_float(src[3]);
		dst[4] = cfixed_from_float(src[4]);
	}
	ibitmap_update(dst, b, i_update_trans, 0, transform);
}

// 色彩变幻：加法
void ibitmap_color_add(IBITMAP *dst, const IRECT *b, IUINT32 color)
{
	ibitmap_update(dst, b, i_update_add, 0, &color);
}

// 色彩变幻：减法
void ibitmap_color_sub(IBITMAP *dst, const IRECT *b, IUINT32 color)
{
	ibitmap_update(dst, b, i_update_sub, 0, &color);
}

// 色彩变幻：乘法
void ibitmap_color_mul(IBITMAP *dst, const IRECT *b, IUINT32 color)
{
	ibitmap_update(dst, b, i_update_mul, 0, &color);
}



//---------------------------------------------------------------------
// 原始作图
//---------------------------------------------------------------------


//---------------------------------------------------------------------
// internal
//---------------------------------------------------------------------
#define _iabs(x)    ( ((x) < 0) ? (-(x)) : (x) )
#define _imin(x, y) ( ((x) < (y)) ? (x) : (y) )
#define _imax(x, y) ( ((x) > (y)) ? (x) : (y) )


//---------------------------------------------------------------------
// this line clipping based heavily off of code from
// http://www.ncsa.uiuc.edu/Vis/Graphics/src/clipCohSuth.c 
//---------------------------------------------------------------------
#define LEFT_EDGE   0x1
#define RIGHT_EDGE  0x2
#define BOTTOM_EDGE 0x4
#define TOP_EDGE    0x8
#define INSIDE(a)   (!a)
#define REJECT(a,b) (a&b)
#define ACCEPT(a,b) (!(a|b))

static inline int _iencode(int x, int y, int left, int top, int right, 
	int bottom)
{
	int code = 0;
	if (x < left)   code |= LEFT_EDGE;
	if (x > right)  code |= RIGHT_EDGE;
	if (y < top)    code |= TOP_EDGE;
	if (y > bottom) code |= BOTTOM_EDGE;
	return code;
}

static inline int _iencode_float(float x, float y, int left, int top, 
	int right, int bottom)
{
	int code = 0;
	if (x < left)   code |= LEFT_EDGE;
	if (x > right)  code |= RIGHT_EDGE;
	if (y < top)    code |= TOP_EDGE;
	if (y > bottom) code |= BOTTOM_EDGE;
	return code;
}

static inline int _iclipline(int* pts, int left, int top, int right, 
	int bottom)
{
	int x1 = pts[0];
	int y1 = pts[1];
	int x2 = pts[2];
	int y2 = pts[3];
	int code1, code2;
	int draw = 0;
	int swaptmp;
	float m; /*slope*/

	right--;
	bottom--;

	while(1)
	{
		code1 = _iencode(x1, y1, left, top, right, bottom);
		code2 = _iencode(x2, y2, left, top, right, bottom);
		if (ACCEPT(code1, code2)) {
			draw = 1;
			break;
		}
		else if (REJECT(code1, code2))
			break;
		else {
			if (INSIDE(code1)) {
				swaptmp = x2; x2 = x1; x1 = swaptmp;
				swaptmp = y2; y2 = y1; y1 = swaptmp;
				swaptmp = code2; code2 = code1; code1 = swaptmp;
			}
			if (x2 != x1)
				m = (y2 - y1) / (float)(x2 - x1);
			else
				m = 1.0f;
			if (code1 & LEFT_EDGE) {
				y1 += (int)((left - x1) * m);
				x1 = left;
			}
			else if (code1 & RIGHT_EDGE) {
				y1 += (int)((right - x1) * m);
				x1 = right;
			}
			else if(code1 & BOTTOM_EDGE) {
				if(x2 != x1)
					x1 += (int)((bottom - y1) / m);
				y1 = bottom;
			}
			else if (code1 & TOP_EDGE) {
				if (x2 != x1)
					x1 += (int)((top - y1) / m);
				y1 = top;
			}
		}
	}
	if (draw) {
		pts[0] = x1; pts[1] = y1;
		pts[2] = x2; pts[3] = y2;
	}
	return draw;
}

#define ipaint_push_pixel(paint, x, y) do { \
		*paint++ = (IUINT32)x; \
		*paint++ = (IUINT32)y; \
	}	while (0)


#define ipaint_get_count(paint, base) \
	(((long)((IINT32*)paint - (IINT32*)base)) >> 1)


int ibitmap_put_line_low(IBITMAP *dst, int x1, int y1, int x2, int y2,
	IUINT32 color, int additive, const IRECT *clip, void *workmem)
{
	IUINT32 *paint;
	int cl, ct, cr, cb;
	int x, y, p, n, tn;
	int pts[4];
	int count;

	if (workmem == NULL) {
		int dx = (x1 < x2)? (x2 - x1) : (x1 - x2);
		int dy = (y1 < y2)? (y2 - y1) : (y1 - y2);
		long ds1 = (dst->w + dst->h);
		long ds2 = (dx + dy);
		long size = (ds1 < ds2)? ds1 : ds2;
		return (size + 2) * 4 * sizeof(IUINT32);
	}

	if (clip) {
		cl = clip->left;
		ct = clip->top;
		cr = clip->right - 1;
		cb = clip->bottom - 1;
	}	else {
		cl = 0;
		ct = 0;
		cr = (int)dst->w - 1;
		cb = (int)dst->h - 1;
	}

	if (cr < cl || cb < ct) 
		return -1;

	pts[0] = x1; 
	pts[1] = y1;
	pts[2] = x2;
	pts[3] = y2;

	if (_iclipline(pts, cl, ct, cr, cb) == 0 || (color >> 24) == 0) 
		return -2;

	x1 = pts[0];
	y1 = pts[1];
	x2 = pts[2];
	y2 = pts[3];

	paint = (IUINT32*)workmem;
	
	if (y1 == y2) {	
		if (x1 > x2) x = x1, x1 = x2, x2 = x;
		for (x = x1; x <= x2; x++) ipaint_push_pixel(paint, x, y1);
	}
	else if (x1 == x2)	{  
		if (y1 > y2) y = y2, y2 = y1, y1 = y;
		for (y = y1; y <= y2; y++) ipaint_push_pixel(paint, x1, y);
	}
	else if (_iabs(y2 - y1) <= _iabs(x2 - x1)) {
		if ((y2 < y1 && x2 < x1) || (y1 <= y2 && x1 > x2)) {
			x = x2, y = y2, x2 = x1, y2 = y1, x1 = x, y1 = y;
		}
		if (y2 >= y1 && x2 >= x1) {
			x = x2 - x1, y = y2 - y1;
			p = 2 * y, n = 2 * x - 2 * y, tn = x;
			for (; x1 <= x2; x1++) {
				if (tn >= 0) tn -= p;
				else tn += n, y1++;
				ipaint_push_pixel(paint, x1, y1);
			}
		}	else {
			x = x2 - x1; y = y2 - y1;
			p = -2 * y; n = 2 * x + 2 * y; tn = x;
			for (; x1 <= x2; x1++) {
				if (tn >= 0) tn -= p;
				else tn += n, y1--;
				ipaint_push_pixel(paint, x1, y1);
			}
		}
	}	else {
		x = x1; x1 = y2; y2 = x; y = y1; y1 = x2; x2 = y;
		if ((y2 < y1 && x2 < x1) || (y1 <= y2 && x1 > x2)) {
			x = x2, y = y2, x2 = x1, x1 = x, y2 = y1, y1 = y;
		}
		if (y2 >= y1 && x2 >= x1) {
			x = x2 - x1; y = y2 - y1;
			p = 2 * y; n = 2 * x - 2 * y; tn = x;
			for (; x1 <= x2; x1++)  {
				if (tn >= 0) tn -= p;
				else tn += n, y1++;
				ipaint_push_pixel(paint, y1, x1);
			}
		}	else	{
			x = x2 - x1; y = y2 - y1;
			p = -2 * y; n = 2 * x + 2 * y; tn = x;
			for (; x1 <= x2; x1++) {
				if (tn >= 0) tn -= p;
				else { tn += n; y1--; }
				ipaint_push_pixel(paint, y1, x1);
			}
		}
	}

	count = (int)(paint - (IUINT32*)workmem) / 2;

	ibitmap_draw_pixel_list_sc(dst, (IUINT32*)workmem, count, color,
		additive);

	return 0;
}


int ibitmap_put_line(IBITMAP *dst, int x1, int y1, int x2, int y2,
	IUINT32 color, int additive, const IRECT *clip)
{
	char _buffer[IBITMAP_STACK_BUFFER];
	char *buffer = _buffer;
	int dx = (x1 < x2)? (x2 - x1) : (x1 - x2);
	int dy = (y1 < y2)? (y2 - y1) : (y1 - y2);
	long ds1 = (dst->w + dst->h);
	long ds2 = (dx + dy);
	long size = (ds1 < ds2)? ds1 : ds2;
	int retval = 0;

	size = (size + 2) * 4 * sizeof(IUINT32);
	if (size > IBITMAP_STACK_BUFFER) {
		buffer = (char*)malloc(size);
		if (buffer == NULL) return -100;
	}

	retval = ibitmap_put_line_low(dst, x1, y1, x2, y2, 
		color, additive, clip, buffer);

	if (buffer != _buffer) free(buffer);

	return retval;
}


void ibitmap_put_circle_low(IBITMAP *dst, int x0, int y0, int r, int fill,
	const IRECT *clip, IUINT32 color, int additive, void *workmem, long size)
{
	IINT32 *buffer = (IINT32*)workmem;
	IINT32 *paint = buffer;
	int cx = 0, cy = r;	
	int df = 1 - r, d_e = 3, d_se = -2 * r + 5;	
	int x = 0, y = 0, xmax, tn;
	long limit = size / 8;
	long flush = limit * 3 / 4;
	int cl, ct, cr, cb;

	if (flush < 32) return;
	if (clip == NULL) {
		cl = 0;
		ct = 0;
		cr = (int)dst->w;
		cb = (int)dst->h;
	}	else {
		cl = clip->left;
		ct = clip->top;
		cr = clip->right;
		cb = clip->bottom;
	}

	paint = buffer;

	#define ipaint_push_clip_dot(x, y) do { \
			int xx = (x), yy = (y); \
			if (xx >= cl && yy >= ct && xx < cr && yy < cb) { \
				ipaint_push_pixel(paint, xx, yy); \
			} \
		}	while (0)
	
	#define ipaint_flush_limit(maxcount) do { \
			int count = ipaint_get_count(paint, buffer); \
			if (count > maxcount) { \
				ibitmap_draw_pixel_list_sc(dst, (IUINT32*)buffer, \
					count, color, additive); \
				paint = buffer; \
			} \
		}	while (0)
	
	#define ipaint_hline_clip(x, y, w) do { \
			int yy = (y); \
			if (yy >= ct && yy < cb) { \
				int xl = (x), xr = (x) + (w); \
				if (xl < cl) xl = cl; \
				if (xr > cr) xr = cr; \
				if (xl < xr) { \
					hline(dst->line[y], x, xr - xl, color, NULL, index); \
				} \
			} \
		}	while (0)

	if (fill == 0) {
		y = r; x = 0; xmax = (int)(r * 0.70710678f + 0.5f);
		tn = (1 - r * 2);
		while (x <= xmax) {
			if (tn >= 0) {
				tn += (6 + ((x - y) << 2));	
				y--;
			} else tn += ((x << 2) + 2);
			ipaint_push_clip_dot(x0 + y, y0 + x);
			ipaint_push_clip_dot(x0 + x, y0 + y);
			ipaint_push_clip_dot(x0 - x, y0 + y);
			ipaint_push_clip_dot(x0 - y, y0 + x);
			ipaint_push_clip_dot(x0 - y, y0 - x);
			ipaint_push_clip_dot(x0 - x, y0 - y);
			ipaint_push_clip_dot(x0 + x, y0 - y);
			ipaint_push_clip_dot(x0 + y, y0 - x);
			ipaint_flush_limit(flush);
			x++;
		}
		ipaint_flush_limit(0);
	}	else {
		int pixfmt = ibitmap_pixfmt_guess(dst);
		iHLineDrawProc hline = ipixel_get_hline_proc(pixfmt, additive, 0);
		iColorIndex *index = ibitmap_index_get(dst);

		if (index == NULL) index = _ipixel_dst_index;
		if (hline == NULL) return;

		do {
			ipaint_hline_clip(x0 - cy, y0 - cx, (cy << 1) + 1);
			if (cx) {
				ipaint_hline_clip(x0 - cy, y0 + cx, (cy << 1) + 1);
			}
			if (df < 0) df += d_e, d_e += 2, d_se += 2;
			else {
				if (cx != cy) {	
					ipaint_hline_clip(x0 - cx, y0 - cy, (cx << 1) + 1);
					if (cy) {
						ipaint_hline_clip(x0 - cx, y0 + cy, (cx << 1) + 1);
					}
				}
				df += d_se, d_e += 2, d_se += 4;
				cy--;
			}
			cx++;
		}	while (cx <= cy);
	}

	#undef ipaint_push_clip_dot
	#undef ipaint_flush_limit
	#undef ipaint_hline_clip
}

void ibitmap_put_circle(IBITMAP *dst, int x0, int y0, int r, int fill,
	const IRECT *clip, IUINT32 color, int additive)
{
	char buffer[IBITMAP_STACK_BUFFER];
	ibitmap_put_circle_low(dst, x0, y0, r, fill, clip, color, additive, 
		buffer, IBITMAP_STACK_BUFFER);
}


//---------------------------------------------------------------------
// 缓存管理
//---------------------------------------------------------------------

// 初始化缓存
void cvector_init(struct CVECTOR *vector)
{
	vector->data = NULL;
	vector->size = 0;
	vector->block = 0;
}

// 销毁缓存
void cvector_destroy(struct CVECTOR *vector)
{
	if (vector->data) free(vector->data);
	vector->data = NULL;
	vector->size = 0;
	vector->block = 0;
}

// 改变缓存大小
int cvector_resize(struct CVECTOR *vector, size_t size)
{
	unsigned char*lptr;
	size_t block, min;
	size_t nblock;

	if (vector == NULL) return -1;

	if (size >= vector->size && size <= vector->block) { 
		vector->size = size; 
		return 0; 
	}

	if (size == 0) {
		if (vector->block > 0) {
			free(vector->data);
			vector->block = 0;
			vector->data = NULL;
			vector->size = 0;
		}
		return 0;
	}

	for (nblock = sizeof(char*); nblock < size; ) nblock <<= 1;
	block = nblock;

	if (block == vector->block) { 
		vector->size = size; 
		return 0; 
	}

	if (vector->block == 0 || vector->data == NULL) {
		vector->data = (unsigned char*)malloc(block);
		if (vector->data == NULL) return -1;
		vector->size = size;
		vector->block = block;
	}   else {
		lptr = (unsigned char*)malloc(block);
		if (lptr == NULL) return -1;

		min = (vector->size <= size)? vector->size : size;
		memcpy(lptr, vector->data, (size_t)min);
		free(vector->data);

		vector->data = lptr;
		vector->size = size;
		vector->block = block;
	}

	return 0;
}

// 添加数据
int cvector_push(struct CVECTOR *vector, const void *data, size_t size)
{
	size_t offset = vector->size;
	if (cvector_resize(vector, vector->size + size) != 0) 
		return -1;
	if (data) {
		memcpy(vector->data + offset, data, size);
	}
	return 0;
}


//---------------------------------------------------------------------
// 原始作图
//---------------------------------------------------------------------
static struct CVECTOR ipixel_scratch = { 0, 0, 0 };

// 批量填充梯形
int ipixel_render_traps(IBITMAP *dst, const ipixel_trapezoid_t *traps, 
	int ntraps, IBITMAP *alpha, const ipixel_source_t *src, int isadd,
	const IRECT *clip, struct CVECTOR *scratch)
{
	iHLineDrawProc hline;
	ipixel_span_t *spans;
	iSpanDrawProc draws;
	IRECT bound, rect;
	int cx, cy, ch, cw, n, i, fmt;
	iColorIndex *index;
	IUINT32 color;
	IUINT32 *card;
	long size;

	if (ipixel_trapezoid_bound(traps, ntraps, &bound) == 0)
		return -1;

	if (clip == NULL) {
		clip = &rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = (int)dst->w;
		rect.bottom = (int)dst->h;
	}

	ipixel_rect_intersection(&bound, clip);

	if (bound.right - bound.left > (int)alpha->w) 
		bound.right = bound.left + (int)alpha->w;

	if (bound.bottom - bound.top > (int)alpha->h)
		bound.bottom = bound.top + (int)alpha->h;

	if (bound.right <= bound.left || bound.bottom <= bound.top)
		return -2;

	cx = bound.left;
	cy = bound.top;
	ch = bound.bottom - bound.top;
	cw = bound.right - bound.left;

	ipixel_rect_offset(&bound, -cx, -cy);
	
	if (scratch == NULL) scratch = &ipixel_scratch;
	size = (ch + 5) * sizeof(ipixel_span_t) * 2;

	if (src->type != IPIXEL_SOURCE_SOLID) 
		size += ch * sizeof(IUINT32);
	
	if (size > (long)scratch->size) {
		if (cvector_resize(scratch, size) != 0)
			return -3;
	}

	if (src->type == IPIXEL_SOURCE_SOLID) {
		spans = (ipixel_span_t*)scratch->data;
		card = NULL;
		color = src->source.solid.color;
	}	else {
		spans = (ipixel_span_t*)(scratch->data + sizeof(IUINT32) * cw);
		card = (IUINT32*)scratch->data;
		color = 0;
	}

	n = ibitmap_imode(dst, subpixel);

	fmt = ibitmap_pixfmt_guess(dst);
	size = ipixel_trapezoid_spans(traps, ntraps, n, spans, -cx, -cy, &bound);
	hline = ipixel_get_hline_proc(fmt, isadd, 0);
	draws = ipixel_get_span_proc(fmt, isadd, 0);

	for (i = 0; i < (int)size; i++) {
		int y = spans[i].y;
		IUINT8 *mask = (IUINT8*)alpha->line[y] + spans[i].x;
		memset(mask, 0, spans[i].w);
	}

	ipixel_raster_traps(alpha, traps, ntraps, -cx, -cy, &bound);

	index = (iColorIndex*)dst->extra;
	if (index == NULL) index = _ipixel_dst_index;

	for (i = 0; i < (int)size; i++) {
		int y = spans[i].y;
		IUINT8 *mask = (IUINT8*)alpha->line[y] + spans[i].x;
		int dx = cx + spans[i].x;
		int dy = cy + spans[i].y;
		int dw = spans[i].w;
		int retval;
		if (card == NULL) {
			hline(dst->line[dy], dx, dw, color, mask, index);
		}	else {
			assert(dw <= cw);
			retval = ipixel_source_fetch(src, dx, dy, dw, card, mask);
			if (retval != 0) continue;
			draws(dst->line[dy], dx, dw, card, mask, index);
		}
	}

	return 0;
}


// 绘制多边形
int ipixel_render_polygon(IBITMAP *dst, const ipixel_point_fixed_t *pts,
	int npts, IBITMAP *alpha, const ipixel_source_t *src, int isadditive,
	const IRECT *clip, struct CVECTOR *scratch)
{
	char _buffer[IBITMAP_STACK_BUFFER];
	char *buffer = _buffer;
	ipixel_trapezoid_t *traps;
	int ntraps;
	long size;
	int retval;

	if (npts < 3) return -10;

	size = sizeof(ipixel_trapezoid_t) * npts * 2;
	if (size > IBITMAP_STACK_BUFFER) {
		buffer = (char*)malloc(size);
		if (buffer == NULL) return -20;
	}

	traps = (ipixel_trapezoid_t*)buffer;
	size = sizeof(ipixel_point_fixed_t) * npts;

	if (scratch == NULL) scratch = &ipixel_scratch;

	if (size > (long)scratch->size) {
		if (cvector_resize(scratch, size) != 0) {
			retval = -30;
			goto exit_label;
		}
	}

	ntraps = ipixel_traps_from_polygon(traps, pts, npts, 0, scratch->data);

	if (ntraps == 0) {
		ntraps = ipixel_traps_from_polygon(traps, pts, npts, 1, 
			scratch->data);
		if (ntraps == 0) {
			retval = -40;
			goto exit_label;
		}
	}

	retval = ipixel_render_traps(dst, traps, ntraps, alpha, src, 
		isadditive, clip, scratch);

exit_label:
	if (buffer != _buffer) free(buffer);

	return retval;
}



//---------------------------------------------------------------------
// 作图接口
//---------------------------------------------------------------------
static void ipaint_init(ipaint_t *paint)
{
	paint->image = NULL;
	paint->alpha = NULL;
	paint->additive = 0;
	paint->pts = NULL;
	paint->npts = 0;
	paint->color = 0xff000000;
	paint->text_color = 0xff000000;
	paint->text_backgrnd = 0xffffffff;
	paint->npts = 0;
	paint->line_width = 1.0;
	cvector_init(&paint->scratch);
	cvector_init(&paint->points);
	cvector_init(&paint->pointf);
	cvector_init(&paint->gradient);
	ipixel_source_init_solid(&paint->source, 0xff444444);
	paint->current = &paint->source;
}

int ipaint_set_image(ipaint_t *paint, IBITMAP *image)
{
	int subpixel = 0;

	paint->image = image;

	if (paint->alpha) {
		subpixel = ibitmap_imode(paint->alpha, subpixel);
		if (paint->alpha->w < image->w || paint->alpha->h < image->h ||
			paint->alpha->w > image->w * 4) {
			ibitmap_release(paint->alpha);
			paint->alpha = NULL;
		}
	}

	if (paint->alpha == NULL) {
		paint->alpha = ibitmap_create((int)image->w, (int)image->h, 8);
		if (paint->alpha == NULL) 
			return -1;
		ibitmap_pixfmt_set(paint->alpha, IPIX_FMT_A8);
	}

	ibitmap_imode(paint->alpha, subpixel) = subpixel;

	paint->npts = 0;
	ipaint_set_clip(paint, NULL);

	return 0;
}

ipaint_t *ipaint_create(IBITMAP *image)
{
	ipaint_t *paint;
	paint = (ipaint_t*)malloc(sizeof(ipaint_t));
	if (paint == NULL) return NULL;
	ipaint_init(paint);
	if (ipaint_set_image(paint, image) != 0) {
		free(paint);
		return NULL;
	}
	return paint;
}

void ipaint_destroy(ipaint_t *paint)
{
	if (paint == NULL) return;
	if (paint->alpha) ibitmap_release(paint->alpha);
	paint->alpha = NULL;
	cvector_destroy(&paint->scratch);
	cvector_destroy(&paint->points);
	cvector_destroy(&paint->pointf);
	cvector_destroy(&paint->gradient);
	free(paint);
}


static inline int ipaint_point_push(ipaint_t *paint, double x, double y)
{
	long size = sizeof(ipixel_point_t) * (paint->npts + 1);
	if (size > (long)paint->points.size) {
		if (size < 256) size = 256;
		if (cvector_resize(&paint->points, size) != 0)
			return -1;
		paint->pts = (ipixel_point_t*)paint->points.data;
	}
	paint->pts[paint->npts].x = x;
	paint->pts[paint->npts].y = y;
	paint->npts++;
	return 0;
}

int ipaint_point_append(ipaint_t *paint, const ipixel_point_t *pts, int n)
{
	long size = sizeof(ipixel_point_t) * (n + paint->npts);
	if (size > (long)paint->points.size) {
		if (cvector_resize(&paint->points, size) != 0)
			return -1;
		paint->pts = (ipixel_point_t*)paint->points.data;
	}
	memcpy(paint->pts + paint->npts, pts, n * sizeof(ipixel_point_t));
	paint->npts += n;
	return 0;
}

void ipaint_point_reset(ipaint_t *paint)
{
	paint->npts = 0;
}


static int ipaint_point_convert(ipaint_t *paint)
{
	ipixel_point_fixed_t *dst = (ipixel_point_fixed_t*)paint->pointf.data;
	ipixel_point_t *src = (ipixel_point_t*)paint->points.data;
	int npts = paint->npts;
	int i;

	for (i = 0; i < npts; i++) {
		dst->x = cfixed_from_double(src->x);
		dst->y = cfixed_from_double(src->y);
		dst++;
		src++;
	}

	return 0;
}

static ipixel_point_fixed_t *ipaint_point_to_fixed(ipaint_t *paint)
{
	ipixel_point_fixed_t *pts;
	long size;

	if (paint->npts == 0) 
		return NULL;

	size = sizeof(ipixel_point_fixed_t) * paint->npts;

	if (size > (long)paint->pointf.size) {
		if (size < 256) size = 256;
		if (cvector_resize(&paint->pointf, size) != 0)
			return NULL;
	}

	pts = (ipixel_point_fixed_t*)paint->pointf.data;

	if (ipaint_point_convert(paint) != 0)
		return NULL;

	return pts;
}

int ipaint_draw_primitive(ipaint_t *paint)
{
	ipixel_point_fixed_t *pts;
	int retval;

	if (paint->npts < 3) return -10;
	pts = ipaint_point_to_fixed(paint);

	if (pts == NULL) return -20;

	retval = ipixel_render_polygon(paint->image, pts, paint->npts, 
		paint->alpha, paint->current, paint->additive, &paint->clip, 
		&paint->scratch);

	return retval;
}

int ipaint_draw_traps(ipaint_t *paint, ipixel_trapezoid_t *traps, int ntraps)
{
	int retval;
	retval = ipixel_render_traps(paint->image, traps, ntraps, paint->alpha,
		paint->current, paint->additive, &paint->clip, &paint->scratch);
	return retval;
}

// 色彩源：设置当前色彩源
void ipaint_source_set(ipaint_t *paint, ipixel_source_t *source)
{
	paint->current = (source)? source : &(paint->source);
}

void ipaint_set_color(ipaint_t *paint, IUINT32 color)
{
	paint->color = color;
	paint->source.source.solid.color = color;
	paint->current = &paint->source;
}

void ipaint_set_clip(ipaint_t *paint, const IRECT *clip)
{
	if (clip != NULL) {
		IRECT size;
		ipixel_rect_set(&size, 0, 0, (int)paint->image->w, (int)paint->image->h);
		ipixel_rect_intersection(&size, clip);
		paint->clip = size;
	}	else {
		paint->clip.left = 0;
		paint->clip.top = 0;
		paint->clip.right = (int)paint->image->w;
		paint->clip.bottom = (int)paint->image->h;
	}
}

void ipaint_text_color(ipaint_t *paint, IUINT32 color)
{
	paint->text_color = color;
}

void ipaint_text_background(ipaint_t *paint, IUINT32 color)
{
	paint->text_backgrnd = color;
}

void ipaint_anti_aliasing(ipaint_t *paint, int level)
{
	if (level < 0) level = 0;
	if (level > 2) level = 2;
	switch (level)
	{
	case 0:
		ibitmap_imode(paint->alpha, subpixel) = IPIXEL_SUBPIXEL_1;
		break;
	case 1:
		ibitmap_imode(paint->alpha, subpixel) = IPIXEL_SUBPIXEL_4;
		break;
	case 2:
		ibitmap_imode(paint->alpha, subpixel) = IPIXEL_SUBPIXEL_8;
		break;
	}
}

int ipaint_draw_polygon(ipaint_t *paint, const ipixel_point_t *pts, int n)
{
	ipaint_point_reset(paint);
	if (ipaint_point_append(paint, pts, n) != 0)
		return -100;
	return ipaint_draw_primitive(paint);
}


int ipaint_draw_line(ipaint_t *paint, double x1, double y1, double x2,
	double y2)
{
	double dx, dy, dist, nx, ny, hx, hy, half;
	ipaint_point_reset(paint);

	dx = x2 - x1;
	dy = y2 - y1;

	dist = sqrt(dx * dx + dy * dy);
	if (dist == 0.0) return -1;
	if (paint->line_width <= 0.01) return -1;

	dist = 1.0 / dist;
	nx = dx * dist;
	ny = dy * dist;
	hx = -ny * paint->line_width * 0.5;
	hy = nx * paint->line_width * 0.5;
	half = paint->line_width * 0.5;
	
	if (paint->line_width < 1.5) {
		ipaint_point_push(paint, x1 - hx, y1 - hy);
		ipaint_point_push(paint, x1 + hx, y1 + hy);
		ipaint_point_push(paint, x2 + hx, y2 + hy);
		ipaint_point_push(paint, x2 - hx, y2 - hy);
	}	else {
		int count = (int)half + 1;
		ipixel_point_t *pts;
		double theta, start;
		long size;
		int i;
		if (count > 8) count = 8;
		if (half > 30.0) {
			if (half < 70.0) count = 24;
			else if (half < 150) count = 48;
			else if (half < 250) count = 60;
			else count = 70;
		}
		size = count * sizeof(ipixel_point_t);
		if (size > (long)paint->scratch.size) {
			if (cvector_resize(&paint->scratch, size) != 0)
				return -1;
		}
		pts = (ipixel_point_t*)paint->scratch.data;

		ipaint_point_push(paint, x1 + hx, y1 + hy);

		start = atan2(hy, hx);
		theta = 3.1415926535 / (count + 2);
		start += theta;

		//printf("
		for (i = 0; i < count; i++) {
			double cx = cos(start) * half;
			double cy = sin(start) * half;
			ipaint_point_push(paint, x1 + cx, y1 + cy);
			pts[i].x = -cx;
			pts[i].y = -cy;
			start += theta;
		}

		ipaint_point_push(paint, x1 - hx, y1 - hy);
		ipaint_point_push(paint, x2 - hx, y2 - hy);

		for (i = 0; i < count; i++) {
			ipaint_point_push(paint, x2 + pts[i].x, y2 + pts[i].y);
		}

		ipaint_point_push(paint, x2 + hx, y2 + hy);
	}

	ipaint_draw_primitive(paint);

	return 0;
}


void ipaint_line_width(ipaint_t *paint, double width)
{
	paint->line_width = width;
}

int ipaint_draw_ellipse(ipaint_t *paint, double x, double y, double rx,
	double ry)
{
	int rm = (int)((rx > ry)? rx : ry) + 1;
	ipixel_trapezoid_t *traps;
	int count, ntraps, i;
	double theta, dt;
	long size;

	if (rm < 2) count = 4;
	else if (rm < 10) count = 5;
	else if (rm < 50) count = 6;
	else if (rm < 100) count = 7;
	else if (rm < 200) count = 10;
	else count = rm / 100;

	ntraps = (count + 1) * 2;
	size = ntraps * sizeof(ipixel_trapezoid_t);

	if (size > (long)paint->pointf.size) {
		if (cvector_resize(&paint->pointf, size) != 0)
			return -100;
	}

	traps = (ipixel_trapezoid_t*)paint->pointf.data;

	dt = 3.141592653589793 * 0.5 / count;
	theta = 3.141592653589793 * 0.5;

	for (i = 0, ntraps = 0; i < count; i++) {
		double xt, yt, xb, yb;
		ipixel_trapezoid_t t;
		if (i == 0) {
			xt = 0;
			yt = ry;
		}	else {
			xt = rx * cos(theta);
			yt = ry * sin(theta);
		}
		if (i == count - 1) {
			xb = rx;
			yb = 0;
		}	else {
			xb = rx * cos(theta - dt);
			yb = ry * sin(theta - dt);
		}

		theta -= dt;

		t.top = cfixed_from_double(y - yt);
		t.bottom = cfixed_from_double(y - yb);
		t.left.p1.x = cfixed_from_double(x - xt);
		t.left.p1.y = t.top;
		t.right.p1.x = cfixed_from_double(x + xt);
		t.right.p1.y = t.top;
		t.left.p2.x = cfixed_from_double(x - xb);
		t.left.p2.y = t.bottom;
		t.right.p2.x = cfixed_from_double(x + xb);
		t.right.p2.y = t.bottom;
		traps[ntraps++] = t;

		t.top = cfixed_from_double(y + yb);
		t.bottom = cfixed_from_double(y + yt);
		t.left.p1.x = cfixed_from_double(x - xb);
		t.left.p1.y = t.top;
		t.right.p1.x = cfixed_from_double(x + xb);
		t.right.p1.y = t.top;
		t.left.p2.x = cfixed_from_double(x - xt);
		t.left.p2.y = t.bottom;
		t.right.p2.x = cfixed_from_double(x + xt);
		t.right.p2.y = t.bottom;
		traps[ntraps++] = t;
	}
	
	return ipaint_draw_traps(paint, traps, ntraps);
}

int ipaint_draw_circle(ipaint_t *paint, double x, double y, double r)
{
	return ipaint_draw_ellipse(paint, x, y, r, r);
}

void ipaint_fill(ipaint_t *paint, const IRECT *rect, IUINT32 color)
{
	IRECT box;
	if (rect == NULL) {
		box.left = 0;
		box.top = 0;
		box.right = (int)paint->image->w;
		box.bottom = (int)paint->image->h;
	}	else {
		box = *rect;
	}
	ipixel_rect_intersection(&box, &paint->clip);
	ibitmap_rectfill(paint->image, box.left, box.top, box.right - box.left,
		box.bottom - box.top, color);
}

void ipaint_cprintf(ipaint_t *paint, int x, int y, const char *fmt, ...)
{
	char buffer[4096];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	ibitmap_draw_text(paint->image, x, y, buffer, &paint->clip,
		paint->text_color, paint->text_backgrnd, paint->additive);
}


void ipaint_sprintf(ipaint_t *paint, int x, int y, const char *fmt, ...)
{
	char buffer[4096];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	ibitmap_draw_text(paint->image, x + 1, y + 1, buffer, &paint->clip,
		0x80000000, 0, paint->additive);
	ibitmap_draw_text(paint->image, x, y, buffer, &paint->clip,
		paint->text_color, 0, paint->additive);
}


int ipaint_raster(ipaint_t *paint, const ipixel_point_t *pts, 
	const IBITMAP *image, const IRECT *rect, IUINT32 color, int flag)
{
	return ibitmap_raster_float(paint->image, pts, image, rect, color, 
		flag, &paint->clip);
}


int ipaint_raster_draw(ipaint_t *paint, double x, double y, const IBITMAP *src,
	const IRECT *rect, double off_x, double off_y, double scale_x, 
	double scale_y, double angle, IUINT32 color)
{
	return ibitmap_raster_draw(paint->image, x, y, src, rect, off_x, off_y,
		scale_x, scale_y, angle, color, &paint->clip);
}

int ipaint_raster_draw_3d(ipaint_t *paint, double x, double y, double z, 
	const IBITMAP *src, const IRECT *rect, double off_x, double off_y,
	double scale_x, double scale_y, double angle_x, double angle_y,
	double angle_z, IUINT32 color)
{
	return ibitmap_raster_draw_3d(paint->image, x, y, z, src, rect, off_x, 
		off_y, scale_x, scale_y, angle_x, angle_y, angle_z, color,
		&paint->clip);
}

int ipaint_draw(ipaint_t *paint, int x, int y, const IBITMAP *src, 
	const IRECT *bound, IUINT32 color, int flags)
{
	int sx, sy, sw, sh;
	if (bound == NULL) {
		sx = sy = 0;
		sw = (int)src->w;
		sh = (int)src->h;
	}	else {
		sx = bound->left;
		sy = bound->top;
		sw = bound->right - bound->left;
		sh = bound->bottom - bound->top;
	}
	ibitmap_blend(paint->image, x, y, src, sx, sy, sw, sh,
		color, &paint->clip, flags);
	return 0;
}





//---------------------------------------------------------------------
// 色彩空间
//---------------------------------------------------------------------

#define ICONV_BITS		10
#define ICONV_HALF		(1 << (ICONV_BITS - 1))
#define ICONV_FIX(F)	((int)((F) * (1 << ICONV_BITS) + 0.5))

#define ICONV_MAX(a, b) (((a) > (b))? (a) : (b))
#define ICONV_MIN(a, b) (((a) < (b))? (a) : (b))
#define ICONV_MID(a, x, b) ICONV_MAX(a, ICONV_MIN(x, b))

#define ICONV_CLAMP(x)	{ if (x < 0) x = 0; else if (x > 255) x = 255; }
#define ICONV_CLAMP1(r, g, b) { \
	r >>= ICONV_BITS; g >>= ICONV_BITS; b >>= ICONV_BITS; \
	if (((unsigned)(r) | (unsigned)(g) | (unsigned)(b)) & (~255u)) { \
		ICONV_CLAMP(r); ICONV_CLAMP(g); ICONV_CLAMP(b); } \
	}
#define ICONV_CLAMP2(r1, g1, b1, r2, g2, b2) { \
	r1 >>= ICONV_BITS; g1 >>= ICONV_BITS; b1 >>= ICONV_BITS; \
	r2 >>= ICONV_BITS; g2 >>= ICONV_BITS; b2 >>= ICONV_BITS; \
	if (((unsigned)r1 | (unsigned)g1 | (unsigned)b1 |	\
		 (unsigned)r2 | (unsigned)g2 | (unsigned)b2) & (~255u)) {	\
		ICONV_CLAMP(r1); ICONV_CLAMP(g1); ICONV_CLAMP(b1); \
		ICONV_CLAMP(r2); ICONV_CLAMP(g2); ICONV_CLAMP(b2); } \
	}

#define ICONV_MUL(r, g, b, x, y, z, w) \
	(ICONV_FIX(x) * r + ICONV_FIX(y) * g + ICONV_FIX(z) * b + w)

#define ICONV_RGB_TO_Y(r, g, b) \
	ICONV_MUL(r, g, b, 0.299, 0.584, 0.117, 0)
#define ICONV_RGB_TO_Cr(r, g, b) \
	ICONV_MUL(r, g, b, 0.5, -0.4187, -0.0813, (128 << ICONV_BITS))
#define ICONV_RGB_TO_Cb(r, g, b) \
	ICONV_MUL(r, g, b, -0.1687, -0.3313, 0.5, (128 << ICONV_BITS))

#define ICONV_SCALE(f, x) (ICONV_FIX(f) * (x - 128) + ICONV_HALF)

#define ICONV_YCrCb_TO_R(Y, Cr, Cb) \
	((Y << ICONV_BITS) + ICONV_SCALE(1.402, Cr))
#define ICONV_YCrCb_TO_G(Y, Cr, Cb) \
	((Y << ICONV_BITS) - ICONV_SCALE(0.34414, Cb) - \
	ICONV_SCALE(0.71414, Cr))
#define ICONV_YCrCb_TO_B(Y, Cr, Cb) \
	((Y << ICONV_BITS) + ICONV_SCALE(1.772, Cb))

#define ICONV_GET_RGB(input, x, y, z, e) { \
	x = input->r; y = input->g; z = input->b; e = input->reserved; input++; }
#define ICONV_PUT_RGB(output, x, y, z, e) { \
	output->r = (unsigned char)x; \
	output->g = (unsigned char)y; \
	output->b = (unsigned char)z; \
	output->reserved = (unsigned char)e; \
	output++; }

#define ICONV_GET_YCrCb(input, x, y, z, e) { \
	x = input->Y; y = input->Cr; z = input->Cb; \
	e = input->reserved; input++; }
#define ICONV_PUT_YCrCb(output, x, y, z, e) { \
	output->Y = (unsigned char)x; \
	output->Cr = (unsigned char)y; \
	output->Cb = (unsigned char)z; \
	output->reserved = e; \
	output++; }


void iconv_RGB_to_YCrCb(const IRGB *input, long size, IYCrCb *output)
{
	int Y1, Cr1, Cb1, Y2, Cr2, Cb2, e1, e2;
	int r1, g1, b1, r2, g2, b2;

	#define iconv_rgb_to_YCrCb_x1 {		\
		ICONV_GET_RGB(input, r1, g1, b1, e1); \
		Y1 = ICONV_RGB_TO_Y(r1, g1, b1); \
		Cr1 = ICONV_RGB_TO_Cr(r1, g1, b1); \
		Cb1 = ICONV_RGB_TO_Cb(r1, g1, b1); \
		ICONV_CLAMP1(Y1, Cr1, Cb1); \
		ICONV_PUT_YCrCb(output, Y1, Cr1, Cb1, e1); \
	}

	#define iconv_rgb_to_YCrCb_x2 { \
		ICONV_GET_RGB(input, r1, g1, b1, e1); \
		ICONV_GET_RGB(input, r2, g2, b2, e2); \
		Y1 = ICONV_RGB_TO_Y(r1, g1, b1); \
		Cr1 = ICONV_RGB_TO_Cr(r1, g1, b1); \
		Cb1 = ICONV_RGB_TO_Cb(r1, g1, b1); \
		Y2 = ICONV_RGB_TO_Y(r2, g2, b2); \
		Cr2 = ICONV_RGB_TO_Cr(r2, g2, b2); \
		Cb2 = ICONV_RGB_TO_Cb(r2, g2, b2); \
		ICONV_CLAMP2(Y1, Cr1, Cb1, Y2, Cr2, Cb2); \
		ICONV_PUT_YCrCb(output, Y1, Cr1, Cb1, e1); \
		ICONV_PUT_YCrCb(output, Y2, Cr2, Cb2, e2); \
	}

	ILINS_LOOP_DOUBLE (
			iconv_rgb_to_YCrCb_x1,
			iconv_rgb_to_YCrCb_x2,
			size
		);

	#undef iconv_rgb_to_YCrCb_x1
	#undef iconv_rgb_to_YCrCb_x2
}

void iconv_YCrCb_to_RGB(const IYCrCb *input, long size, IRGB *output)
{
	int r1, g1, b1, r2, g2, b2, e1, e2;
	int Y1, Cr1, Cb1, Y2, Cr2, Cb2;
	#define iconv_YCrCb_to_RGB_x1 { \
		ICONV_GET_YCrCb(input, Y1, Cr1, Cb1, e1); \
		r1 = ICONV_YCrCb_TO_R(Y1, Cr1, Cb1); \
		g1 = ICONV_YCrCb_TO_G(Y1, Cr1, Cb1); \
		b1 = ICONV_YCrCb_TO_B(Y1, Cr1, Cb1); \
		ICONV_CLAMP1(r1, g1, b1); \
		ICONV_PUT_RGB(output, r1, g1, b1, e1); \
	}

	#define iconv_YCrCb_to_RGB_x2 { \
		ICONV_GET_YCrCb(input, Y1, Cr1, Cb1, e1); \
		ICONV_GET_YCrCb(input, Y2, Cr2, Cb2, e2); \
		r1 = ICONV_YCrCb_TO_R(Y1, Cr1, Cb1); \
		g1 = ICONV_YCrCb_TO_G(Y1, Cr1, Cb1); \
		b1 = ICONV_YCrCb_TO_B(Y1, Cr1, Cb1); \
		r2 = ICONV_YCrCb_TO_R(Y2, Cr2, Cb2); \
		g2 = ICONV_YCrCb_TO_G(Y2, Cr2, Cb2); \
		b2 = ICONV_YCrCb_TO_B(Y2, Cr2, Cb2); \
		ICONV_CLAMP2(r1, g1, b1, r2, g2, b2); \
		ICONV_PUT_RGB(output, r1, g1, b1, e1); \
		ICONV_PUT_RGB(output, r2, g2, b2, e2); \
	}

	ILINS_LOOP_DOUBLE (
			iconv_YCrCb_to_RGB_x1,
			iconv_YCrCb_to_RGB_x2,
			size
		);

	#undef iconv_YCrCb_to_RGB_x1
	#undef iconv_YCrCb_to_RGB_x2
}

void iconv_RGB_to_HSV(const IRGB *input, long size, IHSV *output)
{
	float min, max, delta, h, s, v;
	float r, g, b;
	for (; size > 0; size--, input++, output++) {
		r = input->r * (1.0f / 255.0f);
		g = input->g * (1.0f / 255.0f);
		b = input->b * (1.0f / 255.0f);
		min = ICONV_MIN(r, ICONV_MIN(g, b));
		max = ICONV_MAX(r, ICONV_MAX(g, b));
		v = max;
		delta = max - min;
		if (max == 0.0f) {
			s = 0.0f;
			h = 0.0f;
		}	else {
			s = delta / max;
			if (r == max) h = (g - b) / delta;
			else if (g == max) h = 2.0f + (b - r) / delta;
			else h = 4.0f + (r - g) / delta;
			h *= 60.0f;
			if (h < 0.0f) h += 360.0f;
		}
		output->H = ICONV_MID(0.0f, h, 359.0f);
		output->S = ICONV_MID(0.0f, s, 1.0f);
		output->V = ICONV_MID(0.0f, v, 1.0f);
		output->reserved = input->reserved;
	}
}

void iconv_HSV_to_RGB(const IHSV *input, long size, IRGB *output)
{
	float r, g, b, h, s, v, f, p, q, t;
	int i;
	for (; size > 0; size--, input++, output++) {
		h = input->H;
		s = input->S;
		v = input->V;
		if (s == 0.0f) {
			r = g = b = v;
		}	else {
			if (h < 0.0f || h >= 360.0f) {
				while (h < 0.0f) h += 360.0f;
				while (h >= 360.0f) h -= 360.0f;
			}
			h /= 60.0f;
			i = (int)h;
			f = h - i;
			p = v * (1.0f - s);
			q = v * (1.0f - s * f);
			t = v * (1.0f - s * (1.0f - f));
			switch (i)
			{
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			case 5: r = v; g = p; b = q; break;
			case 6: r = v; g = t; b = p; break;
			default:
				r = g = b = 0;
				printf("error: i=%d\n", i);
				assert(i < 6);
				break;
			}
		}
		r = ICONV_MID(0.0f, r, 1.0f);
		g = ICONV_MID(0.0f, g, 1.0f);
		b = ICONV_MID(0.0f, b, 1.0f);
		output->r = (unsigned char)(r * 255.0f);
		output->g = (unsigned char)(g * 255.0f);
		output->b = (unsigned char)(b * 255.0f);
		output->reserved = input->reserved;
	}
}



//---------------------------------------------------------------------
// 特效若干
//---------------------------------------------------------------------

// 生成特效
IBITMAP *ibitmap_glossy_make(IBITMAP *bmp, int radius, int border, int light,
	int shadow, int shadow_pos)
{
	IBITMAP *input = NULL;
	IBITMAP *output = NULL;
	int w = (int)bmp->w;
	int h = (int)bmp->h;
	int limit = ((w < h)? w : h) / 3;
	if (radius > limit) radius = limit;
	if (border > limit) border = limit;
	if (border) {
		IBITMAP *small;
		int r1 = radius + border / 3;
		int r2 = radius;
		output = ibitmap_round_rect(bmp, r1, 2);
		if (output == NULL) return NULL;

		small = ibitmap_chop(bmp, border, border, w - border * 2, 
			h - border * 2);

		if (small == NULL) {
			ibitmap_release(output);
			return NULL;
		}
		input = ibitmap_round_rect(small, r2, light);

		ibitmap_release(small);
		if (input == NULL) {
			ibitmap_release(output);
			return NULL;
		}
		ibitmap_pixfmt_set(output, IPIX_FMT_A8R8G8B8);
		ibitmap_color_sub(output, NULL, 0x00101010);

		ibitmap_blend(output, border, border, input, 0, 0, 
			(int)input->w, (int)input->h, 0xffffffff, NULL, 0);
		ibitmap_release(input);
	}
	else {
		output = ibitmap_round_rect(bmp, radius, light);
		if (output == NULL) return NULL;
	}

	if (shadow > 0 && shadow_pos >= 0) {
		IBITMAP *tmp;
		tmp = ibitmap_drop_shadow(output, shadow, shadow);

		if (tmp == NULL) {
			ibitmap_release(output);
			return NULL;
		}

		input = ibitmap_create((int)tmp->w, (int)tmp->h, 32);
		ibitmap_pixfmt_set(input, IPIX_FMT_A8R8G8B8);
		ibitmap_fill(input, 0, 0, (int)input->w, (int)input->h, 0, 0);

		ibitmap_convert(input, 0, 0, tmp, 0, 0, (int)tmp->w, (int)tmp->h, 
			NULL, 0);

		ibitmap_release(tmp);

		shadow_pos = shadow_pos % 3;
		if (shadow_pos == 0) {
			ibitmap_blend(input, shadow, shadow, output, 0, 0,
				w, h, 0xffffffff, NULL, 0);
		}
		else if (shadow_pos == 1) {
			ibitmap_blend(input, shadow, 0, output, 0, 0,
				w, h, 0xffffffff, NULL, 0);
		}
		else if (shadow_pos == 2) {
			ibitmap_blend(input, 0, 0, output, 0, 0,
				w, h, 0xffffffff, NULL, 0);
		}
		ibitmap_release(output);
		output = input;
	}

	return output;
}

// 更新 HSV
static int ibitmap_update_hsv(int x, int y, int w, IUINT32 *card, void *user)
{
	float h = ((IHSV*)user)->H;
	float s = ((IHSV*)user)->S;
	float v = ((IHSV*)user)->V;
	float H, S, V;
	IUINT32 cc, r, g, b, a, i;
	for (i = w; i > 0; card++, i--) {
		cc = card[0];
		IRGBA_FROM_A8R8G8B8(cc, r, g, b, a);
		ipixel_cvt_rgb_to_hsv(r, g, b, &H, &S, &V);
		H += h;
		S *= s;
		V *= v;
		if (S > 1.0f) S = 1.0f;
		if (V > 1.0f) V = 1.0f;
		cc = ipixel_cvt_hsv_to_rgb(H, S, V);
		card[0] = cc | (a << 24);
	}
	return 0;
}

// 更新 HSL
static int ibitmap_update_hsl(int x, int y, int w, IUINT32 *card, void *user)
{
	float h = ((IHSV*)user)->H;
	float s = ((IHSV*)user)->S;
	float l = ((IHSV*)user)->V;
	float H, S, L;
	IUINT32 cc, r, g, b, a, i;
	for (i = w; i > 0; card++, i--) {
		cc = card[0];
		IRGBA_FROM_A8R8G8B8(cc, r, g, b, a);
		ipixel_cvt_rgb_to_hsl(r, g, b, &H, &S, &L);
		H += h;
		S *= s;
		L *= l;
		if (S > 1.0f) S = 1.0f;
		if (L > 1.0f) L = 1.0f;
		cc = ipixel_cvt_hsl_to_rgb(H, S, L);
		card[0] = cc | (a << 24);
	}
	return 0;
}

// 调整色调
void ibitmap_adjust_hsv(IBITMAP *bmp, float hue, float saturation, 
	float value, const IRECT *bound)
{
	IHSV hsv;
	hsv.H = (float)hue;
	hsv.S = (float)saturation;
	hsv.V = (float)value;
	ibitmap_update(bmp, bound, ibitmap_update_hsv, 0, &hsv);
}


// 调整色调
void ibitmap_adjust_hsl(IBITMAP *bmp, float hue, float saturation, 
	float lightness, const IRECT *bound)
{
	IHSV hsv;
	hsv.H = (float)hue;
	hsv.S = (float)saturation;
	hsv.V = (float)lightness;
	ibitmap_update(bmp, bound, ibitmap_update_hsl, 0, &hsv);
}



//---------------------------------------------------------------------
// Android Patch9 支持
//---------------------------------------------------------------------

// 计算新大小下面的 Patch9
int ipixel_patch_nine(const int *patch, int count, int newsize, int *out)
{
	int total, white = 0, black = 0;
	int nblack = 0, i, k, x, t;
	int srclen = 0;
	int dstlen = 0;
	for (i = 0; i < count; i++) {
		if (patch[i] < 0) {
			black += (-patch[i]);
			nblack++;
		}
		else if (patch[i] > 0) {
			white += patch[i];
		}
	}
	total = black + white;
	if (newsize < white) newsize = white;
	srclen = black;
	dstlen = newsize - white;
	for (i = 0, k = 0, x = 0, t = 0; i < count; i++) {
		if (patch[i] >= 0) {
			out[i] = patch[i];
			t += patch[i];
		}	else {
			int oldlen = -patch[i];
		#ifdef _MSC_VER
			int newlen = (int)((((__int64)oldlen) * dstlen) / srclen);
		#else
			int newlen = (int)((((long long)oldlen) * dstlen) / srclen);
		#endif
			if (k == nblack - 1) {
				int j, z;
				for (j = i + 1, z = 0; j < count; j++) {
					if (patch[j] >= 0) z += patch[j];
					else z += (-patch[j]);
				}
				z = newsize - t - z;
				newlen = z;
			}
			out[i] = -newlen;
			t += newlen;
			k++;
		}
	}
	return newsize;
}

// 扫描 Patch
int ipixel_patch_scan(const void *src, long step, int size, int *patch)
{
	const unsigned char *ptr = (const unsigned char*)src;
	int index = 0, mode = 0, i = 0, count = 0;
	unsigned long endian = 0x11223344;
	int lsb = (*((char*)&endian) == 0x44)? 1 : 0;
	int A = lsb? 3 : 0;
	int R = lsb? 2 : 1;
	int G = lsb? 1 : 2;
	int B = lsb? 0 : 3;
	for (i = 0; i < size; ptr += step, i++) {
		unsigned int a1 = ptr[A] + (ptr[A] >> 7);
		unsigned int a2 = 256 - a1;
		unsigned int cr = (ptr[R] * a1 + 255 * a2) >> 8;
		unsigned int cg = (ptr[G] * a1 + 255 * a2) >> 8;
		unsigned int cb = (ptr[B] * a1 + 255 * a2) >> 8;
		unsigned int cc = (cr + cg + cg + cb) >> 2;
		int white = (cc < 128)? 0 : 1;
		if (mode == 0) {
			if (white) mode = 1, count = 1;
			else mode = -1, count = 1;
		}
		else if (mode > 0 && white != 0 && i < size - 1) count++;
		else if (mode < 0 && white == 0 && i < size - 1) count++;
		else {
			if (i == size - 1) count++;
			if (patch) patch[index] = (mode >= 0)? count : -count;
			index++;
			mode = white? 1 : -1;
			count = 1;
		}
	}
	return index;
}

// 位图 patch 扫描
int ibitmap_patch_scan(const IBITMAP *src, int n, int *patch)
{
	const unsigned char *ptr = (const unsigned char*)src->pixel;
	long pitch = (long)src->pitch;
	int w = (int)src->w;
	int h = (int)src->h;
	int count;
	if (src->bpp != 32 || w < 2 || h < 2) return -1;
	if (n == 0) {
		count = ipixel_patch_scan(ptr + 4, 4, w - 2, patch);
		if (count == 1 && patch) patch[0] = -patch[0];
		return count;
	}
	if (n == 1) {
		count = ipixel_patch_scan(ptr + pitch, pitch, h - 2, patch);
		if (count == 1 && patch) patch[0] = -patch[0];
		return count;
	}
	if (n == 2) {
		ptr += pitch * (h - 1);
		count = ipixel_patch_scan(ptr + 4, 4, w - 2, patch);
		if (count == 1 && patch) patch[0] = -patch[0];
		return count;
	}	else {
		n = 3;
		ptr += 4 * (w - 1);
		count = ipixel_patch_scan(ptr + pitch, pitch, h - 2, patch);
		if (count == 1 && patch) patch[0] = -patch[0];
		return count;
	}
	return 0;
}

// 平滑缩放
int ibitmap_smooth_resize(IBITMAP *dst, const IRECT *rectdst, 
	const IBITMAP *src, const IRECT *rectsrc);

// 位图 patch
IBITMAP *ibitmap_patch_nine_i(const IBITMAP *src, int nw, int nh, int *code)
{
	int countx, county;
	int *srcx, *srcy;
	int *dstx, *dsty;
	int i, j, sx, sy, dx, dy;
	IBITMAP *dst;

	countx = ibitmap_patch_scan(src, 0, NULL);
	if (countx < 0) {
		if (code) code[0] = 1;
		return NULL;
	}

	county = ibitmap_patch_scan(src, 1, NULL);
	if (county < 0) {
		if (code) code[0] = 1;
		return NULL;
	}

	srcx = (int*)malloc(sizeof(int) * (countx + county) * 4);
	if (srcx == NULL) {
		if (code) code[0] = 3;
		return NULL;
	}

	srcy = srcx + countx;
	dstx = srcy + county;
	dsty = dstx + countx;

	ibitmap_patch_scan(src, 0, srcx);
	ibitmap_patch_scan(src, 1, srcy);

	nw = ipixel_patch_nine(srcx, countx, nw, dstx);
	nh = ipixel_patch_nine(srcy, county, nh, dsty);

	dst = ibitmap_create(nw, nh, 32);
	if (dst == NULL) {
		free(srcx);
		if (code) code[0] = 4;
		return NULL;
	}

	sx = 1, sy = 1;
	dx = 0, dy = 0;

	ibitmap_fill(dst, 0, 0, nw, nh, 0, 0);
	ibitmap_pixfmt_set(dst, ibitmap_pixfmt_guess(src));

	for (j = 0; j < county; j++) {
		int src_h = (srcy[j] >= 0)? srcy[j] : -srcy[j];
		int dst_h = (dsty[j] >= 0)? dsty[j] : -dsty[j];
		IRECT boundsrc;
		IRECT bounddst;
		sx = 1; dx = 0;

		for (i = 0; i < countx; i++) {
			int src_w = (srcx[i] >= 0)? srcx[i] : -srcx[i];
			int dst_w = (dstx[i] >= 0)? dstx[i] : -dstx[i];

			boundsrc.left = sx;
			boundsrc.top = sy;
			boundsrc.right = sx + src_w;
			boundsrc.bottom = sy + src_h;
			bounddst.left = dx;
			bounddst.top = dy;
			bounddst.right = dx + dst_w;
			bounddst.bottom = dy + dst_h;

			assert(sx + src_w <= (int)src->w - 0);
			assert(sy + src_h <= (int)src->h - 0);
			assert(dx + dst_w <= (int)dst->w);
			assert(dy + dst_h <= (int)dst->h);
			
			if (src_w > 0 && src_h > 0 && dst_w > 0 && dst_h > 0) {
				ibitmap_smooth_resize(dst, &bounddst, src, &boundsrc);
			}

			sx += src_w;
			dx += dst_w;
		}
		sy += src_h;
		dy += dst_h;
	}

	free(srcx);
	if (code) code[0] = 0;

	return dst;
}


// 位图 patch
IBITMAP *ibitmap_patch_nine(const IBITMAP *src, int nw, int nh, int *code)
{
	IBITMAP *tmp, *pp;
	int sw = (int)src->w;
	int sh = (int)src->h;
	int fmt = ibitmap_pixfmt_guess(src);
	int i, j;

	tmp = ibitmap_create((int)src->w, (int)src->h, 32);
	if (tmp == NULL) return NULL;

	ibitmap_pixfmt_set(tmp, IPIX_FMT_A8R8G8B8);
	ibitmap_convert(tmp, 0, 0, src, 0, 0, sw, sh, NULL, 0);

	for (j = 0; j < sh; j++) {
		IUINT32 *ptr = (IUINT32*)tmp->line[j];
		for (i = sw; i > 0; ptr++, i--) {
			if ((ptr[0] >> 24) == 0) ptr[0] = 0;
		}
	}

	pp = ibitmap_patch_nine_i(tmp, nw, nh, code);
	ibitmap_release(tmp);

	if (pp == NULL) {
		return NULL;
	}

	if (fmt == IPIX_FMT_A8R8G8B8) {
		ibitmap_pixfmt_set(pp, fmt);
		return pp;
	}

	tmp = ibitmap_create((int)pp->w, (int)pp->h, src->bpp);
	ibitmap_pixfmt_set(tmp, fmt);

	if (tmp) {
		tmp->extra = src->extra;
		ibitmap_convert(tmp, 0, 0, pp, 0, 0, (int)pp->w, (int)pp->h, 0, 0);
	}

	ibitmap_release(pp);

	return tmp;
}


// 取得客户区
int ibitmap_patch_client(const IBITMAP *src, IRECT *client)
{
	int _buffer[1024];
	int *buffer = _buffer;
	int n3, n4, nn;
	int *patch3, *patch4;
	IRECT bound;

	n3 = ibitmap_patch_scan(src, 2, NULL);
	if (n3 <= 0) return -1;
	n4 = ibitmap_patch_scan(src, 3, NULL);
	if (n4 <= 0) return -1;
	if (n3 < 3) n3 = 3;
	if (n4 < 3) n4 = 3;
	nn = n3 + n4;

	if (nn > 1024) buffer = (int*)malloc(sizeof(int) * nn);
	if (buffer == NULL) return -2;

	patch3 = buffer;
	patch4 = patch3 + n3;

	ibitmap_patch_scan(src, 2, patch3);
	ibitmap_patch_scan(src, 3, patch4);

	memset(&bound, 0, sizeof(bound));

	for (nn = 0; nn < 2; nn++) {
		int *patch = (nn == 0)? patch3 : patch4;
		int count = (nn == 0)? n3 : n4;
		int total = (nn == 0)? ((int)src->w - 2) : ((int)src->h - 2);
		int first = 0, last = 0, i, j;
		for (i = 0, j = -1; i < count; i++) {
			if (patch[i] < 0) { j = i; break; }
		}
		if (j < 0) {
			patch[0] = 0;
			patch[1] = -total;
			patch[2] = 0;
			first = 0;
			last = 0;
			count = 3;
		}	else {
			//ipixel_patch_print(patch, count);
			for (i = 0, first = 0; i < count; i++) {
				if (patch[i] < 0) break;
				first += patch[i];
			}
			for (j = count - 1, last = 0; j > i; j--) {
				if (patch[j] < 0) break;
				last += patch[j];
			}
			patch[0] = first;
			patch[1] = -(total - first - last);
			patch[2] = last;
			count = 3;
		}
		if (nn == 0) {
			n3 = count;
			bound.left = first;
			bound.right = first + (total - first - last);
		}
		else {
			n4 = count;
			bound.top = first;
			bound.bottom = first + (total - first - last);
		}
	}
	
	if (buffer != _buffer) free(buffer);

	if (client) client[0] = bound;

	return 0;
}




