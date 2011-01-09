/**********************************************************************
 *
 * ibitmap.c - the basic definition of the bitmap
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

#include "ibitmap.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/**********************************************************************
 * MEMORY ALLOCATOR FUNCTIONS
 **********************************************************************/
void*(*icmalloc)(size_t size) = malloc;
void (*icfree)(void *ptr) = free;


/**********************************************************************
 * BASIC BITMAP FUNCTIONS
 **********************************************************************/

/*
 * ibitmap_create - create bitmap and return the pointer to struct IBITMAP
 * width  - bitmap width
 * height - bitmap height
 * bpp    - bitmap color depth (8, 16, 24, 32)
 */
struct IBITMAP *ibitmap_create(int w, int h, int bpp)
{
    struct IBITMAP *bmp;
    unsigned char*line;
    int pixelbyte = (bpp + 7) >> 3;
    int i;

    /* check for invalid parametes */
    assert(bpp >= 8 && bpp <=32 && w > 0 && h > 0);

    /* allocate memory for the IBITMAP struct */
    bmp = (struct IBITMAP*)icmalloc(sizeof(struct IBITMAP));
    if (bmp == NULL) return NULL;

    /* setup bmp struct */
    bmp->w = w;
    bmp->h= h;
    bmp->bpp = bpp;
    bmp->pitch = (((long)w * pixelbyte + 7) >> 3) << 3;
    bmp->code = 0;

    /* allocate memory for pixel data */
    bmp->pixel = (char*)icmalloc(bmp->pitch * h);

    if (bmp->pixel == NULL) {
        icfree(bmp);
        return NULL;
    }

    bmp->mode = 0;
    bmp->mask = 0;
    bmp->extra = NULL;

    /* caculate the offset of each scanline */
    bmp->line = (void**)icmalloc(sizeof(void*) * h);
    if (bmp->line == NULL) {
        icfree(bmp->pixel);
        icfree(bmp);
        return NULL;
    }

    line = (unsigned char*)bmp->pixel;
    for (i = 0; i < h; i++, line += bmp->pitch) 
        bmp->line[i] = line;

    return bmp;
}

/*
 * ibitmap_release - release bitmap
 * bmp - pointer to the IBITMAP struct
 */
void ibitmap_release(struct IBITMAP *bmp)
{
    assert(bmp);
    if (bmp->pixel) icfree(bmp->pixel);
    bmp->pixel = NULL;
    if (bmp->line) icfree(bmp->line);
    bmp->line = NULL;
    bmp->w = 0;
    bmp->h = 0;
    bmp->pitch = 0;
    bmp->bpp = 0;
    icfree(bmp);
}


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
    int *rectsrc, int mode)
{
    int dcl = clipdst[0];       /* dest clip: left     */
    int dct = clipdst[1];       /* dest clip: top      */
    int dcr = clipdst[2];       /* dest clip: right    */
    int dcb = clipdst[3];       /* dest clip: bottom   */
    int scl = clipsrc[0];       /* source clip: left   */
    int sct = clipsrc[1];       /* source clip: top    */
    int scr = clipsrc[2];       /* source clip: right  */
    int scb = clipsrc[3];       /* source clip: bottom */
    int dx = *x;                /* dest x position     */
    int dy = *y;                /* dest y position     */
    int sl = rectsrc[0];        /* source rectangle: left   */
    int st = rectsrc[1];        /* source rectangle: top    */
    int sr = rectsrc[2];        /* source rectangle: right  */
    int sb = rectsrc[3];        /* source rectangle: bottom */
    int hflip, vflip;
    int w, h, d;
    
    hflip = (mode & IBLIT_HFLIP)? 1 : 0;
    vflip = (mode & IBLIT_VFLIP)? 1 : 0;

    if (dcr <= dcl || dcb <= dct || scr <= scl || scb <= sct) 
        return -1;

    if (sr <= scl || sb <= sct || sl >= scr || st >= scb) 
        return -2;

    /* check dest clip: left */
    if (dx < dcl) {
        d = dcl - dx;
        dx = dcl;
        if (!hflip) sl += d;
        else sr -= d;
    }

    /* check dest clip: top */
    if (dy < dct) {
        d = dct - dy;
        dy = dct;
        if (!vflip) st += d;
        else sb -= d;
    }

    w = sr - sl;
    h = sb - st;

    if (w < 0 || h < 0) 
        return -3;

    /* check dest clip: right */
    if (dx + w > dcr) {
        d = dx + w - dcr;
        if (!hflip) sr -= d;
        else sl += d;
    }

    /* check dest clip: bottom */
    if (dy + h > dcb) {
        d = dy + h - dcb;
        if (!vflip) sb -= d;
        else st += d;
    }

    if (sl >= sr || st >= sb) 
        return -4;

    /* check source clip: left */
    if (sl < scl) {
        d = scl - sl;
        sl = scl;
        if (!hflip) dx += d;
    }

    /* check source clip: top */
    if (st < sct) {
        d = sct - st;
        st = sct;
        if (!vflip) dy += d;
    }

    if (sl >= sr || st >= sb) 
        return -5;

    /* check source clip: right */
    if (sr > scr) {
        d = sr - scr;
        sr = scr;
        if (hflip) dx += d;
    }

    /* check source clip: bottom */
    if (sb > scb) {
        d = sb - scb;
        sb = scb;
        if (vflip) dy += d;
    }

    if (sl >= sr || st >= sb) 
        return -6;

    *x = dx;
    *y = dy;

    rectsrc[0] = sl;
    rectsrc[1] = st;
    rectsrc[2] = sr;
    rectsrc[3] = sb;

    return 0;
}


