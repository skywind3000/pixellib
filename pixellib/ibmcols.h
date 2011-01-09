//=====================================================================
//
// ibmcols.h - ibitmap color components interface
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __IBMCOLS_H__
#define __IBMCOLS_H__

#include "ibitmap.h"
#include "ibmbits.h"


//---------------------------------------------------------------------
// IUINT64 / IINT64 definition
//---------------------------------------------------------------------
#ifndef __IINT64_DEFINED
	#define __IINT64_DEFINED
	#if defined(_MSC_VER) || defined(__BORLANDC__)
		typedef __int64 IINT64;
	#else
		typedef long long IINT64;
	#endif
#endif

#ifndef __IUINT64_DEFINED
	#define __IUINT64_DEFINED
	#if defined(_MSC_VER) || defined(__BORLANDC__)
		typedef unsigned __int64 IUINT64;
	#else
		typedef unsigned long long IUINT64;
	#endif
#endif


//---------------------------------------------------------------------
// misc structures
//---------------------------------------------------------------------

// 采样越界控制
enum IBOM
{
	IBOM_TRANSPARENT = 0,	// 越界时，使用透明色，即 IBITMAP::mask
	IBOM_REPEAT = 1,		// 越界时，重复边界颜色
	IBOM_WRAP = 2,			// 越界时，使用下一行颜色
	IBOM_MIRROR = 3,		// 越界时，使用镜像颜色
};

// 滤波模式
enum IPIXELFILTER
{
	IPIXEL_FILTER_BILINEAR	= 0,	// 滤波模式：双线性过滤采样
	IPIXEL_FILTER_NEAREST	= 1,	// 滤波模式：最近采样
};

// 矩形申明
struct IRECT
{
	int left;
	int top;
	int right;
	int bottom;
};

// 位图模式
struct IMODE
{
	union {
		struct { 
			unsigned char reserve1;		// 保留
			unsigned char reserve2;		// 保留
			unsigned char pixfmt : 6;	// 颜色格式
			unsigned char fmtset : 1;	// 是否设置颜色格式
			unsigned char overflow : 2;	// 越界采样模式，见 IBOM定义
			unsigned char filter : 2;	// 过滤器
			unsigned char refbits : 1;	// 是否引用数据
			unsigned char subpixel : 2;	// 子像素模式
		};
		unsigned long mode;
	};
};

typedef struct IBITMAP IBITMAP;
typedef struct IRECT IRECT;
typedef struct IMODE IMODE;
typedef enum IBOM IBOM;
typedef enum IPIXELFILTER IPIXELFILTER;


#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// 位图操作
//---------------------------------------------------------------------

// 取得像素格式，见ibmbits.h中的 IPIX_FMT_xxx
int ibitmap_pixfmt_get(const IBITMAP *bmp);

// 设置像素格式
void ibitmap_pixfmt_set(IBITMAP *bmp, int pixfmt);

// 猜测颜色格式
int ibitmap_pixfmt_guess(const IBITMAP *src);

// 取得over flow模式
enum IBOM ibitmap_overflow_get(const IBITMAP *bmp);

// 设置over flow模式
void ibitmap_overflow_set(IBITMAP *bmp, enum IBOM mode);

// 取得颜色索引
iColorIndex *ibitmap_index_get(IBITMAP *bmp);

// 设置颜色索引
void ibitmap_index_set(IBITMAP *bmp, iColorIndex *index);

// 设置滤波器
void ibitmap_filter_set(IBITMAP *bmp, enum IPIXELFILTER filter);

// 取得滤波器
enum IPIXELFILTER ibitmap_filter_get(const IBITMAP *bmp);

// BLIT 裁剪
int ibitmap_clipex(const IBITMAP *dst, int *dx, int *dy, const IBITMAP *src,
	int *sx, int *sy, int *sw, int *sh, const IRECT *clip, int flags);


#define IBLIT_NOCLIP	16
#define IBLIT_ADDITIVE	32

