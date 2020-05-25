/**********************************************************************
 *
 * ibmbits.h - bitmap bits accessing
 *
 * Maintainer: skywind3000 (at) gmail.com, 2009, 2010, 2011, 2013
 * Homepage: https://github.com/skywind3000/pixellib
 *
 * FEATURES:
 *
 * - up to 64 pixel-formats supported
 * - fetching pixel from any pixel-formats to card(A8R8G8B8)
 * - storing card(A8R8G8B8) to any pixel-formats
 * - drawing scanline (copy/blend/additive) to any pixel-formats
 * - blit (with/without color key) between bitmaps
 * - converting between any format to another one
 * - compositing scanline with 35 operators
 * - many useful macros to access/process pixels
 * - all the routines can be replaced by calling **_set_**
 * - using 203KB static memory for look-up-tables
 * - self contained, C89 compatible
 *
 * INTERFACES:
 *
 * - ipixel_get_fetch: get function to fetch pixels into A8R8G8B8 format.
 * - ipixel_get_store: get function to store pixels from A8R8G8B8 format.
 * - ipixel_get_span_proc: get function to draw scanline with cover.
 * - ipixel_get_hline_proc: get function to draw horizontal line with cover.
 * - ipixel_blit: blit between two bitmaps with same color depth.
 * - ipixel_blend: blend between two bitmaps with different pixel-format.
 * - ipixel_convert: convert pixel-format.
 * - ipixel_fmt_init: initialize customized pixel-format with rgb masks.
 * - ipixel_fmt_cvt: convert between customized pixel-formats.
 * - ipixel_composite_get: get scanline compositor with 35 operators.
 * - ipixel_clip: valid rectangle clip.
 * - ipixel_set_dots: batch draw dots from the position array to a bitmap.
 * - ipixel_palette_fit: fint best fit color in the palette.
 * - ipixel_palette_fetch: fetch from color index buffer to A8R8G8B8.
 * - ipixel_palette_store: store from A8R8G8B8 to color index buffer.
 * - ipixel_card_reverse: reverse A8R8G8B8 scanline.
 * - ipixel_card_shuffle: color components shuffle.
 * - ipixel_card_multi: A8R8G8B8 scanline color multiplication.
 * - ipixel_card_cover: A8R8G8B8 scanline coverage.
 *
 * HISTORY:
 *
 * 2009.09.12  skywind  create this file
 * 2009.12.07  skywind  optimize fetch and store
 * 2009.12.30  skywind  optimize 16 bits convertion using lut
 * 2010.01.05  skywind  implement scanline blending & additive
 * 2010.03.07  skywind  implement hline blending & additive
 * 2010.10.25  skywind  implement bliting and converting and cliping
 * 2011.03.04  skywind  implement card operations
 * 2011.08.10  skywind  implement compositing
 * 2013.09.11  skywind  free format converting
 *
 **********************************************************************/
#ifndef _IBMBITS_H_
#define _IBMBITS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/**********************************************************************
 * DEFINITION OF INTEGERS
 **********************************************************************/
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__
#if defined(__UINT32_TYPE__) && defined(__UINT32_TYPE__)
	typedef __UINT32_TYPE__ ISTDUINT32;
	typedef __INT32_TYPE__ ISTDINT32;
#elif defined(__UINT_FAST32_TYPE__) && defined(__INT_FAST32_TYPE__)
	typedef __UINT_FAST32_TYPE__ ISTDUINT32;
	typedef __INT_FAST32_TYPE__ ISTDINT32;
#elif defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(_M_AMD64)
	typedef unsigned int ISTDUINT32;
	typedef int ISTDINT32;
#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
	typedef unsigned long ISTDUINT32;
	typedef long ISTDINT32;
#elif defined(__MACOS__)
	typedef UInt32 ISTDUINT32;
	typedef SInt32 ISTDINT32;
#elif defined(__APPLE__) && defined(__MACH__)
	#include <sys/types.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif defined(__BEOS__)
	#include <sys/inttypes.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif (defined(_MSC_VER) || defined(__BORLANDC__)) && (!defined(__MSDOS__))
	typedef unsigned __int32 ISTDUINT32;
	typedef __int32 ISTDINT32;
#elif defined(__GNUC__) && (__GNUC__ > 3)
	#include <stdint.h>
	typedef uint32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#else 
	typedef unsigned long ISTDUINT32; 
	typedef long ISTDINT32;
#endif
#endif

#ifndef __IINT8_DEFINED
    #define __IINT8_DEFINED
    typedef char IINT8;
#endif

#ifndef __IUINT8_DEFINED
    #define __IUINT8_DEFINED
    typedef unsigned char IUINT8;
#endif

#ifndef __IUINT16_DEFINED
    #define __IUINT16_DEFINED
    typedef unsigned short IUINT16;
#endif

#ifndef __IINT16_DEFINED
    #define __IINT16_DEFINED
    typedef short IINT16;
#endif

#ifndef __IINT32_DEFINED
    #define __IINT32_DEFINED
    typedef ISTDINT32 IINT32;
#endif

#ifndef __IUINT32_DEFINED
    #define __IUINT32_DEFINED
    typedef ISTDUINT32 IUINT32;
#endif



/**********************************************************************
 * DETECTION WORD ORDER
 **********************************************************************/
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MISPEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif

#ifdef __MSDOS__
	#error This file cannot be compiled in 16 bits (only 32, 64 bits)
#endif


/**********************************************************************
 * PIXEL FORMAT 
 **********************************************************************/
#ifndef IPIX_FMT_COUNT

/* pixel format: 32 bits */
#define IPIX_FMT_A8R8G8B8        0
#define IPIX_FMT_A8B8G8R8        1
#define IPIX_FMT_R8G8B8A8        2
#define IPIX_FMT_B8G8R8A8        3
#define IPIX_FMT_X8R8G8B8        4
#define IPIX_FMT_X8B8G8R8        5
#define IPIX_FMT_R8G8B8X8        6
#define IPIX_FMT_B8G8R8X8        7
#define IPIX_FMT_P8R8G8B8        8

/* pixel format: 24 bits */
#define IPIX_FMT_R8G8B8          9
#define IPIX_FMT_B8G8R8          10

/* pixel format: 16 bits */
#define IPIX_FMT_R5G6B5          11
#define IPIX_FMT_B5G6R5          12
#define IPIX_FMT_X1R5G5B5        13
#define IPIX_FMT_X1B5G5R5        14
#define IPIX_FMT_R5G5B5X1        15
#define IPIX_FMT_B5G5R5X1        16
#define IPIX_FMT_A1R5G5B5        17
#define IPIX_FMT_A1B5G5R5        18
#define IPIX_FMT_R5G5B5A1        19
#define IPIX_FMT_B5G5R5A1        20
#define IPIX_FMT_X4R4G4B4        21
#define IPIX_FMT_X4B4G4R4        22
#define IPIX_FMT_R4G4B4X4        23
#define IPIX_FMT_B4G4R4X4        24
#define IPIX_FMT_A4R4G4B4        25
#define IPIX_FMT_A4B4G4R4        26
#define IPIX_FMT_R4G4B4A4        27
#define IPIX_FMT_B4G4R4A4        28

/* pixel format: 8 bits */
#define IPIX_FMT_C8              29
#define IPIX_FMT_G8              30
#define IPIX_FMT_A8              31
#define IPIX_FMT_R3G3B2          32
#define IPIX_FMT_B2G3R3          33
#define IPIX_FMT_X2R2G2B2        34
#define IPIX_FMT_X2B2G2R2        35
#define IPIX_FMT_R2G2B2X2        36
#define IPIX_FMT_B2G2R2X2        37
#define IPIX_FMT_A2R2G2B2        38
#define IPIX_FMT_A2B2G2R2        39
#define IPIX_FMT_R2G2B2A2        40
#define IPIX_FMT_B2G2R2A2        41
#define IPIX_FMT_X4C4            42
#define IPIX_FMT_X4G4            43
#define IPIX_FMT_X4A4            44
#define IPIX_FMT_C4X4            45
#define IPIX_FMT_G4X4            46
#define IPIX_FMT_A4X4            47

/* pixel format: 4 bits */
#define IPIX_FMT_C4              48
#define IPIX_FMT_G4              49
#define IPIX_FMT_A4              50
#define IPIX_FMT_R1G2B1          51
#define IPIX_FMT_B1G2R1          52
#define IPIX_FMT_A1R1G1B1        53
#define IPIX_FMT_A1B1G1R1        54
#define IPIX_FMT_R1G1B1A1        55
#define IPIX_FMT_B1G1R1A1        56
#define IPIX_FMT_X1R1G1B1        57
#define IPIX_FMT_X1B1G1R1        58
#define IPIX_FMT_R1G1B1X1        59
#define IPIX_FMT_B1G1R1X1        60

/* pixel format: 1 bit */
#define IPIX_FMT_C1              61
#define IPIX_FMT_G1              62
#define IPIX_FMT_A1              63

#define IPIX_FMT_COUNT           64
#define IPIX_FMT_UNKNOWN         (-1)

/* pixel format types */
#define IPIX_FMT_TYPE_ARGB       0
#define IPIX_FMT_TYPE_RGB        1
#define IPIX_FMT_TYPE_INDEX      2
#define IPIX_FMT_TYPE_ALPHA      3
#define IPIX_FMT_TYPE_GRAY       4
#define IPIX_FMT_TYPE_PREMUL     5

#endif



/**********************************************************************
 * Global Structures
 **********************************************************************/

/* pixel format descriptor */
struct IPIXELFMT
{
    int format;              /* pixel format code       */
    int bpp;                 /* bits per pixel          */
    int alpha;               /* 0: no alpha, 1: alpha   */
    int type;                /* IPIX_FMT_TYPE_...       */
    int pixelbyte;           /* nbytes per pixel        */
    const char *name;        /* name string             */
    IUINT32 rmask;           /* red component mask      */
    IUINT32 gmask;           /* green component mask    */
    IUINT32 bmask;           /* blue component mask     */
    IUINT32 amask;           /* alpha component mask    */
    int rshift;              /* red component shift     */
    int gshift;              /* green component shift   */
    int bshift;              /* blue component shift    */
    int ashift;              /* alpha component shift   */
    int rloss;               /* red component loss      */
    int gloss;               /* green component loss    */
    int bloss;               /* blue component loss     */
    int aloss;               /* alpha component loss    */
};

/* RGB descript */
struct IRGB
{
	unsigned char r, g, b;
	unsigned char reserved;
};

/* color index: used to lookup colors */
struct iColorIndex
{
    int color;                   /* how many colors can be used */
    IUINT32 rgba[256];           /* C8 -> A8R8G8B8 lookup       */
    unsigned char ent[32768];    /* X1R5G5B5 -> C8 lookup       */
};


typedef struct IRGB IRGB;
typedef struct iColorIndex iColorIndex;
typedef struct IPIXELFMT iPixelFmt;


/* scanline fetch: fetching from different pixel formats to A8R8GB8 */
typedef void (*iFetchProc)(const void *bits, int x, int w, IUINT32 *buffer, 
	const iColorIndex *idx);

/* scanline store: storing from A8R8G8B8 to other formats */
typedef void (*iStoreProc)(void *bits, const IUINT32 *buffer, int x, int w, 
	const iColorIndex *idx);

/* pixel fetch: fetching color from diferent pixel format to A8R8G8B8 */
typedef IUINT32 (*iFetchPixelProc)(const void *bits, int offset, 
	const iColorIndex *idx);


#ifndef INLINE
#ifdef __GNUC__
#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
    #define INLINE         __inline__ __attribute__((always_inline))
#else
    #define INLINE         __inline__
#endif
#elif defined(_MSC_VER)
	#define INLINE __forceinline
#elif (defined(__BORLANDC__) || defined(__WATCOMC__))
    #define INLINE __inline
#else
    #define INLINE 
#endif
#endif

#ifndef inline
    #define inline INLINE
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************
 * Global Variables
 **********************************************************************/

/* pixel format declaration */
extern const struct IPIXELFMT ipixelfmt[IPIX_FMT_COUNT];

/* color component bit scale */
extern const IUINT32 _ipixel_scale_1[2];
extern const IUINT32 _ipixel_scale_2[4];
extern const IUINT32 _ipixel_scale_3[8];
extern const IUINT32 _ipixel_scale_4[16];
extern const IUINT32 _ipixel_scale_5[32];
extern const IUINT32 _ipixel_scale_6[64];

/* default index */
extern iColorIndex *_ipixel_src_index;
extern iColorIndex *_ipixel_dst_index;

/* default palette */
extern IRGB _ipaletted[256];

extern const unsigned char IMINMAX256[770];
extern const unsigned char *iclip256;

/* pixel alpha lookup table: initialized by ipixel_lut_init() */
extern unsigned char ipixel_blend_lut[2048 * 2];

/* multiplication & division table: initialized by ipixel_lut_init() */
extern unsigned char _ipixel_mullut[256][256];  /* [x][y] = x * y / 255 */
extern unsigned char _ipixel_divlut[256][256];  /* [x][y] = y * 255 / x */

/* component scale for 0-8 bits */
extern unsigned char _ipixel_fmt_scale[9][256];

#define ICLIP_256(x) IMINMAX256[256 + (x)]
#define ICLIP_FAST(x) ( (IUINT32) ( (IUINT8) ((x) | (0 - ((x) >> 8))) ) )


#define IPIXEL_FORMAT_BPP(pixfmt)      ipixelfmt[pixfmt].bpp
#define IPIXEL_FORMAT_TYPE(pixfmt)     ipixelfmt[pixfmt].type
#define IPIXEL_FORMAT_NAME(pixfmt)     ipixelfmt[pixfmt].name
#define IPIXEL_FORMAT_ALPHA(pixfmt)    ipixelfmt[pixfmt].alpha

#define IPIXEL_FORMAT_RMASK(pixfmt)    ipixelfmt[pixfmt].rmask
#define IPIXEL_FORMAT_GMASK(pixfmt)    ipixelfmt[pixfmt].gmask
#define IPIXEL_FORMAT_BMASK(pixfmt)    ipixelfmt[pixfmt].bmask
#define IPIXEL_FORMAT_AMASK(pixfmt)    ipixelfmt[pixfmt].amask

#define IPIXEL_FORMAT_RSHIFT(pixfmt)   ipixelfmt[pixfmt].rshift
#define IPIXEL_FORMAT_GSHIFT(pixfmt)   ipixelfmt[pixfmt].gshift
#define IPIXEL_FORMAT_BSHIFT(pixfmt)   ipixelfmt[pixfmt].bshift
#define IPIXEL_FORMAT_ASHIFT(pixfmt)   ipixelfmt[pixfmt].ashift

#define IPIXEL_FORMAT_RLOSS(pixfmt)    ipixelfmt[pixfmt].rloss
#define IPIXEL_FORMAT_GLOSS(pixfmt)    ipixelfmt[pixfmt].gloss
#define IPIXEL_FORMAT_BLOSS(pixfmt)    ipixelfmt[pixfmt].bloss
#define IPIXEL_FORMAT_ALOSS(pixfmt)    ipixelfmt[pixfmt].aloss


/**********************************************************************
 * BITS ACCESSING
 **********************************************************************/

#define IPIXEL_ACCESS_MODE_NORMAL		0		/* fast mode, with lut */
#define IPIXEL_ACCESS_MODE_ACCURATE		1		/* fast mode without lut */
#define IPIXEL_ACCESS_MODE_BUILTIN		2		/* default accurate */

/* get color fetching procedure */
iFetchProc ipixel_get_fetch(int pixfmt, int access_mode);

/* get color storing procedure */
iStoreProc ipixel_get_store(int pixfmt, int access_mode);

/* get color pixel fetching procedure */
iFetchPixelProc ipixel_get_fetchpixel(int pixfmt, int access_mode);

#define IPIXEL_PROC_TYPE_FETCH			0
#define IPIXEL_PROC_TYPE_STORE			1
#define IPIXEL_PROC_TYPE_FETCHPIXEL		2

/* set procedure */
void ipixel_set_proc(int pixfmt, int type, void *proc);


/* find the best fit color in palette */
int ipixel_palette_fit(const IRGB *pal, int r, int g, int b, int palsize);

/* convert palette to index */
int ipalette_to_index(iColorIndex *index, const IRGB *pal, int palsize);

/* get raw color */
IUINT32 ipixel_assemble(int pixfmt, int r, int g, int b, int a);

/* get r, g, b, a from color */
void ipixel_desemble(int pixfmt, IUINT32 c, IINT32 *r, IINT32 *g, 
	IINT32 *b, IINT32 *a);

