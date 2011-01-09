#include "PixelBitmap.h"
#include "ipicture.h"
#include "ibmdata.h"
#include "ibmwink.h"
#include "ibmfont.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_END(name) }

NAMESPACE_BEGIN(Pixel);

Bitmap::~Bitmap() 
{
	Release();
}

Bitmap::Bitmap()
{
	classcode = 0;
	bitmap = NULL;
	ownbits = false;
	reference = false;
}

Bitmap::Bitmap(int width, int height, enum Format pixfmt)
{
	classcode = 0;
	ownbits = false;
	reference = false;
	bitmap = NULL;
	if (Create(width, height, pixfmt) != 0) {
		throw new BitmapError("cannot create bitmap");
	}
}

Bitmap::Bitmap(const char *filename, IRGB *pal)
{
	classcode = 0;
	bitmap = NULL;
	ownbits = false;
	reference = false;
	if (Load(filename, pal) != 0) {
		char text[1024];
#ifndef _MSC_VER
		strcpy(text, "cannot load: ");
		strcat(text, filename);
#else
		strcpy_s(text, "cannot load: ");
		strcat_s(text, filename);
#endif
		throw new BitmapError(text);
	}
}

Bitmap::Bitmap(const void *picmem, size_t size, IRGB *pal)
{
	classcode = 0;
	bitmap = NULL;
	ownbits = false;
	reference = false;
	if (Load(picmem, size, pal) != 0) {
		char text[1024];
#ifndef _MSC_VER
		strcpy(text, "cannot load from memory");
#else
		strcpy_s(text, "cannot load from memory");
#endif
	}
}

Bitmap::Bitmap(int width, int height, enum Format pixfmt, long pitch, void *bits)
{
	classcode = 0;
	bitmap = NULL;
	ownbits = false;
	reference = false;
	if (Create(width, height, pixfmt, pitch, bits) != 0) {
		throw new BitmapError("cannot create bitmap");
	}
}

Bitmap::Bitmap(IBITMAP *bmp, bool ownbits)
{
	classcode = 0;
	bitmap = NULL;
	this->ownbits = ownbits;
	reference = false;
	Assign(bmp, ownbits);
}



int Bitmap::Create(int width, int height, enum Format pixfmt)
{
	int fmt = (int)pixfmt;
	if (bitmap) Release();
	bitmap = ibitmap_create(width, height, ipixelfmt[fmt].bpp);
	if (bitmap == NULL) return -1;
	ibitmap_pixfmt_set(bitmap, fmt);
	ownbits = true;
	reference = false;
	this->pixfmt = fmt;
	SetClip(NULL);
	return 0;
}

int Bitmap::Create(int width, int height, enum Format pixfmt, long pitch, void *bits)
{
	int fmt = (int)pixfmt;
	if (bitmap) Release();
	bitmap = ibitmap_reference_new(bits, pitch, width, height, fmt);
	if (bitmap == NULL) return -1;
	ibitmap_pixfmt_set(bitmap, fmt);
	ownbits = true;
	reference = true;
	this->pixfmt = fmt;
	SetClip(NULL);
	return 0;
}

void Bitmap::Release()
{
	if (bitmap) {
		if (ownbits) {
			if (reference == false) ibitmap_release(bitmap);
			else ibitmap_reference_del(bitmap);
		}
		bitmap = NULL;
		ownbits = false;
		reference = false;
	}
}

int Bitmap::Load(const char *filename, IRGB *pal)
{
	if (bitmap) Release();
	bitmap = ipic_load_file(filename, 0, pal);
	if (bitmap == NULL) return -1;
	ibitmap_pixfmt_set(bitmap, ibitmap_pixfmt_guess(bitmap));
	ownbits = true;
	reference = false;
	this->pixfmt = ibitmap_pixfmt_guess(bitmap);
	SetClip(NULL);
	return 0;
}

// 从内存读取
int Bitmap::Load(const void *picmem, size_t size, IRGB *pal)
{
	if (bitmap) Release();
	bitmap = ipic_load_mem(picmem, (long)size, pal);
	if (bitmap == NULL) return -1;
	ibitmap_pixfmt_set(bitmap, ibitmap_pixfmt_guess(bitmap));
	ownbits = true;
	reference = false;
	this->pixfmt = ibitmap_pixfmt_guess(bitmap);
	SetClip(NULL);
	return 0;
}

int Bitmap::Assign(IBITMAP *bmp, bool ownbits) 
{
	if (bitmap) Release();
	bitmap = bmp;
	this->ownbits = ownbits;
	SetClip(NULL);
	this->pixfmt = ibitmap_pixfmt_guess(bitmap);
	return 0;
}

int Bitmap::GetW() const { 
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return (bitmap)? (int)bitmap->w : -1;
}

int Bitmap::GetH() const { 
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return (bitmap)? (int)bitmap->h : -1;
}

int Bitmap::GetBPP() const {
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return (bitmap)? (int)bitmap->bpp : -1; 
}

long Bitmap::GetPitch() const { 
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return (bitmap)? (long)bitmap->pitch : -1; 
}

