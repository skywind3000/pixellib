//=====================================================================
//
// mswindx.h - Microsoft Window DirectDraw7 Easy Interface
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#ifndef __MSWINDX_H__
#define __MSWINDX_H__

#if (defined(_WIN32) || defined(WIN32))

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


//---------------------------------------------------------------------
// Structure Reference
//---------------------------------------------------------------------
struct CSURFACE;
typedef struct CSURFACE CSURFACE;


#ifdef __cplusplus
extern "C" {
#endif
//---------------------------------------------------------------------
// Interfaces
//---------------------------------------------------------------------

// 初始化 DirectDraw
int DDrawInit(void);

// 销毁
void DDrawDestroy(void);

// 像素格式
#define PIXFMT_DEFAULT		0
#define PIXFMT_8			1
#define PIXFMT_RGB15		2
#define PIXFMT_RGB16		3
#define PIXFMT_RGB24		4
#define PIXFMT_RGB32		5
#define PIXFMT_ARGB32		6

// 模式
#define DDSM_SYSTEMMEM		0
#define DDSM_VIDEOMEM		1

// 创建表面
// fmt 为像素格式，即 DDPF_xx
// mode为 DDSM_SYSTEMMEM或者 DDSM_VIDEOMEM
CSURFACE *DDrawSurfaceCreate(int w, int h, int fmt, int mode, HRESULT *hr);

// 删除表面
void DDrawSurfaceRelease(CSURFACE *surface);

// 创建 DirectDraw屏幕
// 如果 bpp为0的话则创建窗口模式，w,h失效，否则全屏幕
CSURFACE *DDrawCreateScreen(HWND hWnd, int w, int h, int bpp, HRESULT *hr);


// 取得表面信息
int DDrawSurfaceInfo(const CSURFACE *s, int *w, int *h, int *bpp, int *fmt);


#define DDBLIT_MASK		1		// 使用关键色
#define DDBLIT_NOWAIT	2		// 异步BLIT
#define DDBLIT_NOCLIP	3		// 不裁剪

// BLIT: 
// (dx, dy, dw, dh)  -  目标表面的区域
// (sx, sy, sw, sh)  -  源表面的区域
// flags的设置在上面那些 DDBLIT_* 开头的宏
// 返回 0表示成功，其他值表示 DirectDrawSurface7::Blt的返回
int DDrawSurfaceBlit(CSURFACE *dst, int dx, int dy, int dw, int dh, 
	CSURFACE *src, int sx, int sy, int sw, int sh, int flags);


#define DDLOCK_NOWAIT		1	// 尝试然后马上返回
#define DDLOCK_OREAD		2	// 只读
#define DDLOCK_OWRITE		4	// 只写


// 锁定表面：锁定后才能写数据，flags值如上面三个的组合
// 返回    0 成功，
// 返回    1 表示还在绘制，无法锁定(没有NOWAIT时)
// 返回   -1 表示已经锁定过了，无法再次锁定
// 返回   -2 锁定时候碰到的其他错误 
int DDrawSurfaceLock(CSURFACE *dst, void **bits, long *pitch, int flags);


// 解锁表面：解锁后才能调用BLIT
// 返回    0 成功，
// 返回   -1 表示已经锁定过了，无法再次锁定
// 返回   -2 锁定时候碰到的其他错误 
int DDrawSurfaceUnlock(CSURFACE *dst);


// 设置关键色
int DDrawSurfaceSetMask(CSURFACE *dst, unsigned long mask);


// 垂直同步: 0对应DDWAITVB_BLOCKBEGIN, 其他为DDWAITVB_BLOCKEND 
int DDrawWaitForVerticalBlank(int mode);


#ifdef __cplusplus
}
#endif


#endif

#endif



