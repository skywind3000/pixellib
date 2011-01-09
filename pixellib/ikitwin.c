//=====================================================================
//
// ikitwin.c - windib & x11 basic drawing support
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#include "ikitwin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(__unix)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlibint.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef NO_SHARED_MEMORY
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#elif defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include <mmsystem.h>
#include <wingdi.h>
#include <time.h>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#endif

#else
#error This file can only be compiled under windows or unix
#endif


//=====================================================================
// 通用配置区
//=====================================================================
typedef struct
{
	int w;
	int h;
	int bpp;
}	IModeInfo;


int (*ikitwin_private_active)(int mode, int type) = 0;
int (*ikitwin_private_mouse_motion)(int bs, int rl, int x, int y) = 0;
int (*ikitwin_private_mouse_button)(int st, int bt, int x, int y) = 0;
int (*ikitwin_private_keyboard)(int st, int keycode) = 0;
int (*ikitwin_private_quit)(void) = 0;

void *ikitwin_private = NULL;
int ikitwin_fullscreen = 0;
int ikitwin_screen_w = 0;
int ikitwin_screen_h = 0;
int ikitwin_screen_bpp = 0;
int ikitwin_winx = 0;
int ikitwin_winy = 0;
int ikitwin_winw = 0;
int ikitwin_winh = 0;
int ikitwin_resizing = 0;
int ikitwin_mouserel = 0;
char ikitwin_windowid[64] = "";

int ikitwin_mouse_x = 0;
int ikitwin_mouse_y = 0;
int ikitwin_mouse_btn = 0;
int ikitwin_mouse_in = 0;
int ikitwin_keymap[512];
int ikitwin_closing = 0;
int ikitwin_refresh = 1;


#ifdef __unix
//=====================================================================
// X11 区
//=====================================================================
typedef struct 
{
	Display *display_x11;			// 用于窗口管理的消息队列
	Display *display_gfx;			// 用于图形绘制的消息队列
	Window window;					// 当前窗口
	Visual *visual;					// 当前视图
	int screen;						// 默认屏幕
	pthread_mutex_t lock;
	Colormap display_colormap;		// 默认颜色表
	Colormap private_colormap;		// 创建的颜色表
	Colormap xcolormap;				// 当前窗口颜色表
	void *blank_cursor;				// 不可见的指针
	int use_dga;
#ifndef NO_SHARED_MEMORY
	int use_mitshm;					// 是否使用共享内存模式
	XShmSegmentInfo shminfo;		// 共享内存信息
#endif
	XImage *image;					// 绘图用XImage
	GC gc;							// 绘图上下文
	char *windowid;
	int *xpixels;					// 分配过的颜色
	Atom DELWIN;					// 删除窗口
	IModeInfo *modelist;			// 模式列表
	unsigned char *pixel;			// 当前颜色矩阵
	int screen_w;					// 原始屏幕的宽度
	int screen_h;					// 原始屏幕的高度
	int depth;						// 屏幕的色彩深度
	int w;							// 当前宽
	int h;							// 当前高
	long pitch;						// 当前pitch
	int async;						// 是否异步更新
	int blit_queued;				// 异步blit的标志
	int autorefresh;				// 是否自动刷新
	int swap_pixels;				// 是否交互像素字节
	unsigned int rmask;				// 红色掩码
	unsigned int gmask;				// 绿色掩码
	unsigned int bmask;				// 蓝色掩码
	unsigned int amask;				// 透明掩码
	void *parent;
}	IVideoPrivateX11;


#ifndef NO_SHARED_MEMORY
static int ishm_error = 0;
static int (*ishm_error_handler_saved)(Display *, XErrorEvent *) = NULL;
static int ishm_error_handler(Display *d, XErrorEvent *e)
{
	if (e->error_code == BadAccess) {
		ishm_error = 1;
		return 0;
	}	else {
		return ishm_error_handler_saved(d, e);
	}
}

static void x11_try_mitshm(IVideoPrivateX11 *self)
{
	XShmSegmentInfo *shminfo = &(self->shminfo);
	if (self->use_mitshm == 0) return;
	shminfo->shmid = shmget(IPC_PRIVATE, self->h * self->pitch,
		IPC_CREAT | 0777);
	if (shminfo->shmid >= 0) {
		shminfo->shmaddr = (char*)shmat(shminfo->shmid, 0, 0);
		shminfo->readOnly = 0;
		if (shminfo->shmaddr != (char*)-1) {
			ishm_error = 0;
			ishm_error_handler_saved = XSetErrorHandler(ishm_error_handler);
			XShmAttach(self->display_x11, shminfo);
			XSync(self->display_x11, 1);
			XSetErrorHandler(ishm_error_handler_saved);
			if (ishm_error) 
				shmdt(shminfo->shmaddr);
		}	else {
			ishm_error = 1;
		}
		shmctl(shminfo->shmid, IPC_RMID, NULL);
	}	else {
		ishm_error = 1;
	}
	if (ishm_error != 0) 
		self->use_mitshm = 0;
	if (self->use_mitshm) {
		self->pixel = (unsigned char*)shminfo->shmaddr;
		memset(self->pixel, 0, self->h * self->pitch);
	}
}	

#endif

static int x11_create_image(IVideoPrivateX11 *self)
{
	static long endian = 0x11223344;
#ifndef NO_SHARED_MEMORY
	x11_try_mitshm(self);
	if (self->use_mitshm) {
		self->image = XShmCreateImage(self->display_x11, self->visual, 
			DefaultDepth(self->display_x11, self->screen), 
			ZPixmap, self->shminfo.shmaddr, &(self->shminfo), 
			self->w, self->h);
		if (self->image == NULL) {
			XShmDetach(self->display_x11, &self->shminfo);
			XSync(self->display_x11, 0);
			shmdt(self->shminfo.shmaddr);
			self->pixel = NULL;
			self->image = NULL;
			return -1;
		}
		//printf("created a mitshm image (%d, %d)\n", self->w, self->h);
	}
	if (!self->use_mitshm) 
#endif
	{
		int bpp;
		self->pixel = (unsigned char*)Xmalloc(self->h * self->pitch);
		if (self->pixel == NULL) {
			self->image = NULL;
			return -2;
		}
		bpp = (self->depth + 7) / 8;
		self->image = XCreateImage(self->display_x11, self->visual, 
			DefaultDepth(self->display_x11, self->screen), 
			ZPixmap, 0, (char*)self->pixel,
			self->w, self->h, 32, self->pitch);
		if (self->image == NULL) {
			free(self->pixel);
			self->pixel = NULL;
			return -3;
		}
		//printf("create a normal image (%d, %d)\n", self->w, self->h);
	}

	if (((unsigned char*)&endian)[0] == 0x44) {
		self->image->byte_order = LSBFirst;
	}	else {
		self->image->byte_order = MSBFirst;
	}

	return 0;
}


static void x11_destroy_image(IVideoPrivateX11 *self)
{
	if (self->image) {
		XDestroyImage(self->image);
#ifndef NO_SHARED_MEMORY
		if (self->use_mitshm) {
			XShmDetach(self->display_x11, &self->shminfo);
			XSync(self->display_x11, 0);
			shmdt(self->shminfo.shmaddr);
		}	else 
#endif
		{
			// nothing todo with normal image but keep the brace
			self->pixel = NULL;
		}
		self->pixel = NULL;
		self->image = NULL;
	}
}

static int unix_ncpus(void)
{
	static int num_cpus = 0;
	if (!num_cpus) {
	#ifdef __linux
		char line[1025];
		FILE *fp = fopen("/proc/stat", "r");
		if (fp) {
			while (fgets(line, 1024, fp)) {
				if (memcmp(line, "cpu", 3) == 0 && line[3] != ' ') 
					num_cpus++;
			}
			fclose(fp);
		}
	#elif defined(__sgi)
		num_cpus = sysconf(_SC_NPROC_ONLN);
	#elif defined(_SC_NPROCESSOR_ONLN)
		num_cpus = sysconf(_SC_NPROCESSOR_ONLN);
	#elif defined(_SC_NPROCESSOR_CONF)
		num_cpus = sysconf(_SC_NPROCESSOR_CONF);
	#endif
		if (num_cpus <= 0) {
			char line[1025];
			FILE *fp = fopen("/proc/stat", "r");
			if (fp) {
				while (fgets(line, 1024, fp)) {
					if (memcmp(line, "cpu", 3) == 0 && line[3] != ' ') 
						num_cpus++;
				}
				fclose(fp);
			}
		}
		if (num_cpus <= 0) 
			num_cpus = 1;
	}
	return num_cpus;
}


static int x11_resize_image(IVideoPrivateX11 *self)
{
	int retval = 0;
	x11_destroy_image(self);
	retval = x11_create_image(self);
	if (self->async) {
		if (unix_ncpus() <= 1) 
			self->async = 0;
	}
	return retval;
}

static int x11_lock_image(IVideoPrivateX11 *self)
{
	if (self->blit_queued) {
		XSync(self->display_gfx, 0);
		self->blit_queued = 0;
	}
	return 0;
}

static int x11_unlock_image(IVideoPrivateX11 *self)
{
	return 0;
}