/**********************************************************************
 * BLITTER DEFINITION
 **********************************************************************/
int ibitmap_blitnc(char *dst, long pitch1, const char *src, long pitch2,
    int w, int h, int pixsize, long linesize);
int ibitmap_blitmc(char *dst, long pitch1, const char *src, long pitch2, 
    int w, int h, int pixsize, long linesize, unsigned long mask);
int ibitmap_blitfc(char *dst, long pitch1, const char *src, long pitch2,
    int w, int h, int pixsize, long linesize, unsigned long mask, int flag);


/* set blitters interface to default blitters (c version) */
ibitmap_blitter_norm ibitmap_blitn = ibitmap_blitnc;
ibitmap_blitter_mask ibitmap_blitm = ibitmap_blitmc;
ibitmap_blitter_flip ibitmap_blitf = ibitmap_blitfc;



/**********************************************************************
 * BLITTER INTERFACE - CORE OPERATION
 **********************************************************************/

/*
 * ibitmap_blit - blit from source bitmap to dest bitmap
 * returns zero for successful, others for error    
 * dst       - dest bitmap to draw on
 * x, y      - target position of dest bitmap to draw on
 * src       - source bitmap 
 * sx, sy    - source rectangle position in source bitmap
 * w, h      - source rectangle width and height in source bitmap
 * mode      - blit flag bits (IBLIT_CLIP, IBLIT_MASK)
 */
int ibitmap_blit(struct IBITMAP *dst, int x, int y, 
    const struct IBITMAP *src, int sx, int sy, int w, int h, int mode)
{
    long pitch1, pitch2;
    long linesize, d1, d2;
    int pixsize, r;
    char *pixel1;
    char *pixel2;

    /* check whether parametes is error */
    assert(src && dst);
    assert(src->bpp == dst->bpp);

    /* check whether need to clip rectangle */
    if (mode & IBLIT_CLIP) {
        int clipdst[4], clipsrc[4], rect[4];
        clipdst[0] = 0;
        clipdst[1] = 0;
        clipdst[2] = (int)dst->w;
        clipdst[3] = (int)dst->h;
        clipsrc[0] = 0;
        clipsrc[1] = 0;
        clipsrc[2] = (int)src->w;
        clipsrc[3] = (int)src->h;
        rect[0] = sx;
        rect[1] = sy;
        rect[2] = sx + (int)w;
        rect[3] = sy + (int)h;
        r = ibitmap_clip(clipdst, clipsrc, &x, &y, rect, mode);
        if (r != 0) return 0;
        sx = rect[0];
        sy = rect[1];
        w = rect[2] - rect[0];
        h = rect[3] - rect[1];
    }

    /* get the size of one pixel */
    pixsize = (src->bpp + 7) >> 3;
    pitch1 = dst->pitch;
    pitch2 = src->pitch;

    /* choose linear offset */
    switch (pixsize) {
    case 1: 
        linesize = w; 
        d1 = x; 
        d2 = sx; 
        break;
    case 2: 
        linesize = (w << 1); 
        d1 = (x << 1); 
        d2 = (sx << 1); 
        break;
    case 3:
        linesize = (w << 1) + w;
        d1 = (x << 1) + x;
        d2 = (sx << 1) + sx;
        break;
    case 4:
        linesize = (w << 2);
        d1 = (x << 2);
        d2 = (sx << 2);
        break;
    default:
        linesize = w * pixsize;
        d1 = x * pixsize;
        d2 = sx * pixsize;
        break;
    }

    /* get the first scanlines of two bitmaps */
    pixel1 = (char*)dst->line[y] + d1;
    pixel2 = (char*)src->line[sy] + d2;

    /* detect whether blit with flip */
    if ((mode & (IBLIT_VFLIP | IBLIT_HFLIP)) != 0) {
        if (mode & IBLIT_VFLIP) {
            pixel2 = (char*)src->line[sy + h - 1] + d2;
        }
        r = ibitmap_blitf(pixel1, pitch1, pixel2, pitch2, w, h, 
            pixsize, linesize, src->mask, mode);
        if (r) ibitmap_blitfc(pixel1, pitch1, pixel2, pitch2, w, h,
            pixsize, linesize, src->mask, mode);
        return 0;
    }

    /* check to use the normal blitter or the transparent blitter */
    if ((mode & IBLIT_MASK) == 0) {
        r = ibitmap_blitn(pixel1, pitch1, pixel2, pitch2, w, h, 
            pixsize, linesize);
        if (r) ibitmap_blitnc(pixel1, pitch1, pixel2, pitch2, w, h, 
            pixsize, linesize);
    }   else {
        r = ibitmap_blitm(pixel1, pitch1, pixel2, pitch2, w, h, 
            pixsize, linesize, src->mask);
        if (r) ibitmap_blitmc(pixel1, pitch1, pixel2, pitch2, w, h, 
            pixsize, linesize, src->mask);
    }

    return 0;
}