enum Format Bitmap::GetFormat() const { 
	if (bitmap == NULL) {
		throw new BitmapError("no bitmap created");
	}
	int fmt = ibitmap_pixfmt_get(bitmap);
	return (Format)fmt;
}

IBITMAP *Bitmap::GetBitmap() { 
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return bitmap; 
}

const IBITMAP* Bitmap::GetBitmap() const
{
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return bitmap;
}

IUINT8 *Bitmap::GetLine(int line) {
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	if (line < 0 || line >= (int)bitmap->h) {
		throw new BitmapError("out of bitmap line index");
	}
	return (IUINT8*)bitmap->line[line];
}

IUINT8 *Bitmap::GetPixel() {
	if (bitmap == NULL) throw new BitmapError("no bitmap created");
	return (IUINT8*)bitmap->pixel;
}


// 设置绘制裁剪矩形
void Bitmap::SetClip(const IRECT *rect)
{
	if (rect == NULL) {
		clip.left = 0;
		clip.top = 0;
		if (bitmap) {
			clip.right = (int)bitmap->w;
			clip.bottom = (int)bitmap->h;
		}	else {
			clip.right = 0;
			clip.bottom = 0;
		}
	}	else {
		clip.left = rect->left;
		clip.top = rect->top;
		clip.right = rect->right;
		clip.bottom = rect->bottom;
	}
	if (clip.left < 0) clip.left = 0;
	if (clip.top < 0) clip.top = 0;
	if (bitmap) {
		if (clip.right >= (int)bitmap->w) clip.right = (int)bitmap->w;
		if (clip.left >= (int)bitmap->h) clip.right = (int)bitmap->h;
	}	else {
		clip.right = 0;
		clip.bottom = 0;
	}
}

void Bitmap::SetClip(int left, int top, int right, int bottom)
{
	IRECT rect;
	rect.left = left;
	rect.top = top;
	rect.right = right;
	rect.bottom = bottom;
	SetClip(&rect);
}

// 设置关键色
void Bitmap::SetMask(IUINT32 mask)
{
	if (bitmap == NULL) 
		throw new BitmapError("no bitmap created");
	bitmap->mask = (unsigned long)mask;
}

// 设置采样过滤
void Bitmap::SetFilter(enum Filter filter)
{
	if (bitmap == NULL) 
		throw new BitmapError("no bitmap created");
	ibitmap_imode(bitmap, filter) = (int)filter;
}

// 设置越界模式
void Bitmap::SetOverflow(enum OverflowMode mode)
{
	if (bitmap == NULL) 
		throw new BitmapError("no bitmap created");
	ibitmap_imode(bitmap, overflow) = (int)mode;
}

// 设置抗锯齿
void Bitmap::SetAntiAliasing(enum AntiAliasing mode)
{
	if (bitmap == NULL) 
		throw new BitmapError("no bitmap created");
	ibitmap_imode(bitmap, subpixel) = (int)mode;
}


//---------------------------------------------------------------------
// 图像基础
//---------------------------------------------------------------------
int Bitmap::Blit(int x, int y, const IBITMAP *src, const IRECT *rect, int mode)
{
	return ibitmap_blit2(bitmap, x, y, src, rect, &clip, mode);
}

int Bitmap::Blit(int x, int y, const IBITMAP *src, int sx, int sy, int sw, int sh, int mode)
{
	IRECT bound;
	ipixel_rect_set(&bound, sx, sy, sx + sw, sy + sh);
	return Blit(x, y, src, &bound, mode);
}

int Bitmap::Blit(int x, int y, const Bitmap *src, const IRECT *rect, int mode)
{
	if (bitmap == NULL || src == NULL) 
		return -10;
	return Blit(x, y, src->bitmap, rect, mode);
}

int Bitmap::Blit(int x, int y, const Bitmap *src, int sx, int sy, int sw, int sh, int mode)
{
	if (bitmap == NULL || src == NULL) 
		return -10;
	return Blit(x, y, src->bitmap, sx, sy, sw, sh, mode);
}

int Bitmap::Stretch(const IRECT *bound_dst, const IBITMAP *src, const IRECT *bound_src, int mode)
{
	if (bitmap == NULL || src == NULL) 
		return 10;
	return ibitmap_scale(bitmap, bound_dst, src, bound_src, &clip, mode);
}

int Bitmap::Stretch(const IRECT *bound_dst, const Bitmap *src, const IRECT *bound_src, int mode)
{
	if (bitmap == NULL || src == NULL) 
		return 10;
	return Stretch(bound_dst, src->bitmap, bound_src, mode);
}

void Bitmap::FillRaw(IUINT32 rawcolor, const IRECT *rect)
{
	IRECT bound;
	if (bitmap == NULL) return;
	if (rect == NULL) {
		ipixel_rect_set(&bound, 0, 0, (int)bitmap->w, (int)bitmap->h);
	}	else {
		ipixel_rect_copy(&bound, rect);
	}
	ipixel_rect_intersection(&bound, &clip);
	ibitmap_fill(bitmap, bound.left, bound.top, bound.right - bound.left,
		bound.bottom - bound.top, rawcolor, 1);
}