// 混色绘制
void ibitmap_blend(IBITMAP *dst, int dx, int dy, const IBITMAP *src, int sx, 
	int sy, int w, int h, IUINT32 color, const IRECT *clip, int flags);


// 颜色转换
void ibitmap_convert(IBITMAP *dst, int dx, int dy, const IBITMAP *src, 
	int sx, int sy, int w, int h, const IRECT *clip, int flags);

// 颜色转换，生成新的IBITMAP
IBITMAP *ibitmap_convfmt(int dfmt, const IBITMAP *src, const IRGB *spal, 
	const IRGB *dpal);

// 引用BITMAP
IBITMAP *ibitmap_reference_new(void *ptr, long pitch, int w, int h, int fmt);

// 删除引用
void ibitmap_reference_del(IBITMAP *bitmap);

// 调整引用
void ibitmap_reference_adjust(IBITMAP *bmp, void *ptr, long pitch);

// 绘制矩形
void ibitmap_rectfill(IBITMAP *dst, int x, int y, int w, int h, IUINT32 c);

// 带遮罩的填充，alpha必须是一个8位格式的alpha位图(IPIX_FMT_A8)
void ibitmap_maskfill(IBITMAP *dst, int dx, int dy, const IBITMAP *alpha,
	int sx, int sy, int sw, int sh, IUINT32 color, const IRECT *clip);

// 截取IBITMAP 区域中的部分
IBITMAP *ibitmap_chop(const IBITMAP *src, int x, int y, int w, int h);


//=====================================================================
// 32位定点数：高16位表示整数，低16位为小数
//=====================================================================
#ifndef CFIXED_16_16
#define CFIXED_16_16
typedef IINT32 cfixed;

#define cfixed_from_int(i)		(((cfixed)(i)) << 16)
#define cfixed_from_float(x)	((cfixed)((x) * 65536.0f))
#define cfixed_from_double(d)	((cfixed)((d) * 65536.0))
#define cfixed_to_int(f)		((f) >> 16)
#define cfixed_to_float(x)		((float)((x) / 65536.0f))
#define cfixed_to_double(f)		((double)((f) / 65536.0))
#define cfixed_const_1			(cfixed_from_int(1))
#define cfixed_const_e			((cfixed)(1))
#define cfixed_const_1_m_e		(cfixed_const_1 - cfixed_const_e)
#define cfixed_const_half		(cfixed_const_1 >> 1)
#define cfixed_frac(f)			((f) & cfixed_const_1_m_e)
#define cfixed_floor(f)			((f) & (~cfixed_const_1_m_e))
#define cfixed_ceil(f)			(cfixed_floor((f) + 0xffff))
#define cfixed_mul(x, y)		((cfixed)((((IINT64)(x)) * (y)) >> 16))
#define cfixed_div(x, y)		((cfixed)((((IINT64)(x)) << 16) / (y)))
#define cfixed_const_max		((IINT64)0x7fffffff)
#define cfixed_const_min		(-((((IINT64)1) << 31)))

#endif



//=====================================================================
// 扫描线操作
//=====================================================================

void ibitmap_scanline_store(IBITMAP *bmp, int x, int y, int w,
	const IUINT32 *card, const IRECT *clip, iStoreProc store);

void ibitmap_scanline_blend(IBITMAP *bmp, int x, int y, int w, const IUINT32 
	*card, const IUINT8 *cover, const IRECT *clip, iSpanDrawProc span);


// 取得扫描线像素的函数定义
typedef void (*iBitmapFetchProc)(const IBITMAP *bmp, IUINT32 *card, int w,
	const cfixed *pos, const cfixed *step, const IUINT8 *mask, 
	const IRECT *clip);

// 浮点数取得扫描线的函数定义
typedef void (*iBitmapFetchFloat)(const IBITMAP *bmp, IUINT32 *card, int w,
	const float *pos, const float *step, const IUINT8 *mask, 
	const IRECT *clip);