static int x11_update_image(IVideoPrivateX11 *self, const int *rect, int n)
{
	int i;
	for (i = 0; i < n; i++, rect += 4) {
		int x = rect[0];
		int y = rect[1];
		int w = rect[2];
		int h = rect[3];
		if (w == 0 || h == 0) continue;
	#ifndef NO_SHARED_MEMORY
		if (self->use_mitshm) 
		{
			XShmPutImage(self->display_gfx, self->window, self->gc, 
				self->image, x, y, x, y, w, h, 0);
		}	else 
	#endif
		{
			XPutImage(self->display_gfx, self->window, self->gc, 
				self->image, x, y, x, y, w, h);
		}
	}
	if (self->async) {
		XFlush(self->display_gfx);
		self->blit_queued = 1;
	}	else {
		XSync(self->display_gfx, 0);
	}
	return 0;
}

static void x11_refresh(IVideoPrivateX11 *self)
{
	if (self->image == NULL || self->autorefresh == 0) {
		return;
	}
#ifndef NO_SHARED_MEMORY
	if (self->use_mitshm) 
	{
		XShmPutImage(self->display_x11, self->window, self->gc, self->image,
			0, 0, 0, 0, self->w, self->h, 0);
	}	else
#endif
	{
		XPutImage(self->display_x11, self->window, self->gc, self->image,
			0, 0, 0, 0, self->w, self->h);
	}
	XSync(self->display_x11, 0);
}

static int (*ixio_error_handler_saved)(Display *) = NULL;

static int ixio_error_handler(Display *disp)
{
	fprintf(stderr, "x11 io error\n");
	fflush(stderr);
	abort();
	if (ixio_error_handler_saved != ixio_error_handler)
		return ixio_error_handler_saved(disp);
	return 0;
}


static int x11_init(IVideoPrivateX11 *self, const char *windowid)
{
	XGCValues gcv;
	char *display;
	int localx11;

	display = NULL;

	if (strncmp(XDisplayName(display), ":", 1) == 0) {
		localx11 = 1;
	}	else {
		localx11 = 0;
	}

	memset(self, 0, sizeof(IVideoPrivateX11));

	if (windowid == NULL) self->windowid = NULL;
	else {
		self->windowid = (char*)malloc(sizeof(windowid) + 1);
		memcpy(self->windowid, windowid, strlen(windowid) + 1);
	}

	self->display_x11 = XOpenDisplay(display);
	self->display_gfx = XOpenDisplay(display);

	if (self->display_x11 == NULL || self->display_gfx == NULL) {
		if (self->display_x11) XCloseDisplay(self->display_x11);
		if (self->display_gfx) XCloseDisplay(self->display_gfx);
		return -1;
	}

	ixio_error_handler_saved = XSetIOErrorHandler(ixio_error_handler);

	self->DELWIN = XInternAtom(self->display_x11, "WM_DELETE_WINDOW", 0);

	self->screen = (int)DefaultScreen(self->display_x11);
	self->visual = DefaultVisual(self->display_x11, self->screen);

	{
		XPixmapFormatValues *pixfmt;
		int i, num;
		self->depth = DefaultDepth(self->display_x11, self->screen);
		pixfmt = XListPixmapFormats(self->display_x11, &num);
		if (pixfmt == NULL) {
			fprintf(stderr, "cannot detect display modes\n");
			fflush(stderr);
			XCloseDisplay(self->display_x11);
			XCloseDisplay(self->display_gfx);
			return -2;
		}
		for (i = 0; i < num; i++) {
			if (self->depth == pixfmt[i].depth) 
				break;
		}
		if (i < num) {
			self->depth = pixfmt[i].bits_per_pixel;
		}
		XFree(pixfmt);
	}

	if (self->depth > 8) {
		self->rmask = self->visual->red_mask;
		self->gmask = self->visual->green_mask;
		self->bmask = self->visual->blue_mask;
		self->amask = 0;
	}

#ifndef NO_SHARED_MEMORY
	if (localx11) {
		self->use_mitshm = XShmQueryExtension(self->display_x11);
	}	else {
		self->use_mitshm = 0;
	}
#endif

	// 查看是用已有窗口还是建立窗口
	if (self->windowid != NULL) {
		self->window = atoi(self->windowid);
	}	else {
		self->window = XCreateSimpleWindow(self->display_x11,
			DefaultRootWindow(self->display_x11), 0, 0,
			64, 64, 1,
			BlackPixel(self->display_x11, self->screen),
			BlackPixel(self->display_x11, self->screen));
	}
	
	// 设置HINT
	{
		XClassHint *hint;
		hint = XAllocClassHint();
		if (hint) {
			hint->res_name = "PIXELLIB_APP";
			hint->res_class = "PIXELLIB_APP";
			XSetClassHint(self->display_x11, self->window, hint);
			XFree(hint);
		}
	}

	XSetWMProtocols(self->display_x11, self->window, &self->DELWIN, 1);
	
	// 缓存窗口
	{
		unsigned long mask;
		Screen *screen;
		XSetWindowAttributes a;

		screen = DefaultScreenOfDisplay(self->display_x11);
		if (DoesBackingStore(screen) != NotUseful) {
			mask = CWBackingStore;
			a.backing_store = DoesBackingStore(screen);
		}	else {
			mask = CWSaveUnder;
			a.save_under = DoesSaveUnders(screen);
		}
		XChangeWindowAttributes(self->display_x11, self->window, mask, &a);
	}

	XSync(self->display_x11, 0);

	// 创建图形上下文
	gcv.graphics_exposures = 0;
	self->gc = XCreateGC(self->display_x11, self->window, 
		GCGraphicsExposures, &gcv);

	if (self->gc == NULL) {
		if (self->windowid == NULL) {
			XDestroyWindow(self->display_x11, self->window);
		}
		fprintf(stderr, "cannot create graphics context\n");
		fflush(stderr);
		XCloseDisplay(self->display_x11);
		XCloseDisplay(self->display_gfx);
		return -3;
	}

	if (self->depth == 8) {
		int ncolors;
		self->display_colormap = DefaultColormap(self->display_x11, 
			self->screen);
		self->xcolormap = self->display_colormap;
		ncolors = 1 << self->depth;
		self->xpixels = (int*)malloc(ncolors * sizeof(int));
		if (self->xpixels == NULL) {
			if (self->windowid == NULL) {
				XDestroyWindow(self->display_x11, self->window);
			}
			fprintf(stderr, "not enough memory\n");
			fflush(stderr);
			XCloseDisplay(self->display_x11);
			XCloseDisplay(self->display_gfx);
			return -4;
		}
		memset(self->xpixels, 0, sizeof(int) * ncolors);
	}

	self->blank_cursor = NULL;

	if (self->windowid == NULL) {
		XSelectInput(self->display_x11, self->window,
			FocusChangeMask | PropertyChangeMask | EnterWindowMask |
			LeaveWindowMask | KeyPressMask | KeyReleaseMask |
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
			ExposureMask | StructureNotifyMask);
	}

	{
		static unsigned long endian = 0x11223344;
		self->swap_pixels = 0;
		if (*((unsigned char*)&endian) == 0x44) {
			if (XImageByteOrder(self->display_x11) == MSBFirst) 
				self->swap_pixels = 1;
		}	else {
			if (XImageByteOrder(self->display_x11) == LSBFirst)
				self->swap_pixels = 1;
		}
	}

	self->modelist = NULL;
	self->image = NULL;
	self->pixel = NULL;
	ikitwin_screen_w = DisplayWidth(self->display_x11, self->screen);
	ikitwin_screen_h = DisplayHeight(self->display_x11, self->screen);
	self->screen_w = ikitwin_screen_w;
	self->screen_h = ikitwin_screen_h;
	ikitwin_screen_bpp = self->depth;

	XSync(self->display_x11, 0);
	ikitwin_closing = 0;

	return 0;
}


static int x11_show_window(IVideoPrivateX11 *self, int w, int h, int full)
{
	XSizeHints *sizehints;
	XWindowAttributes xwinattr;
	XEvent event;

	// 如果是全屏模式
	if (full) {
		XSetWindowAttributes xsetattr;
		Window u1; int u2; unsigned int u3;
		XGetWindowAttributes(self->display_x11, self->window, &xwinattr);
		if (xwinattr.override_redirect == 0) {
			if (xwinattr.map_state != IsUnmapped) {
				XUnmapWindow(self->display_x11, self->window);
				XSync(self->display_x11, 0);
				for (; ; ) {
					XNextEvent(self->display_x11, &event);
					if (event.type == UnmapNotify) break;
				}
			}
		}
		XMoveWindow(self->display_x11, self->window, 0, 0);

		xsetattr.override_redirect = 1;
		XChangeWindowAttributes(self->display_x11, self->window, 
			CWOverrideRedirect, &xsetattr);

		XGetGeometry(self->display_x11, DefaultRootWindow(self->display_x11),
			&u1, &u2, &u2, (unsigned int*)&w, (unsigned int*)&h, &u3, &u3);
	}	else {
		XSetWindowAttributes xsetattr;
		XGetWindowAttributes(self->display_x11, self->window, &xwinattr);
		if (xwinattr.override_redirect) {
			if (xwinattr.map_state != IsUnmapped) {
				XUnmapWindow(self->display_x11, self->window);
				XSync(self->display_x11, 0);
				for (; ; ) {
					XNextEvent(self->display_x11, &event);
					if (event.type == UnmapNotify) break;
				}
			}
		}
		XMoveWindow(self->display_x11, self->window, 
			(self->screen_w - w) / 2, (self->screen_h - h) / 2);
		xsetattr.override_redirect = 0;
		XChangeWindowAttributes(self->display_x11, self->window, 
			CWOverrideRedirect, &xsetattr);
	}

	sizehints = XAllocSizeHints();
	if (sizehints == NULL) {
		fprintf(stderr, "cannot alloc size hints\n");
		fflush(stderr);
		return -1;
	}

	sizehints->min_width = w;
	sizehints->max_width = w;
	sizehints->min_height = h;
	sizehints->max_height = h;
	sizehints->flags = PMinSize | PMaxSize;

	XSetWMNormalHints(self->display_x11, self->window, sizehints);
	XFree(sizehints);

	XResizeWindow(self->display_x11, self->window, w, h);

	XGetWindowAttributes(self->display_x11, self->window, &xwinattr);
	if (xwinattr.map_state == IsUnmapped) {
		XMapRaised(self->display_x11, self->window);
		for (; ; ) {
			XNextEvent(self->display_x11, &event);
			if (event.type == MapNotify) break;
		}
	}

	return 0;
}