void Bitmap::FillRaw(IUINT32 rawcolor, int x, int y, int w, int h)
{
	IRECT bound;
	ipixel_rect_set(&bound, x, y, x + w, y + h);
	FillRaw(rawcolor, &bound);
}


int Bitmap::Blend(int x, int y, const IBITMAP *src, const IRECT *rect, IUINT32 color, int mode)
{
	int sx, sy, sw, sh;
	if (bitmap == NULL || src == NULL)
		return -100;
	if (rect != NULL) {
		sx = rect->left;
		sy = rect->top;
		sw = rect->right - sx;
		sh = rect->bottom - sy;
	}	else {
		sx = 0;
		sy = 0;
		sw = (int)src->w;
		sh = (int)src->h;
	}
	ibitmap_blend(bitmap, x, y, src, sx, sy, sw, sh, color, &clip, mode);
	return 0;
}

int Bitmap::Blend(int x, int y, const IBITMAP *src, int sx, int sy, int sw, int sh, IUINT32 color, int mode)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	ibitmap_blend(bitmap, x, y, src, sx, sy, sw, sh, color, &clip, mode);
	return 0;
}


int Bitmap::Blend(int x, int y, const Bitmap *src, const IRECT *rect, IUINT32 color, int mode)
{
	if (bitmap == NULL) 
		return -100;
	return Blend(x, y, src->bitmap, rect, color, mode);
}

int Bitmap::Blend(int x, int y, const Bitmap *src, int sx, int sy, int sw, int sh, IUINT32 color, int mode)
{
	if (bitmap == NULL) 
		return -100;
	return Blend(x, y, src->bitmap, sx, sy, sw, sh, color, mode);
}

void Bitmap::Fill(IUINT32 rgba, const IRECT *rect)
{
	IRECT bound;
	if (bitmap == NULL) return;
	if (rect == NULL) {
		ipixel_rect_set(&bound, 0, 0, (int)bitmap->w, (int)bitmap->h);
	}	else {
		ipixel_rect_copy(&bound, rect);
	}
	ipixel_rect_intersection(&bound, &clip);
	ibitmap_rectfill(bitmap, bound.left, bound.top, bound.right - bound.left,
		bound.bottom - bound.top, rgba);
}

void Bitmap::Fill(IUINT32 rgba, int x, int y, int w, int h)
{
	IRECT bound;
	ipixel_rect_set(&bound, x, y, x + w, y + h);
	Fill(rgba, &bound);
}

int Bitmap::MaskFill(int x, int y, const IBITMAP *src, const IRECT *rect, IUINT32 color)
{
	int sx, sy, sw, sh;
	if (bitmap == NULL || src == NULL)
		return -100;
	if (rect != NULL) {
		sx = rect->left;
		sy = rect->top;
		sw = rect->right - sx;
		sh = rect->bottom - sy;
	}	else {
		sx = 0;
		sy = 0;
		sw = (int)src->w;
		sh = (int)src->h;
	}
	ibitmap_maskfill(bitmap, x, y, src, sx, sy, sw, sh, color, &clip);
	return 0;
}

int Bitmap::MaskFill(int x, int y, const IBITMAP *src, int sx, int sy, int sw, int sh, IUINT32 color)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	ibitmap_maskfill(bitmap, x, y, src, sx, sy, sw, sh, color, &clip);
	return 0;
}

int Bitmap::MaskFill(int x, int y, const Bitmap *src, const IRECT *rect, IUINT32 color)
{
	if (src->bitmap == NULL)
		return -200;
	return MaskFill(x, y, src->bitmap, rect, color);
}

int Bitmap::MaskFill(int x, int y, const Bitmap *src, int sx, int sy, int sw, int sh, IUINT32 color)
{
	if (src->bitmap == NULL)
		return -200;
	return MaskFill(x, y, src->bitmap, sx, sy, sw, sh, color);
}



//---------------------------------------------------------------------
// 图像合成
//---------------------------------------------------------------------
int Bitmap::Composite(int x, int y, const IBITMAP *src, int sx, int sy, int sw, int sh, int op, int flags)
{
	return ibitmap_composite(bitmap, x, y, src, sx, sy, sw, sh, &clip, op, flags);
}

int Bitmap::Composite(int x, int y, const Bitmap *src, int sx, int sy, int sw, int sh, int op, int flags)
{
	return ibitmap_composite(bitmap, x, y, src->bitmap, sx, sy, sw, sh, &clip, op, flags);
}

int Bitmap::Composite(int x, int y, const IBITMAP *src, int op, const IRECT *rect, int flags)
{
	int sx, sy, sw, sh;
	if (rect != NULL) {
		sx = rect->left;
		sy = rect->top;
		sw = rect->right - rect->left;
		sh = rect->bottom - rect->top;
	}	else {
		sx = sy = 0;
		sw = (int)src->w;
		sh = (int)src->h;
	}
	return ibitmap_composite(bitmap, x, y, src, sx, sy, sw, sh, &clip, op, flags);
}