/* stand along memcpy */
void *ipixel_memcpy(void *dst, const void *src, size_t size);
	

/* returns pixel format names */
const char *ipixelfmt_name(int fmt);



/**********************************************************************
 * SPAN DRAWING
 **********************************************************************/

/* span drawing proc */
typedef void (*iSpanDrawProc)(void *bits, int startx, int w, 
	const IUINT32 *card, const IUINT8 *cover, const iColorIndex *index);

/* get a span drawing function with given pixel format */
iSpanDrawProc ipixel_get_span_proc(int fmt, int op, int builtin);

/* set a span drawing function */
void ipixel_set_span_proc(int fmt, int op, iSpanDrawProc proc);


/* blending options */
#define IPIXEL_BLEND_OP_COPY		0
#define IPIXEL_BLEND_OP_BLEND		1
#define IPIXEL_BLEND_OP_SRCOVER		2
#define IPIXEL_BLEND_OP_ADD			3

#define IPIXEL_FLIP_NONE			0
#define IPIXEL_FLIP_HFLIP			1
#define IPIXEL_FLIP_VFLIP			2


/* ipixel_blend - blend between two formats 
 * you must provide a working memory pointer to workmem. if workmem eq NULL,
 * this function will do nothing but returns how many bytes needed in workmem
 * dfmt        - dest pixel format
 * dbits       - dest pixel buffer
 * dpitch      - dest row stride
 * dx          - dest x offset
 * sfmt        - source pixel format
 * sbits       - source pixel buffer
 * spitch      - source row stride
 * sx          - source x offset
 * w           - width
 * h           - height
 * color       - const color
 * op          - blending operator (IPIXEL_BLEND_OP_BLEND, ADD, COPY)
 * flip        - flip (IPIXEL_FLIP_NONE, HFLIP, VFLIP)
 * dindex      - dest index
 * sindex      - source index
 * workmem     - working memory
 * this function need some memory to work with. to avoid allocating, 
 * you must provide a memory block whose size is (w * 4) to it.
 */
long ipixel_blend(int dfmt, void *dbits, long dpitch, int dx, int sfmt, 
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 color,
	int op, int flip, const iColorIndex *dindex, 
	const iColorIndex *sindex, void *workmem);



/**********************************************************************
 * HLINE
 **********************************************************************/

/* draw hline routine */
typedef void (*iHLineDrawProc)(void *bits, int startx, int w,
	IUINT32 col, const IUINT8 *cover, const iColorIndex *index);

/* get a hline drawing function with given pixel format */
iHLineDrawProc ipixel_get_hline_proc(int fmt, int isadd, int usedefault);

/* set a hline drawing function */
void ipixel_set_hline_proc(int fmt, int isadditive, iHLineDrawProc proc);



/**********************************************************************
 * PIXEL BLIT
 **********************************************************************/

#define IPIXEL_BLIT_NORMAL		0
#define IPIXEL_BLIT_MASK		4


/* mask blit procedure: with or without mask (colorkey) */
typedef int (*iBlitProc)(void *dst, long dpitch, int dx, const void *src, 
	long spitch, int sx, int w, int h, IUINT32 mask, int flip);


/* get normal blit procedure */
/* if ismask equals to zero, returns normal bliter */
/* if ismask doesn't equal to zero, returns transparent bliter */
iBlitProc ipixel_blit_get(int bpp, int ismask, int isdefault);

/* set normal blit procedure */
/* if ismask equals to zero, set normal bliter */
/* if ismask doesn't equal to zero, set transparent bliter */
void ipixel_blit_set(int bpp, int ismask, iBlitProc proc);


/* ipixel_blit - bliting (copy pixel from one rectangle to another)
 * it will only copy pixels in the same depth (1/4/8/16/24/32).
 * no color format will be convert (using ipixel_convert to do it)
 * bpp    - color depth of the two bitmap
 * dst    - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * src    - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * for transparent bliting, set mode with IPIXEL_BLIT_MASK, bliter will 
 * skip the colors equal to 'mask' parameter.
 */
void ipixel_blit(int bpp, void *dst, long dpitch, int dx, const void *src, 
	long spitch, int sx, int w, int h, IUINT32 mask, int mode);



/**********************************************************************
 * CARD OPERATION
 **********************************************************************/

/* reverse card */
void ipixel_card_reverse(IUINT32 *card, int size);

/* multi card */
void ipixel_card_multi(IUINT32 *card, int size, IUINT32 color);

/* mask card */
void ipixel_card_mask(IUINT32 *card, int size, const IUINT32 *mask);

/* mask cover */
void ipixel_card_cover(IUINT32 *card, int size, const IUINT8 *cover);

/* card composite: src over */
void ipixel_card_over(IUINT32 *dst, int size, const IUINT32 *card, 
	const IUINT8 *cover);

/* card shuffle */
void ipixel_card_shuffle(IUINT32 *card, int w, int a, int b, int c, int d);

/* card proc set */
void ipixel_card_set_proc(int id, void *proc);


/**********************************************************************
 * CONVERTING
 **********************************************************************/

/* pixel format converting procedure */
typedef int (*iPixelCvt)(void *dbits, long dpitch, int dx, const void *sbits,
	long spitch, int sx, int w, int h, IUINT32 mask, int flip,
	const iColorIndex *dindex, const iColorIndex *sindex);

/* get converting procedure */
iPixelCvt ipixel_cvt_get(int dfmt, int sfmt, int istransparent);

/* set converting procedure */
void ipixel_cvt_set(int dfmt, int sfmt, int istransparent, iPixelCvt proc);

/* get converting procedure */
iPixelCvt ipixel_cvt_get(int dfmt, int sfmt, int istransparent);


/* ipixel_convert: convert pixel format and blit
 * you must provide a working memory pointer to mem. if mem eq NULL,
 * this function will do nothing but returns how many bytes needed in mem
 * dfmt   - dest color format
 * dbits  - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * sfmt   - source color format
 * sbits  - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * didx   - dest color index
 * sidx   - source color index
 * mem    - work memory
 * for transparent converting, set mode with IPIXEL_BLIT_MASK, it will 
 * skip the colors equal to 'mask' parameter.
 */
long ipixel_convert(int dfmt, void *dbits, long dpitch, int dx, int sfmt, 
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 mask, 
	int mode, const iColorIndex *didx, const iColorIndex *sidx, void *mem);


/**********************************************************************
 * FREE FORMAT CONVERT
 **********************************************************************/

/* free format reader procedure */
typedef int (*iPixelFmtReader)(const iPixelFmt *fmt, 
		const void *bits, int x, int w, IUINT32 *card);

/* free format writer procedure */
typedef int (*iPixelFmtWriter)(const iPixelFmt *fmt,
		void *bits, int x, int w, const IUINT32 *card);

/* set free format reader */
void ipixel_fmt_set_reader(int depth, iPixelFmtReader reader);

/* set free format writer */
void ipixel_fmt_set_writer(int depth, iPixelFmtWriter writer);

/* get free format reader */
iPixelFmtReader ipixel_fmt_get_reader(int depth, int isdefault);

/* get free format writer */
iPixelFmtWriter ipixel_fmt_get_writer(int depth, int isdefault);


/* ipixel_fmt_init: init pixel format structure
 * depth: color bits, one of 8, 16, 24, 32
 * rmask: mask for red, eg:   0x00ff0000
 * gmask: mask for green, eg: 0x0000ff00
 * bmask: mask for blue, eg:  0x000000ff
 * amask: mask for alpha, eg: 0xff000000
 */
int ipixel_fmt_init(iPixelFmt *fmt, int depth, 
	IUINT32 rmask, IUINT32 gmask, IUINT32 bmask, IUINT32 amask);


/* ipixel_fmt_cvt: free format convert
 * you must provide a working memory pointer to mem. if mem eq NULL,
 * this function will do nothing but returns how many bytes needed in mem
 * dfmt   - dest pixel format structure
 * dbits  - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * sfmt   - source pixel format structure
 * sbits  - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * mem    - work memory
 * for transparent converting, set mode with IPIXEL_BLIT_MASK, it will 
 * skip the colors equal to 'mask' parameter.
 */
long ipixel_fmt_cvt(const iPixelFmt *dfmt, void *dbits, long dpitch, 
	int dx, const iPixelFmt *sfmt, const void *sbits, long spitch, int sx,
	int sw, int sh, IUINT32 mask, int mode, void *mem);


/**********************************************************************
 * CLIPPING
 **********************************************************************/

/*
 * ipixel_clip - clip the rectangle from the src clip and dst clip then
 * caculate a new rectangle which is shared between dst and src cliprect:
 * clipdst  - dest clip array (left, top, right, bottom)
 * clipsrc  - source clip array (left, top, right, bottom)
 * (x, y)   - dest position
 * rectsrc  - source rect
 * mode     - check IPIXEL_FLIP_HFLIP or IPIXEL_FLIP_VFLIP
 * return zero for successful, return non-zero if there is no shared part
 */
int ipixel_clip(const int *clipdst, const int *clipsrc, int *x, int *y,
    int *rectsrc, int mode);


/**********************************************************************
 * COMPOSITE
 **********************************************************************/

/* pixel composite procedure */
typedef void (*iPixelComposite)(IUINT32 *dst, const IUINT32 *src, int width);

/* pixel composite operator */
#define IPIXEL_OP_SRC				0
#define IPIXEL_OP_DST				1
#define IPIXEL_OP_CLEAR				2
#define IPIXEL_OP_BLEND				3
#define IPIXEL_OP_ADD				4
#define IPIXEL_OP_SUB				5
#define IPIXEL_OP_SUB_INV			6
#define IPIXEL_OP_XOR				7
#define IPIXEL_OP_PLUS				8
#define IPIXEL_OP_SRC_ATOP			9
#define IPIXEL_OP_SRC_IN			10
#define IPIXEL_OP_SRC_OUT			11
#define IPIXEL_OP_SRC_OVER			12
#define IPIXEL_OP_DST_ATOP			13
#define IPIXEL_OP_DST_IN			14
#define IPIXEL_OP_DST_OUT			15
#define IPIXEL_OP_DST_OVER			16
#define IPIXEL_OP_PREMUL_XOR		17
#define IPIXEL_OP_PREMUL_PLUS		18
#define IPIXEL_OP_PREMUL_SRC_OVER	19
#define IPIXEL_OP_PREMUL_SRC_IN		20
#define IPIXEL_OP_PREMUL_SRC_OUT	21
#define IPIXEL_OP_PREMUL_SRC_ATOP	22
#define IPIXEL_OP_PREMUL_DST_OVER	23
#define IPIXEL_OP_PREMUL_DST_IN		24
#define IPIXEL_OP_PREMUL_DST_OUT	25
#define IPIXEL_OP_PREMUL_DST_ATOP	26
#define IPIXEL_OP_PREMUL_BLEND		27
#define IPIXEL_OP_ALLANON			28
#define IPIXEL_OP_TINT				29
#define IPIXEL_OP_DIFF				30
#define IPIXEL_OP_DARKEN			31
#define IPIXEL_OP_LIGHTEN			32
#define IPIXEL_OP_SCREEN			33
#define IPIXEL_OP_OVERLAY			34


/* get composite operator names */
iPixelComposite ipixel_composite_get(int op, int isdefault);

/* set compositor */
void ipixel_composite_set(int op, iPixelComposite composite);

/* get op names */
const char *ipixel_composite_opname(int op);



/**********************************************************************
 * Palette & Others
 **********************************************************************/

/* fetch card from IPIX_FMT_C8 */
void ipixel_palette_fetch(const unsigned char *src, int w, IUINT32 *card, 
	const IRGB *palette);

/* store card into IPIX_FMT_C8 */
void ipixel_palette_store(unsigned char *dst, int w, const IUINT32 *card, 
	const IRGB *palette, int palsize);

/* batch draw dots */
int ipixel_set_dots(int bpp, void *bits, long pitch, int w, int h,
	IUINT32 color, const short *xylist, int count, const int *clip);


/**********************************************************************
 * MACRO: Pixel Fetching & Storing
 **********************************************************************/
#define _ipixel_fetch_8(ptr, offset)  (((const IUINT8 *)(ptr))[offset])
#define _ipixel_fetch_16(ptr, offset) (((const IUINT16*)(ptr))[offset])
#define _ipixel_fetch_32(ptr, offset) (((const IUINT32*)(ptr))[offset])

#define _ipixel_fetch_24_lsb(ptr, offset) \
    ( (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[0]) <<  0 ) | \
      (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[1]) <<  8 ) | \
      (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[2]) << 16 ))

#define _ipixel_fetch_24_msb(ptr, offset) \
    ( (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[0]) << 16 ) | \
      (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[1]) <<  8 ) | \
      (((IUINT32)(((const IUINT8*)(ptr)) + (offset) * 3)[2]) <<  0 ))

#define _ipixel_fetch_4_lsb(ptr, offset) \
    (((offset) & 1)? (_ipixel_fetch_8(ptr, (offset) >> 1) >> 4) : \
     (_ipixel_fetch_8(ptr, (offset) >> 1) & 0xf))

#define _ipixel_fetch_4_msb(ptr, offset) \
    (((offset) & 1)? (_ipixel_fetch_8(ptr, (offset) >> 1) & 0xf) : \
     (_ipixel_fetch_8(ptr, (offset) >> 1) >> 4))

#define _ipixel_fetch_1_lsb(ptr, offset) \
    ((_ipixel_fetch_8(ptr, (offset) >> 3) >> ((offset) & 7)) & 1)

#define _ipixel_fetch_1_msb(ptr, offset) \
    ((_ipixel_fetch_8(ptr, (offset) >> 3) >> (7 - ((offset) & 7))) & 1)


#define _ipixel_store_8(ptr, off, c)  (((IUINT8 *)(ptr))[off] = (IUINT8)(c))
#define _ipixel_store_16(ptr, off, c) (((IUINT16*)(ptr))[off] = (IUINT16)(c))
#define _ipixel_store_32(ptr, off, c) (((IUINT32*)(ptr))[off] = (IUINT32)(c))

#define _ipixel_store_24_lsb(ptr, off, c) do { \
        ((IUINT8*)(ptr))[(off) * 3 + 0] = (IUINT8) (((c) >>  0) & 0xff); \
        ((IUINT8*)(ptr))[(off) * 3 + 1] = (IUINT8) (((c) >>  8) & 0xff); \
        ((IUINT8*)(ptr))[(off) * 3 + 2] = (IUINT8) (((c) >> 16) & 0xff); \
    }   while (0)

#define _ipixel_store_24_msb(ptr, off, c)  do { \
        ((IUINT8*)(ptr))[(off) * 3 + 0] = (IUINT8) (((c) >> 16) & 0xff); \
        ((IUINT8*)(ptr))[(off) * 3 + 1] = (IUINT8) (((c) >>  8) & 0xff); \
        ((IUINT8*)(ptr))[(off) * 3 + 2] = (IUINT8) (((c) >>  0) & 0xff); \
    }   while (0)


#define _ipixel_store_4_lsb(ptr, off, c) do { \
        IUINT32 __byte_off = (off) >> 1; \
        IUINT32 __byte_value = _ipixel_fetch_8(ptr, __byte_off); \
        IUINT32 __v4 = (c) & 0xf; \
        if ((off) & 1) { \
            __byte_value = ((__byte_value) & 0x0f) | ((__v4) << 4); \
        }    else { \
            __byte_value = ((__byte_value) & 0xf0) | ((__v4)); \
        } \
        _ipixel_store_8(ptr, __byte_off, __byte_value); \
    }   while (0)

#define _ipixel_store_4_msb(ptr, off, c) do { \
        IUINT32 __byte_off = (off) >> 1; \
        IUINT32 __byte_value = _ipixel_fetch_8(ptr, __byte_off); \
        IUINT32 __v4 = (c) & 0xf; \
        if ((off) & 1) { \
            __byte_value = ((__byte_value) & 0xf0) | ((__v4)); \
        }    else { \
            __byte_value = ((__byte_value) & 0x0f) | ((__v4) << 4); \
        } \
        _ipixel_store_8(ptr, __byte_off, __byte_value); \
    }   while (0)

#define _ipixel_store_1_lsb(ptr, off, c) do { \
        IUINT32 __byte_off = (off) >> 3; \
        IUINT32 __byte_bit = (off) & 7; \
        IUINT32 __byte_value = _ipixel_fetch_8(ptr, __byte_off); \
        __byte_value &= ((~(1 << __byte_bit)) & 0xff); \
        __byte_value |= ((c) & 1) << __byte_bit; \
        _ipixel_store_8(ptr, __byte_off, __byte_value); \
    }   while (0)