static int x11_release_mode(IVideoPrivateX11 *self)
{
	x11_destroy_image(self);
	if (self->windowid) {
		free(self->windowid);
		self->windowid = NULL;
	}
	return 0;
}

static int x11_set_mode(IVideoPrivateX11 *self, int w, int h, int bpp, int f)
{
	int bytesperpixel;

	f &= ~1;
	x11_release_mode(self);
	if (self->windowid == NULL) {
		if (x11_show_window(self, w, h, f) != 0) {
			return -1;
		}
	}

	bytesperpixel = (self->depth + 7) / 8;
	self->w = w;
	self->h = h;
	self->pitch = ((unsigned long)w * bytesperpixel + 3) & ~3;
	ikitwin_winw = w;
	ikitwin_winh = h;

	if (x11_create_image(self) != 0) {
		fprintf(stderr, "cannot create XImage\n");
		fflush(stderr);
		return -2;
	}

	self->async = (unix_ncpus() > 1)? 1 : 0;
	ikitwin_private = self;
	XSync(self->display_x11, 0);

	return 0;
}

static void x11_quit(IVideoPrivateX11 *self)
{
	if (self->display_x11) {
		x11_release_mode(self);
		if (self->window && self->windowid == NULL) {
			XDestroyWindow(self->display_x11, self->window);
			self->window = 0;
		}
		if (self->modelist) {
			free(self->modelist);
			self->modelist = NULL;
		}
		if (self->private_colormap) {
			XFreeColormap(self->display_x11, self->private_colormap);
			self->private_colormap = 0;
		}
		if (self->xpixels) {
			unsigned long pixel;
			int ncolors = 1 << self->depth;
			for (pixel = 0; pixel < ncolors; pixel++) {
				while (self->xpixels[pixel] > 0) {
					XFreeColors(self->display_x11, self->display_colormap,
						&pixel, 1, 0);
					self->xpixels[pixel]--;
				}
			}
			free(self->xpixels);
			self->xpixels = NULL;
		}
		if (self->display_gfx) {
			XCloseDisplay(self->display_gfx);
			self->display_gfx = NULL;
		}
		if (self->display_x11) {
			XCloseDisplay(self->display_x11);
			self->display_x11 = NULL;
		}
	}
	memset(self, 0, sizeof(IVideoPrivateX11));
}

static short ikeymap_odd[256];
static short ikeymap_misc[256];


static void x11_keymap_init(void)
{
	int i;
	for (i = 0; i < 512; i++) {
		ikeymap_odd[i] = ikeymap_misc[i] = IKEY_UNKNOWN;
	}
#ifdef XK_dead_circumflex
	/* These X keysyms have 0xFE as the high byte */
	ikeymap_odd[XK_dead_circumflex&0xFF] = IKEY_CARET;
#endif
#ifdef XK_ISO_Level3_Shift
	ikeymap_odd[XK_ISO_Level3_Shift&0xFF] = IKEY_MODE; /* "Alt Gr" key */
#endif
	ikeymap_misc[XK_BackSpace&0xFF] = IKEY_BACKSPACE;
	ikeymap_misc[XK_Tab&0xFF] = IKEY_TAB;
	ikeymap_misc[XK_Clear&0xFF] = IKEY_CLEAR;
	ikeymap_misc[XK_Return&0xFF] = IKEY_RETURN;
	ikeymap_misc[XK_Pause&0xFF] = IKEY_PAUSE;
	ikeymap_misc[XK_Escape&0xFF] = IKEY_ESCAPE;
	ikeymap_misc[XK_Delete&0xFF] = IKEY_DELETE;

	ikeymap_misc[XK_KP_0&0xFF] = IKEY_KP0;		/* Keypad 0-9 */
	ikeymap_misc[XK_KP_1&0xFF] = IKEY_KP1;
	ikeymap_misc[XK_KP_2&0xFF] = IKEY_KP2;
	ikeymap_misc[XK_KP_3&0xFF] = IKEY_KP3;
	ikeymap_misc[XK_KP_4&0xFF] = IKEY_KP4;
	ikeymap_misc[XK_KP_5&0xFF] = IKEY_KP5;
	ikeymap_misc[XK_KP_6&0xFF] = IKEY_KP6;
	ikeymap_misc[XK_KP_7&0xFF] = IKEY_KP7;
	ikeymap_misc[XK_KP_8&0xFF] = IKEY_KP8;
	ikeymap_misc[XK_KP_9&0xFF] = IKEY_KP9;
	ikeymap_misc[XK_KP_Insert&0xFF] = IKEY_KP0;
	ikeymap_misc[XK_KP_End&0xFF] = IKEY_KP1;	
	ikeymap_misc[XK_KP_Down&0xFF] = IKEY_KP2;
	ikeymap_misc[XK_KP_Page_Down&0xFF] = IKEY_KP3;
	ikeymap_misc[XK_KP_Left&0xFF] = IKEY_KP4;
	ikeymap_misc[XK_KP_Begin&0xFF] = IKEY_KP5;
	ikeymap_misc[XK_KP_Right&0xFF] = IKEY_KP6;
	ikeymap_misc[XK_KP_Home&0xFF] = IKEY_KP7;
	ikeymap_misc[XK_KP_Up&0xFF] = IKEY_KP8;
	ikeymap_misc[XK_KP_Page_Up&0xFF] = IKEY_KP9;
	ikeymap_misc[XK_KP_Delete&0xFF] = IKEY_KP_PERIOD;
	ikeymap_misc[XK_KP_Decimal&0xFF] = IKEY_KP_PERIOD;
	ikeymap_misc[XK_KP_Divide&0xFF] = IKEY_KP_DIVIDE;
	ikeymap_misc[XK_KP_Multiply&0xFF] = IKEY_KP_MULTIPLY;
	ikeymap_misc[XK_KP_Subtract&0xFF] = IKEY_KP_MINUS;
	ikeymap_misc[XK_KP_Add&0xFF] = IKEY_KP_PLUS;
	ikeymap_misc[XK_KP_Enter&0xFF] = IKEY_KP_ENTER;
	ikeymap_misc[XK_KP_Equal&0xFF] = IKEY_KP_EQUALS;

	ikeymap_misc[XK_Up&0xFF] = IKEY_UP;
	ikeymap_misc[XK_Down&0xFF] = IKEY_DOWN;
	ikeymap_misc[XK_Right&0xFF] = IKEY_RIGHT;
	ikeymap_misc[XK_Left&0xFF] = IKEY_LEFT;
	ikeymap_misc[XK_Insert&0xFF] = IKEY_INSERT;
	ikeymap_misc[XK_Home&0xFF] = IKEY_HOME;
	ikeymap_misc[XK_End&0xFF] = IKEY_END;
	ikeymap_misc[XK_Page_Up&0xFF] = IKEY_PAGEUP;
	ikeymap_misc[XK_Page_Down&0xFF] = IKEY_PAGEDOWN;

	ikeymap_misc[XK_F1&0xFF] = IKEY_F1;
	ikeymap_misc[XK_F2&0xFF] = IKEY_F2;
	ikeymap_misc[XK_F3&0xFF] = IKEY_F3;
	ikeymap_misc[XK_F4&0xFF] = IKEY_F4;
	ikeymap_misc[XK_F5&0xFF] = IKEY_F5;
	ikeymap_misc[XK_F6&0xFF] = IKEY_F6;
	ikeymap_misc[XK_F7&0xFF] = IKEY_F7;
	ikeymap_misc[XK_F8&0xFF] = IKEY_F8;
	ikeymap_misc[XK_F9&0xFF] = IKEY_F9;
	ikeymap_misc[XK_F10&0xFF] = IKEY_F10;
	ikeymap_misc[XK_F11&0xFF] = IKEY_F11;
	ikeymap_misc[XK_F12&0xFF] = IKEY_F12;
	ikeymap_misc[XK_F13&0xFF] = IKEY_F13;
	ikeymap_misc[XK_F14&0xFF] = IKEY_F14;
	ikeymap_misc[XK_F15&0xFF] = IKEY_F15;

	ikeymap_misc[XK_Num_Lock&0xFF] = IKEY_NUMLOCK;
	ikeymap_misc[XK_Caps_Lock&0xFF] = IKEY_CAPSLOCK;
	ikeymap_misc[XK_Scroll_Lock&0xFF] = IKEY_SCROLLOCK;
	ikeymap_misc[XK_Shift_R&0xFF] = IKEY_RSHIFT;
	ikeymap_misc[XK_Shift_L&0xFF] = IKEY_LSHIFT;
	ikeymap_misc[XK_Control_R&0xFF] = IKEY_RCTRL;
	ikeymap_misc[XK_Control_L&0xFF] = IKEY_LCTRL;
	ikeymap_misc[XK_Alt_R&0xFF] = IKEY_RALT;
	ikeymap_misc[XK_Alt_L&0xFF] = IKEY_LALT;
	ikeymap_misc[XK_Meta_R&0xFF] = IKEY_RMETA;
	ikeymap_misc[XK_Meta_L&0xFF] = IKEY_LMETA;
	ikeymap_misc[XK_Super_L&0xFF] = IKEY_LSUPER; /* Left "Windows" */
	ikeymap_misc[XK_Super_R&0xFF] = IKEY_RSUPER; /* Right "Windows */
	ikeymap_misc[XK_Mode_switch&0xFF] = IKEY_MODE; /* "Alt Gr" key */
	ikeymap_misc[XK_Multi_key&0xFF] = IKEY_COMPOSE; /* Multi-key compose */

	ikeymap_misc[XK_Help&0xFF] = IKEY_HELP;
	ikeymap_misc[XK_Print&0xFF] = IKEY_PRINT;
	ikeymap_misc[XK_Sys_Req&0xFF] = IKEY_SYSREQ;
	ikeymap_misc[XK_Break&0xFF] = IKEY_BREAK;
	ikeymap_misc[XK_Menu&0xFF] = IKEY_MENU;
	ikeymap_misc[XK_Hyper_R&0xFF] = IKEY_MENU;   /* Windows "Menu" key */

	#define IMAPKEY(s, d) ikeymap_misc[(s) & 0xff] = d
	#define ITRANSK(x) IMAPKEY(XK_##x, IKEY_##x)

	ITRANSK(A); ITRANSK(B); ITRANSK(C); ITRANSK(D); ITRANSK(E); ITRANSK(F);
	ITRANSK(G); ITRANSK(H); ITRANSK(I); ITRANSK(J); ITRANSK(K); ITRANSK(L);
	ITRANSK(M); ITRANSK(N); ITRANSK(O); ITRANSK(P); ITRANSK(Q); ITRANSK(R);
	ITRANSK(S); ITRANSK(T); ITRANSK(U); ITRANSK(V); ITRANSK(W); ITRANSK(X);
	ITRANSK(Y); ITRANSK(Z);

	#undef ITRANSK
	#undef IMAPKEY
}