int Bitmap::Composite(int x, int y, const Bitmap *src, int op, const IRECT *rect, int flags)
{
	return Composite(x, y, src->bitmap, op, rect, flags);
}


//---------------------------------------------------------------------
// 图像绘制
//---------------------------------------------------------------------
int Bitmap::ConvertSelf(enum Format NewFormat)
{
	IBITMAP *savebmp;
	if (bitmap == NULL) return -1;
	savebmp = ibitmap_create(GetW(), GetH(), GetBPP());
	if (savebmp == NULL) return -2;
	ibitmap_pixfmt_set(savebmp, ibitmap_pixfmt_guess(bitmap));
	ibitmap_blit(savebmp, 0, 0, bitmap, 0, 0, GetW(), GetH(), 0);
	if (Create((int)savebmp->w, (int)savebmp->h, NewFormat) != 0)
		return -3;
	ibitmap_convert(bitmap, 0, 0, savebmp, 0, 0, GetW(), GetH(), NULL, 0);
	ibitmap_release(savebmp);
	return 0;
}

Bitmap *Bitmap::Convert(enum Format NewFormat, const IRGB *spal, const IRGB *dpal)
{
	Bitmap *newbmp;
	IBITMAP *newbitmap;
	if (bitmap == NULL) return NULL;
	newbitmap = ibitmap_convfmt((int)NewFormat, bitmap, spal, dpal);
	if (newbitmap == NULL) return NULL;
	newbmp = new Bitmap();
	if (newbmp == NULL) {
		ibitmap_release(newbitmap);
		return NULL;
	}
	newbmp->Assign(newbitmap);
	newbmp->ownbits = true;
	return newbmp;
}

Bitmap *Bitmap::Chop(const IRECT *bound)
{
	Bitmap *newbmp;
	IBITMAP *newbitmap;
	if (bitmap == NULL) return NULL;
	if (bound == NULL) return NULL;
	newbitmap = ibitmap_chop(bitmap, bound->left, bound->top, 
		bound->right - bound->left, bound->bottom - bound->top);
	if (newbitmap == NULL) return NULL;
	newbmp = new Bitmap();
	if (newbmp == NULL) {
		ibitmap_release(newbitmap);
		return NULL;
	}
	newbmp->Assign(newbitmap);
	newbmp->ownbits = true;
	return newbmp;
}

Bitmap *Bitmap::Chop(int left, int top, int right, int bottom)
{
	IRECT rect;
	ipixel_rect_set(&rect, left, top, right, bottom);
	return Chop(&rect);
}

Bitmap *Bitmap::Resample(int NewWidth, int NewHeight, const IRECT *bound)
{
	Bitmap *newbmp;
	IBITMAP *newbitmap;
	if (bitmap == NULL) return NULL;
	newbitmap = ibitmap_resample(bitmap, bound, NewWidth, NewHeight, 0);
	if (newbitmap == NULL) return NULL;
	newbmp = new Bitmap();
	if (newbmp == NULL) {
		ibitmap_release(newbitmap);
		return NULL;
	}
	newbmp->Assign(newbitmap);
	newbmp->ownbits = true;
	return newbmp;
}

void Bitmap::InitColorIndex(iColorIndex *index, const IRGB *pal, int palsize)
{
	ipalette_to_index(index, pal, palsize);
}


int Bitmap::Raster(const Point *pts, const IBITMAP *src, const IRECT *bound, IUINT32 color, int mode)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	return ibitmap_raster_float(bitmap, (const ipixel_point_t*)pts, src, bound, color, mode, &clip);
}

int Bitmap::Raster(const Point *pts, const Bitmap *src, const IRECT *bound, IUINT32 color, int mode)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	if (src->bitmap == NULL)
		return -200;
	return Raster(pts, src->bitmap, bound, color, mode);
}

int Bitmap::Draw2D(	double x, double y, const IBITMAP *src, const IRECT *bound, 
					double OffsetX, double OffsetY, double ScaleX, double ScaleY, 
					double Angle, IUINT32 color)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	return ibitmap_raster_draw(bitmap, x, y, src, bound, OffsetX, OffsetY, ScaleX, ScaleY, Angle, color, &clip);
}


int Bitmap::Draw2D( double x, double y, const Bitmap *src, const IRECT *bound, 
					double OffsetX, double OffsetY, double ScaleX, double ScaleY, 
					double Angle, IUINT32 color)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	if (src->bitmap == NULL)
		return -200;
	return Draw2D(x, y, src->bitmap, bound, OffsetX, OffsetY, ScaleX, ScaleY, Angle, color);
}

int Bitmap::Draw3D( double x, double y, double z, const IBITMAP *src, const IRECT *bound,
					double OffsetX, double OffsetY, double ScaleX, double ScaleY,
					double AngleX, double AngleY, double AngleZ, IUINT32 color)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	return ibitmap_raster_draw_3d(bitmap, x, y, z, src, bound, OffsetX, OffsetY, ScaleX, ScaleY,
		AngleX, AngleY, AngleZ, color, &clip);
}