/**********************************************************************
 * ibitmap_blitnc - default blitter for normal blit (no transparent)
 **********************************************************************/
int ibitmap_blitnc(char *dst, long pitch1, const char *src, long pitch2,
    int w, int h, int pixelbyte, long linesize)
{
    char *ss = (char*)src;
    char *sd = (char*)dst;

    /* avoid never use warnings */
    if (pixelbyte == 1) linesize = w;

    /* copy each line */
    for (; h; h--) 
    {
        memcpy(sd, ss, linesize);
        sd += pitch1;
        ss += pitch2;
    }

    return 0;
}


/**********************************************************************
 * ibitmap_blitmc - default blitter for transparent blit
 **********************************************************************/
int ibitmap_blitmc(char *dst, long pitch1, const char *src, long pitch2,
    int w, int h, int pixelbyte, long linesize, unsigned long mask)
{
    static unsigned long endian = 0x11223344;
    unsigned char *p1, *p2;
    unsigned long key24, c;
    unsigned char key08;
    unsigned short key16;
    unsigned int imask, k;
    long inc1, inc2, i;
    long longsize, isize;

    /* caculate the inc offset of two array */
    inc1 = pitch1 - linesize;
    inc2 = pitch2 - linesize;

    longsize = sizeof(long);
    isize = sizeof(int);

    /* copying pixel for all color depth */
    switch (pixelbyte)
    {
    /* for 8 bits colors depth */
    case 1:
        key08 = (unsigned char)mask;
        for (; h; h--) {                 /* copy each scanline */
            for (i = w; i; i--) {        /* copy each pixel */
                if (*(unsigned char*)src != key08) *dst = *src;
                src++;
                dst++;
            }
            dst += inc1;
            src += inc2;
        }
        break;
    
    /* for 15/16 bits colors depth */
    case 2:        
        key16 = (unsigned short)mask;
        for (; h; h--) {                 /* copy each scanline */
            for (i = w; i; i--)       {  /* copy each pixel */
                if (*(unsigned short*)src != key16) 
                    *(unsigned short*)dst = *(unsigned short*)src;
                src += 2;
                dst += 2;
            }
            dst += inc1;
            src += inc2;
        }
        break;

    /* for 24 bits colors depth */
    case 3:
        if (((unsigned char*)&endian)[0] != 0x44) key24 = mask;
        else key24 = ((mask & 0xFFFF) << 8) | ((mask >> 16) & 0xFF);
        p1 = (unsigned char*)dst;
        p2 = (unsigned char*)src;

        /* 32/16 BIT: detect word-size in runtime instead of using */
        /* macros, in order to be compiled in any unknown platform */
        if (longsize == 4) {        /* 32/16 BIT PLATFORM */
            for (; h; h--) {                 /* copy each scanline */
                for (i = w; i; i--)       {  /* copy each pixel */
                    c = (((unsigned long)(*(unsigned short*)p2)) << 8);
                    if ((c | p2[2]) != key24) {
                        p1[0] = p2[0];
                        p1[1] = p2[1];
                        p1[2] = p2[2];
                    }
                    p1 += 3;
                    p2 += 3;
                }
                p1 += inc1;
                p2 += inc2;
            }
        }
        else if (isize == 4) {    /* 64 BIT PLATFORM */
            imask = (unsigned int)(key24 & 0xffffffff);
            for (; h; h--) {                 /* copy each scanline */
                for (i = w; i; i--)       {  /* copy each pixel */
                    k = (((unsigned int)(*(unsigned short*)p2)) << 8);
                    if ((k | p2[2]) != imask) {
                        p1[0] = p2[0];
                        p1[1] = p2[1];
                        p1[2] = p2[2];
                    }
                    p1 += 3;
                    p2 += 3;
                }
                p1 += inc1;
                p2 += inc2;
            }
        }
        break;

    /* for 32 bits colors depth */
    case 4: 
        /* 32/16 BIT: detect word-size in runtime instead of using */
        /* macros, in order to be compiled in any unknown platform */
        if (longsize == 4) {             /* 32/16 BIT PLATFORM */
            for (; h; h--) {                 /* copy each scanline */
                for (i = w; i; i--)       {  /* copy each pixel */
                    if (*(unsigned long*)src != mask) 
                        *(unsigned long*)dst = *(unsigned long*)src;
                    src += 4;
                    dst += 4;
                }
                dst += inc1;
                src += inc2;
            }
        }    
        else if (isize == 4) {         /* 64 BIT PLATFORM */
            imask = (unsigned int)(mask & 0xffffffff);
            for (; h; h--) {                 /* copy each scanline */
                for (i = w; i; i--)       {  /* copy each pixel */
                    if (*(unsigned int*)src != imask) 
                        *(unsigned int*)dst = *(unsigned int*)src;
                    src += 4;
                    dst += 4;
                }
                dst += inc1;
                src += inc2;
            }
        }
        break;
    }

    return 0;
}