static int x11_translate_key(Display *display, XKeyEvent *xkey, KeyCode kc)
{
	static int inited = 0;
	KeySym xsym;
	int keycode = -1;

	if (inited == 0) {
		x11_keymap_init();
		inited = 1;
	}

	xsym = XKeycodeToKeysym(display, kc, 0);

	switch (xsym >> 8)
	{
	case 0x1005FF:
#ifdef SunXK_F36
		if (xsym == SunXK_F36) keycode = IKEY_F11;
#endif
#ifdef SunXK_F37
		if (xsym == SunXK_F37) keycode = IKEY_F12;
#endif
		break;
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
		keycode = xsym & 0xff;
		if (keycode >= 'A' && keycode <= 'Z') keycode += 'a' - 'A';
		break;
	case 0xfe:
		keycode = ikeymap_odd[xsym & 0xff];
		break;
	case 0xff:
		keycode = ikeymap_misc[xsym & 0xff];
		break;
	}

	return keycode;
}

static int x11_repeat_key(IVideoPrivateX11 *self, XEvent *event)
{
	XEvent peekevent;
	int repeat = 0;
	if (XPending(self->display_x11)) {
		XPeekEvent(self->display_x11, &peekevent);
		if (peekevent.type == KeyPress && 
			peekevent.xkey.keycode == event->xkey.keycode &&
			peekevent.xkey.time == event->xkey.time) {
			repeat = 1;
			XNextEvent(self->display_x11, &peekevent);
		}
	}
	return repeat;
}

static int x11_dispatch(IVideoPrivateX11 *self)
{
	XEvent event;
	int keycode;

	if (XPending(self->display_x11) <= 0) {
		return -1;
	}

	XNextEvent(self->display_x11, &event);

	switch (event.type)
	{
	case ConfigureNotify:
		break;
	case EnterNotify: 
		if (ikitwin_private_active)
			ikitwin_private_active(1, IKIT_ACTIVE_FOCUSMOUSE);
		ikitwin_mouse_in = 1;
		break;
	case LeaveNotify:
		if (ikitwin_private_active)
			ikitwin_private_active(0, IKIT_ACTIVE_FOCUSMOUSE);
		ikitwin_mouse_in = 0;
		break;
	case FocusIn:
		if (ikitwin_private_active)
			ikitwin_private_active(1, IKIT_ACTIVE_FOCUSINPUT);
		break;
	case FocusOut:
		if (ikitwin_private_active)
			ikitwin_private_active(0, IKIT_ACTIVE_FOCUSINPUT);
		break;
	case KeyPress:
		keycode = x11_translate_key(self->display_x11, &event.xkey, 
			event.xkey.keycode);
		ikitwin_keymap[keycode & 511] = 1;
		if (ikitwin_private_keyboard)
			ikitwin_private_keyboard(IKIT_KEYDOWN, keycode);
		break;
	case KeyRelease:
		keycode = x11_translate_key(self->display_x11, &event.xkey, 
			event.xkey.keycode);
		if (!x11_repeat_key(self, &event)) {
			ikitwin_keymap[keycode & 511] = 0;
			if (ikitwin_private_keyboard)
				ikitwin_private_keyboard(IKIT_KEYUP, keycode);
		}
		break;
	case MotionNotify:
		ikitwin_mouse_x = event.xmotion.x;
		ikitwin_mouse_y = event.xmotion.y;
		if (ikitwin_private_mouse_motion) 
			ikitwin_private_mouse_motion(0, 0, event.xmotion.x, 
				event.xmotion.y);
		break;
	case ButtonPress: {
			int key = 32;
			if (event.xbutton.button == 1) key = IMOUSE_LEFT;
			else if (event.xbutton.button == 2) key = IMOUSE_MIDDLE;
			else if (event.xbutton.button == 3) key = IMOUSE_RIGHT;
			ikitwin_mouse_btn |= (1 << key);
			if (ikitwin_private_mouse_button)
				ikitwin_private_mouse_button(1, key, ikitwin_mouse_x,
					ikitwin_mouse_y);
		}
		break;
	case ButtonRelease: {
			int key = 32;
			if (event.xbutton.button == 1) key = IMOUSE_LEFT;
			else if (event.xbutton.button == 2) key = IMOUSE_MIDDLE;
			else if (event.xbutton.button == 3) key = IMOUSE_RIGHT;
			ikitwin_mouse_btn &= ~(1 << key);
			if (ikitwin_private_mouse_button)
				ikitwin_private_mouse_button(0, key, ikitwin_mouse_x,
					ikitwin_mouse_y);
		}
		break;
	case UnmapNotify:
		if (ikitwin_private_active) 
			ikitwin_private_active(0, IKIT_ACTIVE_APP | 
				IKIT_ACTIVE_FOCUSMOUSE);
		break;
	case MapNotify:
		if (ikitwin_private_active) 
			ikitwin_private_active(1, IKIT_ACTIVE_APP | 
				IKIT_ACTIVE_FOCUSMOUSE);
		break;
	case Expose:
		if (event.xexpose.count == 0) {
			int rect[4] = { 0, 0, 0, 0 };
			rect[2] = self->w;
			rect[3] = self->h;
			if (ikitwin_refresh) 
				x11_update_image(self, rect, 1);
		}
		break;
	case ClientMessage:
		if (event.xclient.format == 32 && 
			event.xclient.data.l[0] == self->DELWIN) {
			ikitwin_closing = 1;
			if (ikitwin_private_quit) 
				ikitwin_private_quit();
		}
		break;
	case ResizeRequest:
		ikitwin_winw = event.xresizerequest.width;
		ikitwin_winh = event.xresizerequest.height;
		break;
	case DestroyNotify:
		break;
	}

	return 0;
}

void ikitwin_x11_disable_warnings(IVideoPrivateX11 *self)
{
	x11_resize_image(self);
	x11_refresh(self);
}

static int x11_set_caption(IVideoPrivateX11 *self, const char *caption)
{
	XTextProperty titleprop;
	int error = XLocaleNotSupported;
	#ifdef X_HAVE_UTF8_STRING
	error = Xutf8TextListToTextProperty(self->display_x11,
		(char**)&caption, 1, XUTF8StringStyle, &titleprop);
	#endif
	if (error != Success) {
		XStringListToTextProperty((char**)&caption, 1, 
			&titleprop);
	}
	XSetWMName(self->display_x11, self->window, &titleprop);
	XFree(titleprop.value);
	return 0;
}


#elif defined(WIN32) || defined(_WIN32)
//=====================================================================
// Windows 区
//=====================================================================
typedef struct
{
	HWND hWnd;
	HBITMAP hBitmap;
	HPALETTE hPalette;
	BITMAPINFO *info;
	LOGPALETTE *pal;
	LPRGBQUAD palette;
	HDC hDC;
	char *windowid;
	int screen_w;
	int screen_h;
	int w;
	int h;
	int depth;		// screen depth
	long pitch;
	unsigned char *pixel;
	void *parent;
	unsigned int rmask;
	unsigned int gmask;
	unsigned int bmask;
	unsigned int amask;
}	IVideoPrivateWin;