int Bitmap::Draw3D( double x, double y, double z, const Bitmap *src, const IRECT *bound,
					double OffsetX, double OffsetY, double ScaleX, double ScaleY,
					double AngleX, double AngleY, double AngleZ, IUINT32 color)
{
	if (bitmap == NULL || src == NULL)
		return -100;
	if (src->bitmap == NULL)
		return -200;
	return ibitmap_raster_draw_3d(bitmap, x, y, z, src->bitmap, bound, OffsetX, OffsetY, ScaleX, ScaleY,
		AngleX, AngleY, AngleZ, color, &clip);
}


//---------------------------------------------------------------------
// 色彩变幻
//---------------------------------------------------------------------

void Bitmap::ColorTransform(const ColorMatrix *t, const IRECT *bound)
{
	float matrix[20];
	if (sizeof(ColorMatrix) == sizeof(float) * 20) {
		memcpy(matrix, t, sizeof(float) * 20);
	}	else {
		memcpy(matrix +  0, t->m[0], sizeof(float) * 5);
		memcpy(matrix +  5, t->m[1], sizeof(float) * 5);
		memcpy(matrix + 10, t->m[2], sizeof(float) * 5);
		memcpy(matrix + 15, t->m[3], sizeof(float) * 5);
	}
	ibitmap_color_transform(bitmap, bound, matrix);
}

void Bitmap::ColorAdd(IUINT32 color, const IRECT *bound)
{
	ibitmap_color_add(bitmap, bound, color);
}

void Bitmap::ColorSub(IUINT32 color, const IRECT *bound)
{
	ibitmap_color_sub(bitmap, bound, color);
}

void Bitmap::ColorMul(IUINT32 color, const IRECT *bound)
{
	ibitmap_color_mul(bitmap, bound, color);
}

int Bitmap::ColorUpdate(UpdateProc updater, bool readonly, void *user, const IRECT *bound)
{
	return ibitmap_update(bitmap, bound, (iBitmapUpdate)updater, readonly? 1 : 0, user);
}

void Bitmap::Blur(int rx, int ry, const IRECT *bound)
{
	ibitmap_stackblur(bitmap, rx, ry, bound);
}

int Bitmap::UpdateGray(int x, int y, int w, IUINT32 *card, void *user)
{
	IUINT32 r, g, b, a;
	for (; w > 0; card++, w--) {
		_ipixel_load_card(card, r, g, b, a);
		r = IRGBA_TO_G8(r, g, b, a);
		card[0] = IRGBA_TO_A8R8G8B8(r, r, r, a);
	}
	return 0;
}

void Bitmap::Gray(const IRECT *bound)
{
	ColorUpdate(UpdateGray, false, NULL, bound);
}


//---------------------------------------------------------------------
// 图像特效
//---------------------------------------------------------------------
Bitmap* Bitmap::DropShadow(int rx, int ry)
{
	IBITMAP *bmp;
	bmp = ibitmap_drop_shadow(bitmap, rx, ry);
	return new Bitmap(bmp, true);
}

Bitmap* Bitmap::RoundCorner(int radius, int style)
{
	IBITMAP *bmp;
	bmp = ibitmap_round_rect(bitmap, radius, style);
	return new Bitmap(bmp, true);
}

Bitmap* Bitmap::GlossyMake(int radius, int border, int light, int shadow, int shadow_pos)
{
	IBITMAP *bmp;
	bmp = ibitmap_glossy_make(bitmap, radius, border, light, shadow, shadow_pos);
	return new Bitmap(bmp, true);
}

Bitmap* Bitmap::MiiMake(int lightmode, bool roundcorner, bool border, bool shadow)
{
	int w = GetW();
	int h = GetH();
	int d = (w + h) / 2;
	int _radius = roundcorner? d / 8 : 0;
	int _border = border? d / 16 : 0;
	int _shadow = shadow? 4 : 0;
	return GlossyMake(_radius, _border, lightmode, _shadow, 0);
}

void Bitmap::AdjustHSV(float Hue, float Saturation, float Value)
{
	ibitmap_adjust_hsv(bitmap, Hue, Saturation, Value, NULL);
}

void Bitmap::AdjustHSL(float Hue, float Saturation, float Lightness)
{
	ibitmap_adjust_hsl(bitmap, Hue, Saturation, Lightness, NULL);
}

Bitmap* Bitmap::AndroidPatch9(int NewWidth, int NewHeight, int *errcode)
{
	IBITMAP *patch = ibitmap_patch_nine(bitmap, NewWidth, NewHeight, errcode);
	if (patch == NULL) return NULL;
	return new Bitmap(patch, true);
}

int Bitmap::AndroidClient(IRECT *client)
{
	return ibitmap_patch_client(bitmap, client);
}


//=====================================================================
// 色彩梯度
//=====================================================================
void Gradient::initialize()
{
	cvector_init(&float_stops);
	cvector_init(&fixed_stops);
	fstops = NULL;
	stops = NULL;
	nstops = 0;
	dirty = false;
	gradient.stops = NULL;
	gradient.nstops = 0;
	gradient.overflow = IBOM_REPEAT;
	gradient.transparent = 0;
}

