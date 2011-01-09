/**********************************************************************
 *
 * ibitmap.h - the basic definition of the bitmap
 *
 * IBITMAP is designed under the desire that it can exactly describe  
 * one basic bitmap, which is considered as simple and pure.
 *
 * there are five basic bitmap operating interfaces:
 *
 * create  - create a new bitmap and return the struct address
 * release - free the bitmap 
 * blit    - copy the speciafied rectangle from one bitmap to another
 * setmask - set the color key for transparent blit (IBLIT_MASK on)
 * fill    - fill a rectange in the bitmap
 *
 * the history of this file:
 * Feb.  7 2003    skywind    created including create/release/blit
 * Dec. 15 2004    skywind    add filling
 *
 * aim to keep it simply and stupid, this file will only provide the 
 * most basic bitmap operation interfaces. pixel format declearation,
 * color components modification must not be included in it.
 *
 * aim to platform independence, all of the sources are writen in ansi 
 * c but you can replace the default blitter by ibitmap_funcset() to 
 * optimized the benchmark of ibitmap_blit in the speciafied platform
 *
 **********************************************************************/

#ifndef __IBITMAP_H__
#define __IBITMAP_H__


/**********************************************************************
 * IBITMAP DECLEARATION
 **********************************************************************/

struct IBITMAP
{
    unsigned long w;         /* width of the bitmap   */
    unsigned long h;         /* height of the bitmap  */
    unsigned long bpp;       /* color depth of bitmap */
    unsigned long pitch;     /* pitch of the bitmap   */
    unsigned long mask;      /* bitmap bit flags      */
    unsigned long mode;      /* additional mode data  */
    unsigned long code;      /* bitmap class code     */
    void *pixel;             /* pixels data in bitmap */
    void *extra;             /* extra data structure  */
    void **line;             /* pointer to each line in bitmap */
};



/**********************************************************************
 * Bitmap Global Interface
 **********************************************************************/
#ifndef IBLIT_CLIP
#define IBLIT_CLIP    1    /* blit mode: enable clip */
#endif

#ifndef IBLIT_MASK
#define IBLIT_MASK    2    /* blit mode: enable transparent blit */
#endif

#ifndef IBLIT_HFLIP
#define IBLIT_HFLIP   4    /* flip horizon */
#endif

#ifndef IBLIT_VFLIP
#define IBLIT_VFLIP   8    /* flip vertical */
#endif