static int mswin_get_depth(void)
{
#ifdef NO_GETDIBITS
	int depth;
	HDC hDC;
	hDC = GetDC(NULL);
	depth = GetDeviceCaps(hDC, PLANES) * GetDeviceCaps(hDC, BITSPIXEL);
	ReleaseDC(NULL, hDC);
	#ifndef _WIN32_WCE
	if (depth == 16) {
		return 15;
	}
	#endif
	return depth;
#else
	int dib_size;
	int depth;
	LPBITMAPINFOHEADER dib_hdr;
	HDC hdc;
	HBITMAP hbm;
	dib_size = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
	dib_hdr = (LPBITMAPINFOHEADER)malloc(dib_size);
	memset(dib_hdr, 0, dib_size);
	dib_hdr->biSize = sizeof(BITMAPINFOHEADER);
	hdc = GetDC(NULL);
	hbm = CreateCompatibleBitmap(hdc, 1, 1);
	// must call GetDIBits twice: first get header, second get bitfields
	GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO)dib_hdr, DIB_RGB_COLORS);
	GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO)dib_hdr, DIB_RGB_COLORS);
	DeleteObject(hbm);
	ReleaseDC(NULL, hdc);
	switch (dib_hdr->biBitCount) {
	case 8: depth = 8; break;
	case 24: depth = 24; break;
	case 32: depth = 32; break;
	case 16:
		if (dib_hdr->biCompression == BI_BITFIELDS) {
			switch (((DWORD*)((char*)dib_hdr + dib_hdr->biSize))[0]) {
			case 0xf800: depth = 16; break;
			case 0x7c00: depth = 15; break;
			default: depth = 0; break;
			}
		}	else {
			depth = 0;
		}
		break;
	default: depth = 0; break;
	}
	free(dib_hdr);
	return depth;
#endif
}


//---------------------------------------------------------------------
// Window 内部接口
//---------------------------------------------------------------------
HINSTANCE mswin_instance = NULL;
void *mswin_handle = NULL;

#ifdef _WIN32_WCE
LPWSTR mswin_appname = NULL;
#else
LPSTR mswin_appname = NULL;
#endif

HWND mswin_hwnd = NULL;
RECT mswin_bounds = { 0, 0, 0, 0 };
#ifndef NO_CHANGEDISPLAY
DEVMODE mswin_fullscreen_mode;
#endif
WORD *mswin_gamma_saved = NULL;
int mswin_posted = 0;
HCURSOR mswin_cursor = NULL;


LRESULT (*mswin_handle_message)(void *current, HWND, 
	UINT, WPARAM, LPARAM) = 0;

void (*WIN_RealizePalette)(void *current) = 0;
void (*WIN_PaletteChanged)(void *current, HWND hwnd) = 0;
void (*WIN_WinPaint)(void *current, HDC hdc, HWND hwnd) = 0;


LRESULT CALLBACK 
mswin_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#if defined(WM_MOUSELEAVE) && (!defined(NOTRACKMOUSEEVENT))
static BOOL (WINAPI *_TrackMouseEvent)(TRACKMOUSEEVENT *ptme) = NULL;
static VOID CALLBACK
TrackMouseTimerProc(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	RECT rect;
	POINT pt, pts[2];
	GetClientRect(hWnd, &rect);
	MapWindowPoints(hWnd, NULL, (LPPOINT)&pts, 2);
	rect.left = pts[0].x;
	rect.top = pts[0].y;
	rect.right = pts[1].x;
	rect.bottom = pts[1].y;
	GetCursorPos(&pt);
	if (!PtInRect(&rect, pt) || (WindowFromPoint(pt) != hWnd)) {
		if (!KillTimer(hWnd, idEvent)) {
			// error
			MessageBoxA(NULL, "ikitwin: error killing time", "ERROR", MB_OK);
			abort();
		}
		PostMessage(hWnd, WM_MOUSELEAVE, 0, 0);
	}
}
static BOOL WINAPI WIN_TrackMouseEvent(TRACKMOUSEEVENT *ptme)
{
	if (ptme->dwFlags == TME_LEAVE) {
		return (BOOL)SetTimer(ptme->hwndTrack, ptme->dwFlags, 100,
			(TIMERPROC)TrackMouseTimerProc);
	}
	return FALSE;
}
#endif