void Gradient::destroy()
{
	cvector_destroy(&float_stops);
	cvector_destroy(&fixed_stops);
	fstops = NULL;
	stops = NULL;
	nstops = 0;
}

Gradient::~Gradient()
{
	destroy();
}

Gradient::Gradient()
{
	initialize();
}

Gradient::Gradient(const GradientStop *stops, int nstops)
{
	initialize();
	if (stops && nstops > 0) {
		if (resize_stops(nstops)) {
			throw new BitmapError("not enough memory"); 
		}
		memcpy(fstops, stops, nstops * sizeof(GradientStop));
		dirty = true;
	}
}

int Gradient::resize_stops(int len, bool force)
{
	size_t s1 = sizeof(GradientStop) * len;
	size_t s2 = sizeof(ipixel_gradient_stop_t) * len;

	assert(len >= 0);
	
	if (float_stops.size < s1 || force) {
		if (cvector_resize(&float_stops, s1))
			return -1;
	}
	if (fixed_stops.size < s2 || force) {
		if (cvector_resize(&fixed_stops, s2))
			return -2;
	}

	fstops = (GradientStop*)float_stops.data;
	stops = (ipixel_gradient_stop_t*)fixed_stops.data;
	nstops = len;

	gradient.stops = stops;
	gradient.nstops = nstops;

	return 0;
}

void Gradient::Init(const GradientStop *stops, int nstops)
{
	if (resize_stops(nstops)) {
		throw new BitmapError("not enough memory"); 
	}
	memcpy(fstops, stops, nstops * sizeof(GradientStop));
	dirty = true;
}

void Gradient::AddStop(double x, IUINT32 color)
{
	if (resize_stops(nstops + 1)) {
		throw new BitmapError("not enough memory"); 
	}
	fstops[nstops - 1].x = x;
	fstops[nstops - 1].color = color;
	dirty = true;
}

void Gradient::AddStop(const GradientStop *stop)
{
	AddStop(stop->x, stop->color);
}


void Gradient::Reset()
{
	resize_stops(0);
	update();
}

int Gradient::update()
{
	double xmin, xmax, range;
	ipixel_gradient_stop_t t;
	int i, j;

	if (nstops < 2) 
		return 0;

	xmin = xmax = fstops[0].x;

	for (i = 0; i < nstops; i++) {
		double x = fstops[i].x;
		if (x < xmin) xmin = x;
		if (x > xmax) xmax = x;
	}

	range = xmax - xmin;

	if (range == 0) 
		return 0;

	double factor = 65535.0 / range;

	for (i = 0; i < nstops; i++) {
		double x = fstops[i].x;
		double y = (x - xmin) * factor;
		cfixed pos = (int)y;
		if (x == xmin) pos = 0;
		else if (x == xmax) pos = 0xffff;
		else if (pos < 0) pos = 0;
		else if (pos > 0xffff) pos = 0xffff;
		stops[i].x = pos;
		stops[i].color = fstops[i].color;
	}

	for (i = 0; i < nstops - 1; i++) {
		for (j = i + 1; j < nstops; j++) {
			if (stops[i].x > stops[j].x) {
				t = stops[i];
				stops[i] = stops[j];
				stops[j] = t;
			}
		}
	}

	dirty = false;

	return 0;
}

ipixel_gradient_t* Gradient::GetGradient()
{
	if (dirty) update();
	return &gradient;
}

ipixel_gradient_stop_t* Gradient::GetStops()
{
	if (nstops == 0) return NULL;
	if (dirty) update();
	return stops;
}

int Gradient::GetLength()
{
	if (dirty) update();
	return nstops;
}

void Gradient::SetOverflow(enum OverflowMode mode)
{

}



//=====================================================================
// 像素源
//=====================================================================
Source::Source()
{
}

Source::Source(IUINT32 color)
{
	InitSolid(color);
}

Source::Source(IBITMAP *bitmap)
{
	InitBitmap(bitmap);
}

Source::Source(Bitmap *bitmap)
{
	InitBitmap(bitmap);
}

void Source::InitSolid(IUINT32 color)
{
	ipixel_source_init_solid(&source, color);
}

void Source::InitBitmap(IBITMAP *bitmap)
{
	ipixel_source_init_bitmap(&source, bitmap);
}

void Source::InitBitmap(Bitmap *bitmap)
{
	ipixel_source_init_bitmap(&source, bitmap->GetBitmap());
}

void Source::InitGradientLinear(ipixel_gradient_t *g, const Point *p1, const Point *p2)
{
	InitGradientLinear(g, p1->x, p1->y, p2->x, p2->y);
}

void Source::InitGradientLinear(ipixel_gradient_t *g, double x1, double y1, double x2, double y2)
{
	ipixel_point_fixed_t p1;
	ipixel_point_fixed_t p2;
	p1.x = cfixed_from_double(x1);
	p1.y = cfixed_from_double(y1);
	p2.x = cfixed_from_double(x2);
	p2.y = cfixed_from_double(y2);

	ipixel_source_init_gradient_linear(
		&source, &p1, &p2, g->stops, g->nstops);
}