#define _ipixel_store_1_msb(ptr, off, c) do { \
        IUINT32 __byte_off = (off) >> 3; \
        IUINT32 __byte_bit = 7 - ((off) & 7); \
        IUINT32 __byte_value = _ipixel_fetch_8(ptr, __byte_off); \
        __byte_value &= ((~(1 << __byte_bit)) & 0xff); \
        __byte_value |= ((c) & 1) << __byte_bit; \
        _ipixel_store_8(ptr, __byte_off, __byte_value); \
    }   while (0)



#define IFETCH24_LSB(a) ( \
	(((IUINT32)(((const unsigned char*)(a))[0]))      ) | \
	(((IUINT32)(((const unsigned char*)(a))[1])) <<  8) | \
	(((IUINT32)(((const unsigned char*)(a))[2])) << 16) )

#define IFETCH24_MSB(a) ( \
	(((IUINT32)(((const unsigned char*)(a))[0])) << 16) | \
	(((IUINT32)(((const unsigned char*)(a))[1])) <<  8) | \
	(((IUINT32)(((const unsigned char*)(a))[2]))      ) )

#define ISTORE24_LSB(a, c) do { \
		((unsigned char*)(a))[0] = (IUINT8)(((c)      ) & 0xff); \
		((unsigned char*)(a))[1] = (IUINT8)(((c) >>  8) & 0xff); \
		((unsigned char*)(a))[2] = (IUINT8)(((c) >> 16) & 0xff); \
	}	while (0)

#define ISTORE24_MSB(a, c) do { \
		((unsigned char*)(a))[0] = (IUINT8)(((c) >> 16) & 0xff); \
		((unsigned char*)(a))[1] = (IUINT8)(((c) >>  8) & 0xff); \
		((unsigned char*)(a))[2] = (IUINT8)(((c)      ) & 0xff); \
	}	while (0)



#if IWORDS_BIG_ENDIAN
#define _ipixel_fetch_24 _ipixel_fetch_24_msb
#define _ipixel_store_24 _ipixel_store_24_msb
#define _ipixel_fetch_4  _ipixel_fetch_4_msb
#define _ipixel_store_4  _ipixel_store_4_msb
#define _ipixel_fetch_1  _ipixel_fetch_1_msb
#define _ipixel_store_1  _ipixel_store_1_msb
#define IFETCH24         IFETCH24_MSB
#define ISTORE24         ISTORE24_MSB
#else
#define _ipixel_fetch_24 _ipixel_fetch_24_lsb
#define _ipixel_store_24 _ipixel_store_24_lsb
#define _ipixel_fetch_4 _ipixel_fetch_4_lsb
#define _ipixel_store_4 _ipixel_store_4_lsb
#define _ipixel_fetch_1 _ipixel_fetch_1_lsb
#define _ipixel_store_1 _ipixel_store_1_lsb
#define IFETCH24         IFETCH24_LSB
#define ISTORE24         ISTORE24_LSB
#endif

#define _ipixel_fetch(nbits, ptr, off) _ipixel_fetch_##nbits(ptr, off)
#define _ipixel_store(nbits, ptr, off, c) _ipixel_store_##nbits(ptr, off, c)



/**********************************************************************
 * MACRO: Pixel Assembly & Disassembly 
 **********************************************************************/

/* assembly 32 bits */
#define _ipixel_asm_8888(a, b, c, d) ((IUINT32)( \
            ((IUINT32)(a) << 24) | \
            ((IUINT32)(b) << 16) | \
            ((IUINT32)(c) <<  8) | \
            ((IUINT32)(d) <<  0)))

/* assembly 24 bits */
#define _ipixel_asm_888(a, b, c) ((IUINT32)( \
            ((IUINT32)(a) << 16) | \
            ((IUINT32)(b) <<  8) | \
            ((IUINT32)(c) <<  0)))

/* assembly 16 bits */
#define _ipixel_asm_1555(a, b, c, d) ((IUINT16)( \
            ((IUINT16)((a) & 0x80) << 8) | \
            ((IUINT16)((b) & 0xf8) << 7) | \
            ((IUINT16)((c) & 0xf8) << 2) | \
            ((IUINT16)((d) & 0xf8) >> 3)))

#define _ipixel_asm_5551(a, b, c, d) ((IUINT16)( \
            ((IUINT16)((a) & 0xf8) << 8) | \
            ((IUINT16)((b) & 0xf8) << 3) | \
            ((IUINT16)((c) & 0xf8) >> 2) | \
            ((IUINT16)((d) & 0x80) >> 7)))

#define _ipixel_asm_565(a, b, c)  ((IUINT16)( \
            ((IUINT16)((a) & 0xf8) << 8) | \
            ((IUINT16)((b) & 0xfc) << 3) | \
            ((IUINT16)((c) & 0xf8) >> 3)))

#define _ipixel_asm_4444(a, b, c, d) ((IUINT16)( \
            ((IUINT16)((a) & 0xf0) << 8) | \
            ((IUINT16)((b) & 0xf0) << 4) | \
            ((IUINT16)((c) & 0xf0) << 0) | \
            ((IUINT16)((d) & 0xf0) >> 4)))

/* assembly 8 bits */
#define _ipixel_asm_332(a, b, c) ((IUINT8)( \
            ((IUINT8)((a) & 0xe0) >>  0) | \
            ((IUINT8)((b) & 0xe0) >>  3) | \
            ((IUINT8)((c) & 0xf0) >>  6)))

#define _ipixel_asm_233(a, b, c) ((IUINT8)( \
            ((IUINT8)((a) & 0xc0) >>  0) | \
            ((IUINT8)((b) & 0xe0) >>  2) | \
            ((IUINT8)((c) & 0xe0) >>  5)))

#define _ipixel_asm_2222(a, b, c, d) ((IUINT8)( \
            ((IUINT8)((a) & 0xc0) >> 0) | \
            ((IUINT8)((b) & 0xc0) >> 2) | \
            ((IUINT8)((c) & 0xc0) >> 4) | \
            ((IUINT8)((d) & 0xc0) >> 6)))

/* assembly 4 bits */
#define _ipixel_asm_1111(a, b, c, d) ((IUINT8)( \
            ((IUINT8)((a) & 0x80) >> 4) | \
            ((IUINT8)((a) & 0x80) >> 5) | \
            ((IUINT8)((a) & 0x80) >> 6) | \
            ((IUINT8)((a) & 0x80) >> 7)))

#define _ipixel_asm_121(a, b, c) ((IUINT8)( \
            ((IUINT8)((a) & 0x80) >> 4) | \
            ((IUINT8)((b) & 0xc0) >> 5) | \
            ((IUINT8)((c) & 0xc0) >> 7)))


/* disassembly 32 bits */
#define _ipixel_disasm_8888(x, a, b, c, d) do { \
            (a) = ((x) >> 24) & 0xff; \
            (b) = ((x) >> 16) & 0xff; \
            (c) = ((x) >>  8) & 0xff; \
            (d) = ((x) >>  0) & 0xff; \
        }   while (0)

#define _ipixel_disasm_888X(x, a, b, c) do { \
            (a) = ((x) >> 24) & 0xff; \
            (b) = ((x) >> 16) & 0xff; \
            (c) = ((x) >>  8) & 0xff; \
        }   while (0)

#define _ipixel_disasm_X888(x, a, b, c) do { \
            (a) = ((x) >> 16) & 0xff; \
            (b) = ((x) >>  8) & 0xff; \
            (c) = ((x) >>  0) & 0xff; \
        }   while (0)

/* disassembly 24 bits */
#define _ipixel_disasm_888(x, a, b, c) do { \
            (a) = ((x) >> 16) & 0xff; \
            (b) = ((x) >>  8) & 0xff; \
            (c) = ((x) >>  0) & 0xff; \
        }   while (0)

/* disassembly 16 bits */
#define _ipixel_disasm_1555(x, a, b, c, d) do { \
            (a) = _ipixel_scale_1[(x) >> 15]; \
            (b) = _ipixel_scale_5[((x) >> 10) & 0x1f]; \
            (c) = _ipixel_scale_5[((x) >>  5) & 0x1f]; \
            (d) = _ipixel_scale_5[((x) >>  0) & 0x1f]; \
        }   while (0)

#define _ipixel_disasm_X555(x, a, b, c) do { \
            (a) = _ipixel_scale_5[((x) >> 10) & 0x1f]; \
            (b) = _ipixel_scale_5[((x) >>  5) & 0x1f]; \
            (c) = _ipixel_scale_5[((x) >>  0) & 0x1f]; \
        }   while (0)

#define _ipixel_disasm_5551(x, a, b, c, d) do { \
            (a) = _ipixel_scale_5[((x) >> 11) & 0x1f]; \
            (b) = _ipixel_scale_5[((x) >>  6) & 0x1f]; \
            (c) = _ipixel_scale_5[((x) >>  1) & 0x1f]; \
            (d) = _ipixel_scale_1[((x) >>  0) & 0x01]; \
        }   while (0)

#define _ipixel_disasm_555X(x, a, b, c) do { \
            (a) = _ipixel_scale_5[((x) >> 11) & 0x1f]; \
            (b) = _ipixel_scale_5[((x) >>  6) & 0x1f]; \
            (c) = _ipixel_scale_5[((x) >>  1) & 0x1f]; \
        }   while (0)

#define _ipixel_disasm_565(x, a, b, c) do { \
            (a) = _ipixel_scale_5[((x) >> 11) & 0x1f]; \
            (b) = _ipixel_scale_6[((x) >>  5) & 0x3f]; \
            (c) = _ipixel_scale_5[((x) >>  0) & 0x1f]; \
        }   while (0)

#define _ipixel_disasm_4444(x, a, b, c, d) do { \
            (a) = _ipixel_scale_4[((x) >> 12) & 0xf]; \
            (b) = _ipixel_scale_4[((x) >>  8) & 0xf]; \
            (c) = _ipixel_scale_4[((x) >>  4) & 0xf]; \
            (d) = _ipixel_scale_4[((x) >>  0) & 0xf]; \
        }   while (0)

#define _ipixel_disasm_X444(x, a, b, c) do { \
            (a) = _ipixel_scale_4[((x) >>  8) & 0xf]; \
            (b) = _ipixel_scale_4[((x) >>  4) & 0xf]; \
            (c) = _ipixel_scale_4[((x) >>  0) & 0xf]; \
        }   while (0)

#define _ipixel_disasm_444X(x, a, b, c) do { \
            (a) = _ipixel_scale_4[((x) >> 12) & 0xf]; \
            (b) = _ipixel_scale_4[((x) >>  8) & 0xf]; \
            (c) = _ipixel_scale_4[((x) >>  4) & 0xf]; \
        }   while (0)

/* disassembly 8 bits */
#define _ipixel_disasm_233(x, a, b, c) do { \
            (a) = _ipixel_scale_2[((x) >>  6) & 3]; \
            (b) = _ipixel_scale_3[((x) >>  3) & 7]; \
            (c) = _ipixel_scale_3[((x) >>  0) & 7]; \
        }   while (0)

#define _ipixel_disasm_332(x, a, b, c) do { \
            (a) = _ipixel_scale_3[((x) >>  5) & 7]; \
            (b) = _ipixel_scale_3[((x) >>  2) & 7]; \
            (c) = _ipixel_scale_2[((x) >>  0) & 3]; \
        }   while (0)

#define _ipixel_disasm_2222(x, a, b, c, d) do { \
            (a) = _ipixel_scale_2[((x) >>  6) & 3]; \
            (b) = _ipixel_scale_2[((x) >>  4) & 3]; \
            (c) = _ipixel_scale_2[((x) >>  2) & 3]; \
            (d) = _ipixel_scale_2[((x) >>  0) & 3]; \
        }   while (0)

#define _ipixel_disasm_X222(x, a, b, c) do { \
            (a) = _ipixel_scale_2[((x) >>  4) & 3]; \
            (b) = _ipixel_scale_2[((x) >>  2) & 3]; \
            (c) = _ipixel_scale_2[((x) >>  0) & 3]; \
        }   while (0)

#define _ipixel_disasm_222X(x, a, b, c) do { \
            (a) = _ipixel_scale_2[((x) >>  6) & 3]; \
            (b) = _ipixel_scale_2[((x) >>  4) & 3]; \
            (c) = _ipixel_scale_2[((x) >>  2) & 3]; \
        }   while (0)

/* disassembly 4 bits */
#define _ipixel_disasm_1111(x, a, b, c, d) do { \
            (a) = _ipixel_scale_1[((x) >> 3) & 1]; \
            (b) = _ipixel_scale_1[((x) >> 2) & 1]; \
            (c) = _ipixel_scale_1[((x) >> 1) & 1]; \
            (d) = _ipixel_scale_1[((x) >> 0) & 1]; \
        }   while (0)

#define _ipixel_disasm_X111(x, a, b, c) do { \
            (a) = _ipixel_scale_1[((x) >> 2) & 1]; \
            (b) = _ipixel_scale_1[((x) >> 1) & 1]; \
            (c) = _ipixel_scale_1[((x) >> 0) & 1]; \
        }   while (0)

#define _ipixel_disasm_111X(x, a, b, c) do { \
            (a) = _ipixel_scale_1[((x) >> 3) & 1]; \
            (b) = _ipixel_scale_1[((x) >> 2) & 1]; \
            (c) = _ipixel_scale_1[((x) >> 1) & 1]; \
        }   while (0)

#define _ipixel_disasm_121(x, a, b, c) do { \
            (a) = _ipixel_scale_1[((x) >> 3) & 1]; \
            (b) = _ipixel_scale_2[((x) >> 1) & 3]; \
            (c) = _ipixel_scale_1[((x) >> 0) & 1]; \
        }   while (0)


/**********************************************************************
 * MACRO: Color Convertion
 **********************************************************************/
#define _ipixel_norm(color) (((color) >> 7) + (color))
#define _ipixel_unnorm(color) ((((color) << 8) - (color)) >> 8)
#define _imul_y_div_255(x, y) (((x) * _ipixel_norm(y)) >> 8)
#define _ipixel_fast_div_255(x) (((x) + (((x) + 257) >> 8)) >> 8)

#define _ipixel_to_gray(r, g, b) \
        ((19595 * (r) + 38469 * (g) + 7472 * (b)) >> 16)

#define _ipixel_to_pmul(c, r, g, b, a) do { \
            IUINT32 __a = (a); \
            IUINT32 __b = (__a); \
            IUINT32 __X1 = __a; \
            IUINT32 __X2 = ((r) * __b); \
            IUINT32 __X3 = ((g) * __b); \
            IUINT32 __X4 = ((b) * __b); \
            __X2 = (__X2 + (__X2 >> 8)) >> 8; \
            __X3 = (__X3 + (__X3 >> 8)) >> 8; \
            __X4 = (__X4 + (__X4 >> 8)) >> 8; \
            c = _ipixel_asm_8888(__X1, __X2, __X3, __X4); \
        }   while (0)

#define _ipixel_from_pmul(c, r, g, b, a) do { \
            IUINT32 __SA = ((c) >> 24); \
            IUINT32 __FA = (__SA); \
            (a) = __SA; \
            if (__FA > 0) { \
                (r) = ((((c) >> 16) & 0xff) * 255) / __FA; \
                (g) = ((((c) >>  8) & 0xff) * 255) / __FA; \
                (b) = ((((c) >>  0) & 0xff) * 255) / __FA; \
            }    else { \
                (r) = 0; (g) = 0; (b) = 0; \
            }    \
        }   while (0)

#define _ipixel_RGBA_to_P8R8G8B8(r, g, b, a) ( \
        (((a)) << 24) | \
        ((((r) * _ipixel_norm(a)) >> 8) << 16) | \
        ((((g) * _ipixel_norm(a)) >> 8) <<  8) | \
        ((((b) * _ipixel_norm(a)) >> 8) <<  0))

#define _ipixel_R8G8B8_to_R5G5B5(c) \
    ((((c) >> 3) & 0x001f) | (((c) >> 6) & 0x03e0) | (((c) >> 9) & 0x7c00))


#define _ipixel_R5G5B5_to_ent(index, c) ((index)->ent[c])
#define _ipixel_A8R8G8B8_from_index(index, c) ((index)->rgba[c])


#define _ipixel_R8G8B8_to_ent(index, c) \
        _ipixel_R5G5B5_to_ent(index, _ipixel_R8G8B8_to_R5G5B5(c))
#define _ipixel_RGB_to_ent(index, r, g, b) \
        _ipixel_R5G5B5_to_ent(index, _ipixel_asm_1555(0, r, g, b))

