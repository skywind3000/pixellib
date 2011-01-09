//=====================================================================
//
// ibmsse2.c - sse2 & mmx optimize
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#include "iblit386.h"
#include "ibitmap.h"
#include "ibmbits.h"
#include "ibmcols.h"
#include "ibmsse2.h"


#ifdef _MSC_VER
#pragma warning(disable: 4799)
#endif


//---------------------------------------------------------------------
// pixel convert
//---------------------------------------------------------------------
#define IPIXEL_CVT_FROM(fmt, bpp) \
static inline IUINT32 _ipixel_cvt_from_##fmt(const void *bits) { \
    IUINT32 c, r, g, b, a; \
    c = _ipixel_fetch(bpp, bits, 0); \
    IRGBA_FROM_PIXEL(fmt, c, r, g, b, a); \
    return IRGBA_TO_PIXEL(A8R8G8B8, r, g, b, a); \
}

#define IPIXEL_CVT_TO(fmt, bpp) \
static inline void _ipixel_cvt_to_##fmt(void *bits, IUINT32 pixel) { \
	IUINT32 c, r, g, b, a; \
	IRGBA_FROM_A8R8G8B8(pixel, r, g, b, a); \
	c = IRGBA_TO_PIXEL(fmt, r, g, b, a); \
	_ipixel_store(bpp, bits, 0, c); \
}

#define IPIXEL_CVT_LUT(fmt) \
static inline IUINT32 _ipixel_cvt_from_##fmt(const void *bpp) { \
	IUINT32 c1 = _ipixel_cvt_lut_##fmt[((const IUINT8*)bpp)[0] +   0]; \
	IUINT32 c2 = _ipixel_cvt_lut_##fmt[((const IUINT8*)bpp)[1] + 256]; \
	return c1 | c2; \
}

static inline IUINT32 ipixel_cvt_from_A8R8G8B8(const void *bits) {
	return *(const IUINT32*)bits;
}

static inline IUINT32 _ipixel_cvt_from_A8B8G8R8(const void *bits) {
	IUINT32 pixel = *(const IUINT32*)bits;
	return ((pixel & 0xff00ff00) | ((pixel >> 16) & 0xff) | 
		((pixel & 0xff) << 16));
}

static inline IUINT32 _ipixel_cvt_from_R8G8B8A8(const void *bits) {
	IUINT32 pixel = ((const IUINT32*)(bits))[0];
	return ((pixel & 0xff) << 24) | ((pixel >> 8) & 0xffffff);
}

static inline IUINT32 _ipixel_cvt_from_B8G8R8A8(const void *bits) {
	IUINT32 pixel = ((const IUINT32*)(bits))[0];
	return		((pixel & 0x000000ff) << 24) |
				((pixel & 0x0000ff00) <<  8) |
				((pixel & 0x00ff0000) >>  8) |
				((pixel & 0xff000000) >> 24);
}

static inline IUINT32 _ipixel_cvt_from_X8R8G8B8(const void *bits) {
	return _ipixel_fetch(32, bits, 0) | 0xff000000;
}

static inline IUINT32 _ipixel_cvt_from_X8B8G8R8(const void *bits) {
	IUINT32 pixel = ((IUINT32*)(bits))[0];
	return ((pixel & 0x0000ff00) | ((pixel >> 16) & 0xff) | 
		((pixel & 0xff) << 16)) | 0xff000000;
}

static inline IUINT32 _ipixel_cvt_from_R8G8B8X8(const void *bits) {
	return (((IUINT32*)(bits))[0] >> 8) | 0xff000000;
}

static inline IUINT32 _ipixel_cvt_from_B8G8R8X8(const void *bits) {
	IUINT32 pixel = ((const IUINT32*)(bits))[0];
	return		((pixel & 0x0000ff00) <<  8) |
				((pixel & 0x00ff0000) >>  8) |
				((pixel & 0xff000000) >> 24) | 0xff000000;
}


static inline IUINT32 _ipixel_cvt_from_R8G8B8(const void *bits) {
	return _ipixel_fetch(24, bits, 0) | 0xff000000;
}

static inline IUINT32 _ipixel_cvt_from_B8G8R8(const void *bits) {
	IUINT32 pixel = _ipixel_fetch(24, bits, 0);
	return ((pixel & 0x0000ff00) | ((pixel >> 16) & 0xff) | 
		((pixel & 0xff) << 16)) | 0xff000000;
}


IPIXEL_CVT_LUT(R5G6B5);
IPIXEL_CVT_LUT(B5G6R5);
IPIXEL_CVT_LUT(A1R5G5B5);
IPIXEL_CVT_LUT(A1B5G5R5);
IPIXEL_CVT_LUT(R5G5B5A1);
IPIXEL_CVT_LUT(B5G5R5A1);
IPIXEL_CVT_LUT(X1R5G5B5);
IPIXEL_CVT_LUT(X1B5G5R5);
IPIXEL_CVT_LUT(R5G5B5X1);
IPIXEL_CVT_LUT(B5G5R5X1);
IPIXEL_CVT_LUT(A4R4G4B4);
IPIXEL_CVT_LUT(A4B4G4R4);
IPIXEL_CVT_LUT(R4G4B4A4);
IPIXEL_CVT_LUT(B4G4R4A4);