void Source::InitGradientLinear(Gradient *g, const Point *p1, const Point *p2)
{
	InitGradientLinear(g->GetGradient(), p1, p2);
}

void Source::InitGradientLinear(Gradient *g, double x1, double y1, double x2, double y2)
{
	InitGradientLinear(g->GetGradient(), x1, y1, x2, y2);
}


void Source::InitGradientRadial(ipixel_gradient_t *g, const Point *inner, const Point *outer, 
	double inner_radius, double outer_radius)
{
	InitGradientRadial(g, inner->x, inner->y, outer->x, outer->y, inner_radius, 
		outer_radius);
}

void Source::InitGradientRadial(ipixel_gradient_t *g, double inner_x, double inner_y, 
	double outer_x, double outer_y, double inner_r, double outer_r)
{
	ipixel_point_fixed_t p1;
	ipixel_point_fixed_t p2;
	p1.x = cfixed_from_double(inner_x);
	p1.y = cfixed_from_double(inner_y);
	p2.x = cfixed_from_double(outer_x);
	p2.y = cfixed_from_double(outer_y);

	ipixel_source_init_gradient_radial(
		&source, &p1, &p2, 
		cfixed_from_double(inner_r), 
		cfixed_from_double(outer_r),
		g->stops,
		g->nstops);
}

void Source::InitGradientRadial(Gradient *g, const Point *inner, const Point *outer, 
		double inner_radius, double outer_radius)
{
	InitGradientRadial(g->GetGradient(), inner, outer, inner_radius, outer_radius);
}

void Source::InitGradientRadial(Gradient *g, double inner_x, double inner_y, 
		double outer_x, double outer_y, double inner_r, double outer_r)
{
	InitGradientRadial(g->GetGradient(), inner_x, inner_y, outer_x, outer_y, 
		inner_r, outer_r);
}

void Source::InitGradientConical(ipixel_gradient_t *g, const Point *center, double angle)
{
	InitGradientConical(g, center->x, center->y, angle);
}

void Source::InitGradientConical(ipixel_gradient_t *g, double cx, double cy, double angle)
{
	ipixel_point_fixed_t p;
	p.x = cfixed_from_double(cx);
	p.y = cfixed_from_double(cy);

	ipixel_source_init_gradient_conical(&source, &p, cfixed_from_double(angle),
		g->stops, g->nstops);
}

void Source::InitGradientConical(Gradient *g, const Point *center, double angle)
{
	InitGradientConical(g->GetGradient(), center, angle);
}

void Source::InitGradientConical(Gradient *g, double cx, double cy, double angle)
{
	InitGradientConical(g->GetGradient(), cx, cy, angle);
}

void Source::SetTransform(const ipixel_transform_t *t)
{
	ipixel_source_set_transform(&source, t);
}

void Source::SetOverflow(enum OverflowMode mode, IUINT32 transparent)
{
	ipixel_source_set_overflow(&source, (enum IBOM)((int)mode), transparent);
}

void Source::SetFilter(enum Filter filter)
{
	ipixel_source_set_filter(&source, (enum IPIXELFILTER)((int)filter));
}

void Source::SetBound(const IRECT *bound)
{
	ipixel_source_set_bound(&source, bound);
}

int Source::FetchScanline(int x, int y, int width, IUINT32 *card, const IUINT8 *mask)
{
	return ipixel_source_fetch(&source, x, y, width, card, mask);
}


//=====================================================================
// 作图对象
//=====================================================================
void Paint::initialize()
{
	paint = NULL;
}

void Paint::destroy()
{
	if (paint != NULL) {
		ipaint_destroy(paint);
		paint = NULL;
	}
}

void Paint::Create(IBITMAP *bitmap)
{
	Release();
	paint = ipaint_create(bitmap);
}

void Paint::Create(Bitmap *bitmap)
{
	Create(bitmap->GetBitmap());
}

void Paint::Release()
{
	destroy();
}

void Paint::SetBitmap(IBITMAP *bitmap)
{
	if (paint == NULL) {
		Create(bitmap);
	}	else {
		ipaint_set_image(paint, bitmap);
	}
}

void Paint::SetBitmap(Bitmap *bitmap)
{
	SetBitmap(bitmap->GetBitmap());
}

Paint::~Paint()
{
	Release();
}

Paint::Paint(IBITMAP *bitmap)
{
	initialize();
	Create(bitmap);
}

Paint::Paint(Bitmap *bitmap)
{
	initialize();
	Create(bitmap);
}

void Paint::SetSource(ipixel_source_t *source)
{
	assert(paint);
	ipaint_source_set(paint, source);
}

void Paint::SetSource(Source *source)
{
	assert(paint);
	ipaint_source_set(paint, &source->source);
}

ipaint_t* Paint::GetPaint()
{
	return paint;
}

const ipaint_t* Paint::GetPaint() const
{
	return paint;
}

int Paint::GetW() const {
	return (int)paint->image->w;
}

int Paint::GetH() const {
	return (int)paint->image->h;
}

void Paint::SetColor(IUINT32 color)
{
	ipaint_set_color(paint, color);
}