#ifdef  __cplusplus
extern "C" {
#endif



/**********************************************************************
 * Bitmap Function Definition
 **********************************************************************/

/*
 * ibitmap_create - create bitmap and return the pointer to struct IBITMAP
 * width  - bitmap width
 * height - bitmap height
 * bpp    - bitmap color depth (8, 16, 24, 32)
 */
struct IBITMAP *ibitmap_create(int width, int height, int bpp);


/*
 * ibitmap_release - release bitmap
 * bmp - pointer to the IBITMAP struct
 */
void ibitmap_release(struct IBITMAP *bmp);


/*
 * ibitmap_clip - clip the rectangle from the src clip and dst clip then
 * caculate a new rectangle which is shared between dst and src cliprect:
 * clipdst  - dest clip array (left, top, right, bottom)
 * clipsrc  - source clip array (left, top, right, bottom)
 * (x, y)   - dest position
 * rectsrc  - source rect
 * mode     - check IBLIT_HFLIP or IBLIT_VFLIP
 * return zero for successful, return non-zero if there is no shared part
 */
int ibitmap_clip(const int *clipdst, const int *clipsrc, int *x, int *y,
    int *rectsrc, int mode);


/*
 * ibitmap_blit - blit from source bitmap to dest bitmap
 * returns zero for successful, others for error    
 * dst       - dest bitmap to draw on
 * x, y      - target position of dest bitmap to draw on
 * src       - source bitmap 
 * sx, sy    - source rectangle position in source bitmap
 * w, h      - source rectangle width and height in source bitmap
 * mode      - flags of IBLIT_CLIP, IBLIT_MASK, IBLIT_HFLIP, IBLIT_VFLIP...
 */
int ibitmap_blit(struct IBITMAP *dst, int x, int y, 
    const struct IBITMAP *src, int sx, int sy, int w, int h, int mode);


/*
 * ibitmap_setmask - change mask(colorkey) of the bitmap
 * when blit with IBLIT_MASK, this value can be used as the key color to 
 * transparent. you can change bmp->mask directly without calling it.
 */
int ibitmap_setmask(struct IBITMAP *bmp, unsigned long mask);


/* ibitmap_fill - fill the rectangle with given color 
 * returns zero for successful, others for error
 * dst     - dest bitmap to draw on
 * dx, dy  - target position of dest bitmap to draw on
 * w, h    - width and height of the rectangle to be filled
 * col     - indicate the color to fill the rectangle
 */
int ibitmap_fill(struct IBITMAP *dst, int dx, int dy, int w, int h,
    unsigned long col, int noclip);


/* ibitmap_stretch - copies a bitmap from a source rectangle into a 
 * destination rectangle, stretching or compressing the bitmap to fit 
 * the dimensions of the destination rectangle
 * returns zero for successful, others for invalid rectangle
 * dst       - dest bitmap to draw on
 * dx, dy    - target rectangle position of dest bitmap to draw on
 * dw, dh    - target rectangle width and height in dest bitmap
 * src       - source bitmap 
 * sx, sy    - source rectangle position in source bitmap
 * sw, sh    - source rectangle width and height in source bitmap
 * mode      - flags of IBLIT_MASK, IBLIT_HFLIP, IBLIT_VFLIP...
 * it uses bresenham like algorithm instead of fixed point or indexing 
 * to avoid integer size overflow and memory allocation, just use it 
 * when you don't have a stretch function.
 */
int ibitmap_stretch(struct IBITMAP *dst, int dx, int dy, int dw, int dh,
    const struct IBITMAP *src, int sx, int sy, int sw, int sh, int mode);


/**********************************************************************
 * Bitmap Basic Interface
 **********************************************************************/
typedef int (*ibitmap_blitter_norm)(char *, long, const char *, long, 
              int, int, int, long);
typedef int (*ibitmap_blitter_mask)(char *, long, const char *, long,
              int, int, int, long, unsigned long);

typedef int (*ibitmap_blitter_flip)(char *, long, const char *, long,
              int, int, int, long, unsigned long, int);

typedef int (*ibitmap_filler)(char *, long, int, int, int, unsigned long);


#define IBITMAP_BLITER_NORM  0
#define IBITMAP_BLITER_MASK  1
#define IBITMAP_BLITER_FLIP  2

#define IBITMAP_FILLER   3

#define IBITMAP_MALLOC   4
#define IBITMAP_FREE     5



/*
 * ibitmap_funcset - set basic bitmap functions, returns zero for successful
 * mode - function index, there are two blitters, (0/1 normal/transparent) 
 * proc - extra function pointer, set it to null to use default method:
 *   int imyblitter0(char *dst, long pitch1, const char *src, long pitch2,
 *       int w, int h, int pixelbyte, long linesize);
 *   int imyblitter1(char *dst, long pitch1, const char *src, long pitch2
 *       int w, int h, int pixelbyte, unsigned long mask, long linesize);
 * there are two blitters, normal blitter and transparent blitter, which 
 * ibitmap_blit() will call to complete blit operation. 
 */
int ibitmap_funcset(int functionid, const void *proc);


/*
 * ibitmap_funcget - get the blitter function pointer, returns the address
 * of the blitter function, and returns NULL for error. for more information
 * please see the comment of ibitmap_funcset.
 */
void *ibitmap_funcget(int functionid, int version);


#ifdef  __cplusplus
}
#endif


#endif