/**********************************************************************
 * ibitmap_blitfc - default blitter for flip blit
 **********************************************************************/
int ibitmap_blitfc(char *dst, long pitch1, const char *src, long pitch2,
    int w, int h, int pixsize, long linesize, unsigned long mask, int flag)
{
    static unsigned long endian = 0x11223344;
    unsigned char *p1, *p2;
    unsigned long key24, c;
    unsigned char key08;
    unsigned short key16;
    unsigned int imask, k;
    long inc1, inc2, i;
    long longsize, isize;

    /* flip vertical without mask */
    if ((flag & (IBLIT_MASK | IBLIT_HFLIP)) == 0) {
        for (; h; h--) {
            memcpy(dst, src, linesize);
            dst += pitch1;
            src -= pitch2;
        }
        return 0;
    }

    /* flip vertical with mask */
    if ((flag & IBLIT_MASK) != 0 && (flag & IBLIT_HFLIP) == 0) {
        for (; h; h--) {
            ibitmap_blitmc(dst, pitch1, src, pitch2, w, 1, pixsize, 
                linesize, mask);
            dst += pitch1;
            src -= pitch2;
        }
        return 0;
    }

    /* flip horizon */
    src += linesize - pixsize;
    inc1 = pitch1 - linesize;

    longsize = sizeof(long);
    isize = sizeof(int);

    /* horizon & vertical or horizon only */
    if (flag & IBLIT_VFLIP) inc2 = - (pitch2 - linesize);
    else inc2 = pitch2 + linesize;

    /* copying pixel for all color depth */
    switch (pixsize)
    {
    /* flip in 8 bits colors depth */
    case 1: 
        key08 = (unsigned char)mask;
        for (; h; h--) {
            if (flag & IBLIT_MASK) {
                for (i = w; i; i--) {      /* copy each pixel with mask */
                    if (*(unsigned char*)src != key08) *dst = *src;
                    dst++;
                    src--;
                }
            }    else {
                for (i = w; i; i--) {      /* copy pixels without mask  */
                    *dst = *src;
                    dst++;
                    src--;
                }
            }
            dst += inc1;
            src += inc2;
        }
        break;

    /* flip in 15/16 bits colors depth */
    case 2:
        key16 = (unsigned short)mask;
        for (; h; h--) {
            if (flag & IBLIT_MASK) {
                for (i = w; i; i--) {      /* copy each pixel with mask */
                    if (*(unsigned short*)src != key16) 
                        *(unsigned short*)dst = *(unsigned short*)src;
                    dst += 2;
                    src -= 2;
                }
            }    else {
                for (i = w; i; i--) {      /* copy pixels without mask  */
                    *(unsigned short*)dst = *(unsigned short*)src;
                    dst += 2;
                    src -= 2;
                }
            }
            dst += inc1;
            src += inc2;
        }
        break;

    /* flip in 24 bits colors depth */
    case 3:
        if (((unsigned char*)&endian)[0] != 0x44) key24 = mask;
        else key24 = ((mask & 0xFFFF) << 8) | ((mask >> 16) & 0xFF);
        p1 = (unsigned char*)dst;
        p2 = (unsigned char*)src;
        for (; h; h--) {                   /* copy each scanline */
            if (flag & IBLIT_MASK) {
                if (longsize == 4) {   /* 32/16 BIT PLATFORM */
                    for (i = w; i; i--) {  /* copy each pixel with mask */
                        c = (((unsigned long)(*(unsigned short*)p2)) << 8);
                        if ((c | p2[2]) != key24) {
                            p1[0] = p2[0];
                            p1[1] = p2[1];
                            p1[2] = p2[2];
                        }
                        p1 += 3;
                        p2 -= 3;
                    }
                }
                else if (isize == 4) {  /* 64 BIT PLATFORM */
                    imask = (unsigned int)(key24 & 0xffffffff);
                    for (i = w; i; i--) {  /* copy each pixel with mask */
                        k = (((unsigned int)(*(unsigned short*)p2)) << 8);
                        if ((k | p2[2]) != imask) {
                            p1[0] = p2[0];
                            p1[1] = p2[1];
                            p1[2] = p2[2];
                        }
                        p1 += 3;
                        p2 -= 3;
                    }
                }
            }    else {                     /* copy pixels without mask */
                for (i = w; i; i--) {
                    p1[0] = p2[0];
                    p1[1] = p2[1];
                    p1[2] = p2[2];
                    p1 += 3;
                    p2 -= 3;
                }
            }
            p1 += inc1;
            p2 += inc2;
        }
        break;

    /* flip in 32 bits colors depth */
    case 4:
        for (; h; h--) {
            if (longsize == 4) {       /* 32/16 BIT PLATFORM */
                if (flag & IBLIT_MASK) {
                    for (i = w; i; i--) {  /* copy each pixel with mask */
                        if (*(unsigned long*)src != mask) 
                            *(unsigned long*)dst = *(unsigned long*)src;
                        dst += 4;
                        src -= 4;
                    }
                }    else {
                    for (i = w; i; i--) {  /* copy pixels without mask  */
                        *(unsigned long*)dst = *(unsigned long*)src;
                        dst += 4;
                        src -= 4;
                    }
                }
            }
            else if (isize == 4) {   /* 64 BIT PLATFORM */
                imask = (unsigned int)(mask & 0xffffffff);
                if (flag & IBLIT_MASK) {
                    for (i = w; i; i--) {  /* copy each pixel with mask */
                        if (*(unsigned int*)src != imask) 
                            *(unsigned int*)dst = *(unsigned int*)src;
                        dst += 4;
                        src -= 4;
                    }
                }    else {
                    for (i = w; i; i--) {  /* copy pixels without mask  */
                        *(unsigned int*)dst = *(unsigned int*)src;
                        dst += 4;
                        src -= 4;
                    }
                }
            }
            dst += inc1;
            src += inc2;
        }
        break;
    }

    return 0;
}