void Paint::SetClip(const IRECT *clip)
{
	ipaint_set_clip(paint, clip);
}

void Paint::SetTextColor(IUINT32 color)
{
	ipaint_text_color(paint, color);
}

void Paint::SetTextBackground(IUINT32 color)
{
	ipaint_text_background(paint, color);
}

void Paint::SetAntiAlising(int level)
{
	ipaint_anti_aliasing(paint, level);
}

void Paint::SetLineWidth(double width)
{
	ipaint_line_width(paint, width);
}

void Paint::SetAdditive(bool additive)
{
	paint->additive = additive? 1 : 0;
}

void Paint::Fill(const IRECT *bound, IUINT32 color)
{
	ipaint_fill(paint, bound, color);
}

void Paint::Fill(int left, int top, int right, int bottom, IUINT32 color)
{
	IRECT rect;
	ipixel_rect_set(&rect, left, top, right, bottom);
	ipaint_fill(paint, &rect, color);
}

int Paint::DrawPolygon(const Point *pts, int npts)
{
	return ipaint_draw_polygon(paint, (const ipixel_point_t*)pts, npts);
}

int Paint::DrawLine(double x1, double y1, double x2, double y2)
{
	return ipaint_draw_line(paint, x1, y1, x2, y2);
}

int Paint::DrawCircle(double x, double y, double radius)
{
	return ipaint_draw_circle(paint, x, y, radius);
}

int Paint::DrawEllipse(double x, double y, double rx, double ry)
{
	return ipaint_draw_ellipse(paint, x, y, rx, ry);
}

int Paint::DrawRectangle(double x1, double y1, double x2, double y2)
{
	Point pts[4];
	pts[0].x = x1;
	pts[0].y = y1;
	pts[1].x = x1;
	pts[1].y = y2;
	pts[2].x = x2;
	pts[2].y = y2;
	pts[3].x = x2;
	pts[3].y = y1;
	return DrawPolygon(pts, 4);
}

void Paint::Draw(int x, int y, const IBITMAP *bmp, const IRECT *bound, IUINT32 color, int flag)
{
	ipaint_draw(paint, x, y, bmp, bound, color, flag);
}

void Paint::Draw(int x, int y, const Bitmap *bmp, const IRECT *bound, IUINT32 color, int flag)
{
	ipaint_draw(paint, x, y, bmp->GetBitmap(), bound, color, flag);
}

void Paint::Raster(const ipixel_point_t *pts, const IBITMAP *bmp, const IRECT *bound, IUINT32 color, int flag)
{
	ipaint_raster(paint, pts, bmp, bound, color, flag);
}

void Paint::Raster(const ipixel_point_t *pts, const Bitmap *bmp, const IRECT *bound, IUINT32 color, int flag)
{
	ipaint_raster(paint, pts, bmp->GetBitmap(), bound, color, flag);
}

void Paint::Draw2D(double x, double y, const IBITMAP *bmp, IRECT *bound,
	double offset_x, double offset_y, double scalex, double scaley, 
	double angle, IUINT32 color)
{
	ipaint_raster_draw(paint, x, y, bmp, bound, offset_x, offset_y, scalex, scaley, angle, color);
}

void Paint::Draw2D(double x, double y, const Bitmap *bmp, IRECT *bound,
	double offset_x, double offset_y, double scalex, double scaley, 
	double angle, IUINT32 color)
{
	ipaint_raster_draw(paint, x, y, bmp->GetBitmap(), bound, offset_x, offset_y, scalex, scaley, angle, color);
}

void Paint::Draw3D(double x, double y, double z, const IBITMAP *bmp, IRECT *bound,
	double offset_x, double offset_y, double scalex, double scaley,
	double anglex, double angley, double anglez, IUINT32 color)
{
	ipaint_raster_draw_3d(paint, x, y, z, bmp, bound, offset_x, offset_y, scalex, scaley, 
		anglex, angley, anglez, color);
}

void Paint::Draw3D(double x, double y, double z, const Bitmap *bmp, IRECT *bound,
	double offset_x, double offset_y, double scalex, double scaley,
	double anglex, double angley, double anglez, IUINT32 color)
{
	ipaint_raster_draw_3d(paint, x, y, z, bmp->GetBitmap(), bound, offset_x, offset_y, scalex, scaley, 
		anglex, angley, anglez, color);
}

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

void Paint::Printf(int x, int y, const char *fmt, ...)
{
	char buffer[4096];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	ibitmap_draw_text(paint->image, x, y, buffer, &paint->clip,
		paint->text_color, paint->text_backgrnd, paint->additive);
}

void Paint::ShadowPrintf(int x, int y, const char *fmt, ...)
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


NAMESPACE_END(Pixel)


//! flag: -O3, -Wall
//! int: obj
//! out: libpixel
//! mode: lib
//! src: PixelBitmap.cpp
//! src: ikitwin.c, mswindx.c, ibmfont.c, ibmsse2.c, ibmwink.c, npixel.c
//! src: ibitmap.c, ibmbits.c, iblit386.c, ibmcols.c, ipicture.c, ibmdata.c