#define _ipixel_RGB_from_index(index, c, r, g, b) do { \
            IUINT32 __rgba = _ipixel_A8R8G8B8_from_index(index, c); \
            _ipixel_disasm_X888(__rgba, r, g, b);  \
        } while (0)

#define _ipixel_RGBA_from_index(index, c, r, g, b, a) do { \
            IUINT32 __rgba = _ipixel_A8R8G8B8_from_index(index, c); \
            _ipixel_disasm_8888(__rgba, a, r, g, b); \
        } while (0)

#define ISPLIT_ARGB(c, a, r, g, b) do { \
            (a) = (((IUINT32)(c)) >> 24); \
            (r) = (((IUINT32)(c)) >> 16) & 0xff; \
            (g) = (((IUINT32)(c)) >>  8) & 0xff; \
            (b) = (((IUINT32)(c))      ) & 0xff; \
        } while (0)

#define ISPLIT_RGB(c, r, g, b) do { \
            (r) = (((IUINT32)(c)) >> 16) & 0xff; \
            (g) = (((IUINT32)(c)) >>  8) & 0xff; \
            (b) = (((IUINT32)(c))      ) & 0xff; \
        } while (0)

#define _ipixel_fetch_bpp(bpp, ptr, pos, c) do { \
            switch (bpp) { \
            case  1: c = _ipixel_fetch( 1, ptr, pos); break; \
            case  4: c = _ipixel_fetch( 4, ptr, pos); break; \
            case  8: c = _ipixel_fetch( 8, ptr, pos); break; \
            case 16: c = _ipixel_fetch(16, ptr, pos); break; \
            case 24: c = _ipixel_fetch(24, ptr, pos); break; \
            case 32: c = _ipixel_fetch(32, ptr, pos); break; \
            default: c = 0; break; \
            } \
        } while (0)

#define _ipixel_store_bpp(bpp, ptr, pos, c) do { \
            switch (bpp) { \
            case  1: _ipixel_store( 1, ptr, pos, c); break; \
            case  4: _ipixel_store( 4, ptr, pos, c); break; \
            case  8: _ipixel_store( 8, ptr, pos, c); break; \
            case 16: _ipixel_store(16, ptr, pos, c); break; \
            case 24: _ipixel_store(24, ptr, pos, c); break; \
            case 32: _ipixel_store(32, ptr, pos, c); break; \
            } \
        } while (0)

#define _ipixel_load_card_lsb(ptr, r, g, b, a) do { \
			(a) = ((IUINT8*)(ptr))[3]; \
			(r) = ((IUINT8*)(ptr))[2]; \
			(g) = ((IUINT8*)(ptr))[1]; \
			(b) = ((IUINT8*)(ptr))[0]; \
		} while (0)

#define _ipixel_load_card_msb(ptr, r, g, b, a) do { \
			(a) = ((IUINT8*)(ptr))[0]; \
			(r) = ((IUINT8*)(ptr))[1]; \
			(g) = ((IUINT8*)(ptr))[2]; \
			(b) = ((IUINT8*)(ptr))[3]; \
		} while (0)


#if IWORDS_BIG_ENDIAN
	#define _ipixel_load_card	_ipixel_load_card_msb
#else
	#define _ipixel_load_card	_ipixel_load_card_lsb
#endif

#if IWORDS_BIG_ENDIAN
	#define _ipixel_card_alpha	0
#else
	#define _ipixel_card_alpha	3
#endif


/**********************************************************************
 * LIN's LOOP UNROLL MACROs: 
 * Actually Duff's unroll macro isn't compatible with vc7 because
 * of non-standard usage of 'switch' & 'for' statement. 
 * the macros below are standard implementation of loop unroll
 **********************************************************************/
#ifndef ILINS_LOOP_QUATRO
#define ILINS_LOOP_QUATRO(actionx1, actionx2, actionx4, width) do { \
    unsigned long __width = (unsigned long)(width);    \
    unsigned long __increment = __width >> 2; \
    for (; __increment > 0; __increment--) { actionx4; }    \
    if (__width & 2) { actionx2; } \
    if (__width & 1) { actionx1; } \
}   while (0)
#endif

#ifndef ILINS_LOOP_DOUBLE
#define ILINS_LOOP_DOUBLE(actionx1, actionx2, width) do { \
	unsigned long __width = (unsigned long)(width); \
	unsigned long __increment = __width >> 2; \
	for (; __increment > 0; __increment--) { actionx2; actionx2; } \
	if (__width & 2) { actionx2; } \
	if (__width & 1) { actionx1; }  \
}	while (0)
#endif

#ifndef ILINS_LOOP_ONCE
#define ILINS_LOOP_ONCE(action, width) do { \
    unsigned long __width = (unsigned long)(width); \
    for (; __width > 0; __width--) { action; } \
}   while (0)
#endif



/**********************************************************************
 * MACRO: PIXEL DISASSEMBLE
 **********************************************************************/