LRESULT CALLBACK 
mswin_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int inwindow = 0;
	static int pressed = 0;
	int posted = 0;
	switch (msg)
	{
		case WM_ACTIVATE: {
			int active, minimized;
			minimized = HIWORD(wParam);
			active = LOWORD(wParam);
			if (minimized && !active) {
				if (ikitwin_private_active) 
					posted = ikitwin_private_active(0, IKIT_ACTIVE_APP);
			}
			if (!minimized && active) {
				if (ikitwin_private_active)
					posted = ikitwin_private_active(1, IKIT_ACTIVE_APP);
			}
		}
		return 0;

		case WM_MOUSEMOVE: {
			if (ikitwin_fullscreen == 0) {
				short x, y;
				if (!inwindow) {
			#if defined(WM_MOUSELEAVE) && (!defined(NOTRACKMOUSEEVENT))
					TRACKMOUSEEVENT tme;
					tme.cbSize = sizeof(tme);
					tme.dwFlags = TME_LEAVE;
					tme.hwndTrack = hWnd;
					_TrackMouseEvent(&tme);
			#endif
					inwindow = 1;
					if (ikitwin_private_active)
						posted = 
						ikitwin_private_active(1, IKIT_ACTIVE_FOCUSINPUT);
					ikitwin_mouse_in = 1;
				}
				x = LOWORD(lParam);
				y = HIWORD(lParam);
				if (ikitwin_mouserel) {
					POINT center;
					center.x = ikitwin_screen_w / 2;
					center.y = ikitwin_screen_h / 2;
					x -= (short)center.x;
					y -= (short)center.y;
					if (x || y) {
						ClientToScreen(hWnd, &center);
						SetCursorPos(center.x, center.y);
						ikitwin_mouse_x = x;
						ikitwin_mouse_y = y;
						if (ikitwin_private_mouse_motion)
							posted = 
							ikitwin_private_mouse_motion(0, 1, x, y);
					}
				}	else {
					ikitwin_mouse_x = x;
					ikitwin_mouse_y = y;
					if (ikitwin_private_mouse_motion)
						posted = ikitwin_private_mouse_motion(0, 0, x, y);
				}
			}
		}
		return 0;
		
#if defined(WM_MOUSELEAVE) && (!defined(NOTRACKMOUSEEVENT))
		case WM_MOUSELEAVE: {
			if (ikitwin_fullscreen == 0) {
				if (ikitwin_private_active)
					posted = ikitwin_private_active(0, IKIT_ACTIVE_FOCUSINPUT);
			}
			inwindow = 0;
			ikitwin_mouse_in = 0;
		}
		return 0;
#endif

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP: {
			if (ikitwin_fullscreen == 0) {
				unsigned short x, y;
				int btn, state;
				SetFocus(hWnd);
				switch (msg)
				{
				case WM_LBUTTONDOWN: btn = IMOUSE_LEFT, state = 1; break;
				case WM_RBUTTONDOWN: btn = IMOUSE_RIGHT, state = 1; break;
				case WM_MBUTTONDOWN: btn = IMOUSE_MIDDLE, state = 1; break;
				case WM_LBUTTONUP:   btn = IMOUSE_LEFT, state = 0; break;
				case WM_RBUTTONUP:   btn = IMOUSE_RIGHT, state = 0; break;
				case WM_MBUTTONUP:   btn = IMOUSE_MIDDLE, state = 0; break;
				default: btn = state = 0; break;
				}
				if (state) {
					if (++pressed > 0) {
						SetCapture(hWnd);
					}
				}	else {
					if (--pressed <= 0) {
						ReleaseCapture();
						pressed = 0;
					}
				}
				if (ikitwin_mouserel) {
					x = y = 0;
				}	else {
					x = LOWORD(lParam);
					y = HIWORD(lParam);
				}

				if (state != 0) ikitwin_mouse_btn |= (1 << btn);
				else ikitwin_mouse_btn &= ~(1 << btn);

				if (ikitwin_private_mouse_button)
					posted = ikitwin_private_mouse_button(state, btn, x, y);
			}
		}
		return 0;
		
	#if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
		#ifdef WM_MOUSEWHEEL
		case WM_MOUSEWHEEL: 
			if (!ikitwin_fullscreen) {
				int move = (short)HIWORD(wParam);
				if (move) {
					int button = (move > 0)? IMOUSE_WHEELUP: IMOUSE_WHEELDW;
					if (ikitwin_private_mouse_button) {
						posted = ikitwin_private_mouse_button(
							1, button, 0, 0);
						posted = ikitwin_private_mouse_button(
							0, button, 0, 0);
					}
				}
			}
			return 0;
		#endif
	#endif
	
	#ifdef WM_GETMINMAXINFO
		case WM_GETMINMAXINFO: {
			MINMAXINFO *info;
			RECT size;
			int x, y;
			int width;
			int height;
			if (ikitwin_resizing)
				return 0;
			GetWindowRect(hWnd, &size);
			x = size.left;
			y = size.top;
			size.top = 0;
			size.left = 0;
			size.right = ikitwin_winw;
			size.bottom = ikitwin_winh;
			AdjustWindowRect(&size, GetWindowLong(hWnd, GWL_STYLE), FALSE);
			width = size.right - size.left;
			height = size.bottom - size.top;
			info = (MINMAXINFO*)lParam;
			info->ptMaxSize.x = width;
			info->ptMaxSize.y = height;
			info->ptMaxPosition.x = x;
			info->ptMaxPosition.y = y;
			info->ptMinTrackSize.x = width;
			info->ptMinTrackSize.y = height;
			info->ptMaxTrackSize.x = width;
			info->ptMaxTrackSize.y = height;
		}
		return 0;
	#endif

		case WM_MOVE: {
			POINT pt1, pt2;
			GetClientRect(hWnd, &mswin_bounds);
			pt1.x = mswin_bounds.left;
			pt1.y = mswin_bounds.top;
			pt2.x = mswin_bounds.right;
			pt2.y = mswin_bounds.bottom;
			ClientToScreen(hWnd, (LPPOINT)&pt1);
			ClientToScreen(hWnd, (LPPOINT)&pt2);
			ikitwin_winx = pt1.x;
			ikitwin_winy = pt1.y;
		}
		break;

		case WM_SETCURSOR: {
			unsigned short hittest;
			hittest = LOWORD(lParam);
			if (hittest == HTCLIENT && mswin_cursor) {
				SetCursor(mswin_cursor);
				return 1;
			}
		}
		break;

		case WM_QUERYNEWPALETTE: {
			if (WIN_PaletteChanged)
				WIN_PaletteChanged(ikitwin_private, (HWND)wParam);
		}
		break;

		case WM_PAINT: {
			HDC hDC;
			PAINTSTRUCT ps;
			hDC = BeginPaint(hWnd, &ps);
			if (ikitwin_refresh) {
				if (WIN_WinPaint)
					WIN_WinPaint(ikitwin_private, hDC, hWnd);
			}
			EndPaint(hWnd, &ps);
		}
		break;

		case WM_CLOSE: {
			ikitwin_closing = 1;
			if (ikitwin_private_quit) {
				ikitwin_private_quit();
			}
			if (posted) {
				PostQuitMessage(0);
			}
		}
		return 0;

		case WM_DESTROY: {
			PostQuitMessage(0);
		}
		return 0;

		default: {
			if (mswin_handle_message) {
				return mswin_handle_message(ikitwin_private,
					hWnd, msg, wParam, lParam);
			}
		}
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void mswin_set_module_handle(void *handle)
{
	mswin_handle = handle;
}

void *mswin_get_module_handle(void)
{
	void *handle;
	if (mswin_handle) {
		handle = mswin_handle;
	}	else {
		handle = GetModuleHandle(NULL);
	}
	return handle;
}

int mswin_register(const char *name, unsigned long style, void *hInst)
{
	static int initialized = 0;
	WNDCLASSA wc;
#if defined(WM_MOUSELEAVE) && (!defined(NOTRACKMOUSEEVENT))
	HMODULE handle;
#endif
	if (initialized) 
		return 0;
	if (!hInst)
		hInst = mswin_get_module_handle();

#ifdef _WIN32_WCE
	{
		int len = strlen(name) + 1;
		mswin_appname = (LPWSTR)malloc(len * 2);
		MultiByteToWideChar(CP_ACP, 0, name, -1, mswin_appname, len);
	}
#else
	{
		int len = (int)strlen(name) + 1;
		mswin_appname = (LPSTR)malloc(len);
		memcpy(mswin_appname, name, len);
	}
#endif
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = (HICON)LoadImageA((HINSTANCE)hInst, mswin_appname, 
		IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = mswin_appname;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hInstance = (HINSTANCE)hInst;
	wc.style = style;
#ifdef HAVE_OPENGL
	wc.style |= CS_OWNDC;
#endif
	wc.lpfnWndProc = mswin_message;
	wc.cbWndExtra = 0;
	wc.cbClsExtra = 0;
	if (!RegisterClassA(&wc)) {
		fprintf(stderr, "error to register class");
		fflush(stderr);
		MessageBoxA(NULL, "ikitwin: error for register", "ERROR", MB_OK);
		return -1;
	}
	mswin_instance = (HINSTANCE)hInst;
#if defined(WM_MOUSELEAVE) && (!defined(NOTRACKMOUSEEVENT))
	_TrackMouseEvent = NULL;
	handle = GetModuleHandleA("USER32.DLL");
	if (handle) {
		_TrackMouseEvent = (BOOL (WINAPI*)(TRACKMOUSEEVENT*))
			GetProcAddress(handle, "TrackMouseEvent");
	}
	if (_TrackMouseEvent == NULL) {
		_TrackMouseEvent = WIN_TrackMouseEvent;
	}
#endif
	initialized = 1;
	return 0;
}

static int mswin_create_window(IVideoPrivateWin *self)
{
	mswin_register("ADI_APP",CS_BYTEALIGNCLIENT, 0);
	if (self->windowid) {
#if defined(_WIN64) || defined(WIN64)
		self->hWnd = (HWND)_strtoui64(self->windowid, NULL, 10);
#elif defined(_MSC_VER)
	#if _MSC_VER >= 1400
		self->hWnd = (HWND)_strtoui64(self->windowid, NULL, 10);
	#else
		self->hWnd = (HWND)atol(self->windowid);
	#endif
#else
		self->hWnd = (HWND)atol(self->windowid);
#endif
	}	else {
		self->hWnd = CreateWindowA(mswin_appname, mswin_appname,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
			0, 0, 0, 0, NULL, NULL, mswin_instance, NULL);
		if (self->hWnd == NULL) {
			MessageBoxA(NULL, "cannot create window", "ERROR", MB_OK);
			return -1;
		}
		ShowWindow(self->hWnd, SW_HIDE);
	}
	mswin_hwnd = self->hWnd;
	return 0;
}

static void mswin_destroy_window(IVideoPrivateWin *self)
{
	if (self->windowid == NULL) {
		DestroyWindow(self->hWnd);
	}
}


#define VK_SEMICOLON	0xBA
#define VK_EQUALS	0xBB
#define VK_COMMA	0xBC
#define VK_MINUS	0xBD
#define VK_PERIOD	0xBE
#define VK_SLASH	0xBF
#define VK_GRAVE	0xC0
#define VK_LBRACKET	0xDB
#define VK_BACKSLASH	0xDC
#define VK_RBRACKET	0xDD
#define VK_APOSTROPHE	0xDE
#define VK_BACKTICK	0xDF
#define REPEATED_KEYMASK	(1<<30)
#define EXTENDED_KEYMASK	(1<<24)


static int mswin_translate_key(WPARAM wparam, LPARAM lparam)
{
	static short keymap[512];
	static int inited = 0;
	int keycode = -1, i = 0;
	if (inited == 0) {
		for (i = 0; i < 512; i++) keymap[i] = IKEY_UNKNOWN;
		keymap[VK_BACK] = IKEY_BACKSPACE;
		keymap[VK_TAB] = IKEY_TAB;
		keymap[VK_CLEAR] = IKEY_CLEAR;
		keymap[VK_RETURN] = IKEY_RETURN;
		keymap[VK_PAUSE] = IKEY_PAUSE;
		keymap[VK_ESCAPE] = IKEY_ESCAPE;
		keymap[VK_SPACE] = IKEY_SPACE;
		keymap[VK_APOSTROPHE] = IKEY_QUOTE;
		keymap[VK_COMMA] = IKEY_COMMA;
		keymap[VK_MINUS] = IKEY_MINUS;
		keymap[VK_PERIOD] = IKEY_PERIOD;
		keymap[VK_SLASH] = IKEY_SLASH;
		keymap[VK_SEMICOLON] = IKEY_SEMICOLON;
		keymap[VK_EQUALS] = IKEY_EQUALS;
		keymap[VK_LBRACKET] = IKEY_LEFTBRACKET;
		keymap[VK_BACKSLASH] = IKEY_BACKSLASH;
		keymap[VK_RBRACKET] = IKEY_RIGHTBRACKET;
		keymap[VK_GRAVE] = IKEY_BACKQUOTE;
		keymap[VK_BACKTICK] = IKEY_BACKQUOTE;
		keymap[VK_DECIMAL] = IKEY_KP_PERIOD;
		keymap[VK_DIVIDE] = IKEY_KP_DIVIDE;
		keymap[VK_MULTIPLY] = IKEY_KP_MULTIPLY;
		keymap[VK_SUBTRACT] = IKEY_KP_MINUS;
		keymap[VK_ADD] = IKEY_KP_PLUS;
		keymap[VK_UP] = IKEY_UP;
		keymap[VK_DOWN] = IKEY_DOWN;
		keymap[VK_RIGHT] = IKEY_RIGHT;
		keymap[VK_LEFT] = IKEY_LEFT;
		keymap[VK_INSERT] = IKEY_INSERT;
		keymap[VK_HOME] = IKEY_HOME;
		keymap[VK_END] = IKEY_END;
		keymap[VK_PRIOR] = IKEY_PAGEUP;
		keymap[VK_NEXT] = IKEY_PAGEDOWN;
		keymap[VK_NUMLOCK] = IKEY_NUMLOCK;
		keymap[VK_CAPITAL] = IKEY_CAPSLOCK;
		keymap[VK_SCROLL] = IKEY_SCROLLOCK;
		keymap[VK_RSHIFT] = IKEY_RSHIFT;
		keymap[VK_LSHIFT] = IKEY_LSHIFT;
		keymap[VK_RCONTROL] = IKEY_RCTRL;
		keymap[VK_LCONTROL] = IKEY_LCTRL;
		keymap[VK_RMENU] = IKEY_RALT;
		keymap[VK_LMENU] = IKEY_LALT;
		keymap[VK_RWIN] = IKEY_RSUPER;
		keymap[VK_LWIN] = IKEY_LSUPER;
		keymap[VK_HELP] = IKEY_HELP;
		#ifdef VK_PRINT
		keymap[VK_PRINT] = IKEY_PRINT;
		#endif
		keymap[VK_SNAPSHOT] = IKEY_PRINT;
		keymap[VK_CANCEL] = IKEY_BREAK;
		keymap[VK_APPS] = IKEY_MENU;
		#define IMAPKEY(s, d) keymap[(s)] = (d)
		#define ITRANSK(x) IMAPKEY(VK_##x, IKEY_##x)
		for (i = 0; i <= 25; i++) IMAPKEY('A' + i, IKEY_A + i);
		for (i = 0; i <= 9; i++) {
			IMAPKEY('0' + i, IKEY_0 + i);
			IMAPKEY(VK_NUMPAD0 + i, IKEY_KP0 + i);
		}
		for (i = 0; i < 12; i++) IMAPKEY(VK_F1 + i, IKEY_F1 + i);
		#undef ITRANSK
		#undef IMAPKEY
		keymap[11] = IKEY_LCTRL;
		inited = 1;
	}
	keycode = keymap[wparam & 511];
	return keycode;
}

static LRESULT mswin_message_handle(void *current, HWND hWnd, 
	UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int shift1 = 0;
	static int shift2 = 0;
	int keycode;
	int posted;
	switch (msg) {
		case WM_ACTIVATEAPP: {
			int active;
			active = (wParam && (GetForegroundWindow() == hWnd))? 1 : 0;
			if (ikitwin_private_active) 
				posted = ikitwin_private_active(active, 
					IKIT_ACTIVE_FOCUSINPUT | IKIT_ACTIVE_FOCUSMOUSE);
		}
		break;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			if (lParam & REPEATED_KEYMASK) return 0;
			switch (wParam) {
			case VK_CONTROL:
				if (lParam & EXTENDED_KEYMASK) {
					wParam = VK_RCONTROL;
				}	else {
					wParam = VK_LCONTROL;
				}
				break;
			case VK_SHIFT:
				if (shift1 == 0 && (GetKeyState(VK_LSHIFT) & 0x8000)) {
					wParam = VK_LSHIFT;
					shift1 = 1;
				}
				else if (!shift2 && (GetKeyState(VK_RSHIFT) & 0x8000)) {
					wParam = VK_RSHIFT;
					shift2 = 1;
				}
				break;
			case VK_MENU:
				if (lParam & EXTENDED_KEYMASK) {
					wParam = VK_RMENU;
				}	else {
					wParam = VK_LMENU;
				}
				break;
			}
			keycode = mswin_translate_key(wParam, lParam);
			ikitwin_keymap[keycode] = 1;
			if (ikitwin_private_keyboard)
				posted = ikitwin_private_keyboard(IKIT_KEYDOWN, keycode);
		}
		return 0;

		case WM_SYSKEYUP:
		case WM_KEYUP: {
			switch (wParam)
			{
			case VK_CONTROL:
				if (lParam & EXTENDED_KEYMASK) {
					wParam = VK_RCONTROL;
				}	else {
					wParam = VK_LCONTROL;
				}
				break;
			case VK_SHIFT:
				if (shift1 && (GetKeyState(VK_LSHIFT) & 0x8000)) {
					wParam = VK_LSHIFT;
					shift1 = 0;
				}
				else if (shift2 && (GetKeyState(VK_RSHIFT) & 0x8000)) {
					wParam = VK_RSHIFT;
					shift2 = 0;
				}
				break;
			case VK_MENU:
				if (lParam & EXTENDED_KEYMASK) {
					wParam = VK_RMENU;
				}	else {
					wParam = VK_LMENU;
				}
				break;
			}
			keycode = mswin_translate_key(wParam, lParam);
			ikitwin_keymap[keycode] = 0;
			if (ikitwin_private_keyboard)
				posted = ikitwin_private_keyboard(IKIT_KEYUP, keycode);
		}
		return 0;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

static int mswin_dispatch(IVideoPrivateWin *self)
{
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (GetMessage(&msg, NULL, 0, 0)) {
			DispatchMessage(&msg);
			return 0;
		}
	}
	return -1;
}

static void mswin_WinPaint(void *current, HDC hdc, HWND hwnd)
{
	IVideoPrivateWin *self = (IVideoPrivateWin*)current;
	BitBlt(hdc, 0, 0, self->w, self->h, self->hDC, 0, 0, SRCCOPY);
}

static int mswin_init(IVideoPrivateWin *self, const char *windowid)
{
	memset(self, 0, sizeof(IVideoPrivateWin));

	if (windowid == NULL) self->windowid = NULL;
	else {
		self->windowid = (char*)malloc(strlen(windowid) + 1);
		memcpy(self->windowid, windowid, strlen(windowid) + 1);
	}
	
	ikitwin_screen_w = GetSystemMetrics(SM_CXSCREEN);
	ikitwin_screen_h = GetSystemMetrics(SM_CYSCREEN);

	self->screen_w = ikitwin_screen_w;
	self->screen_h = ikitwin_screen_h;

	if (mswin_create_window(self) != 0) {
		return -1;
	}

	self->depth = mswin_get_depth();
	ikitwin_screen_bpp = self->depth;
	
	switch (self->depth) {
	case 15:
		self->rmask = 0x00007c00;
		self->gmask = 0x000003e0;
		self->bmask = 0x0000001f;
		break;
	case 16:
		self->rmask = 0x0000f800;
		self->gmask = 0x000007e0;
		self->bmask = 0x0000001f;
		break;
	case 24:
	case 32:
		self->rmask = 0x00ff0000;
		self->gmask = 0x0000ff00;
		self->bmask = 0x000000ff;
		break;
	}

	if (self->depth <= 8) {
		LOGPALETTE *palette;
		int ncolors;
		HDC hdc;
		ncolors = 1 << self->depth;
		palette = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) * ncolors +
			sizeof(*palette));
		if (palette == NULL) {
			mswin_destroy_window(self);
			return -2;
		}
		palette->palVersion = 0x300;
		palette->palNumEntries = ncolors;
		hdc = GetDC(self->hWnd);
		GetSystemPaletteEntries(hdc, 0, ncolors, palette->palPalEntry);
		ReleaseDC(self->hWnd, hdc);
		self->hPalette = CreatePalette(palette);
		self->pal = palette;
	}	else {
		self->hPalette = NULL;
	}
	
	self->pixel = NULL;
	ikitwin_closing = 0;
	ikitwin_private = self;
	mswin_handle_message = mswin_message_handle;
	WIN_WinPaint = mswin_WinPaint;
	return 0;
}