// 取得扫描线的模式
#define IBITMAP_FETCH_GENERAL_NEAREST				0
#define IBITMAP_FETCH_GENERAL_BILINEAR				1
#define IBITMAP_FETCH_TRANSLATE_NEAREST				2
#define IBITMAP_FETCH_TRANSLATE_BILINEAR			3
#define IBITMAP_FETCH_SCALE_NEAREST					4
#define IBITMAP_FETCH_SCALE_BILINEAR				5
#define IBITMAP_FETCH_REPEAT_TRANSLATE_NEAREST		6
#define IBITMAP_FETCH_REPEAT_TRANSLATE_BILINEAR		7
#define IBITMAP_FETCH_REPEAT_SCALE_NEAREST			8
#define IBITMAP_FETCH_REPEAT_SCALE_BILINEAR			9
#define IBITMAP_FETCH_WRAP_TRANSLATE_NEAREST		10
#define IBITMAP_FETCH_WRAP_TRANSLATE_BILINEAR		11
#define IBITMAP_FETCH_WRAP_SCALE_NEAREST			12
#define IBITMAP_FETCH_WRAP_SCALE_BILINEAR			13
#define IBITMAP_FETCH_MIRROR_TRANSLATE_NEAREST		14
#define IBITMAP_FETCH_MIRROR_TRANSLATE_BILINEAR		15
#define IBITMAP_FETCH_MIRROR_SCALE_NEAREST			16
#define IBITMAP_FETCH_MIRROR_SCALE_BILINEAR			17


// 取得扫描线函数
iBitmapFetchProc ibitmap_scanline_get_proc(int pixfmt, int mode, int isdef);

// 取得浮点数扫描线函数
iBitmapFetchFloat ibitmap_scanline_get_float(int pixfmt, int isdefault);

// 设置扫描线函数
void ibitmap_scanline_set_proc(int pixfmt, int mode, iBitmapFetchProc proc);

// 设置扫描线浮点函数
void ibitmap_scanline_set_float(int pixfmt, iBitmapFetchFloat proc);

// 取得模式
int ibitmap_scanline_get_mode(const IBITMAP *bmp, const cfixed *src,
	const cfixed *step);


// 通用取得浮点扫描线
void ibitmap_scanline_float(const IBITMAP *bmp, IUINT32 *card, int w,
	const float *srcvec, const float *stepvec, const IUINT8 *mask,
	const IRECT *clip);

// 通用取得描线：使用浮点数内核的定点数接口
void ibitmap_scanline_fixed(const IBITMAP *bmp, IUINT32 *card, int w,
	const cfixed *srcvec, const cfixed *stepvec, const IUINT8 *mask,
	const IRECT *clip);


//---------------------------------------------------------------------
// 通用部分
//---------------------------------------------------------------------
#define ibitmap_imode(bmp, _FIELD) (((IMODE*)&((bmp)->mode))->_FIELD)

#define ibitmap_imode_const(bmp, _FIELD) \
	(((const IMODE*)&((bmp)->mode))->_FIELD)

#ifndef IBITMAP_STACK_BUFFER
	#define IBITMAP_STACK_BUFFER	16384
#endif

/* draw pixel list with single color */
typedef void (*iPixelListSC)(IBITMAP *bmp, const IUINT32 *xy, int count,
	IUINT32 color, int additive);

/* draw pixel list with different color */
typedef void (*iPixelListMC)(IBITMAP *bmp, const IUINT32 *xycol, int count,
	int additive);


iPixelListSC ibitmap_get_pixel_list_sc_proc(int pixfmt, int isdefault);

iPixelListMC ibitmap_get_pixel_list_mc_proc(int pixfmt, int isdefault);

void ibitmap_set_pixel_list_sc_proc(int pixfmt, iPixelListSC proc);

void ibitmap_set_pixel_list_mc_proc(int pixfmt, iPixelListMC proc);