/* pixel format: 32 bits */
#define IRGBA_FROM_A8R8G8B8(c, r, g, b, a) _ipixel_disasm_8888(c, a, r, g, b)
#define IRGBA_FROM_A8B8G8R8(c, r, g, b, a) _ipixel_disasm_8888(c, a, b, g, r)
#define IRGBA_FROM_R8G8B8A8(c, r, g, b, a) _ipixel_disasm_8888(c, r, g, b, a)
#define IRGBA_FROM_B8G8R8A8(c, r, g, b, a) _ipixel_disasm_8888(c, b, g, r, a)
#define IRGBA_FROM_X8R8G8B8(c, r, g, b, a) do { \
        _ipixel_disasm_X888(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_X8B8G8R8(c, r, g, b, a) do { \
        _ipixel_disasm_X888(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_R8G8B8X8(c, r, g, b, a) do { \
        _ipixel_disasm_888X(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B8G8R8X8(c, r, g, b, a) do { \
        _ipixel_disasm_888X(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_P8R8G8B8(c, r, g, b, a) do { \
        _ipixel_from_pmul(c, r, g, b, a); } while (0)

/* pixel format: 24 bits */
#define IRGBA_FROM_R8G8B8(c, r, g, b, a) do { \
        _ipixel_disasm_888(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B8G8R8(c, r, g, b, a) do { \
        _ipixel_disasm_888(c, b, g, r); (a) = 255; } while (0)

/* pixel format: 16 bits */
#define IRGBA_FROM_R5G6B5(c, r, g, b, a) do { \
        _ipixel_disasm_565(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B5G6R5(c, r, g, b, a) do { \
        _ipixel_disasm_565(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_X1R5G5B5(c, r, g, b, a) do { \
        _ipixel_disasm_X555(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_X1B5G5R5(c, r, g, b, a) do { \
        _ipixel_disasm_X555(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_R5G5B5X1(c, r, g, b, a) do { \
        _ipixel_disasm_555X(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B5G5R5X1(c, r, g, b, a) do { \
        _ipixel_disasm_555X(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_A1R5G5B5(c, r, g, b, a) _ipixel_disasm_1555(c, a, r, g, b)
#define IRGBA_FROM_A1B5G5R5(c, r, g, b, a) _ipixel_disasm_1555(c, a, b, g, r)
#define IRGBA_FROM_R5G5B5A1(c, r, g, b, a) _ipixel_disasm_5551(c, r, g, b, a)
#define IRGBA_FROM_B5G5R5A1(c, r, g, b, a) _ipixel_disasm_5551(c, b, g, r, a)
#define IRGBA_FROM_X4R4G4B4(c, r, g, b, a) do { \
        _ipixel_disasm_X444(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_X4B4G4R4(c, r, g, b, a) do { \
        _ipixel_disasm_X444(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_R4G4B4X4(c, r, g, b, a) do { \
        _ipixel_disasm_444X(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B4G4R4X4(c, r, g, b, a) do { \
        _ipixel_disasm_444X(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_A4R4G4B4(c, r, g, b, a) _ipixel_disasm_4444(c, a, r, g, b)
#define IRGBA_FROM_A4B4G4R4(c, r, g, b, a) _ipixel_disasm_4444(c, a, b, g, r)
#define IRGBA_FROM_R4G4B4A4(c, r, g, b, a) _ipixel_disasm_4444(c, r, g, b, a)
#define IRGBA_FROM_B4G4R4A4(c, r, g, b, a) _ipixel_disasm_4444(c, b, g, r, a)

/* pixel format: 8 bits */
#define IRGBA_FROM_C8(c, r, g, b, a) do { \
        _ipixel_RGBA_from_index(_ipixel_src_index, c, r, g, b, a); } while (0)
#define IRGBA_FROM_G8(c, r, g, b, a) do { \
        (r) = (g) = (b) = (c); (a) = 255; } while (0)
#define IRGBA_FROM_A8(c, r, g, b, a) do { \
        (r) = (g) = (b) = 0; (a) = (c); } while (0)

#define IRGBA_FROM_R3G3B2(c, r, g, b, a) do { \
        _ipixel_disasm_332(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B2G3R3(c, r, g, b, a) do { \
        _ipixel_disasm_233(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_X2R2G2B2(c, r, g, b, a) do { \
        _ipixel_disasm_X222(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_X2B2G2R2(c, r, g, b, a) do { \
        _ipixel_disasm_X222(c, b, g, r); (a) = 255; } while (0) 
#define IRGBA_FROM_R2G2B2X2(c, r, g, b, a) do { \
        _ipixel_disasm_222X(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B2G2R2X2(c, r, g, b, a) do { \
        _ipixel_disasm_222X(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_A2R2G2B2(c, r, g, b, a) _ipixel_disasm_2222(c, a, r, g, b)
#define IRGBA_FROM_A2B2G2R2(c, r, g, b, a) _ipixel_disasm_2222(c, a, b, g, r)
#define IRGBA_FROM_R2G2B2A2(c, r, g, b, a) _ipixel_disasm_2222(c, r, g, b, a)
#define IRGBA_FROM_B2G2R2A2(c, r, g, b, a) _ipixel_disasm_2222(c, b, g, r, a)

#define IRGBA_FROM_X4C4(c, r, g, b, a) do { \
        _ipixel_RGBA_from_index(_ipixel_src_index, c, r, g, b, a); \
        } while (0)
#define IRGBA_FROM_X4G4(c, r, g, b, a) do { \
        (r) = (g) = (b) = _ipixel_scale_4[c]; (a) = 255; } while (0)
#define IRGBA_FROM_X4A4(c, r, g, b, a) do { \
        (r) = (g) = (b) = 0; (a) = _ipixel_scale_4[c]; } while (0)
#define IRGBA_FROM_C4X4(c, r, g, b, a) do { \
        _ipixel_RGBA_from_index(_ipixel_src_index, (c) >> 4, r, g, b, a); \
        } while (0)
#define IRGBA_FROM_G4X4(c, r, g, b, a) do { \
        (r) = (g) = (b) = _ipixel_scale_4[(c) >> 4]; (a) = 255; } while (0)
#define IRGBA_FROM_A4X4(c, r, g, b, a) do { \
        (r) = (g) = (b) = 0; (a) = _ipixel_scale_4[(c) >> 4]; } while (0)

/* pixel format: 4 bits */
#define IRGBA_FROM_C4(c, r, g, b, a) IRGBA_FROM_X4C4(c, r, g, b, a)
#define IRGBA_FROM_G4(c, r, g, b, a) IRGBA_FROM_X4G4(c, r, g, b, a)
#define IRGBA_FROM_A4(c, r, g, b, a) IRGBA_FROM_X4A4(c, r, g, b, a)
#define IRGBA_FROM_R1G2B1(c, r, g, b, a) do { \
        _ipixel_disasm_121(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B1G2R1(c, r, g, b, a) do { \
        _ipixel_disasm_121(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_A1R1G1B1(c, r, g, b, a) _ipixel_disasm_1111(c, a, r, g, b)
#define IRGBA_FROM_A1B1G1R1(c, r, g, b, a) _ipixel_disasm_1111(c, a, b, g, r)
#define IRGBA_FROM_R1G1B1A1(c, r, g, b, a) _ipixel_disasm_1111(c, r, g, b, a)
#define IRGBA_FROM_B1G1R1A1(c, r, g, b, a) _ipixel_disasm_1111(c, b, g, r, a)
#define IRGBA_FROM_X1R1G1B1(c, r, g, b, a) do { \
        _ipixel_disasm_X111(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_X1B1G1R1(c, r, g, b, a) do { \
        _ipixel_disasm_X111(c, b, g, r); (a) = 255; } while (0)
#define IRGBA_FROM_R1G1B1X1(c, r, g, b, a) do { \
        _ipixel_disasm_111X(c, r, g, b); (a) = 255; } while (0)
#define IRGBA_FROM_B1G1R1X1(c, r, g, b, a) do { \
        _ipixel_disasm_111X(c, b, g, r); (a) = 255; } while (0)

/* pixel format: 1 bit */
#define IRGBA_FROM_C1(c, r, g, b, a) do { \
        _ipixel_RGBA_from_index(_ipixel_src_index, c, r, g, b, a); \
        } while (0)
#define IRGBA_FROM_G1(c, r, g, b, a) do { \
        (r) = (g) = (b) = _ipixel_scale_1[c]; (a) = 255; } while (0)
#define IRGBA_FROM_A1(c, r, g, b, a) do { \
        (r) = (g) = (b) = 0; (a) = _ipixel_scale_1[c]; } while (0)


/**********************************************************************
 * MACRO: PIXEL ASSEMBLE
 **********************************************************************/

/* pixel format: 32 bits */
#define IRGBA_TO_A8R8G8B8(r, g, b, a)  _ipixel_asm_8888(a, r, g, b)
#define IRGBA_TO_A8B8G8R8(r, g, b, a)  _ipixel_asm_8888(a, b, g, r)
#define IRGBA_TO_R8G8B8A8(r, g, b, a)  _ipixel_asm_8888(r, g, b, a)
#define IRGBA_TO_B8G8R8A8(r, g, b, a)  _ipixel_asm_8888(b, g, r, a)
#define IRGBA_TO_X8R8G8B8(r, g, b, a)  _ipixel_asm_8888(0, r, g, b)
#define IRGBA_TO_X8B8G8R8(r, g, b, a)  _ipixel_asm_8888(0, b, g, r)
#define IRGBA_TO_R8G8B8X8(r, g, b, a)  _ipixel_asm_8888(r, g, b, 0)
#define IRGBA_TO_B8G8R8X8(r, g, b, a)  _ipixel_asm_8888(b, g, r, 0)
#define IRGBA_TO_P8R8G8B8(r, g, b, a)  _ipixel_RGBA_to_P8R8G8B8(r, g, b, a)

/* pixel format: 24 bits */
#define IRGBA_TO_R8G8B8(r, g, b, a)   _ipixel_asm_888(r, g, b)
#define IRGBA_TO_B8G8R8(r, g, b, a)   _ipixel_asm_888(b, g, r)

/* pixel format: 16 bits */
#define IRGBA_TO_R5G6B5(r, g, b, a)   _ipixel_asm_565(r, g, b)
#define IRGBA_TO_B5G6R5(r, g, b, a)   _ipixel_asm_565(b, g, r)
#define IRGBA_TO_X1R5G5B5(r, g, b, a) _ipixel_asm_1555(0, r, g, b)
#define IRGBA_TO_X1B5G5R5(r, g, b, a) _ipixel_asm_1555(0, b, g, r)
#define IRGBA_TO_R5G5B5X1(r, g, b, a) _ipixel_asm_5551(r, g, b, 0)
#define IRGBA_TO_B5G5R5X1(r, g, b, a) _ipixel_asm_5551(b, g, r, 0)
#define IRGBA_TO_A1R5G5B5(r, g, b, a) _ipixel_asm_1555(a, r, g, b)
#define IRGBA_TO_A1B5G5R5(r, g, b, a) _ipixel_asm_1555(a, b, g, r)
#define IRGBA_TO_R5G5B5A1(r, g, b, a) _ipixel_asm_5551(r, g, b, a)
#define IRGBA_TO_B5G5R5A1(r, g, b, a) _ipixel_asm_5551(b, g, r, a)
#define IRGBA_TO_X4R4G4B4(r, g, b, a) _ipixel_asm_4444(0, r, g, b)
#define IRGBA_TO_X4B4G4R4(r, g, b, a) _ipixel_asm_4444(0, b, g, r)
#define IRGBA_TO_R4G4B4X4(r, g, b, a) _ipixel_asm_4444(r, g, b, 0)
#define IRGBA_TO_B4G4R4X4(r, g, b, a) _ipixel_asm_4444(b, g, r, 0)
#define IRGBA_TO_A4R4G4B4(r, g, b, a) _ipixel_asm_4444(a, r, g, b)
#define IRGBA_TO_A4B4G4R4(r, g, b, a) _ipixel_asm_4444(a, b, g, r)
#define IRGBA_TO_R4G4B4A4(r, g, b, a) _ipixel_asm_4444(r, g, b, a)
#define IRGBA_TO_B4G4R4A4(r, g, b, a) _ipixel_asm_4444(b, g, r, a)

/* pixel format: 8 bits */
#define IRGBA_TO_C8(r, g, b, a) \
        _ipixel_RGB_to_ent(_ipixel_dst_index, r, g, b)
#define IRGBA_TO_G8(r, g, b, a)       _ipixel_to_gray(r, g, b)
#define IRGBA_TO_A8(r, g, b, a)       (a)
#define IRGBA_TO_R3G3B2(r, g, b, a)   _ipixel_asm_332(r, g, b)
#define IRGBA_TO_B2G3R3(r, g, b, a)   _ipixel_asm_233(b, g, r)
#define IRGBA_TO_X2R2G2B2(r, g, b, a) _ipixel_asm_2222(0, r, g, b)
#define IRGBA_TO_X2B2G2R2(r, g, b, a) _ipixel_asm_2222(0, b, g, r)
#define IRGBA_TO_R2G2B2X2(r, g, b, a) _ipixel_asm_2222(r, g, b, 0)
#define IRGBA_TO_B2G2R2X2(r, g, b, a) _ipixel_asm_2222(b, g, r, 0)
#define IRGBA_TO_A2R2G2B2(r, g, b, a) _ipixel_asm_2222(a, r, g, b)
#define IRGBA_TO_A2B2G2R2(r, g, b, a) _ipixel_asm_2222(a, b, g, r)
#define IRGBA_TO_R2G2B2A2(r, g, b, a) _ipixel_asm_2222(r, g, b, a)
#define IRGBA_TO_B2G2R2A2(r, g, b, a) _ipixel_asm_2222(b, g, r, a)
#define IRGBA_TO_X4C4(r, g, b, a)     (IRGBA_TO_C8(r, g, b, a) & 0xf)
#define IRGBA_TO_X4G4(r, g, b, a)     (IRGBA_TO_G8(r, g, b, a) >>  4)
#define IRGBA_TO_X4A4(r, g, b, a)     (IRGBA_TO_A8(r, g, b, a) >>  4)
#define IRGBA_TO_C4X4(r, g, b, a)     ((IRGBA_TO_C8(r, g, b, a) & 0xf) << 4)
#define IRGBA_TO_G4X4(r, g, b, a)     (IRGBA_TO_G8(r, g, b, a) & 0xf0)
#define IRGBA_TO_A4X4(r, g, b, a)     (IRGBA_TO_A8(r, g, b, a) & 0xf0)

/* pixel format: 4 bits */
#define IRGBA_TO_C4(r, g, b, a)       IRGBA_TO_X4C4(r, g, b, a)
#define IRGBA_TO_G4(r, g, b, a)       IRGBA_TO_X4G4(r, g, b, a)
#define IRGBA_TO_A4(r, g, b, a)       IRGBA_TO_X4A4(r, g, b, a)
#define IRGBA_TO_R1G2B1(r, g, b, a)   _ipixel_asm_121(r, g, b)
#define IRGBA_TO_B1G2R1(r, g, b, a)   _ipixel_asm_121(b, g, r)
#define IRGBA_TO_A1R1G1B1(r, g, b, a) _ipixel_asm_1111(a, r, g, b)
#define IRGBA_TO_A1B1G1R1(r, g, b, a) _ipixel_asm_1111(a, b, g, r)
#define IRGBA_TO_R1G1B1A1(r, g, b, a) _ipixel_asm_1111(r, g, b, a)
#define IRGBA_TO_B1G1R1A1(r, g, b, a) _ipixel_asm_1111(b, g, r, a)

#define IRGBA_TO_X1R1G1B1(r, g, b, a) _ipixel_asm_1111(0, r, g, b)
#define IRGBA_TO_X1B1G1R1(r, g, b, a) _ipixel_asm_1111(0, b, g, r)
#define IRGBA_TO_R1G1B1X1(r, g, b, a) _ipixel_asm_1111(r, g, b, 0)
#define IRGBA_TO_B1G1R1X1(r, g, b, a) _ipixel_asm_1111(b, g, r, 0)

/* pixel format: 1 bit */
#define IRGBA_TO_C1(r, g, b, a)       (IRGBA_TO_C8(r, g, b, a) & 1)
#define IRGBA_TO_G1(r, g, b, a)       (IRGBA_TO_G8(r, g, b, a) >> 7)
#define IRGBA_TO_A1(r, g, b, a)       (IRGBA_TO_A8(r, g, b, a) >> 7)


/**********************************************************************
 * MACRO: PIXEL FORMAT BPP (bits per pixel)
 **********************************************************************/
#define IPIX_FMT_BPP_A8R8G8B8         32
#define IPIX_FMT_BPP_A8B8G8R8         32
#define IPIX_FMT_BPP_R8G8B8A8         32
#define IPIX_FMT_BPP_B8G8R8A8         32
#define IPIX_FMT_BPP_X8R8G8B8         32
#define IPIX_FMT_BPP_X8B8G8R8         32
#define IPIX_FMT_BPP_R8G8B8X8         32
#define IPIX_FMT_BPP_B8G8R8X8         32
#define IPIX_FMT_BPP_P8R8G8B8         32
#define IPIX_FMT_BPP_R8G8B8           24
#define IPIX_FMT_BPP_B8G8R8           24
#define IPIX_FMT_BPP_R5G6B5           16
#define IPIX_FMT_BPP_B5G6R5           16
#define IPIX_FMT_BPP_X1R5G5B5         16
#define IPIX_FMT_BPP_X1B5G5R5         16
#define IPIX_FMT_BPP_R5G5B5X1         16
#define IPIX_FMT_BPP_B5G5R5X1         16
#define IPIX_FMT_BPP_A1R5G5B5         16
#define IPIX_FMT_BPP_A1B5G5R5         16
#define IPIX_FMT_BPP_R5G5B5A1         16
#define IPIX_FMT_BPP_B5G5R5A1         16
#define IPIX_FMT_BPP_X4R4G4B4         16
#define IPIX_FMT_BPP_X4B4G4R4         16
#define IPIX_FMT_BPP_R4G4B4X4         16
#define IPIX_FMT_BPP_B4G4R4X4         16
#define IPIX_FMT_BPP_A4R4G4B4         16
#define IPIX_FMT_BPP_A4B4G4R4         16
#define IPIX_FMT_BPP_R4G4B4A4         16
#define IPIX_FMT_BPP_B4G4R4A4         16
#define IPIX_FMT_BPP_C8               8
#define IPIX_FMT_BPP_G8               8
#define IPIX_FMT_BPP_A8               8
#define IPIX_FMT_BPP_R2G3B2           7
#define IPIX_FMT_BPP_B2G3R2           7
#define IPIX_FMT_BPP_X2R2G2B2         8
#define IPIX_FMT_BPP_X2B2G2R2         8
#define IPIX_FMT_BPP_R2G2B2X2         8
#define IPIX_FMT_BPP_B2G2R2X2         8
#define IPIX_FMT_BPP_A2R2G2B2         8
#define IPIX_FMT_BPP_A2B2G2R2         8
#define IPIX_FMT_BPP_R2G2B2A2         8
#define IPIX_FMT_BPP_B2G2R2A2         8
#define IPIX_FMT_BPP_X4C4             8
#define IPIX_FMT_BPP_X4G4             8
#define IPIX_FMT_BPP_X4A4             8
#define IPIX_FMT_BPP_C4X4             8
#define IPIX_FMT_BPP_G4X4             8
#define IPIX_FMT_BPP_A4X4             8
#define IPIX_FMT_BPP_C4               4
#define IPIX_FMT_BPP_G4               4
#define IPIX_FMT_BPP_A4               4
#define IPIX_FMT_BPP_R1G2B1           4
#define IPIX_FMT_BPP_B1G2R1           4
#define IPIX_FMT_BPP_A1R1G1B1         4
#define IPIX_FMT_BPP_A1B1G1R1         4
#define IPIX_FMT_BPP_R1G1B1A1         4
#define IPIX_FMT_BPP_B1G1R1A1         4
#define IPIX_FMT_BPP_X1R1G1B1         4
#define IPIX_FMT_BPP_X1B1G1R1         4
#define IPIX_FMT_BPP_R1G1B1X1         4
#define IPIX_FMT_BPP_B1G1R1X1         4
#define IPIX_FMT_BPP_C1               1
#define IPIX_FMT_BPP_G1               1
#define IPIX_FMT_BPP_A1               1



/**********************************************************************
 * MACRO: PIXEL CONVERTION (A8R8G8B8 -> *, * -> A8R8G8B8)
 **********************************************************************/
/* pixel convertion look-up-tables
 * using those table to speed-up 16bits->32bits, 8bits->32bits converting.
 * this is much faster than binary shifting. But look-up-tables must be 
 * initialized by ipixel_lut_init or calling ipixel_get_fetch(0, 0)
 */ 

extern IUINT32 _ipixel_cvt_lut_R5G6B5[512];
extern IUINT32 _ipixel_cvt_lut_B5G6R5[512];
extern IUINT32 _ipixel_cvt_lut_X1R5G5B5[512];
extern IUINT32 _ipixel_cvt_lut_X1B5G5R5[512];
extern IUINT32 _ipixel_cvt_lut_R5G5B5X1[512];
extern IUINT32 _ipixel_cvt_lut_B5G5R5X1[512];
extern IUINT32 _ipixel_cvt_lut_A1R5G5B5[512];
extern IUINT32 _ipixel_cvt_lut_A1B5G5R5[512];
extern IUINT32 _ipixel_cvt_lut_R5G5B5A1[512];
extern IUINT32 _ipixel_cvt_lut_B5G5R5A1[512];
extern IUINT32 _ipixel_cvt_lut_X4R4G4B4[512];
extern IUINT32 _ipixel_cvt_lut_X4B4G4R4[512];
extern IUINT32 _ipixel_cvt_lut_R4G4B4X4[512];
extern IUINT32 _ipixel_cvt_lut_B4G4R4X4[512];
extern IUINT32 _ipixel_cvt_lut_A4R4G4B4[512];
extern IUINT32 _ipixel_cvt_lut_A4B4G4R4[512];
extern IUINT32 _ipixel_cvt_lut_R4G4B4A4[512];
extern IUINT32 _ipixel_cvt_lut_B4G4R4A4[512];
extern IUINT32 _ipixel_cvt_lut_R3G3B2[256];
extern IUINT32 _ipixel_cvt_lut_B2G3R3[256];
extern IUINT32 _ipixel_cvt_lut_X2R2G2B2[256];
extern IUINT32 _ipixel_cvt_lut_X2B2G2R2[256];
extern IUINT32 _ipixel_cvt_lut_R2G2B2X2[256];
extern IUINT32 _ipixel_cvt_lut_B2G2R2X2[256];
extern IUINT32 _ipixel_cvt_lut_A2R2G2B2[256];
extern IUINT32 _ipixel_cvt_lut_A2B2G2R2[256];
extern IUINT32 _ipixel_cvt_lut_R2G2B2A2[256];
extern IUINT32 _ipixel_cvt_lut_B2G2R2A2[256];


/* look-up-tables accessing */
#if IWORDS_BIG_ENDIAN
#define IPIXEL_CVT_LUT_2(fmt, x) \
		(	_ipixel_cvt_lut_##fmt[((x) & 0xff) + 256] | \
			_ipixel_cvt_lut_##fmt[((x) >>   8) +   0] )
#else
#define IPIXEL_CVT_LUT_2(fmt, x) \
		(	_ipixel_cvt_lut_##fmt[((x) & 0xff) +   0] | \
			_ipixel_cvt_lut_##fmt[((x) >>   8) + 256] )
#endif

#define IPIXEL_CVT_LUT_1(fmt, x) _ipixel_cvt_lut_##fmt[(x)]


/* convert from 32 bits */
#define IPIXEL_FROM_A8R8G8B8(x) (x) 
#define IPIXEL_FROM_A8B8G8R8(x) \
		((((x) & 0xff00ff00) | \
		(((x) & 0xff0000) >> 16) | \
		(((x) & 0xff) << 16)))
#define IPIXEL_FROM_R8G8B8A8(x) \
		((((x) & 0xff) << 24) | (((x) & 0xffffff00) >> 8))
#define IPIXEL_FROM_B8G8R8A8(x) \
		(	(((x) & 0x000000ff) << 24) | \
			(((x) & 0x0000ff00) <<  8) | \
			(((x) & 0x00ff0000) >>  8) | \
			(((x) & 0xff000000) >> 24))
#define IPIXEL_FROM_X8R8G8B8(x) ((x) | 0xff000000)
#define IPIXEL_FROM_X8B8G8R8(x) \
		(IPIXEL_FROM_A8B8G8R8(x) | 0xff000000)
#define IPIXEL_FROM_R8G8B8X8(x) (((x) >> 8) | 0xff000000)
#define IPIXEL_FROM_B8G8R8X8(x) \
		(	(((x) & 0x0000ff00) <<  8) | \
			(((x) & 0x00ff0000) >>  8) | \
			(((x) & 0xff000000) >> 24) | 0xff000000 )
#define IPIXEL_FROM_P8R8G8B8(x) ( ((x) >> 24) == 0 ? (0) : \
		((((((x) >> 16) & 0xff) * 255) / ((x) >> 24)) << 16) | \
		((((((x) >>  8) & 0xff) * 255) / ((x) >> 24)) << 8) | \
		((((((x) >>  0) & 0xff) * 255) / ((x) >> 24)) << 0) | \
		((x) & 0xff000000) )

/* convert from 24 bits */
#define IPIXEL_FROM_R8G8B8(x) ((x) | 0xff000000)
#define IPIXEL_FROM_B8G8R8(x) \
		(	(((x) & 0x0000ff00) | \
			(((x) & 0xff0000) >> 16) | \
			(((x) & 0xff) << 16)) | 0xff000000 )

/* convert from 16 bits */
#define IPIXEL_FROM_R5G6B5(x) IPIXEL_CVT_LUT_2(R5G6B5, x)
#define IPIXEL_FROM_B5G6R5(x) IPIXEL_CVT_LUT_2(B5G6R5, x)
#define IPIXEL_FROM_A1R5G5B5(x) IPIXEL_CVT_LUT_2(A1R5G5B5, x)
#define IPIXEL_FROM_A1B5G5R5(x) IPIXEL_CVT_LUT_2(A1B5G5R5, x)
#define IPIXEL_FROM_R5G5B5A1(x) IPIXEL_CVT_LUT_2(R5G5B5A1, x)
#define IPIXEL_FROM_B5G5R5A1(x) IPIXEL_CVT_LUT_2(B5G5R5A1, x)
#define IPIXEL_FROM_X1R5G5B5(x) IPIXEL_CVT_LUT_2(X1R5G5B5, x)
#define IPIXEL_FROM_X1B5G5R5(x) IPIXEL_CVT_LUT_2(X1B5G5R5, x)
#define IPIXEL_FROM_R5G5B5X1(x) IPIXEL_CVT_LUT_2(R5G5B5X1, x)
#define IPIXEL_FROM_B5G5R5X1(x) IPIXEL_CVT_LUT_2(B5G5R5X1, x)
#define IPIXEL_FROM_A4R4G4B4(x) IPIXEL_CVT_LUT_2(A4R4G4B4, x)
#define IPIXEL_FROM_A4B4G4R4(x) IPIXEL_CVT_LUT_2(A4B4G4R4, x)
#define IPIXEL_FROM_R4G4B4A4(x) IPIXEL_CVT_LUT_2(R4G4B4A4, x)
#define IPIXEL_FROM_B4G4R4A4(x) IPIXEL_CVT_LUT_2(B4G4R4A4, x)
#define IPIXEL_FROM_X4R4G4B4(x) IPIXEL_CVT_LUT_2(X4R4G4B4, x)
#define IPIXEL_FROM_X4B4G4R4(x) IPIXEL_CVT_LUT_2(X4B4G4R4, x)
#define IPIXEL_FROM_R4G4B4X4(x) IPIXEL_CVT_LUT_2(R4G4B4X4, x)
#define IPIXEL_FROM_B4G4R4X4(x) IPIXEL_CVT_LUT_2(B4G4R4X4, x)

/* convert from 8 bits */
#define IPIXEL_FROM_C8(x) _ipixel_A8R8G8B8_from_index(_ipixel_src_index, x)
#define IPIXEL_FROM_G8(x) _ipixel_asm_8888(255, x, x, x)
#define IPIXEL_FROM_A8(x) _ipixel_asm_8888(x, 0, 0, 0)
#define IPIXEL_FROM_R3G3B2(x) IPIXEL_CVT_LUT_1(R3G3B2, x)
#define IPIXEL_FROM_B2G3R3(x) IPIXEL_CVT_LUT_1(B2G3R3, x)
#define IPIXEL_FROM_X2R2G2B2(x) IPIXEL_CVT_LUT_1(X2R2G2B2, x)
#define IPIXEL_FROM_X2B2G2R2(x) IPIXEL_CVT_LUT_1(X2B2G2R2, x)
#define IPIXEL_FROM_R2G2B2X2(x) IPIXEL_CVT_LUT_1(R2G2B2X2, x)
#define IPIXEL_FROM_B2G2R2X2(x) IPIXEL_CVT_LUT_1(B2G2R2X2, x)
#define IPIXEL_FROM_A2R2G2B2(x) IPIXEL_CVT_LUT_1(A2R2G2B2, x)
#define IPIXEL_FROM_A2B2G2R2(x) IPIXEL_CVT_LUT_1(A2B2G2R2, x)
#define IPIXEL_FROM_R2G2B2A2(x) IPIXEL_CVT_LUT_1(R2G2B2A2, x)
#define IPIXEL_FROM_B2G2R2A2(x) IPIXEL_CVT_LUT_1(B2G2R2A2, x)
#define IPIXEL_FROM_X4C4(x) IPIXEL_FROM_C8(x)
#define IPIXEL_FROM_X4G4(x) IPIXEL_FROM_G8(_ipixel_scale_4[x])
#define IPIXEL_FROM_X4A4(x) IPIXEL_FROM_A8(_ipixel_scale_4[x])
#define IPIXEL_FROM_C4X4(x) IPIXEL_FROM_X4C4(((x) >> 4))
#define IPIXEL_FROM_G4X4(x) IPIXEL_FROM_X4G4(((x) >> 4))
#define IPIXEL_FROM_A4X4(x) IPIXEL_FROM_X4A4(((x) >> 4))

/* convert from 4 bits */
#define IPIXEL_FROM_C4(x) IPIXEL_FROM_X4C4(x)
#define IPIXEL_FROM_G4(x) IPIXEL_FROM_X4G4(x)
#define IPIXEL_FROM_A4(x) IPIXEL_FROM_X4A4(x)
#define IPIXEL_FROM_R1G2B1(x) \
		(	(_ipixel_scale_1[((x) >> 3)] << 16) | \
			(_ipixel_scale_2[((x) & 6) >> 1] << 8) | \
			(_ipixel_scale_1[((x) >> 0) & 1] << 0) | 0xff000000 )
#define IPIXEL_FROM_B1G2R1(x) \
		(	(_ipixel_scale_1[((x) >> 3)] << 0) | \
			(_ipixel_scale_2[((x) & 6) >> 1] << 8) | \
			(_ipixel_scale_1[((x) >> 0) & 1] << 16) | 0xff000000 )
#define IPIXEL_FROM_1111(x, s1, s2, s3, s4) \
		(	(_ipixel_scale_1[((x) >> 3) & 1] << (s1)) | \
			(_ipixel_scale_1[((x) >> 2) & 1] << (s2)) | \
			(_ipixel_scale_1[((x) >> 1) & 1] << (s3)) | \
			(_ipixel_scale_1[((x) >> 0) & 1] << (s4)))
#define IPIXEL_FROM_A1R1G1B1(x) IPIXEL_FROM_1111(x, 24, 16, 8, 0)
#define IPIXEL_FROM_A1B1G1R1(x) IPIXEL_FROM_1111(x, 24, 0, 8, 16)
#define IPIXEL_FROM_R1G1B1A1(x) IPIXEL_FROM_1111(x, 16, 8, 0, 24)
#define IPIXEL_FROM_B1G1R1A1(x) IPIXEL_FROM_1111(x, 0, 8, 16, 24)
#define IPIXEL_FROM_X111(x, sr, sg, sb) \
		(	(_ipixel_scale_1[((x) >> (sr)) & 1] << 16) | \
			(_ipixel_scale_1[((x) >> (sg)) & 1] <<  8) | \
			(_ipixel_scale_1[((x) >> (sb)) & 1] <<  0) | 0xff000000 )
#define IPIXEL_FROM_X1R1G1B1(x) IPIXEL_FROM_X111(x, 2, 1, 0)
#define IPIXEL_FROM_X1B1G1R1(x) IPIXEL_FROM_X111(x, 0, 1, 2)
#define IPIXEL_FROM_R1G1B1X1(x) IPIXEL_FROM_X111(x, 3, 2, 1)
#define IPIXEL_FROM_B1G1R1X1(x) IPIXEL_FROM_X111(x, 1, 2, 3)

/* convert from 1 bits */
#define IPIXEL_FROM_C1(x) IPIXEL_FROM_C8(x)
#define IPIXEL_FROM_G1(x) IPIXEL_FROM_G8(_ipixel_scale_1[(x)])
#define IPIXEL_FROM_A1(x) IPIXEL_FROM_A8(_ipixel_scale_1[(x)])


/* pixel convert to 32 bits */
#define IPIXEL_TO_A8R8G8B8(x) (x)
#define IPIXEL_TO_A8B8G8R8(x) \
			(	((x) & 0xff00ff00) | \
				(((x) >> 16) & 0xff) | (((x) & 0xff) << 16) )
#define IPIXEL_TO_R8G8B8A8(x) \
			(	(((x) & 0xffffff) << 8) | (((x) & 0xff000000) >> 24) )
#define IPIXEL_TO_B8G8R8A8(x) \
		(	(((x) & 0xff) << 24) | \
			(((x) & 0xff00) << 8) | \
			(((x) & 0xff0000) >> 8) | \
			(((x) & 0xff000000) >> 24) )

#define IPIXEL_TO_X8R8G8B8(x) ((x) & 0xffffff)

#define IPIXEL_TO_X8B8G8R8(x) \
		(	(((x) & 0xff) << 16) | \
			((x) & 0xff00) | \
			(((x) & 0xff0000) >> 16) )

#define IPIXEL_TO_R8G8B8X8(x) ((x) << 8)

#define IPIXEL_TO_B8G8R8X8(x) \
		(	(((x) & 0xff) << 24) | \
			(((x) & 0xff00) << 8) | \
			(((x) & 0xff0000) >> 8) )

#define IPIXEL_TO_P8R8G8B8(x) \
		IRGBA_TO_P8R8G8B8(	(((x) & 0xff0000) >> 16), \
							(((x) & 0xff00) >> 8), \
							(((x) & 0xff)), \
							(((x) & 0xff000000) >> 24) )

/* pixel convert to 24 bits */
#define IPIXEL_TO_R8G8B8(x) ((x) & 0xffffff)

#define IPIXEL_TO_B8G8R8(x) \
		(	(((x) & 0xff) << 16) | \
			((x) & 0xff00) | \
			(((x) & 0xff0000) >> 16) )

/* pixel convert to 16 bits */
#define IPIXEL_TO_R5G6B5(x) \
		(	(((x) & 0xf80000) >> 8) | \
			(((x) & 0xfc00) >> 5) | \
			(((x) & 0xf8) >> 3) )

#define IPIXEL_TO_B5G6R5(x) \
		(	(((x) & 0xf8) << 8) | \
			(((x) & 0xfc00) >> 5) | \
			(((x) & 0xf80000) >> 19) )

#define IPIXEL_TO_X1R5G5B5(x) \
		(	(((x) & 0xf80000) >> 9) | \
			(((x) & 0xf800) >> 6) | \
			(((x) & 0xf8) >> 3) )

#define IPIXEL_TO_X1B5G5R5(x) \
		(	(((x) & 0xf8) << 7) | \
			(((x) & 0xf800) >> 6) | \
			(((x) & 0xf80000) >> 19) )

#define IPIXEL_TO_R5G5B5X1(x) \
		(	(((x) & 0xf80000) >> 8) | \
			(((x) & 0xf800) >> 5) | \
			(((x) & 0xf8) >> 2) )

#define IPIXEL_TO_B5G5R5X1(x) \
		(	(((x) & 0xf8) << 8) | \
			(((x) & 0xf800) >> 5) | \
			(((x) & 0xf80000) >> 18) )

#define IPIXEL_TO_A1R5G5B5(x) \
		(	(((x) & 0x80000000) >> 16) | \
			(((x) & 0xf80000) >> 9) | \
			(((x) & 0xf800) >> 6) | \
			(((x) & 0xf8) >> 3) )

#define IPIXEL_TO_A1B5G5R5(x) \
		(	(((x) & 0x80000000) >> 16) | \
			(((x) & 0xf8) << 7) | \
			(((x) & 0xf800) >> 6) | \
			(((x) & 0xf80000) >> 19) )

#define IPIXEL_TO_R5G5B5A1(x) \
		(	(((x) & 0xf80000) >> 8) | \
			(((x) & 0xf800) >> 5) | \
			(((x) & 0xf8) >> 2) | \
			(((x) & 0x80000000) >> 31) )

#define IPIXEL_TO_B5G5R5A1(x) \
		(	(((x) & 0xf8) << 8) | \
			(((x) & 0xf800) >> 5) | \
			(((x) & 0xf80000) >> 18) | \
			(((x) & 0x80000000) >> 31) )

#define IPIXEL_TO_X4R4G4B4(x) \
		(	(((x) & 0xf00000) >> 12) | \
			(((x) & 0xf000) >> 8) | \
			(((x) & 0xf0) >> 4) )

#define IPIXEL_TO_X4B4G4R4(x) \
		(	(((x) & 0xf0) << 4) | \
			(((x) & 0xf000) >> 8) | \
			(((x) & 0xf00000) >> 20) )

#define IPIXEL_TO_R4G4B4X4(x) \
		(	(((x) & 0xf00000) >> 8) | \
			(((x) & 0xf000) >> 4) | \
			((x) & 0xf0) )

#define IPIXEL_TO_B4G4R4X4(x) \
		(	(((x) & 0xf0) << 8) | \
			(((x) & 0xf000) >> 4) | \
			(((x) & 0xf00000) >> 16) )

#define IPIXEL_TO_A4R4G4B4(x) \
		(	(((x) & 0xf0000000) >> 16) | \
			(((x) & 0xf00000) >> 12) | \
			(((x) & 0xf000) >> 8) | \
			(((x) & 0xf0) >> 4) )

#define IPIXEL_TO_A4B4G4R4(x) \
		(	(((x) & 0xf0000000) >> 16) | \
			(((x) & 0xf0) << 4) | \
			(((x) & 0xf000) >> 8) | \
			(((x) & 0xf00000) >> 20) )

#define IPIXEL_TO_R4G4B4A4(x) \
		(	(((x) & 0xf00000) >> 8) | \
			(((x) & 0xf000) >> 4) | \
			((x) & 0xf0) | \
			(((x) & 0xf0000000) >> 28) )

#define IPIXEL_TO_B4G4R4A4(x) \
		(	(((x) & 0xf0) << 8) | \
			(((x) & 0xf000) >> 4) | \
			(((x) & 0xf00000) >> 16) | \
			(((x) & 0xf0000000) >> 28) )

/* pixel convert to 8 bits */
#define IPIXEL_TO_C8(x) _ipixel_RGB_to_ent(_ipixel_dst_index, \
			(((x) & 0xff0000) >> 16), \
			(((x) & 0xff00) >> 8), \
			(((x) & 0xff)) )


#define IPIXEL_TO_G8(x) _ipixel_to_gray( \
			(((x) & 0xff0000) >> 16), \
			(((x) & 0xff00) >> 8), \
			(((x) & 0xff)) )

#define IPIXEL_TO_A8(x) (((x) & 0xff000000) >> 24)

#define IPIXEL_TO_R3G3B2(x) \
		(	(((x) & 0xe00000) >> 16) | \
			(((x) & 0xe000) >> 11) | \
			(((x) & 0xc0) >> 6) )

#define IPIXEL_TO_B2G3R3(x) \
		(	((x) & 0xc0) | \
			(((x) & 0xe000) >> 10) | \
			(((x) & 0xe00000) >> 21) )

#define IPIXEL_TO_X2R2G2B2(x) \
		(	(((x) & 0xc00000) >> 18) | \
			(((x) & 0xc000) >> 12) | \
			(((x) & 0xc0) >> 6) )

#define IPIXEL_TO_X2B2G2R2(x) \
		(	(((x) & 0xc0) >> 2) | \
			(((x) & 0xc000) >> 12) | \
			(((x) & 0xc00000) >> 22) )

#define IPIXEL_TO_R2G2B2X2(x) \
		(	(((x) & 0xc00000) >> 16) | \
			(((x) & 0xc000) >> 10) | \
			(((x) & 0xc0) >> 4) )

#define IPIXEL_TO_B2G2R2X2(x) \
		(	((x) & 0xc0) | \
			(((x) & 0xc000) >> 10) | \
			(((x) & 0xc00000) >> 20) )

#define IPIXEL_TO_A2R2G2B2(x) \
		(	(((x) & 0xc0000000) >> 24) | \
			(((x) & 0xc00000) >> 18) | \
			(((x) & 0xc000) >> 12) | \
			(((x) & 0xc0) >> 6) )

#define IPIXEL_TO_A2B2G2R2(x) \
		(	(((x) & 0xc0000000) >> 24) | \
			(((x) & 0xc0) >> 2) | \
			(((x) & 0xc000) >> 12) | \
			(((x) & 0xc00000) >> 22) )

#define IPIXEL_TO_R2G2B2A2(x) \
		(	(((x) & 0xc00000) >> 16) | \
			(((x) & 0xc000) >> 10) | \
			(((x) & 0xc0) >> 4) | \
			(((x) & 0xc0000000) >> 30) )

#define IPIXEL_TO_B2G2R2A2(x) \
		(	((x) & 0xc0) | \
			(((x) & 0xc000) >> 10) | \
			(((x) & 0xc00000) >> 20) | \
			(((x) & 0xc0000000) >> 30) )

#define IPIXEL_TO_X4C4(x) (IPIXEL_TO_C8(x) & 0xf)
#define IPIXEL_TO_X4G4(x) (IPIXEL_TO_G8(x) >> 4)
#define IPIXEL_TO_X4A4(x) (IPIXEL_TO_A8(x) >> 4)
#define IPIXEL_TO_C4X4(x) ((IPIXEL_TO_C8(x) & 0xf) << 4)
#define IPIXEL_TO_G4X4(x) (IPIXEL_TO_G8(x) & 0xf0)
#define IPIXEL_TO_A4X4(x) (IPIXEL_TO_A8(x) & 0xf0)

/* pixel convert to 4 bits */
#define IPIXEL_TO_C4(x) IPIXEL_TO_X4C4(x)
#define IPIXEL_TO_G4(x) IPIXEL_TO_X4G4(x)
#define IPIXEL_TO_A4(x) IPIXEL_TO_X4A4(x)

#define IPIXEL_TO_R1G2B1(x) \
		(	(((x) & 0x800000) >> 20) | \
			(((x) & 0xc000) >> 13) | \
			(((x) & 0x80) >> 7) )

#define IPIXEL_TO_B1G2R1(x) \
		(	(((x) & 0x80) >> 4) | \
			(((x) & 0xc000) >> 13) | \
			(((x) & 0x800000) >> 23) )

#define IPIXEL_TO_A1R1G1B1(x) \
		(	(((x) & 0x80000000) >> 28) | \
			(((x) & 0x800000) >> 21) | \
			(((x) & 0x8000) >> 14) | \
			(((x) & 0x80) >> 7) )

#define IPIXEL_TO_A1B1G1R1(x) \
		(	(((x) & 0x80000000) >> 28) | \
			(((x) & 0x80) >> 5) | \
			(((x) & 0x8000) >> 14) | \
			(((x) & 0x800000) >> 23) )

#define IPIXEL_TO_R1G1B1A1(x) \
		(	(((x) & 0x800000) >> 20) | \
			(((x) & 0x8000) >> 13) | \
			(((x) & 0x80) >> 6) | \
			(((x) & 0x80000000) >> 31) )

#define IPIXEL_TO_B1G1R1A1(x) \
		(	(((x) & 0x80) >> 4) | \
			(((x) & 0x8000) >> 13) | \
			(((x) & 0x800000) >> 22) | \
			(((x) & 0x80000000) >> 31) )

#define IPIXEL_TO_X1R1G1B1(x) \
		(	(((x) & 0x800000) >> 21) | \
			(((x) & 0x8000) >> 14) | \
			(((x) & 0x80) >> 7) )

#define IPIXEL_TO_X1B1G1R1(x) \
		(	(((x) & 0x80) >> 5) | \
			(((x) & 0x8000) >> 14) | \
			(((x) & 0x800000) >> 23) )

#define IPIXEL_TO_R1G1B1X1(x) \
		(	(((x) & 0x800000) >> 20) | \
			(((x) & 0x8000) >> 13) | \
			(((x) & 0x80) >> 6) )

#define IPIXEL_TO_B1G1R1X1(x) \
		(	(((x) & 0x80) >> 4) | \
			(((x) & 0x8000) >> 13) | \
			(((x) & 0x800000) >> 22) )

/* pixel convert to 1 bit */
#define IPIXEL_TO_C1(x) (IPIXEL_TO_C8(x) & 1)
#define IPIXEL_TO_G1(x) (IPIXEL_TO_G8(x) >> 7)
#define IPIXEL_TO_A1(x) (IPIXEL_TO_A8(x) >> 7)


/**********************************************************************
 * MACRO: PIXEL FORMAT UTILITY
 **********************************************************************/
#define IRGBA_FROM_PIXEL(fmt, c, r, g, b, a) IRGBA_FROM_##fmt(c, r, g, b, a)
#define IRGBA_TO_PIXEL(fmt, r, g, b, a) IRGBA_TO_##fmt(r, g, b, a)
#define IPIXEL_FROM(fmt, c) IPIXEL_FROM_##fmt(c)
#define IPIXEL_TO(fmt, c) IPIXEL_TO_##fmt(c)
#define IPIX_FMT_NBITS(fmt) IPIX_FMT_BPP_##fmt

#define IRGBA_FROM_COLOR(c, fmt, r, g, b, a) do { \
	r = ((((ICOLORD)(c)) & (fmt)->rmask) >> (fmt)->rshift) << (fmt)->rloss; \
	g = ((((ICOLORD)(c)) & (fmt)->gmask) >> (fmt)->gshift) << (fmt)->gloss; \
	b = ((((ICOLORD)(c)) & (fmt)->bmask) >> (fmt)->bshift) << (fmt)->bloss; \
	a = ((((ICOLORD)(c)) & (fmt)->amask) >> (fmt)->ashift) << (fmt)->aloss; \
}	while (0)

#define IRGB_FROM_COLOR(c, fmt, r, g, b) do { \
	r = ((((ICOLORD)(c)) & (fmt)->rmask) >> (fmt)->rshift) << (fmt)->rloss; \
	g = ((((ICOLORD)(c)) & (fmt)->gmask) >> (fmt)->gshift) << (fmt)->gloss; \
	b = ((((ICOLORD)(c)) & (fmt)->bmask) >> (fmt)->bshift) << (fmt)->bloss; \
}	while (0)

#define IRGBA_TO_COLOR(fmt, r, g, b, a) ( \
	(((r) >> (fmt)->rloss) << (fmt)->rshift) | \
	(((g) >> (fmt)->gloss) << (fmt)->gshift) | \
	(((b) >> (fmt)->bloss) << (fmt)->bshift) | \
	(((a) >> (fmt)->aloss) << (fmt)->ashift))

#define IRGB_TO_COLOR(fmt, r, g, b) ( \
	(((r) >> (fmt)->rloss) << (fmt)->rshift) | \
	(((g) >> (fmt)->gloss) << (fmt)->gshift) | \
	(((b) >> (fmt)->bloss) << (fmt)->bshift))


#define IRGBA_DISEMBLE(fmt, c, r, g, b, a) do { \
		switch (fmt) { \
        case IPIX_FMT_A8R8G8B8: IRGBA_FROM_A8R8G8B8(c, r, g, b, a); break; \
        case IPIX_FMT_A8B8G8R8: IRGBA_FROM_A8B8G8R8(c, r, g, b, a); break; \
        case IPIX_FMT_R8G8B8A8: IRGBA_FROM_R8G8B8A8(c, r, g, b, a); break; \
        case IPIX_FMT_B8G8R8A8: IRGBA_FROM_B8G8R8A8(c, r, g, b, a); break; \
        case IPIX_FMT_X8R8G8B8: IRGBA_FROM_X8R8G8B8(c, r, g, b, a); break; \
        case IPIX_FMT_X8B8G8R8: IRGBA_FROM_X8B8G8R8(c, r, g, b, a); break; \
        case IPIX_FMT_R8G8B8X8: IRGBA_FROM_R8G8B8X8(c, r, g, b, a); break; \
        case IPIX_FMT_B8G8R8X8: IRGBA_FROM_B8G8R8X8(c, r, g, b, a); break; \
        case IPIX_FMT_P8R8G8B8: IRGBA_FROM_P8R8G8B8(c, r, g, b, a); break; \
        case IPIX_FMT_R8G8B8: IRGBA_FROM_R8G8B8(c, r, g, b, a); break; \
        case IPIX_FMT_B8G8R8: IRGBA_FROM_B8G8R8(c, r, g, b, a); break; \
        case IPIX_FMT_R5G6B5: IRGBA_FROM_R5G6B5(c, r, g, b, a); break; \
        case IPIX_FMT_B5G6R5: IRGBA_FROM_B5G6R5(c, r, g, b, a); break; \
        case IPIX_FMT_X1R5G5B5: IRGBA_FROM_X1R5G5B5(c, r, g, b, a); break; \
        case IPIX_FMT_X1B5G5R5: IRGBA_FROM_X1B5G5R5(c, r, g, b, a); break; \
        case IPIX_FMT_R5G5B5X1: IRGBA_FROM_R5G5B5X1(c, r, g, b, a); break; \
        case IPIX_FMT_B5G5R5X1: IRGBA_FROM_B5G5R5X1(c, r, g, b, a); break; \
        case IPIX_FMT_A1R5G5B5: IRGBA_FROM_A1R5G5B5(c, r, g, b, a); break; \
        case IPIX_FMT_A1B5G5R5: IRGBA_FROM_A1B5G5R5(c, r, g, b, a); break; \
        case IPIX_FMT_R5G5B5A1: IRGBA_FROM_R5G5B5A1(c, r, g, b, a); break; \
        case IPIX_FMT_B5G5R5A1: IRGBA_FROM_B5G5R5A1(c, r, g, b, a); break; \
        case IPIX_FMT_X4R4G4B4: IRGBA_FROM_X4R4G4B4(c, r, g, b, a); break; \
        case IPIX_FMT_X4B4G4R4: IRGBA_FROM_X4B4G4R4(c, r, g, b, a); break; \
        case IPIX_FMT_R4G4B4X4: IRGBA_FROM_R4G4B4X4(c, r, g, b, a); break; \
        case IPIX_FMT_B4G4R4X4: IRGBA_FROM_B4G4R4X4(c, r, g, b, a); break; \
        case IPIX_FMT_A4R4G4B4: IRGBA_FROM_A4R4G4B4(c, r, g, b, a); break; \
        case IPIX_FMT_A4B4G4R4: IRGBA_FROM_A4B4G4R4(c, r, g, b, a); break; \
        case IPIX_FMT_R4G4B4A4: IRGBA_FROM_R4G4B4A4(c, r, g, b, a); break; \
        case IPIX_FMT_B4G4R4A4: IRGBA_FROM_B4G4R4A4(c, r, g, b, a); break; \
        case IPIX_FMT_C8: IRGBA_FROM_C8(c, r, g, b, a); break; \
        case IPIX_FMT_G8: IRGBA_FROM_G8(c, r, g, b, a); break; \
        case IPIX_FMT_A8: IRGBA_FROM_A8(c, r, g, b, a); break; \
        case IPIX_FMT_R3G3B2: IRGBA_FROM_R3G3B2(c, r, g, b, a); break; \
        case IPIX_FMT_B2G3R3: IRGBA_FROM_B2G3R3(c, r, g, b, a); break; \
        case IPIX_FMT_X2R2G2B2: IRGBA_FROM_X2R2G2B2(c, r, g, b, a); break; \
        case IPIX_FMT_X2B2G2R2: IRGBA_FROM_X2B2G2R2(c, r, g, b, a); break; \
        case IPIX_FMT_R2G2B2X2: IRGBA_FROM_R2G2B2X2(c, r, g, b, a); break; \
        case IPIX_FMT_B2G2R2X2: IRGBA_FROM_B2G2R2X2(c, r, g, b, a); break; \
        case IPIX_FMT_A2R2G2B2: IRGBA_FROM_A2R2G2B2(c, r, g, b, a); break; \
        case IPIX_FMT_A2B2G2R2: IRGBA_FROM_A2B2G2R2(c, r, g, b, a); break; \
        case IPIX_FMT_R2G2B2A2: IRGBA_FROM_R2G2B2A2(c, r, g, b, a); break; \
        case IPIX_FMT_B2G2R2A2: IRGBA_FROM_B2G2R2A2(c, r, g, b, a); break; \
        case IPIX_FMT_X4C4: IRGBA_FROM_X4C4(c, r, g, b, a); break; \
        case IPIX_FMT_X4G4: IRGBA_FROM_X4G4(c, r, g, b, a); break; \
        case IPIX_FMT_X4A4: IRGBA_FROM_X4A4(c, r, g, b, a); break; \
        case IPIX_FMT_C4X4: IRGBA_FROM_C4X4(c, r, g, b, a); break; \
        case IPIX_FMT_G4X4: IRGBA_FROM_G4X4(c, r, g, b, a); break; \
        case IPIX_FMT_A4X4: IRGBA_FROM_A4X4(c, r, g, b, a); break; \
        case IPIX_FMT_C4: IRGBA_FROM_C4(c, r, g, b, a); break; \
        case IPIX_FMT_G4: IRGBA_FROM_G4(c, r, g, b, a); break; \
        case IPIX_FMT_A4: IRGBA_FROM_A4(c, r, g, b, a); break; \
        case IPIX_FMT_R1G2B1: IRGBA_FROM_R1G2B1(c, r, g, b, a); break; \
        case IPIX_FMT_B1G2R1: IRGBA_FROM_B1G2R1(c, r, g, b, a); break; \
        case IPIX_FMT_A1R1G1B1: IRGBA_FROM_A1R1G1B1(c, r, g, b, a); break; \
        case IPIX_FMT_A1B1G1R1: IRGBA_FROM_A1B1G1R1(c, r, g, b, a); break; \
        case IPIX_FMT_R1G1B1A1: IRGBA_FROM_R1G1B1A1(c, r, g, b, a); break; \
        case IPIX_FMT_B1G1R1A1: IRGBA_FROM_B1G1R1A1(c, r, g, b, a); break; \
        case IPIX_FMT_X1R1G1B1: IRGBA_FROM_X1R1G1B1(c, r, g, b, a); break; \
        case IPIX_FMT_X1B1G1R1: IRGBA_FROM_X1B1G1R1(c, r, g, b, a); break; \
        case IPIX_FMT_R1G1B1X1: IRGBA_FROM_R1G1B1X1(c, r, g, b, a); break; \
        case IPIX_FMT_B1G1R1X1: IRGBA_FROM_B1G1R1X1(c, r, g, b, a); break; \
        case IPIX_FMT_C1: IRGBA_FROM_C1(c, r, g, b, a); break; \
        case IPIX_FMT_G1: IRGBA_FROM_G1(c, r, g, b, a); break; \
        case IPIX_FMT_A1: IRGBA_FROM_A1(c, r, g, b, a); break; \
		default: (r) = (g) = (b) = (a) = 0; break; \
		} \
    }   while (0)


#define IRGBA_ASSEMBLE(fmt, c, r, g, b, a) do { \
		switch (fmt) { \
        case IPIX_FMT_A8R8G8B8: c = IRGBA_TO_A8R8G8B8(r, g, b, a); break; \
        case IPIX_FMT_A8B8G8R8: c = IRGBA_TO_A8B8G8R8(r, g, b, a); break; \
        case IPIX_FMT_R8G8B8A8: c = IRGBA_TO_R8G8B8A8(r, g, b, a); break; \
        case IPIX_FMT_B8G8R8A8: c = IRGBA_TO_B8G8R8A8(r, g, b, a); break; \
        case IPIX_FMT_X8R8G8B8: c = IRGBA_TO_X8R8G8B8(r, g, b, a); break; \
        case IPIX_FMT_X8B8G8R8: c = IRGBA_TO_X8B8G8R8(r, g, b, a); break; \
        case IPIX_FMT_R8G8B8X8: c = IRGBA_TO_R8G8B8X8(r, g, b, a); break; \
        case IPIX_FMT_B8G8R8X8: c = IRGBA_TO_B8G8R8X8(r, g, b, a); break; \
        case IPIX_FMT_P8R8G8B8: c = IRGBA_TO_P8R8G8B8(r, g, b, a); break; \
        case IPIX_FMT_R8G8B8: c = IRGBA_TO_R8G8B8(r, g, b, a); break; \
        case IPIX_FMT_B8G8R8: c = IRGBA_TO_B8G8R8(r, g, b, a); break; \
        case IPIX_FMT_R5G6B5: c = IRGBA_TO_R5G6B5(r, g, b, a); break; \
        case IPIX_FMT_B5G6R5: c = IRGBA_TO_B5G6R5(r, g, b, a); break; \
        case IPIX_FMT_X1R5G5B5: c = IRGBA_TO_X1R5G5B5(r, g, b, a); break; \
        case IPIX_FMT_X1B5G5R5: c = IRGBA_TO_X1B5G5R5(r, g, b, a); break; \
        case IPIX_FMT_R5G5B5X1: c = IRGBA_TO_R5G5B5X1(r, g, b, a); break; \
        case IPIX_FMT_B5G5R5X1: c = IRGBA_TO_B5G5R5X1(r, g, b, a); break; \
        case IPIX_FMT_A1R5G5B5: c = IRGBA_TO_A1R5G5B5(r, g, b, a); break; \
        case IPIX_FMT_A1B5G5R5: c = IRGBA_TO_A1B5G5R5(r, g, b, a); break; \
        case IPIX_FMT_R5G5B5A1: c = IRGBA_TO_R5G5B5A1(r, g, b, a); break; \
        case IPIX_FMT_B5G5R5A1: c = IRGBA_TO_B5G5R5A1(r, g, b, a); break; \
        case IPIX_FMT_X4R4G4B4: c = IRGBA_TO_X4R4G4B4(r, g, b, a); break; \
        case IPIX_FMT_X4B4G4R4: c = IRGBA_TO_X4B4G4R4(r, g, b, a); break; \
        case IPIX_FMT_R4G4B4X4: c = IRGBA_TO_R4G4B4X4(r, g, b, a); break; \
        case IPIX_FMT_B4G4R4X4: c = IRGBA_TO_B4G4R4X4(r, g, b, a); break; \
        case IPIX_FMT_A4R4G4B4: c = IRGBA_TO_A4R4G4B4(r, g, b, a); break; \
        case IPIX_FMT_A4B4G4R4: c = IRGBA_TO_A4B4G4R4(r, g, b, a); break; \
        case IPIX_FMT_R4G4B4A4: c = IRGBA_TO_R4G4B4A4(r, g, b, a); break; \
        case IPIX_FMT_B4G4R4A4: c = IRGBA_TO_B4G4R4A4(r, g, b, a); break; \
        case IPIX_FMT_C8: c = IRGBA_TO_C8(r, g, b, a); break; \
        case IPIX_FMT_G8: c = IRGBA_TO_G8(r, g, b, a); break; \
        case IPIX_FMT_A8: c = IRGBA_TO_A8(r, g, b, a); break; \
        case IPIX_FMT_R3G3B2: c = IRGBA_TO_R3G3B2(r, g, b, a); break; \
        case IPIX_FMT_B2G3R3: c = IRGBA_TO_B2G3R3(r, g, b, a); break; \
        case IPIX_FMT_X2R2G2B2: c = IRGBA_TO_X2R2G2B2(r, g, b, a); break; \
        case IPIX_FMT_X2B2G2R2: c = IRGBA_TO_X2B2G2R2(r, g, b, a); break; \
        case IPIX_FMT_R2G2B2X2: c = IRGBA_TO_R2G2B2X2(r, g, b, a); break; \
        case IPIX_FMT_B2G2R2X2: c = IRGBA_TO_B2G2R2X2(r, g, b, a); break; \
        case IPIX_FMT_A2R2G2B2: c = IRGBA_TO_A2R2G2B2(r, g, b, a); break; \
        case IPIX_FMT_A2B2G2R2: c = IRGBA_TO_A2B2G2R2(r, g, b, a); break; \
        case IPIX_FMT_R2G2B2A2: c = IRGBA_TO_R2G2B2A2(r, g, b, a); break; \
        case IPIX_FMT_B2G2R2A2: c = IRGBA_TO_B2G2R2A2(r, g, b, a); break; \
        case IPIX_FMT_X4C4: c = IRGBA_TO_X4C4(r, g, b, a); break; \
        case IPIX_FMT_X4G4: c = IRGBA_TO_X4G4(r, g, b, a); break; \
        case IPIX_FMT_X4A4: c = IRGBA_TO_X4A4(r, g, b, a); break; \
        case IPIX_FMT_C4X4: c = IRGBA_TO_C4X4(r, g, b, a); break; \
        case IPIX_FMT_G4X4: c = IRGBA_TO_G4X4(r, g, b, a); break; \
        case IPIX_FMT_A4X4: c = IRGBA_TO_A4X4(r, g, b, a); break; \
        case IPIX_FMT_C4: c = IRGBA_TO_C4(r, g, b, a); break; \
        case IPIX_FMT_G4: c = IRGBA_TO_G4(r, g, b, a); break; \
        case IPIX_FMT_A4: c = IRGBA_TO_A4(r, g, b, a); break; \
        case IPIX_FMT_R1G2B1: c = IRGBA_TO_R1G2B1(r, g, b, a); break; \
        case IPIX_FMT_B1G2R1: c = IRGBA_TO_B1G2R1(r, g, b, a); break; \
        case IPIX_FMT_A1R1G1B1: c = IRGBA_TO_A1R1G1B1(r, g, b, a); break; \
        case IPIX_FMT_A1B1G1R1: c = IRGBA_TO_A1B1G1R1(r, g, b, a); break; \
        case IPIX_FMT_R1G1B1A1: c = IRGBA_TO_R1G1B1A1(r, g, b, a); break; \
        case IPIX_FMT_B1G1R1A1: c = IRGBA_TO_B1G1R1A1(r, g, b, a); break; \
        case IPIX_FMT_X1R1G1B1: c = IRGBA_TO_X1R1G1B1(r, g, b, a); break; \
        case IPIX_FMT_X1B1G1R1: c = IRGBA_TO_X1B1G1R1(r, g, b, a); break; \
        case IPIX_FMT_R1G1B1X1: c = IRGBA_TO_R1G1B1X1(r, g, b, a); break; \
        case IPIX_FMT_B1G1R1X1: c = IRGBA_TO_B1G1R1X1(r, g, b, a); break; \
        case IPIX_FMT_C1: c = IRGBA_TO_C1(r, g, b, a); break; \
        case IPIX_FMT_G1: c = IRGBA_TO_G1(r, g, b, a); break; \
        case IPIX_FMT_A1: c = IRGBA_TO_A1(r, g, b, a); break; \
		default: c = 0; break; \
		} \
    }   while (0)

#define IPIXEL_FMT_FROM_RGBA(fmt, cc, r, g, b, a) do { \
		IUINT32 __X1 = ((r) >> (fmt)->rloss) << (fmt)->rshift; \
		IUINT32 __X2 = ((g) >> (fmt)->gloss) << (fmt)->gshift; \
		IUINT32 __X3 = ((b) >> (fmt)->bloss) << (fmt)->bshift; \
		IUINT32 __X4 = ((a) >> (fmt)->aloss) << (fmt)->ashift; \
		(cc) = __X1 | __X2 | __X3 | __X4; \
	} while (0)

#define IPIXEL_FMT_TO_RGBA(fmt, cc, r, g, b, a) do { \
		const IUINT8 *__rscale = &_ipixel_fmt_scale[8 - (fmt)->rloss][0]; \
		const IUINT8 *__gscale = &_ipixel_fmt_scale[8 - (fmt)->gloss][0]; \
		const IUINT8 *__bscale = &_ipixel_fmt_scale[8 - (fmt)->bloss][0]; \
		const IUINT8 *__ascale = &_ipixel_fmt_scale[8 - (fmt)->aloss][0]; \
		(r) = __rscale[((cc) & ((fmt)->rmask)) >> ((fmt)->rshift)]; \
		(g) = __gscale[((cc) & ((fmt)->gmask)) >> ((fmt)->gshift)]; \
		(b) = __bscale[((cc) & ((fmt)->bmask)) >> ((fmt)->bshift)]; \
		(a) = __ascale[((cc) & ((fmt)->amask)) >> ((fmt)->ashift)]; \
	} while (0)



/**********************************************************************
 * MACRO: PIXEL FILLING FAST MACRO
 **********************************************************************/
#define _ipixel_fill_32(__bits, __startx, __size, __cc) do { \
		IUINT32 *__ptr = (IUINT32*)(__bits) + (__startx); \
		ILINS_LOOP_DOUBLE( \
			{ *__ptr++ = (IUINT32)(__cc); }, \
			{ *__ptr++ = (IUINT32)(__cc); *__ptr++ = (IUINT32)(__cc); }, \
			__size); \
	}	while (0)

#define _ipixel_fill_24(__bits, __startx, __size, __cc) do { \
		IUINT8 *__ptr = (IUINT8*)(__bits) + (__startx) * 3; \
		IUINT8 *__dst = __ptr; \
		int __cnt; \
		for (__cnt = 12; __size > 0 && __cnt > 0; __size--, __cnt--) { \
			_ipixel_store(24, __ptr, 0, __cc); \
			__ptr += 3; \
		} \
		for (; __size >= 4; __size -= 4) { \
			((IUINT32*)__ptr)[0] = ((const IUINT32*)__dst)[0]; \
			((IUINT32*)__ptr)[1] = ((const IUINT32*)__dst)[1]; \
			((IUINT32*)__ptr)[2] = ((const IUINT32*)__dst)[2]; \
			__ptr += 12; \
		} \
		for (; __size > 0; __size--) { \
			_ipixel_store(24, __ptr, 0, __cc); \
			__ptr += 3; \
		} \
	}	while (0)

#define _ipixel_fill_16(__bits, __startx, __size, __cc) do { \
		IUINT8 *__ptr = (IUINT8*)(__bits) + (__startx) * 2; \
		IUINT32 __c1 = (__cc) & 0xffff; \
		IUINT32 __c2 = (__c1 << 16) | __c1; \
		size_t __length = __size; \
		if ((((size_t)__ptr) & 2) && __length > 0) { \
			*(IUINT16*)__ptr = (IUINT16)__c1; \
			__ptr += 2; __length--; \
		} \
		ILINS_LOOP_DOUBLE( \
			{	*(IUINT16*)__ptr = (IUINT16)__c1; __ptr += 2; }, \
			{	*(IUINT32*)__ptr = (IUINT32)__c2; __ptr += 4; }, \
			__length); \
	}	while (0)

#define _ipixel_fill_8(__bits, __startx, __size, __cc) do { \
		IUINT8 *__ptr = (IUINT8*)(__bits) + (__startx); \
		if (__size <= 64) { \
			IUINT32 __c1 = (__cc) & 0xff; \
			IUINT32 __c2 = (__c1 << 8) | __c1; \
			IUINT32 __c4 = (__c2 << 16) | __c2; \
			ILINS_LOOP_QUATRO( \
				{	*__ptr++ = (IUINT8)__c1; }, \
				{	*(IUINT16*)__ptr = (IUINT16)__c2; __ptr += 2; }, \
				{	*(IUINT32*)__ptr = (IUINT32)__c4; __ptr += 4; }, \
				__size); \
		}	else { \
			memset(__ptr, (IUINT8)(__cc), __size); \
		} \
	}	while (0)

#define _ipixel_fill_4(__bits, __startx, __size, __cc) do { \
		IINT32 __pos = (__startx); \
		IINT32 __cnt; \
		for (__cnt = (__size); __cnt > 0; __pos++, __cnt--) { \
			_ipixel_store_4(__bits, __pos, __cc); \
		} \
	}	while (0)

#define _ipixel_fill_1(__bits, __startx, __size, __cc) do { \
		IINT32 __pos = (__startx); \
		IINT32 __cnt; \
		for (__cnt = (__size); __cnt > 0; __pos++, __cnt--) { \
			_ipixel_store_1(__bits, __pos, __cc); \
		} \
	}	while (0)


#define _ipixel_fill(bpp, bits, startx, size, color) \
		_ipixel_fill_##bpp(bits, startx, size, color)



/**********************************************************************
 * MACRO: PIXEL BLENDING
 **********************************************************************/
/* blend onto a static surface (no alpha channel) */
#define IBLEND_STATIC(sr, sg, sb, sa, dr, dg, db, da) do { \
		IINT32 SA = _ipixel_norm(sa); \
		(dr) = (((((IINT32)(sr)) - ((IINT32)(dr))) * SA) >> 8) + (dr); \
		(dg) = (((((IINT32)(sg)) - ((IINT32)(dg))) * SA) >> 8) + (dg); \
		(db) = (((((IINT32)(sb)) - ((IINT32)(db))) * SA) >> 8) + (db); \
		(da) = 255; \
	}	while (0)

/* blend onto a normal surface (with alpha channel) */
#define IBLEND_NORMAL(sr, sg, sb, sa, dr, dg, db, da) do { \
		IINT32 SA = _ipixel_norm(sa); \
		IINT32 DA = _ipixel_norm(da); \
		IINT32 FA = DA + (((256 - DA) * SA) >> 8); \
		SA = (FA != 0)? ((SA << 8) / FA) : (0); \
		(da) = _ipixel_unnorm(FA); \
		(dr) = (((((IINT32)(sr)) - ((IINT32)(dr))) * SA) >> 8) + (dr); \
		(dg) = (((((IINT32)(sg)) - ((IINT32)(dg))) * SA) >> 8) + (dg); \
		(db) = (((((IINT32)(sb)) - ((IINT32)(db))) * SA) >> 8) + (db); \
	}	while (0)


/* blend onto a normal surface (with alpha channel) in a fast way */
/* looking up alpha values from lut instead of div. calculation */
/* lut must be inited by ipixel_lut_init() */
#define IBLEND_NORMAL_FAST(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 __lutpos = (((da) & 0xf8) << 4) | (((sa) & 0xfc) >> 1); \
		IINT32 SA = ipixel_blend_lut[(__lutpos) + 0]; \
		IINT32 FA = ipixel_blend_lut[(__lutpos) + 1]; \
		SA = _ipixel_norm((SA)); \
		(da) = FA; \
		(dr) = (((((IINT32)(sr)) - ((IINT32)(dr))) * SA) >> 8) + (dr); \
		(dg) = (((((IINT32)(sg)) - ((IINT32)(dg))) * SA) >> 8) + (dg); \
		(db) = (((((IINT32)(sb)) - ((IINT32)(db))) * SA) >> 8) + (db); \
	}	while (0)

/* premultiplied src over */
#define IBLEND_SRCOVER(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 SA = 255 - (sa); \
		(dr) = (dr) * SA; \
		(dg) = (dg) * SA; \
		(db) = (db) * SA; \
		(da) = (da) * SA; \
		(dr) = _ipixel_fast_div_255(dr) + (sr); \
		(dg) = _ipixel_fast_div_255(dg) + (sg); \
		(db) = _ipixel_fast_div_255(db) + (sb); \
		(da) = _ipixel_fast_div_255(da) + (sa); \
	}	while (0)

/* additive blend */
#define IBLEND_ADDITIVE(sr, sg, sb, sa, dr, dg, db, da) do { \
		IINT32 XA = _ipixel_norm(sa); \
		IINT32 XR = (sr) * XA; \
		IINT32 XG = (sg) * XA; \
		IINT32 XB = (sb) * XA; \
		XR = XR >> 8; \
		XG = XG >> 8; \
		XB = XB >> 8; \
		XA = (sa) + (da); \
		XR += (dr); \
		XG += (dg); \
		XB += (db); \
		(dr) = ICLIP_256(XR); \
		(dg) = ICLIP_256(XG); \
		(db) = ICLIP_256(XB); \
		(da) = ICLIP_256(XA); \
	}	while (0)


/* premutiplied 32bits blending: 
   dst = src + (255 - src.alpha) * dst / 255 */
#define IBLEND_PARGB(color_dst, color_src) do { \
		IUINT32 __A = 255 - ((color_src) >> 24); \
		IUINT32 __DST_RB = (color_dst) & 0xff00ff; \
		IUINT32 __DST_AG = ((color_dst) >> 8) & 0xff00ff; \
		__DST_RB *= __A; \
		__DST_AG *= __A; \
		__DST_RB += __DST_RB >> 8; \
		__DST_AG += __DST_AG >> 8; \
		__DST_RB >>= 8; \
		__DST_AG &= 0xff00ff00; \
		__A = (__DST_RB & 0xff00ff) | __DST_AG; \
		(color_dst) = __A + (color_src); \
	}	while (0)


/* premutiplied 32bits blending (with coverage): 
   tmp = src * coverage / 255,
   dst = tmp + (255 - tmp.alpha) * dst / 255 */
#define IBLEND_PARGB_COVER(color_dst, color_src, coverage) do { \
		IUINT32 __r1 = (color_src) & 0xff00ff; \
		IUINT32 __r2 = ((color_src) >> 8) & 0xff00ff; \
		IUINT32 __r3 = _ipixel_norm(coverage); \
		IUINT32 __r4; \
		__r1 *= __r3; \
		__r2 *= __r3; \
		__r3 = (color_dst) & 0xff00ff; \
		__r4 = ((color_dst) >> 8) & 0xff00ff; \
		__r1 = ((__r1) >> 8) & 0xff00ff; \
		__r2 = (__r2) & 0xff00ff00; \
		__r1 = __r1 | __r2; \
		__r2 = 255 - (__r2 >> 24); \
		__r3 *= __r2; \
		__r4 *= __r2; \
		__r3 = ((__r3 + (__r3 >> 8)) >> 8) & 0xff00ff; \
		__r4 = (__r4 + (__r4 >> 8)) & 0xff00ff00; \
		(color_dst) = (__r3 | __r4) + (__r1); \
	}	while (0)


/* compositing */
#define IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, FD) do { \
		(dr) = _ipixel_mullut[(FS)][(sr)] + _ipixel_mullut[(FD)][(dr)]; \
		(dg) = _ipixel_mullut[(FS)][(sg)] + _ipixel_mullut[(FD)][(dg)]; \
		(db) = _ipixel_mullut[(FS)][(sb)] + _ipixel_mullut[(FD)][(db)]; \
		(da) = _ipixel_mullut[(FS)][(sa)] + _ipixel_mullut[(FD)][(da)]; \
	}	while (0)

/* premultiply: src atop */
#define IBLEND_OP_SRC_ATOP(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = (da); \
		IUINT32 FD = 255 - (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, FD); \
	}	while (0)

