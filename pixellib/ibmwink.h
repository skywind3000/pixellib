//=====================================================================
//
// ibmwink.h - animation
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __IBMWINK_H__
#define __IBMWINK_H__

#include "ibitmap.h"
#include "ibmbits.h"
#include "ibmcols.h"
#include "ibmdata.h"


#ifdef __cplusplus
extern "C" {
#endif


//---------------------------------------------------------------------
// 滤波器与通道操作
//---------------------------------------------------------------------

// 使用滤波器
// filter是一个长为9的数组，该函数将以每个像素为中心3x3的9个点中的各个
// 分量乘以filter中对应的值后相加，再除以256，作为该点的颜色保存
int ibitmap_filter(IBITMAP *dst, const short *filter);


// 取得channel
// dst必须是8位的位图，filter的0,1,2,3代表取得src中的r,g,b,a分量
int ibitmap_channel_get(IBITMAP *dst, int dx, int dy, const IBITMAP *src,
	int sx, int sy, int sw, int sh, int channel);

// 设置channel
// src必须是8位的位图，filter的0,1,2,3代表设置dst中的r,g,b,a分量
int ibitmap_channel_set(IBITMAP *dst, int dx, int dy, const IBITMAP *src,
	int sx, int sy, int sw, int sh, int channel);


// 图像更新函数：
// 返回：返回0代表成功，1代表不写回，小于零为错误
typedef int (*iBitmapUpdate)(int x, int y, int w, IUINT32 *card, void *user);


// 图像更新：按照从上到下顺序，每条扫描线调用一次updater
int ibitmap_update(IBITMAP *dst, const IRECT *bound, 
	iBitmapUpdate updater, int readonly, void *user);


//---------------------------------------------------------------------
// 基础特效
//---------------------------------------------------------------------

// 图像模糊
void ibitmap_stackblur(IBITMAP *src, int rx, int ry, const IRECT *bound);

// 无AA绘制直线
int ibitmap_put_line(IBITMAP *dst, int x1, int y1, int x2, int y2,
	IUINT32 color, int additive, const IRECT *clip);

// 无AA绘制圆形
void ibitmap_put_circle(IBITMAP *dst, int x0, int y0, int r, int fill,
	const IRECT *clip, IUINT32 color, int additive);

// 色彩变幻：输入 5 x 5矩阵
void ibitmap_color_transform(IBITMAP *dst, const IRECT *b, const float *t);

// 色彩变幻：加法
void ibitmap_color_add(IBITMAP *dst, const IRECT *b, IUINT32 color);

// 色彩变幻：减法
void ibitmap_color_sub(IBITMAP *dst, const IRECT *b, IUINT32 color);

// 色彩变幻：乘法
void ibitmap_color_mul(IBITMAP *dst, const IRECT *b, IUINT32 color);


// 生成阴影
IBITMAP *ibitmap_drop_shadow(const IBITMAP *src, int rx, int ry);

// 生成圆角
IBITMAP *ibitmap_round_rect(const IBITMAP *src, int radius, int style);



//---------------------------------------------------------------------
// 缓存管理
//---------------------------------------------------------------------
struct CVECTOR
{
	unsigned char *data;
	size_t size;
	size_t block;
};

// 初始化缓存
void cvector_init(struct CVECTOR *vector);

// 销毁缓存
void cvector_destroy(struct CVECTOR *vector);

// 改变缓存大小
int cvector_resize(struct CVECTOR *vector, size_t size);

// 添加数据到末尾
int cvector_push(struct CVECTOR *vector, const void *data, size_t size);


//---------------------------------------------------------------------
// 原始作图
//---------------------------------------------------------------------

// 批量填充梯形
int ipixel_render_traps(IBITMAP *dst, const ipixel_trapezoid_t *traps, 
	int ntraps, IBITMAP *alpha, const ipixel_source_t *src, int isadditive,
	const IRECT *clip, struct CVECTOR *scratch);

// 绘制多边形
int ipixel_render_polygon(IBITMAP *dst, const ipixel_point_fixed_t *pts,
	int npts, IBITMAP *alpha, const ipixel_source_t *src, int isadditive,
	const IRECT *clip, struct CVECTOR *scratch);



//---------------------------------------------------------------------
// 作图对象
//---------------------------------------------------------------------
typedef struct CVECTOR cvector_t;
typedef struct IPAINT  ipaint_t;

// 绘制的结构体
struct IPAINT
{
	IBITMAP *image;
	IBITMAP *alpha;
	IRECT clip;
	IUINT32 color;
	IUINT32 text_color;
	IUINT32 text_backgrnd;
	int additive;
	double line_width;
	ipixel_point_t *pts;
	int npts;
	ipixel_source_t *current;
	ipixel_source_t source;
	cvector_t scratch;
	cvector_t points;
	cvector_t pointf;
	cvector_t gradient;
};


//---------------------------------------------------------------------
// 作图接口
//---------------------------------------------------------------------

// 创建图形对象
ipaint_t *ipaint_create(IBITMAP *image);

// 销毁图形对象
void ipaint_destroy(ipaint_t *paint);

// 设置目标位图
int ipaint_set_image(ipaint_t *paint, IBITMAP *image);


// 色彩源：设置当前色彩源
void ipaint_source_set(ipaint_t *paint, ipixel_source_t *source);

// 设置当前颜色
void ipaint_set_color(ipaint_t *paint, IUINT32 color);

// 设置裁剪窗口
void ipaint_set_clip(ipaint_t *paint, const IRECT *clip);

// 设置字体颜色
void ipaint_text_color(ipaint_t *paint, IUINT32 color);

// 设置字体背景
void ipaint_text_background(ipaint_t *paint, IUINT32 color);

// 设置抗锯齿
void ipaint_anti_aliasing(ipaint_t *paint, int level);


// 绘制多边形
int ipaint_draw_polygon(ipaint_t *paint, const ipixel_point_t *pts, int n);

int ipaint_draw_line(ipaint_t *paint, double x1, double y1, double x2, 
	double y2);

void ipaint_line_width(ipaint_t *paint, double width);

int ipaint_draw_circle(ipaint_t *paint, double x, double y, double r);

int ipaint_draw_ellipse(ipaint_t *paint, double x, double y, double rx,
	double ry);

void ipaint_fill(ipaint_t *paint, const IRECT *rect, IUINT32 color);

void ipaint_cprintf(ipaint_t *paint, int x, int y, const char *fmt, ...);

void ipaint_sprintf(ipaint_t *paint, int x, int y, const char *fmt, ...);

int ipaint_raster(ipaint_t *paint, const ipixel_point_t *pts, 
	const IBITMAP *image, const IRECT *rect, IUINT32 color, int flag);

int ipaint_raster_draw(ipaint_t *paint, double x, double y, 
	const IBITMAP *src, const IRECT *rect, double off_x, double off_y, 
	double scale_x, double scale_y, double angle, IUINT32 color);

int ipaint_raster_draw_3d(ipaint_t *paint, double x, double y, double z, 
	const IBITMAP *src, const IRECT *rect, double off_x, double off_y,
	double scale_x, double scale_y, double angle_x, double angle_y,
	double angle_z, IUINT32 color);

int ipaint_draw(ipaint_t *paint, int x, int y, const IBITMAP *src, 
	const IRECT *bound, IUINT32 color, int flags);



//---------------------------------------------------------------------
// 色彩空间
//---------------------------------------------------------------------
typedef struct IYCRCB
{
	unsigned char Y, Cr, Cb;
	unsigned char reserved;
}	IYCrCb;

typedef struct IHSV
{
	float H, S, V;
	unsigned char reserved;
}	IHSV;


void iconv_RGB_to_YCrCb(const IRGB *input, long size, IYCrCb *output);

void iconv_YCrCb_to_RGB(const IYCrCb *input, long size, IRGB *output);

void iconv_RGB_to_HSV(const IRGB *input, long size, IHSV *output);

void iconv_HSV_to_RGB(const IHSV *input, long size, IRGB *output);




//---------------------------------------------------------------------
// 特效若干
//---------------------------------------------------------------------

// 生成特效
IBITMAP *ibitmap_glossy_make(IBITMAP *bmp, int radius, int border, int light,
	int shadow, int shadow_pos);

// 调整色调
void ibitmap_adjust_hsv(IBITMAP *bmp, float hue, float saturation, 
	float value, const IRECT *bound);

// 调整色调
void ibitmap_adjust_hsl(IBITMAP *bmp, float hue, float saturation, 
	float lightness, const IRECT *bound);

// Android: patch 9 生成
IBITMAP *ibitmap_patch_nine(const IBITMAP *src, int nw, int nh, int *code);

// Android: 取得客户区
int ibitmap_patch_client(const IBITMAP *src, IRECT *client);


#ifdef __cplusplus
}
#endif

#endif