void ibitmap_draw_pixel_list_sc(IBITMAP *bmp, const IUINT32 *xy, int count,
	IUINT32 color, int additive);

void ibitmap_draw_pixel_list_mc(IBITMAP *bmp, const IUINT32 *xyc, int count,
	int additive);


//=====================================================================
// GLYPH IMPLEMENTATION
//=====================================================================
typedef struct
{
	int w;
	int h;
	unsigned char data[1];
}	IGLYPHCHAR;

typedef struct
{
	int w;
	int h;
	const unsigned char *font;
}	IGLYPHFONT;



void ibitmap_draw_glyph(IBITMAP *dst, const IGLYPHCHAR *glyph, int x, int y,
	const IRECT *clip, IUINT32 color, IUINT32 bk, int additive);

void ibitmap_draw_ascii(IBITMAP *dst, const IGLYPHFONT *font, int x, int y,
	const char *string, const IRECT *clip, IUINT32 col, IUINT32 bk, int add);



//=====================================================================
// 位图实用工具
//=====================================================================
// 缩放裁剪
int ibitmap_clip_scale(const IRECT *clipdst, const IRECT *clipsrc, 
	IRECT *bound_dst, IRECT *bound_src, int mode);

// 缩放绘制：接受IBLIT_MASK参数，不接受 IBLIT_HFLIP, IBLIT_VFLIP
int ibitmap_scale(IBITMAP *dst, const IRECT *bound_dst, const IBITMAP *src,
	const IRECT *bound_src, const IRECT *clip, int mode);

// 安全 BLIT，支持不同像素格式
int ibitmap_blit2(IBITMAP *dst, int x, int y, const IBITMAP *src,
	const IRECT *bound_src, const IRECT *clip, int mode);

// 重新采样
IBITMAP *ibitmap_resample(const IBITMAP *src, const IRECT *bound, 
	int newwidth, int newheight, int mode);


//=====================================================================
// 像素合成
//=====================================================================

#define IPIXEL_COMPOSITE_SRC_A8R8G8B8	64
#define IPIXEL_COMPOSITE_DST_A8R8G8B8	128
#define IPIXEL_COMPOSITE_FORCE_32		256

// 位图合成
int ibitmap_composite(IBITMAP *dst, int dx, int dy, const IBITMAP *src, 
	int sx, int sy, int w, int h, const IRECT *clip, int op, int flags);


//=====================================================================
// Inline Utilities
//=====================================================================
static inline IRECT *ipixel_rect_set(IRECT *dst, int l, int t, int r, int b)
{
	dst->left = l;
	dst->top = t;
	dst->right = r;
	dst->bottom = b;
	return dst;
}

static inline IRECT *ipixel_rect_copy(IRECT *dst, const IRECT *src)
{
	dst->left = src->left;
	dst->top = src->top;
	dst->right = src->right;
	dst->bottom = src->bottom;
	return dst;
}

static inline IRECT *ipixel_rect_offset(IRECT *self, int x, int y)
{
	self->left += x;
	self->top += y;
	self->right += x;
	self->bottom += y;
	return self;
}

static inline int ipixel_rect_contains(const IRECT *self, int x, int y)
{
	return (x >= self->left && y >= self->top &&
		x < self->right && y < self->bottom);
}

static inline int ipixel_rect_intersects(const IRECT *self, const IRECT *src)
{
	return !((src->right <= self->left) || (src->bottom <= self->top) ||
		(src->left >= self->right) || (src->top >= self->bottom));
}

static inline IRECT *ipixel_rect_intersection(IRECT *dst, const IRECT *src)
{
	int x1 = (dst->left > src->left)? dst->left : src->left;
	int x2 = (dst->right < src->right)? dst->right : src->right;
	int y1 = (dst->top > src->top)? dst->top : src->top;
	int y2 = (dst->bottom < src->bottom)? dst->bottom : src->bottom;
	if (x1 > x2 || y1 > y2) {
		dst->left = 0;
		dst->top = 0;
		dst->right = 0;
		dst->bottom = 0;
	}	else {
		dst->left = x1;
		dst->top = y1;
		dst->right = x2;
		dst->bottom = y2;
	}
	return dst;
}