/*
 * ibitmap_setmask - change mask(colorkey) of the bitmap
 * when blit with IBLIT_MASK, this value can be used as the key color to 
 * transparent. you can change bmp->mask directly without calling it.
 */
int ibitmap_setmask(struct IBITMAP *bmp, unsigned long mask)
{
    bmp->mask = mask;
    return 0;
}


/**********************************************************************
 * FILLER DEFINITION
 **********************************************************************/
int ibitmap_fillc(char *dst, long pitch, int w, int h, int pixsize, 
    unsigned long c);

/* set filler interface to default filler (c version) */
ibitmap_filler ibitmap_filling = ibitmap_fillc;


/* ibitmap_fill - fill the rectangle with given color 
 * returns zero for successful, others for error
 * dst     - dest bitmap to draw on
 * dx, dy  - target position of dest bitmap to draw on
 * w, h    - width and height of the rectangle to be filled
 * col     - indicate the color to fill the rectangle
 */
int ibitmap_fill(struct IBITMAP *dst, int dx, int dy, int w, int h,
    unsigned long col, int noclip)
{
    int pixsize, r;
    long delta;
    char *pixel;

    assert(dst);
    if (noclip == 0) {
        if (dx >= (long)dst->w || dx + w <= 0 || w < 0) return 0;
        if (dy >= (long)dst->h || dy + h <= 0 || h < 0) return 0;
        if (dx < 0) w += dx, dx = 0;
        if (dy < 0) h += dy, dy = 0;
        if (dx + w >= (long)dst->w) w = (long)dst->w - dx;
        if (dy + h >= (long)dst->h) h = (long)dst->h - dy;
    }

    /* get pixel size */
    pixsize = (dst->bpp + 7) >> 3;

    /* choose linear offset */
    switch (pixsize) {
    case 1: delta = dx; break;
    case 2: delta = (dx << 1);  break;
    case 3: delta = (dx << 1) + dx; break;
    case 4: delta = (dx << 2); break;
    default: delta = dx * pixsize; break;
    }

    /* get the first scanlines of the bitmap */
    pixel = (char*)dst->line[dy] + delta;

    r = ibitmap_filling(pixel, dst->pitch, w, h, pixsize, col);
    if (r) ibitmap_fillc(pixel, dst->pitch, w, h, pixsize, col);

    return 0;
}