static inline void _ipixel_cvt_to_A8R8G8B8(void *bits, IUINT32 pixel) {
	*(IUINT32*)bits = pixel;
}

static inline void _ipixel_cvt_to_A8B8G8R8(void *bits, IUINT32 pixel) {
	*(IUINT32*)bits = (pixel & 0xff00ff00) |
			((pixel >> 16) & 0xff) | ((pixel & 0xff) << 16);
}

static inline void _ipixel_cvt_to_R8G8B8A8(void *bits, IUINT32 pixel) {
	*(IUINT32*)bits = ((pixel & 0xffffff) << 8) | 
		((pixel & 0xff000000) >> 24);
}


IPIXEL_CVT_TO(B8G8R8A8, 32);

static inline void _ipixel_cvt_to_X8R8G8B8(void *bits, IUINT32 pixel) {
	*(IUINT32*)bits = pixel & 0xffffff;
}

IPIXEL_CVT_TO(X8B8G8R8, 32);

static inline void _ipixel_cvt_to_R8G8B8X8(void *bits, IUINT32 pixel) {
	*(IUINT32*)bits = (pixel << 8);
}

IPIXEL_CVT_TO(B8G8R8X8, 32);

static inline void _ipixel_cvt_to_R8G8B8(void *bits, IUINT32 pixel) {
	ISTORE24(bits, pixel);
}

static inline void _ipixel_cvt_to_B8G8R8(void *bits, IUINT32 pixel) {
#if IWORDS_BIG_ENDIAN
	ISTORE24_LSB(bits, pixel);
#else
	ISTORE24_MSB(bits, pixel);
#endif
}