static inline IRECT *ipixel_rect_union(IRECT *dst, const IRECT *src)
{
	int x1 = (dst->left < src->left)? dst->left : src->left;
	int x2 = (dst->right > src->right)? dst->right : src->right;
	int y1 = (dst->top < src->top)? dst->top : src->top;
	int y2 = (dst->bottom > src->bottom)? dst->bottom : src->bottom;
	dst->left = x1;
	dst->top = y1;
	dst->right = x2;
	dst->bottom = y2;
	return dst;
}

static inline cfixed cfixed_abs(cfixed x) {
	return (x < 0)? (-x) : x;
}

static inline cfixed cfixed_min(cfixed x, cfixed y) {
	return (x < y)? x : y;
}

static inline cfixed cfixed_max(cfixed x, cfixed y) {
	return (x > y)? x : y;
}

static inline cfixed cfixed_inverse(cfixed x) {
	return (cfixed) ((((IINT64)cfixed_const_1) * cfixed_const_1) / x);
}

static inline int cfixed_within_epsilon(cfixed a, cfixed b, cfixed epsilon) {
	cfixed c = a - b;
	if (c < 0) c = -c;
	return c <= epsilon;
}

static inline int cfloat_within_epsilon(float a, float b, float epsilon) {
	float c = a - b;
	if (c < 0) c = -c;
	return c <= epsilon;
}

static inline int cfloat_to_int_ieee(float f) {
	union { IINT32 intpart; float floatpart; } convert;
	IINT32 sign, mantissa, exponent, r;
	convert.floatpart = f;
	sign = (convert.intpart >> 31);
	mantissa = (convert.intpart & ((1 << 23) - 1)) | (1 << 23);
	exponent = ((convert.intpart & 0x7ffffffful) >> 23) - 127;
	r = ((IUINT32)(mantissa) << 8) >> (31 - exponent);
	return ((r ^ sign) - sign) & ~(exponent >> 31);
}

static inline int cfloat_ieee_enable() {
	return (cfloat_to_int_ieee(1234.56f) == 1234);
}

static inline cfixed cfixed_from_float_ieee(float f) {
	return (cfixed)cfloat_to_int_ieee(f * 65536.0f);
}


#define IEPSILON_E4     ((float)1E-4)
#define IEPSILON_E5     ((float)1E-5)
#define IEPSILON_E6     ((float)1E-6)

#define CFIXED_EPSILON  ((cfixed)3)
#define CFLOAT_EPSILON  IEPSILON_E5

#define cfixed_is_same(a, b)  (cfixed_within_epsilon(a, b, CFIXED_EPSILON))
#define cfixed_is_zero(a)     cfixed_is_same(a, 0)
#define cfixed_is_one(a)      cfixed_is_same(a, cfixed_const_1)
#define cfixed_is_int(a)      cfixed_is_same(cfixed_frac(a), 0)

#define cfloat_is_same(a, b)  (cfloat_within_epsilon(a, b, CFLOAT_EPSILON))
#define cfloat_is_zero(a)     cfloat_is_same(a, 0.0f)
#define cfloat_is_one(a)      cfloat_is_same(a, 1.0f)


//=====================================================================
// HSV / HSL Color Space
//=====================================================================