/* premultiply: src in */
#define IBLEND_OP_SRC_IN(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = (da); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, 0); \
	}	while (0)

/* premultiply: src out */
#define IBLEND_OP_SRC_OUT(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = 255 - (da); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, 0); \
	}	while (0)

/* premultiply: src over */
#define IBLEND_OP_SRC_OVER(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FD = 255 - (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, 255, FD); \
	}	while (0)

/* premultiply: dst atop */
#define IBLEND_OP_DST_ATOP(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = 255 - (da); \
		IUINT32 FD = (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, FD); \
	}	while (0)

/* premultiply: dst in */
#define IBLEND_OP_DST_IN(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FD = (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, 0, FD); \
	}	while (0)

/* premultiply: dst out */
#define IBLEND_OP_DST_OUT(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FD = 255 - (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, 0, FD); \
	}	while (0)

/* premultiply: dst over */
#define IBLEND_OP_DST_OVER(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = 255 - (da); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, 255); \
	}	while (0)

/* premultiply: xor */
#define IBLEND_OP_XOR(sr, sg, sb, sa, dr, dg, db, da) do { \
		IUINT32 FS = 255 - (da); \
		IUINT32 FD = 255 - (sa); \
		IBLEND_COMPOSITE(sr, sg, sb, sa, dr, dg, db, da, FS, FD); \
	}	while (0)

/* premultiply: plus */
#define IBLEND_OP_PLUS(sr, sg, sb, sa, dr, dg, db, da) do { \
		(dr) = ICLIP_256((sr) + (dr)); \
		(dg) = ICLIP_256((sg) + (dg)); \
		(db) = ICLIP_256((sb) + (db)); \
		(da) = ICLIP_256((sa) + (da)); \
	}	while (0)


#ifdef __cplusplus
}
#endif


#endif

/* vim: set ts=4 sw=4 tw=0 noet :*/