static inline void _ipixel_cvt_to_R5G6B5(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(R5G6B5, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

static inline void _ipixel_cvt_to_B5G6R5(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(B5G6R5, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

static inline void _ipixel_cvt_to_X1R5G5B5(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(X1R5G5B5, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

static inline void _ipixel_cvt_to_X1B5G5R5(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(X1B5G5R5, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

static inline void _ipixel_cvt_to_R5G5B5X1(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(R5G5B5X1, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

static inline void _ipixel_cvt_to_B5G5R5X1(void *bits, IUINT32 pixel) {
	IUINT32 r, g, b;
	ISPLIT_RGB(pixel, r, g, b);
	pixel = IRGBA_TO_PIXEL(B5G5R5X1, r, g, b, 255);
	*(IUINT16*)bits = (IUINT16)pixel;
}

IPIXEL_CVT_TO(A1R5G5B5, 16);
IPIXEL_CVT_TO(A1B5G5R5, 16);
IPIXEL_CVT_TO(R5G5B5A1, 16);
IPIXEL_CVT_TO(B5G5R5A1, 16);

IPIXEL_CVT_TO(A4R4G4B4, 16);
IPIXEL_CVT_TO(A4B4G4R4, 16);
IPIXEL_CVT_TO(R4G4B4A4, 16);
IPIXEL_CVT_TO(B4G4R4A4, 16);


#undef IPIXEL_CVT_FROM
#undef IPIXEL_CVT_TO
#undef IPIXEL_CVT_LUT

void ipixel_lut_2_to_4(int sfmt, int dfmt, IUINT32 *table);

static IUINT8 _ipixel_alpha_mask_lut[4096];

static int _ipixel_cvt_lut_inited = 0;

static void _ipixel_cvt_lut_init(void)
{
	int i;

	if (_ipixel_cvt_lut_inited) return;

	for (i = 0; i < 4096; i++) {
		int alpha = _ipixel_scale_6[i >> 6];
		int mask = _ipixel_scale_6[i & 63];
		int result = alpha * mask / 255;
		_ipixel_alpha_mask_lut[i] = result;
	}

	_ipixel_cvt_lut_inited = 1;
}


static inline void _ipixel_cvt_init(void) {
	if (_ipixel_cvt_lut_inited == 0) 
		_ipixel_cvt_lut_init();
}



//---------------------------------------------------------------------
// BLEND MMX
//---------------------------------------------------------------------
static inline IUINT32 ipixel_mmx_blend(IUINT32 dst, IUINT32 src) 
{
	movd_r_v(mm0, src);
	pxor_r_r(mm7, mm7);

	movq_r_r(mm2, mm0);
	psrlq_r_i(mm2, 24);

	punpcklwd_r_r(mm2, mm2);
	punpckldq_r_r(mm2, mm2);

	pcmpeqb_r_r(mm4, mm4);

	punpcklbw_r_r(mm0, mm7);
	punpcklbw_r_r(mm4, mm7);
	
	psubb_r_r(mm4, mm2);
	pmullw_r_r(mm0, mm2);
	movd_r_v(mm2, dst);
	
	punpcklbw_r_r(mm2, mm7);
	pmullw_r_r(mm2, mm4);

	pcmpeqw_r_r(mm5, mm5);
	punpcklbw_r_r(mm5, mm7);
	paddw_r_r(mm0, mm2);

	movq_r_r(mm1, mm0);
	psrlw_r_i(mm1, 8);
	paddw_r_r(mm0, mm1);
	psrlw_r_i(mm0, 8);
	
	packuswb_r_r(mm0, mm0);
	
	movd_v_r(dst, mm0);

	return dst;
}


//---------------------------------------------------------------------
// BLEND SSE2
//---------------------------------------------------------------------
static inline IUINT32 ipixel_sse2_blend(IUINT32 dst, IUINT32 src) 
{
	movd_r_v(xmm0, src);
	pxor_r_r(xmm7, xmm7);
	pcmpeqb_r_r(xmm4, xmm4);

	movq_r_r(xmm2, xmm0);
	psrlq_r_i(xmm2, 24);

	punpcklwd_r_r(xmm2, xmm2);
	punpckldq_r_r(xmm2, xmm2);

	punpcklbw_r_r(xmm0, xmm7);
	punpcklbw_r_r(xmm4, xmm7);
	
	psubb_r_r(xmm4, xmm2);
	pmullw_r_r(xmm0, xmm2);
	movd_r_v(xmm2, dst);
	
	punpcklbw_r_r(xmm2, xmm7);
	pmullw_r_r(xmm2, xmm4);

	pcmpeqw_r_r(xmm5, xmm5);
	punpcklbw_r_r(xmm5, xmm7);
	paddw_r_r(xmm0, xmm2);

	movq_r_r(xmm1, xmm0);
	psrlw_r_i(xmm1, 8);
	paddw_r_r(xmm0, xmm1);
	psrlw_r_i(xmm0, 8);
	
	packuswb_r_r(xmm0, xmm0);
	
	movd_v_r(dst, xmm0);

	return dst;
}


//---------------------------------------------------------------------
// blend four pixels at same time
//---------------------------------------------------------------------
static inline void ipixel_sse2_blend_quad(void *dst, const void *src)
{
#if defined(__INLINEGNU__)
	#ifndef __i386__
	volatile const void *source = src;
	volatile void *destination = dst;
	size_t x1, x2;
	#endif
	__asm__ __volatile__ (
	#ifdef __i386__
	  "  mov          %0, %%esi\n"
	  "  mov          %1, %%edi\n"
	  "  pxor         %%xmm7, %%xmm7\n"
	  "  movq         (%%esi), %%xmm0\n"
	  "  movq         0x8(%%esi), %%xmm1\n"
	  "  movq         (%%edi), %%mm2\n"
	  "  movq         0x8(%%edi), %%mm3\n"
	#else
	  "  mov          %%rsi, %0\n"
	  "  mov          %%rdi, %1\n"
	  "  mov          %2, %%rsi\n"
	  "  mov          %3, %%rdi\n"
	  "  pxor         %%xmm7, %%xmm7\n"
	  "  movq         (%%rsi), %%xmm0\n"
	  "  movq         0x8(%%rsi), %%xmm1\n"
	  "  movq         (%%rdi), %%mm2\n"
	  "  movq         0x8(%%rdi), %%mm3\n"
	#endif
	  
	  "  movdqu       %%xmm0, %%xmm2\n"
	  "  movdqu       %%xmm1, %%xmm3\n"
	  "  psrld        $0x18, %%xmm2\n"
	  "  psrld        $0x18, %%xmm3\n"
	  
	  "  packuswb     %%xmm2, %%xmm2\n"
	  "  packuswb     %%xmm3, %%xmm3\n"
	  "  pcmpeqb      %%xmm4, %%xmm4\n"     // mm4 = 0xffff...ff
	  "  pcmpeqb      %%xmm5, %%xmm5\n"     // mm5 = 0xffff...ff
	  "  punpcklwd    %%xmm2, %%xmm2\n"
	  "  punpcklwd    %%xmm3, %%xmm3\n"
	  "  punpcklbw    %%xmm7, %%xmm0\n"     // mm0 = src1
	  "  punpcklbw    %%xmm7, %%xmm1\n"     // mm1 = src2
	  "  punpckldq    %%xmm2, %%xmm2\n"   // mm2 = 0 a1 0 a1 0 a1 0 a1 (word)
	  "  punpckldq    %%xmm3, %%xmm3\n"   // mm3 = 0 a2 0 a2 0 a2 0 a2 (word)
	  
	  "  punpcklbw    %%xmm7, %%xmm4\n"
	  "  punpcklbw    %%xmm7, %%xmm5\n"
	  
	  "  pmullw       %%xmm2, %%xmm0\n"     // mm0 = src1 * alpha1
	  "  pmullw       %%xmm3, %%xmm1\n"     // mm1 = src2 * alpha2
	  
	  "  psubb        %%xmm2, %%xmm4\n"     // mm4 = (0xff - a1)...
	  "  psubb        %%xmm3, %%xmm5\n"     // mm5 = (0xff - a2)...
	  
	  "  movq2dq      %%mm2, %%xmm2\n"      // mm2 = dst1
	  "  movq2dq      %%mm3, %%xmm3\n"      // mm3 = dst2
	  "  punpcklbw    %%xmm7, %%xmm2\n"
	  "  punpcklbw    %%xmm7, %%xmm3\n"
	  "  pmullw       %%xmm4, %%xmm2\n"     // mm2 = dst1 * (255 - a1)
	  "  pmullw       %%xmm5, %%xmm3\n"     // mm3 = dst2 * (255 - a2)
	  
	  "  paddw        %%xmm2, %%xmm0\n"
	  "  paddw        %%xmm3, %%xmm1\n"
	  
	  "  movdqu       %%xmm0, %%xmm2\n"
	  "  movdqu       %%xmm1, %%xmm3\n"
	  
	  "  psrlw        $0x8, %%xmm2\n"
	  "  psrlw        $0x8, %%xmm3\n"
	  "  paddw        %%xmm2, %%xmm0\n"
	  "  paddw        %%xmm3, %%xmm1\n"
	  "  psrlw        $0x8, %%xmm0\n"
	  "  psrlw        $0x8, %%xmm1\n"
	  "  packuswb     %%xmm0, %%xmm0\n"
	  "  packuswb     %%xmm1, %%xmm1\n"
	#ifdef __i386__
	  "  movq         %%xmm0, (%%edi)\n"
	  "  movq         %%xmm1, 0x8(%%edi)\n"
	#else
	  "  movq         %%xmm0, (%%rdi)\n"
	  "  movq         %%xmm1, 0x8(%%rdi)\n"
	  "  mov          %0, %%rsi\n"
	  "  mov          %1, %%rdi\n"
	#endif

	#ifdef __i386__
	  :
	  :"m"(src), "m"(dst)
	  :"memory", "esi", "edi"
	#else
	  :"=m"(x1), "=m"(x2)
	  :"m"(source), "m"(destination)
	  :"memory", "rsi", "rdi"
	#endif
	);
#elif defined(__INLINEMSC__)
	_asm {
		mov esi, src;
		mov edi, dst;
		pxor xmm7, xmm7;

		movq xmm0, qword ptr [esi];
		movq xmm1, qword ptr [esi + 8];
		movq mm2, [edi];
		movq mm3, [edi + 8];

		movdqu xmm2, xmm0;
		movdqu xmm3, xmm1;
		psrld xmm2, 24;
		psrld xmm3, 24;

		packuswb xmm2, xmm2;
		packuswb xmm3, xmm3;
		pcmpeqb xmm4, xmm4;			// mm4 = 0xffff...ff
		pcmpeqb xmm5, xmm5;			// mm5 = 0xffff...ff
		punpcklwd xmm2, xmm2;
		punpcklwd xmm3, xmm3;
		punpcklbw xmm0, xmm7;		// mm0 = src1
		punpcklbw xmm1, xmm7;		// mm1 = src2
		punpckldq xmm2, xmm2;		// mm2 = 0 a1 0 a1 0 a1 0 a1 (word)
		punpckldq xmm3, xmm3;		// mm3 = 0 a2 0 a2 0 a2 0 a2 (word)

		punpcklbw xmm4, xmm7;
		punpcklbw xmm5, xmm7;

		pmullw xmm0, xmm2;		// mm0 = src1 * alpha1
		pmullw xmm1, xmm3;		// mm1 = src2 * alpha2

		psubb xmm4, xmm2;			// mm4 = (0xff - a1)...
		psubb xmm5, xmm3;			// mm5 = (0xff - a2)...

		movq2dq xmm2, mm2;		// mm2 = dst1
		movq2dq xmm3, mm3;		// mm3 = dst2
		punpcklbw xmm2, xmm7;
		punpcklbw xmm3, xmm7;
		pmullw xmm2, xmm4;		// mm2 = dst1 * (255 - a1)
		pmullw xmm3, xmm5;		// mm3 = dst2 * (255 - a2)

		paddw xmm0, xmm2;
		paddw xmm1, xmm3;

		movdqu xmm2, xmm0;
		movdqu xmm3, xmm1;

		psrlw xmm2, 8;
		psrlw xmm3, 8;
		paddw xmm0, xmm2;
		paddw xmm1, xmm3;
		psrlw xmm0, 8;
		psrlw xmm1, 8;
		packuswb xmm0, xmm0;
		packuswb xmm1, xmm1;

		movq qword ptr [edi], xmm0;
		movq qword ptr [edi + 8], xmm1;
	}
#else
	int i = 0; 
	for (i = 0; i < 4; i++) {
		*(IUINT32*)dst = ipixel_sse2_blend(*(IUINT32*)dst, *(IUINT32*)src);
	}
#endif
}


//---------------------------------------------------------------------
// pixel convert
//---------------------------------------------------------------------
#define pixel_cvt_from(fmt, bits) _ipixel_cvt_from_##fmt(bits)
#define pixel_cvt_to(fmt, bits, pixel) _ipixel_cvt_to_##fmt(bits, pixel)

static inline IUINT8 _ipixel_alpha_with_mask(int alpha, int mask) {
	IUINT32 pos = (((alpha) & 0xfc) << 4) | ((mask) >> 2);
	return _ipixel_alpha_mask_lut[pos];
}


//---------------------------------------------------------------------
// span drawing
//---------------------------------------------------------------------
#define MMX_SPAN_DRAW_PROC(fmt, nbytes) \
static void mmx_span_draw_##fmt(void *bits, int startx, int w,  \
	const IUINT32 *card, const IUINT8 *cover, const iColorIndex *index) \
{ \
	unsigned char *ptr1 = (unsigned char*)bits + (startx * nbytes); \
	unsigned char *ptr2 = (unsigned char*)card; \
	IUINT32 c1, c2, alpha; \
	int i = 1; \
	if (cover == NULL) { \
		for (i = w; i > 0; i--) { \
			alpha = ((IUINT8*)ptr2)[3]; \
			c1 = ((IUINT32*)ptr2)[0]; \
			if (alpha == 255) { \
				pixel_cvt_to(fmt, ptr1, c1); \
			} \
			else if (alpha > 0) { \
				c2 = pixel_cvt_from(fmt, ptr1); \
				c1 = ipixel_mmx_blend(c2, c1); \
				pixel_cvt_to(fmt, ptr1, c1); \
			} \
			ptr1 += nbytes; \
			ptr2 += 4; \
		} \
	}	else { \
		for (i = w; i > 0; i--) { \
			alpha = ((IUINT8*)ptr2)[3]; \
			c1 = ((IUINT32*)ptr2)[0]; \
			alpha = _ipixel_alpha_with_mask(alpha, *cover++); \
			if (alpha == 255) { \
				pixel_cvt_to(fmt, ptr1, c1); \
			} \
			else if (alpha > 0) { \
				c1 = (c1 & 0xffffff) | (alpha << 24); \
				c2 = pixel_cvt_from(fmt, ptr1); \
				c1 = ipixel_mmx_blend(c2, c1); \
				pixel_cvt_to(fmt, ptr1, c1); \
			} \
			ptr1 += nbytes; \
			ptr2 += 4; \
		} \
	} \
	immx_emms(); \
} 


MMX_SPAN_DRAW_PROC(X8R8G8B8, 4);
MMX_SPAN_DRAW_PROC(X8B8G8R8, 4);
MMX_SPAN_DRAW_PROC(R8G8B8X8, 4);
MMX_SPAN_DRAW_PROC(B8G8R8X8, 4);

MMX_SPAN_DRAW_PROC(R8G8B8, 3);
MMX_SPAN_DRAW_PROC(B8G8R8, 3);

MMX_SPAN_DRAW_PROC(R5G6B5, 2);
MMX_SPAN_DRAW_PROC(B5G6R5, 2);
MMX_SPAN_DRAW_PROC(X1R5G5B5, 2);
MMX_SPAN_DRAW_PROC(X1B5G5R5, 2);
MMX_SPAN_DRAW_PROC(R5G5B5X1, 2);
MMX_SPAN_DRAW_PROC(B5G5R5X1, 2);

#undef MMX_SPAN_DRAW_PROC


static void pixellib_mmx_init_span(void)
{
	ipixel_set_span_proc(IPIX_FMT_X8R8G8B8, 0, mmx_span_draw_X8R8G8B8);
	ipixel_set_span_proc(IPIX_FMT_X8B8G8R8, 0, mmx_span_draw_X8B8G8R8);
	ipixel_set_span_proc(IPIX_FMT_R8G8B8X8, 0, mmx_span_draw_R8G8B8X8);
	ipixel_set_span_proc(IPIX_FMT_B8G8R8X8, 0, mmx_span_draw_B8G8R8X8);

	ipixel_set_span_proc(IPIX_FMT_R8G8B8, 0, mmx_span_draw_R8G8B8);
	ipixel_set_span_proc(IPIX_FMT_B8G8R8, 0, mmx_span_draw_B8G8R8);

	ipixel_set_span_proc(IPIX_FMT_R5G6B5, 0, mmx_span_draw_R5G6B5);
	ipixel_set_span_proc(IPIX_FMT_B5G6R5, 0, mmx_span_draw_B5G6R5);
	ipixel_set_span_proc(IPIX_FMT_X1R5G5B5, 0, mmx_span_draw_X1R5G5B5);
	ipixel_set_span_proc(IPIX_FMT_X1B5G5R5, 0, mmx_span_draw_X1B5G5R5);
	ipixel_set_span_proc(IPIX_FMT_R5G5B5X1, 0, mmx_span_draw_R5G5B5X1);
	ipixel_set_span_proc(IPIX_FMT_B5G5R5X1, 0, mmx_span_draw_B5G5R5X1);
}



//---------------------------------------------------------------------
// Ë®Æ½ÏßÌî³ä
//---------------------------------------------------------------------
static inline void _ipixel_fill_82(void *bits, int startx, int w, IUINT32 c)
{
	IUINT8 *ptr = (IUINT8*)bits + startx;
	IUINT32 cc = c & 0xff;
	IUINT32 c4 = IRGBA_TO_PIXEL(A8R8G8B8, c, c, c, c);
	IUINT32 c2 = c4 & 0xffff;
	ILINS_LOOP_QUATRO(
		{
			*ptr++ = (IUINT8)cc;
		},
		{
			*(IUINT16*)ptr = (IUINT16)c2;
			ptr += 2;
		},
		{
			*(IUINT32*)ptr = (IUINT32)c4;
			ptr += 4;
		},
		w);
}

static inline void _ipixel_fill_162(void *bits, int startx, int w, IUINT32 c)
{
	IUINT8 *ptr = (IUINT8*)bits + startx * 2;
	IUINT32 cc = c & 0xffff;
	IUINT32 c2 = (cc << 16) | cc;
	ILINS_LOOP_DOUBLE(
		{
			*(IUINT16*)ptr = (IUINT16)cc;
			ptr += 2;
		},
		{
			*(IUINT32*)ptr = c2;
			ptr += 4;
		},
		w);
}

static inline void _ipixel_fill_242(void *bits, int startx, int w, IUINT32 c)
{
	IUINT8 *ptr = (IUINT8*)bits + startx * 3;
	IUINT8 *dst = ptr;
	int i;
	for (i = 12; w > 0 && i > 0; w--, i--) {
		_ipixel_store(24, ptr, 0, c);
		ptr += 3;
	}
	for (; w >= 4; w -= 4) {
		((IUINT32*)ptr)[0] = ((IUINT32*)dst)[0];
		((IUINT32*)ptr)[1] = ((IUINT32*)dst)[1];
		((IUINT32*)ptr)[2] = ((IUINT32*)dst)[2];
		ptr += 12;
	}
	for (; w > 0; w--) {
		_ipixel_store(24, ptr, 0, c);
		ptr += 3;
	}
}

static inline void _ipixel_fill_322(void *bits, int startx, int w, IUINT32 c)
{
	IUINT32 *ptr = (IUINT32*)bits + startx;
	for (; w > 0; w--) *ptr++ = c;
}


//---------------------------------------------------------------------
// initialize mmx
//---------------------------------------------------------------------
int pixellib_mmx_init(void)
{
	static int inited = 0;
	if (inited) return 0;
	_x86_detect();
	_ipixel_cvt_init();
	if (!X86_FEATURE(X86_FEATURE_MMX)) 
		return -1;
	pixellib_mmx_init_span();
	inited = 1;
	return 0;
}



//---------------------------------------------------------------------
// sse2 instructions
//---------------------------------------------------------------------
#if defined(__INLINEGNU__) && defined(__i386__)
	#define sse2_blend_quad_main \
	  "  movq         (%%esi), %%xmm0\n" \
	  "  movq         0x8(%%esi), %%xmm1\n" \
	  "  movq         (%%edi), %%mm2\n" \
	  "  movq         0x8(%%edi), %%mm3\n" \
	  "  movdqu       %%xmm0, %%xmm2\n" \
	  "  movdqu       %%xmm1, %%xmm3\n" \
	  "  psrld        $0x18, %%xmm2\n" \
	  "  psrld        $0x18, %%xmm3\n" \
	  "  packuswb     %%xmm2, %%xmm2\n" \
	  "  packuswb     %%xmm3, %%xmm3\n" \
	  "  pcmpeqb      %%xmm4, %%xmm4\n" \
	  "  pcmpeqb      %%xmm5, %%xmm5\n" \
	  "  punpcklwd    %%xmm2, %%xmm2\n" \
	  "  punpcklwd    %%xmm3, %%xmm3\n" \
	  "  punpcklbw    %%xmm7, %%xmm0\n" \
	  "  punpcklbw    %%xmm7, %%xmm1\n" \
	  "  punpckldq    %%xmm2, %%xmm2\n" \
	  "  punpckldq    %%xmm3, %%xmm3\n" \
	  "  punpcklbw    %%xmm7, %%xmm4\n" \
	  "  punpcklbw    %%xmm7, %%xmm5\n" \
	  "  pmullw       %%xmm2, %%xmm0\n" \
	  "  pmullw       %%xmm3, %%xmm1\n" \
	  "  psubb        %%xmm2, %%xmm4\n" \
	  "  psubb        %%xmm3, %%xmm5\n" \
	  "  movq2dq      %%mm2, %%xmm2\n"  \
	  "  movq2dq      %%mm3, %%xmm3\n"  \
	  "  punpcklbw    %%xmm7, %%xmm2\n" \
	  "  punpcklbw    %%xmm7, %%xmm3\n" \
	  "  pmullw       %%xmm4, %%xmm2\n" \
	  "  pmullw       %%xmm5, %%xmm3\n" \
	  "  paddw        %%xmm2, %%xmm0\n" \
	  "  paddw        %%xmm3, %%xmm1\n" \
	  "  movdqu       %%xmm0, %%xmm2\n" \
	  "  movdqu       %%xmm1, %%xmm3\n" \
	  "  psrlw        $0x8, %%xmm2\n"   \
	  "  psrlw        $0x8, %%xmm3\n"   \
	  "  paddw        %%xmm2, %%xmm0\n" \
	  "  paddw        %%xmm3, %%xmm1\n" \
	  "  psrlw        $0x8, %%xmm0\n" \
	  "  psrlw        $0x8, %%xmm1\n" \
	  "  packuswb     %%xmm0, %%xmm0\n" \
	  "  packuswb     %%xmm1, %%xmm1\n" \
	  "  movq         %%xmm0, (%%edi)\n" \
	  "  movq         %%xmm1, 0x8(%%edi)\n" 

#elif defined(__INLINEGNU__)
	#define sse2_blend_quad_main \
	  "  movq         (%%rsi), %%xmm0\n" \
	  "  movq         0x8(%%rsi), %%xmm1\n" \
	  "  movq         (%%rdi), %%mm2\n" \
	  "  movq         0x8(%%rdi), %%mm3\n" \
	  "  movdqu       %%xmm0, %%xmm2\n" \
	  "  movdqu       %%xmm1, %%xmm3\n" \
	  "  psrld        $0x18, %%xmm2\n" \
	  "  psrld        $0x18, %%xmm3\n" \
	  "  packuswb     %%xmm2, %%xmm2\n" \
	  "  packuswb     %%xmm3, %%xmm3\n" \
	  "  pcmpeqb      %%xmm4, %%xmm4\n" \
	  "  pcmpeqb      %%xmm5, %%xmm5\n" \
	  "  punpcklwd    %%xmm2, %%xmm2\n" \
	  "  punpcklwd    %%xmm3, %%xmm3\n" \
	  "  punpcklbw    %%xmm7, %%xmm0\n" \
	  "  punpcklbw    %%xmm7, %%xmm1\n" \
	  "  punpckldq    %%xmm2, %%xmm2\n" \
	  "  punpckldq    %%xmm3, %%xmm3\n" \
	  "  punpcklbw    %%xmm7, %%xmm4\n" \
	  "  punpcklbw    %%xmm7, %%xmm5\n" \
	  "  pmullw       %%xmm2, %%xmm0\n" \
	  "  pmullw       %%xmm3, %%xmm1\n" \
	  "  psubb        %%xmm2, %%xmm4\n" \
	  "  psubb        %%xmm3, %%xmm5\n" \
	  "  movq2dq      %%mm2, %%xmm2\n"  \
	  "  movq2dq      %%mm3, %%xmm3\n"  \
	  "  punpcklbw    %%xmm7, %%xmm2\n" \
	  "  punpcklbw    %%xmm7, %%xmm3\n" \
	  "  pmullw       %%xmm4, %%xmm2\n" \
	  "  pmullw       %%xmm5, %%xmm3\n" \
	  "  paddw        %%xmm2, %%xmm0\n" \
	  "  paddw        %%xmm3, %%xmm1\n" \
	  "  movdqu       %%xmm0, %%xmm2\n" \
	  "  movdqu       %%xmm1, %%xmm3\n" \
	  "  psrlw        $0x8, %%xmm2\n"   \
	  "  psrlw        $0x8, %%xmm3\n"   \
	  "  paddw        %%xmm2, %%xmm0\n" \
	  "  paddw        %%xmm3, %%xmm1\n" \
	  "  psrlw        $0x8, %%xmm0\n" \
	  "  psrlw        $0x8, %%xmm1\n" \
	  "  packuswb     %%xmm0, %%xmm0\n" \
	  "  packuswb     %%xmm1, %%xmm1\n" \
	  "  movq         %%xmm0, (%%rdi)\n" \
	  "  movq         %%xmm1, 0x8(%%rdi)\n" 

#elif defined(__INLINEMSC__)
	#define sse2_blend_quad_main \
		movq xmm0, qword ptr [esi]; \
		movq xmm1, qword ptr [esi + 8]; \
		movq mm2, [edi]; \
		movq mm3, [edi + 8]; \
		movdqu xmm2, xmm0; \
		movdqu xmm3, xmm1; \
		psrld xmm2, 24; \
		psrld xmm3, 24; \
		packuswb xmm2, xmm2; \
		packuswb xmm3, xmm3; \
		pcmpeqb xmm4, xmm4;	\
		pcmpeqb xmm5, xmm5;	\
		punpcklwd xmm2, xmm2; \
		punpcklwd xmm3, xmm3; \
		punpcklbw xmm0, xmm7; \
		punpcklbw xmm1, xmm7; \
		punpckldq xmm2, xmm2; \
		punpckldq xmm3, xmm3; \
		punpcklbw xmm4, xmm7; \
		punpcklbw xmm5, xmm7; \
		pmullw xmm0, xmm2;	\
		pmullw xmm1, xmm3;	\
		psubb xmm4, xmm2;	\
		psubb xmm5, xmm3;	\
		movq2dq xmm2, mm2;		\
		movq2dq xmm3, mm3;		\
		punpcklbw xmm2, xmm7; \
		punpcklbw xmm3, xmm7; \
		pmullw xmm2, xmm4;	  \
		pmullw xmm3, xmm5;	  \
		paddw xmm0, xmm2;  \
		paddw xmm1, xmm3;  \
		movdqu xmm2, xmm0; \
		movdqu xmm3, xmm1; \
		psrlw xmm2, 8;  \
		psrlw xmm3, 8;  \
		paddw xmm0, xmm2;  \
		paddw xmm1, xmm3;  \
		psrlw xmm0, 8;  \
		psrlw xmm1, 8;  \
		packuswb xmm0, xmm0;  \
		packuswb xmm1, xmm1;  \
		movq qword ptr [edi], xmm0;  \
		movq qword ptr [edi + 8], xmm1;

#endif


void sse2_span_draw_X8R8G8B8(void *bits, int startx, int w,  
	const IUINT32 *card, const IUINT8 *cover, const iColorIndex *index) 
{
	unsigned char *ptr1 = (unsigned char*)bits + (startx << 2);
	unsigned char *ptr2 = (unsigned char*)card;
	int i;

	if (cover) {
		mmx_span_draw_X8R8G8B8(bits, startx, w, card, cover, index);
		return;
	}

#if defined(__INLINEGNU__) && defined(__i386__)
	__asm__ __volatile__ (
	  ASM_BEGIN
	  "  mov %0, %%esi\n"
	  "  mov %1, %%edi\n"
	  "  mov %2, %%ecx\n"
	  "  shr $2, %%ecx\n"
	  "  pxor %%xmm7, %%xmm7\n"

	  "1: \n"

	  sse2_blend_quad_main
	  
	  "  add $16, %%esi\n"
	  "  add $16, %%edi\n"
	  "  dec %%ecx\n"
	  "  jnz 1b\n"

	  ASM_ENDUP
	  :
	  :"m"(ptr2), "m"(ptr1), "m"(w)
	  :"memory", "esi", "edi"
	);
#elif defined(__INLINEGNU__)
	size_t x1, x2; 
	__asm__ __volatile__ (
	  ASM_BEGIN
	  "  mov %%rsi, %0\n"
	  "  mov %%rdi, %1\n"
	  "  mov %2, %%rsi\n"
	  "  mov %3, %%rdi\n"
	  "  mov %4, %%ecx\n"
	  "  shr $2, %%ecx\n"
	  "  pxor %%xmm7, %%xmm7\n"

	  "1: \n"

	  sse2_blend_quad_main
	  
	  "  add $16, %%rsi\n"
	  "  add $16, %%rdi\n"
	  "  dec %%ecx\n"
	  "  jnz 1b\n"

	  ASM_ENDUP
	  :"=m"(x1), "=m"(x2)
	  :"m"(ptr2), "m"(ptr1), "m"(w)
	  :"memory", "esi", "edi"
	);
#elif defined(__INLINEMSC__)
	_asm {
		mov esi, ptr2;
		mov edi, ptr1;
		mov ecx, w;
		shr ecx, 2;
		pxor xmm7, xmm7;

	loop_pixel:
		sse2_blend_quad_main

		add esi, 16;
		add edi, 16;
		dec ecx;
		jnz loop_pixel;
	}
#endif

	ptr1 += 4 * (w & ~3);
	ptr2 += 4 * (w & ~3);

	for (i = (w & 3); i > 0; ptr1 += 4, ptr2 += 4, i--) {
		IUINT32 c1 = *(IUINT32*)ptr1;
		IUINT32 c2 = *(IUINT32*)ptr2;
		*(IUINT32*)ptr1 = ipixel_sse2_blend(c1, c2);
	}

	immx_emms();
}


int pixellib_xmm_init(void)
{
//	ipixel_set_span_proc(IPIX_FMT_X8R8G8B8, 0, sse2_span_draw_X8R8G8B8);
	return 0;
}