// RGB -> HSV
static inline void ipixel_cvt_rgb_to_hsv(int r, int g, int b, 
	float *H, float *S, float *V)
{
	int imin = (r < g)? ((r < b)? r : b) : ((g < b)? g : b);
	int imax = (r > g)? ((r > b)? r : b) : ((g > b)? g : b);
	int idelta = imax - imin;

	*V = imax * (1.0f / 255.0f);

	if (imax == 0) {
		*S = *H = 0.0f;
	}	else {
		float h;
		*S = ((float)idelta) / ((float)imax);
		if (r == imax) h = ((float)(g - b)) / idelta;
		else if (g == imax) h = 2.0f + ((float)(b - r)) / idelta;
		else h = 4.0f + ((float)(r - g)) / idelta;
		h *= 60.0f;
		if (h < 0.0f) h += 360.0f;
		*H = h;
	}
}

// HSV -> RGB
static inline IUINT32 ipixel_cvt_hsv_to_rgb(float h, float s, float v)
{
	float r, g, b;
	IINT32 R, G, B;
	v = (1.0f < v)? 1.0f : v;
	s = (1.0f < s)? 1.0f : s;  
	r = g = b = v;
	if (s != 0.0f) {
		float f, p, q, t;
		int i;
		while (h < 0.0f) h += 360.0f;
		while (h >= 360.0f) h -= 360.0f;
		h *= (1.0f / 60.0f);
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
		}
	}
	R = (IINT32)(r * 255.0f);
	G = (IINT32)(g * 255.0f);
	B = (IINT32)(b * 255.0f);
	return IRGBA_TO_A8R8G8B8(R, G, B, 0);
}

// RGB -> HSL
static inline void ipixel_cvt_rgb_to_hsl(int r, int g, int b, 
	float *H, float *S, float *L)
{
	int minval = (r < g)? ((r < b)? r : b) : ((g < b)? g : b);
	int maxval = (r > g)? ((r > b)? r : b) : ((g > b)? g : b);
	float mdiff = (float)(maxval - minval);
	float msum = (float)(maxval + minval);
	*L = msum * (1.0f / 510.0f);
	if (maxval == minval) {
		*S = *H = 0.0f;
	}	else {
		float mdiffinv = 1.0f / mdiff;
		float rnorm = (maxval - r) * mdiffinv;
		float gnorm = (maxval - g) * mdiffinv;
		float bnorm = (maxval - b) * mdiffinv;
		*S = (*L <= 0.5f)? (mdiff / msum) : (mdiff / (510.0f - msum));
		if (r == maxval) *H = 60.0f * (6.0f + bnorm - gnorm);
		else if (g == maxval) *H = 60.0f * (2.0f + rnorm - bnorm); 
		else *H = 60.0f * (4.0f + gnorm - rnorm);
		if (*H > 360.0f) *H -= 360.0f;
	}
}

// HUE -> RGB
static inline IINT32 ipixel_cvt_hue_to_rgb(float rm1, float rm2, float rh)
{
	IINT32 n;
	while (rh > 360.0f) rh -= 360.0f;
	while (rh < 0.0f) rh += 360.0f;
	if (rh < 60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f; 
	else if (rh < 180.0f) rm1 = rm2;
	else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;
	n = (IINT32)(rm1 * 255);
	return (n > 255)? 255 : ((n < 0)? 0 : n);
}

// HSL -> RGB
static inline IUINT32 ipixel_cvt_hsl_to_rgb(float H, float S, float L)
{
	IUINT32 r, g, b;  
	L = (1.0f < L)? 1.0f : L;
	S = (1.0f < S)? 1.0f : S;  
	if (S == 0.0) {
		r = g = b = (IUINT32)(255 * L);  
	}   else {  
		float rm1, rm2;  
		if (L <= 0.5f) rm2 = L + L * S;  
		else rm2 = L + S - L * S;  
		rm1 = 2.0f * L - rm2;     
		r = ipixel_cvt_hue_to_rgb(rm1, rm2, H + 120.0f);  
		g = ipixel_cvt_hue_to_rgb(rm1, rm2, H);  
		b = ipixel_cvt_hue_to_rgb(rm1, rm2, H - 120.0f);  
	}
	return IRGBA_TO_A8R8G8B8(r, g, b, 0);  
}



#ifdef __cplusplus
}
#endif

#endif