static void mswin_release_mode(IVideoPrivateWin *self) 
{
	if (self->hDC) {
		DeleteDC(self->hDC);
		self->hDC = NULL;
	}

	if (self->hBitmap) {
		DeleteObject(self->hBitmap);
		self->hBitmap = NULL;
	}

	if (self->info) {
		free(self->info);
		self->info = NULL;
	}

	if (self->hWnd) {
		ShowWindow(self->hWnd, SW_HIDE);
	}
}

static int 
mswin_set_mode(IVideoPrivateWin *self, int w, int h, int bpp, int f)
{
	BITMAPINFO *info;
	HDC hdc;
	RECT bounds;
	void *pixel;
	int x, y;

	if (bpp < 8) bpp = mswin_get_depth();

	mswin_release_mode(self);

	ikitwin_winw = w;
	ikitwin_winh = h;
	self->w = w;
	self->h = h;
	self->pitch = (((bpp + 7) / 8) * w + 3) & ~3;
	self->depth = bpp;
	
	switch (self->depth) {
	case 15:
		self->rmask = 0x00007c00;
		self->gmask = 0x000003e0;
		self->bmask = 0x0000001f;
		break;
	case 16:
		self->rmask = 0x0000f800;
		self->gmask = 0x000007e0;
		self->bmask = 0x0000001f;
		break;
	case 24:
	case 32:
		self->rmask = 0x00ff0000;
		self->gmask = 0x0000ff00;
		self->bmask = 0x000000ff;
		break;
	}

	if (self->hPalette || self->depth == 8) {
		info = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 
			((1 << self->depth) + 4) * sizeof(RGBQUAD));
	}	else {
		info = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + sizeof(RGBQUAD) * 4);
	}

	if (info == NULL) {
		return -1;
	}

	self->info = info;

	info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth = self->w;
	info->bmiHeader.biHeight = -self->h;
	info->bmiHeader.biPlanes = 1;
	info->bmiHeader.biBitCount = self->depth;
	info->bmiHeader.biCompression = BI_RGB;
	info->bmiHeader.biSizeImage = self->pitch * self->h;
	info->bmiHeader.biXPelsPerMeter = 0;
	info->bmiHeader.biYPelsPerMeter = 0;
	info->bmiHeader.biClrUsed = 0;
	info->bmiHeader.biClrImportant = 0;

	if (bpp == 16 || bpp == 15 || bpp == 32) {
		int *data = (int*)info;
		data[10] = self->rmask;
		data[11] = self->gmask;
		data[12] = self->bmask;
		data[13] = 0;
		info->bmiHeader.biCompression = BI_BITFIELDS;
	}

	if (self->hPalette || self->depth <= 8) {
		long offset = sizeof(BITMAPINFOHEADER);
		int i;
		if (info->bmiHeader.biCompression) offset += 12;
		self->palette = (LPRGBQUAD)((char*)info + offset);
		for (i = 0; i < 256; i++) {
			self->palette[i].rgbBlue = i;
			self->palette[i].rgbGreen = i;
			self->palette[i].rgbRed = i;
			self->palette[i].rgbReserved = 0;
		}
	}	else {
		self->palette = NULL;
	}

	hdc = GetDC(self->hWnd);
	self->hBitmap = CreateDIBSection(hdc, info, DIB_RGB_COLORS,
		(void**)&pixel, NULL, 0);

	self->pixel = (unsigned char*)pixel;

	ReleaseDC(self->hWnd, hdc);

	if (self->hBitmap == NULL) {
		return -2;
	}

	if (self->windowid == NULL) {
		int width, height;
		ikitwin_resizing = 1;
		bounds.left = 0;
		bounds.top = 0;
		bounds.right = self->w;
		bounds.bottom = self->h;
		AdjustWindowRect(&bounds, GetWindowLong(self->hWnd, GWL_STYLE), 0);
		width = bounds.right - bounds.left;
		height = bounds.bottom - bounds.top;
		x = (ikitwin_screen_w - width) / 2;
		y = (ikitwin_screen_h - height) / 2;
		if (y < 0) {
			y -= GetSystemMetrics(SM_CYCAPTION) / 2;
		}
		SetWindowPos(self->hWnd, NULL, x, y, width, height, 
			(SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
		ikitwin_resizing = 0;
		SetForegroundWindow(self->hWnd);
	}

	hdc = GetDC(self->hWnd);
	self->hDC = CreateCompatibleDC(hdc);
	ReleaseDC(self->hWnd, hdc);

	if (self->hDC == NULL) {
		mswin_release_mode(self);
		return -3;
	}

	SelectObject(self->hDC, self->hBitmap);

	return 0;
}

static int mswin_update_image(IVideoPrivateWin *self, const int *rect, int n)
{
	HDC hdc;
	int i;
	hdc = GetDC(self->hWnd);
	for (i = 0; i < n; i++, rect += 4) {
		int x = rect[0];
		int y = rect[1];
		int w = rect[2];
		int h = rect[3];
		if (w == 0 || h == 0) continue;
		BitBlt(hdc, x, y, w, h, self->hDC, x, y, SRCCOPY);
	}
	ReleaseDC(self->hWnd, hdc);
	return 0;
}

static void mswin_quit(IVideoPrivateWin *self)
{
	mswin_release_mode(self);
	if (self->hPalette) {
		DeleteObject(self->hPalette);
		self->hPalette = NULL;
	}
	if (self->hWnd) {
		mswin_destroy_window(self);
		self->hWnd = NULL;
	}
}

static int mswin_lock_image(IVideoPrivateWin *self)
{
	return 0;
}

static int mswin_unlock_image(IVideoPrivateWin *self)
{
	return 0;
}

static int mswin_set_caption(IVideoPrivateWin *self, const char *text)
{
#ifdef _WIN32_WCE
	int nlen = strlen(text) + 1;
	LPWSTR lpszW = malloc(nlen * 2);
	MultiByteToWideChar(CP_ACP, 0, text, -1, lpszW, nlen);
	SetWindowTextW(self->hWnd, lpszW);
	free(lpszW);
#else
	SetWindowTextA(self->hWnd, text);
#endif
	return 0;
}

#endif


//=====================================================================
// 统一接口
//=====================================================================
#ifdef __unix
IVideoPrivateX11 iprivate_video;
#else
IVideoPrivateWin iprivate_video;
#endif

int iprivate_inited = 0;
int iprivate_modeset = 0;


int ikitwin_init(void)
{
	char *windowid = ikitwin_windowid[0]? ikitwin_windowid : NULL;
	int retval;
	#ifdef __unix
	retval = x11_init(&iprivate_video, windowid);
	#else
	retval = mswin_init(&iprivate_video, windowid);
	#endif
	if (retval != 0) return retval;
	iprivate_inited = 1;
	return 0;
}

void ikitwin_quit(void)
{
	if (iprivate_inited == 0) 
		return;
	if (iprivate_modeset != 0)
		ikitwin_release_mode();
	#ifdef __unix
	x11_quit(&iprivate_video);
	#else
	mswin_quit(&iprivate_video);
	#endif
	iprivate_inited = 0;
}

int ikitwin_set_mode(int w, int h, int bpp, int flag)
{
	int retval;
	if (iprivate_inited == 0) 
		return -100;
	if (iprivate_modeset) {
		ikitwin_release_mode();
	}
	#ifdef __unix
	retval = x11_set_mode(&iprivate_video, w, h, bpp, flag);
	#else
	retval = mswin_set_mode(&iprivate_video, w, h, bpp, flag);
	#endif
	if (retval != 0) 
		return retval;
	iprivate_modeset = 1;
	return 0;
}

void ikitwin_release_mode(void)
{
	if (iprivate_inited == 0) 
		return;
	if (iprivate_modeset == 0)
		return;
	#ifdef __unix
	x11_release_mode(&iprivate_video);
	#else
	mswin_release_mode(&iprivate_video);
	#endif
	iprivate_modeset = 0;
}

int ikitwin_dispatch(void)
{
	#ifdef __unix
	while (x11_dispatch(&iprivate_video) == 0);
	#else
	while (mswin_dispatch(&iprivate_video) == 0);
	#endif
	if (ikitwin_closing) return -1;
	return 0;
}

int ikitwin_set_caption(const char *title)
{
	#ifdef __unix
	return x11_set_caption(&iprivate_video, title);
	#else
	return mswin_set_caption(&iprivate_video, title);
	#endif
}

int ikitwin_update(const int *rect, int n)
{
	int fullwindow[4];
	if (iprivate_inited == 0)
		return -100;
	fullwindow[0] = 0;
	fullwindow[1] = 0;
	fullwindow[2] = iprivate_video.w;
	fullwindow[3] = iprivate_video.h;
	if (rect == NULL) {
		rect = fullwindow;
		if (n > 1) n = 1;
	}
	#ifdef __unix
	x11_update_image(&iprivate_video, rect, n);
	#else
	mswin_update_image(&iprivate_video, rect, n);
	#endif
	return 0;
}

int ikitwin_lock(void **pixel, long *pitch)
{
	int retval;
	#ifdef __unix
	retval = x11_lock_image(&iprivate_video);
	#else
	retval = mswin_lock_image(&iprivate_video);
	#endif
	if (retval == 0) {
		if (pixel) *pixel = iprivate_video.pixel;
		if (pitch) *pitch = iprivate_video.pitch;
	}
	return retval;
}

void ikitwin_unlock(void)
{
	#ifdef __unix
	x11_unlock_image(&iprivate_video);
	#else
	mswin_unlock_image(&iprivate_video);
	#endif
}

int ikitwin_depth(int *rmask, int *gmask, int *bmask)
{
	if (rmask) *rmask = iprivate_video.rmask;
	if (gmask) *gmask = iprivate_video.gmask;
	if (bmask) *bmask = iprivate_video.bmask;
	return iprivate_video.depth;
}

void ikitwin_sleep(long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}

void ikitwin_timeofday(long *sec, long *usec)
{
	#if defined(_WIN32) 
	static unsigned long inited = 0, addsec;
	static unsigned long lasttick = 0;
	static CRITICAL_SECTION mutex;
	unsigned long _cvalue;
	#if defined(_MSC_VER) || defined(__BORLANDC__)
	static __int64 hitime = 0;
	__int64 current;
	#else
	static long long hitime = 0;
	long long current;
	#endif
	if (inited == 0) {
		lasttick = timeGetTime();
		addsec = (unsigned long)time(NULL);
		addsec = addsec - lasttick / 1000;
		InitializeCriticalSection(&mutex);
		inited = 1;
	}
	_cvalue = timeGetTime();
	EnterCriticalSection(&mutex);
	if (_cvalue < lasttick) {
		hitime += 0x80000000ul;
		lasttick = _cvalue;
		hitime += 0x80000000ul;
	}
	LeaveCriticalSection(&mutex);
	current = hitime | _cvalue;
	if (sec) *sec = (long)(current / 1000) + addsec;
	if (usec) *usec = (long)(current % 1000) * 1000;
	#else
	static struct timezone tz = { 0, 0 };
	struct timeval time;
	gettimeofday(&time, &tz);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#endif
}


IINT64 ikitwin_clock(void)
{
	long sec, usec;
	ikitwin_timeofday(&sec, &usec);
	return ((IINT64)sec) * 1000 + usec / 1000;
}


float ikitwin_fps(void)
{
	static float result = 0.0f;
	static float fps = 0.0f;
	static IINT64 old_time = 0;
	static IINT64 new_time;
	int i;
	fps += 1.0f;
	new_time = ikitwin_clock();
	if (new_time - old_time > 10000) {
		old_time = new_time;
		result = 0.0f;
	}
	if (new_time - old_time >= 1000) {
		for (i = 0; new_time - old_time >= 1000; i++, old_time += 1000);
		if (i == 0) i = 1;
		result = fps / i;
		fps = 0;
	}
	return result;
}

int ikitwin_tick(int fps)
{
	static IINT64 old = 0, now = 0;
	static int savedfps = 0;
	IINT64 current, start;
	current = ikitwin_clock();
	now = current * fps;
	if (savedfps != fps || now - old >= 5000 * fps) {
		savedfps = fps;
		old = current * fps;
	}
	start = current;
	while (1) {
		long sleep;
		if (now - old >= 1000) {
			old += 1000;
			break;
		}
		sleep = (long)((1000 + old - now) / fps);
		ikitwin_sleep(sleep);
		current = ikitwin_clock();
		now = current * fps;
	}
	return (int)(ikitwin_clock() - start);
}


void ikitwin_info(int *screenw, int *screenh, int *screenbpp)
{
	if (screenw) *screenw = ikitwin_screen_w;
	if (screenh) *screenh = ikitwin_screen_h;
	if (screenbpp) *screenbpp = ikitwin_screen_bpp;
}