/**********************************************************************
 * ibitmap_blitfc - default blitter for flip blit
 **********************************************************************/
int ibitmap_fillc(char *dst, long pitch, int w, int h, int pixsize, 
    unsigned long col)
{
    static unsigned long endian = 0x11223344;
    unsigned short col16;
    unsigned char col8, c1, c2, c3;
    unsigned long *p32;
    unsigned int k, *k32;
    long longsize, isize;
    long inc, i;

    longsize = sizeof(long);
    isize = sizeof(int);

    switch (pixsize)
    {
    /* fill for 8 bits color depth */
    case 1:
        col8 = (unsigned char)(col & 0xff);
        for (; h; h--) {
            memset(dst, col8, w);
            dst += pitch;
        }
        break;

    /* fill for 15/16 bits color depth */
    case 2:
        col16 = (unsigned short)(col & 0xffff);
        col = (col << 16) | (col & 0xffff);
        if (longsize == 4) {        /* 32/16 BIT PLATFORM */
            for (; h; h--) {
                p32 = (unsigned long*)dst;
                for (i = w >> 1; i; i--) *p32++ = col;
                if (w & 1) *(unsigned short*)p32 = col16;
                dst += pitch;
            }
        }
        else if (isize == 4) {    /* 64 BIT PLATFORM */
            k = (unsigned int)(col & 0xffffffff);
            for (; h; h--) {
                k32 = (unsigned int*)dst;
                for (i = w >> 1; i; i--) *k32++ = k;
                if (w & 1) *(unsigned short*)k32 = col16;
                dst += pitch;
            }
        }
        break;
    
    /* fill for 24 bits color depth */
    case 3:
        inc = pitch - ((w << 1) + w);
        if (((unsigned char*)&endian)[0] != 0x44) {
            c1 = (unsigned char)((col >> 16) & 0xff);
            c2 = (unsigned char)((col >> 8) & 0xff);
            c3 = (unsigned char)(col & 0xff);
        }    else {
            c1 = (unsigned char)(col & 0xff);
            c2 = (unsigned char)((col >> 8) & 0xff);
            c3 = (unsigned char)((col >> 16) & 0xff);
        }
        for (; h; h--) {
            for (i = w; i; i--) {
                *dst++ = c1;
                *dst++ = c2;
                *dst++ = c3;
            }
            dst += inc;
        }
        break;

    /* fill for 32 bits color depth */
    case 4:
        if (longsize == 4) {        /* 32/16 BIT PLATFORM */
            for (; h; h--) {
                p32 = (unsigned long*)dst;
                for (i = w; i; i--) *p32++ = col;
                dst += pitch;
            }
        }
        else if (isize == 4) {    /* 64 BIT PLATFORM */
            k = (unsigned int)(col & 0xffffffff);
            for (; h; h--) {
                k32 = (unsigned int*)dst;
                for (i = w; i; i--) *k32++ = k;
                dst += pitch;
            }
        }
        break;
    }

    return 0;
}


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
    const struct IBITMAP *src, int sx, int sy, int sw, int sh, int mode)
{
    int dstwidth, dstheight, dstwidth2, dstheight2, srcwidth2, srcheight2;
    int werr, herr, incx, incy, i, j, nbytes; 
    static unsigned long endian = 0x11223344;
    unsigned long mask, key24;
    long longsize, isize;

    /* check whether parametes is error */
    assert(src && dst);
    assert(src->bpp == dst->bpp);

    if (src->bpp != dst->bpp)
        return -10;

    if (dw == sw && dh == sh) {
        mode |= IBLIT_CLIP;
        return ibitmap_blit(dst, dx, dy, src, sx, sy, sw, sh, mode);
    }

    if (dx < 0 || dx + dw > (int)dst->w || dy < 0 || dy + dh > (int)dst->h ||
        sx < 0 || sx + sw > (int)src->w || sy < 0 || sy + sh > (int)src->h ||
        sh <= 0 || sw <= 0 || dh <= 0 || dw <= 0) 
        return -20;

    dstwidth = dw;
    dstheight = dh;
    dstwidth2 = dw * 2;
    dstheight2 = dh * 2;
    srcwidth2 = sw * 2;
    srcheight2 = sh * 2;

    if (mode & IBLIT_VFLIP) sy = sy + sh - 1, incy = -1;
    else incy = 1;

    herr = srcheight2 - dstheight2;
    nbytes = (src->bpp + 7) / 8;

    isize = sizeof(int);
    longsize = sizeof(long);
    mask = (unsigned long)src->mask;

    for (j = 0; j < dstheight; j++) {
        const unsigned char *srcrow = (const unsigned char*)src->line[sy];
        unsigned char *dstrow = (unsigned char*)dst->line[dy];
        const unsigned char *srcpix = srcrow + nbytes * sx;
        unsigned char *dstpix = dstrow + nbytes * dx;
        incx = nbytes;
        if (mode & IBLIT_HFLIP) {
            srcpix += (sw - 1) * nbytes;
            incx = -nbytes;
        }
        werr = srcwidth2 - dstwidth2;

        switch (nbytes)
        {
        case 1:
            {
                unsigned char mask8;
                if ((mode & IBLIT_MASK) == 0) {
                    for (i = dstwidth; i > 0; i--) {
                        *dstpix++ = *srcpix;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }   else {
                    mask8 = (unsigned char)(src->mask & 0xff);
                    for (i = dstwidth; i > 0; i--) {
                        if (*srcpix != mask8) *dstpix = *srcpix;
                        dstpix++;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }
            }
            break;

        case 2:
            {
                unsigned short mask16;
                if ((mode & IBLIT_MASK) == 0) {
                    for (i = dstwidth; i > 0; i--) {
                        *((unsigned short*)dstpix) = 
                                            *((unsigned short*)srcpix);
                        dstpix += 2;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }   else {
                    mask16 = (unsigned short)(src->mask & 0xffff);
                    for (i = dstwidth; i > 0; i--) {
                        if (*((unsigned short*)srcpix) != mask16) 
                            *((unsigned short*)dstpix) = 
                                            *((unsigned short*)srcpix);
                        dstpix += 2;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }
            }
            break;
        
        case 3:
            if (((unsigned char*)&endian)[0] != 0x44) key24 = mask;
            else key24 = ((mask & 0xFFFF) << 8) | ((mask >> 16) & 0xFF);
            if ((mode & IBLIT_MASK) == 0) {
                for (i = dstwidth; i > 0; i--) {
                    dstpix[0] = srcpix[0];
                    dstpix[1] = srcpix[1];
                    dstpix[2] = srcpix[2];
                    dstpix += 3;
                    while (werr >= 0) {
                        srcpix += incx, werr -= dstwidth2;
                    }
                    werr += srcwidth2;
                }
            }
            else if (longsize == 4) {
                unsigned long longmask, k;
                longmask = key24 & 0xffffff;
                for (i = dstwidth; i > 0; i--) {
                    k = (((unsigned long)(*(unsigned short*)srcpix)) << 8);
                    if ((k | srcpix[2]) != longmask) {
                        dstpix[0] = srcpix[0];
                        dstpix[1] = srcpix[1];
                        dstpix[2] = srcpix[2];
                    }
                    dstpix += 3;
                    while (werr >= 0) {
                        srcpix += incx, werr -= dstwidth2;
                    }
                    werr += srcwidth2;
                }
            }
            else if (isize == 4) {
                unsigned int imask, k;
                imask = key24 & 0xffffff;
                for (i = dstwidth; i > 0; i--) {
                    k = (((unsigned int)(*(unsigned short*)srcpix)) << 8);
                    if ((k | srcpix[2]) != imask) {
                        dstpix[0] = srcpix[0];
                        dstpix[1] = srcpix[1];
                        dstpix[2] = srcpix[2];
                    }
                    dstpix += 3;
                    while (werr >= 0) {
                        srcpix += incx, werr -= dstwidth2;
                    }
                    werr += srcwidth2;
                }
            }
            break;

        case 4:
            if (longsize == 4) {
                unsigned long masklong;
                if ((mode & IBLIT_MASK) == 0) {
                    for (i = dstwidth; i > 0; i--) {
                        *((unsigned long*)dstpix) = 
                                            *((unsigned long*)srcpix);
                        dstpix += 4;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }   else {
                    masklong = (unsigned long)(src->mask);
                    for (i = dstwidth; i > 0; i--) {
                        if (*((unsigned long*)srcpix) != masklong) 
                            *((unsigned long*)dstpix) = 
                                            *((unsigned long*)srcpix);
                        dstpix += 4;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }
            }
            else if (isize == 4) {
                unsigned int maskint;
                if ((mode & IBLIT_MASK) == 0) {
                    for (i = dstwidth; i > 0; i--) {
                        *((unsigned int*)dstpix) = *((unsigned int*)srcpix);
                        dstpix += 4;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }   else {
                    maskint = (unsigned int)(src->mask);
                    for (i = dstwidth; i > 0; i--) {
                        if (*((unsigned int*)srcpix) != maskint) 
                            *((unsigned int*)dstpix) = 
                                            *((unsigned int*)srcpix);
                        dstpix += 4;
                        while (werr >= 0) {
                            srcpix += incx, werr -= dstwidth2;
                        }
                        werr += srcwidth2;
                    }
                }
            }    
            else {
                assert(0);
            }
            break;
        }

        while (herr >= 0) {
            sy += incy, herr -= dstheight2;
        }

        herr += srcheight2; 
        dy++;
    }

    return 0;
}



/*
 * ibitmap_funcset - set basic bitmap functions, returns zero for successful
 * mode - function index, there are two blitters, (0/1 normal/transparent) 
 * proc - extra function pointer, set it to null to use default method
 * there are two blitters, normal blitter and transparent blitter, which 
 * ibitmap_blit will call to complete blit operation. see ibitmapm.c 
 */
int ibitmap_funcset(int functionid, const void *proc)
{
    typedef void*(*icmalloc_t)(size_t);
    typedef void (*icfree_t)(void*ptr);

    switch (functionid)
    {
    case IBITMAP_BLITER_NORM:
        ibitmap_blitn = (proc == NULL)? ibitmap_blitnc : 
                (ibitmap_blitter_norm)proc;
        break;
    case IBITMAP_BLITER_MASK:
        ibitmap_blitm = (proc == NULL)? ibitmap_blitmc : 
                (ibitmap_blitter_mask)proc;
        break;
    case IBITMAP_BLITER_FLIP:
        ibitmap_blitf = (proc == NULL)? ibitmap_blitfc : 
                (ibitmap_blitter_flip)proc;
        break;
    case IBITMAP_FILLER:
        ibitmap_filling = (proc == NULL)? ibitmap_fillc :
                (ibitmap_filler)proc;
        break;
    case IBITMAP_MALLOC:
        icmalloc = (proc == NULL)? malloc : (icmalloc_t)proc;
        break;
    case IBITMAP_FREE:
        icfree = (proc == NULL)? free : (icfree_t)proc;
        break;
    default:
        assert(0);
        break;
    }
    return 0;
}


/*
 * ibitmap_funcget - get the blitter function pointer, returns the address
 * of the blitter function, and returns NULL for error. for more information
 * please see the comment of ibitmap_funcset.
 */
void *ibitmap_funcget(int functionid, int version)
{
    void *proc = NULL;
    switch (functionid)
    {
    case IBITMAP_BLITER_NORM: 
        proc = (version)? (void*)ibitmap_blitnc : (void*)ibitmap_blitn; 
        break;
    case IBITMAP_BLITER_MASK: 
        proc = (version)? (void*)ibitmap_blitmc : (void*)ibitmap_blitm; 
        break;
    case IBITMAP_BLITER_FLIP: 
        proc = (version)? (void*)ibitmap_blitfc : (void*)ibitmap_blitf; 
        break;
    case IBITMAP_FILLER:
        proc = (version)? (void*)ibitmap_fillc : (void*)ibitmap_filling;
        break;
    }
    return proc;
}


