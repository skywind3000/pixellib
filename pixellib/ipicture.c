//=====================================================================
//
// ipicture.c - general picture file operation
//
// FEATURES:
// * I/O stream support
// * save and load tga/bmp/gif
//
// NOTE: 
// require ibitmap.h, ibmbits.h, ibmcols.h
// for more information, please see the readme file
//
//=====================================================================

#include "ibmbits.h"
#include "ibmcols.h"
#include "ipicture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>



//=====================================================================
//
// PICTURE DATA INTERFACE
//
// IMDIO - Media Data Input/Output Interface
// PIXELS   - Pixel Operation for Bitmaps
// FIXUP    - Pixel Format Fixing Up Interface
//
//=====================================================================

//---------------------------------------------------------------------
// Media Data Input/Output Interface
//---------------------------------------------------------------------
int is_getc(IMDIO *stream)
{
	int ch;
	if (stream->_ungetc >= 0) {
		ch = (int)stream->_ungetc;
		stream->_ungetc = -1;
	}	else {
		ch = (stream->getc)(stream);
		if (ch >= 0) stream->_cnt++;
	}
	return ch;
}

int is_putc(IMDIO *stream, int c)
{
	int ch;
	ch = (stream->putc)(stream, c);
	if (ch >= 0) stream->_cnt++;
	return ch;
}

int is_ungetc(IMDIO *stream, int c)
{
	if (stream->_ungetc >= 0) return -1;
	stream->_ungetc = (long)c;
	return c;
}

long is_reader(IMDIO *stream, void *buffer, long size)
{
	unsigned char *lptr = (unsigned char*)buffer;
	long total = 0;
	int ch;

	for (ch = 0; total < size; total++) {
		ch = is_getc(stream);
		if (ch < 0) break;
		*lptr++ = (unsigned char)ch;
	}
	if (total == 0) return -1;
	return total;
}

long is_writer(IMDIO *stream, const void *buffer, long size)
{	
	unsigned char *lptr = (unsigned char*)buffer;
	long total = 0;
	int ch;

	for (ch = 0; total < size; total++) {
		ch = *lptr++; 
		ch = is_putc(stream, ch);
		if (ch < 0) break;
	}
	if (total == 0) return -1;
	return total;
}

int is_igetw(IMDIO *stream)
{
	int c1, c2;
	if ((c1 = is_getc(stream)) < 0) return -1;
	if ((c2 = is_getc(stream)) < 0) return -1;
	return c1 | (c2 << 8);
}

int is_iputw(IMDIO *stream, int w)
{
	int c1 = (w & 0xFF), c2 = ((w >> 8) & 0xFF);
	if (is_putc(stream, c1) < 0) return -1;
	if (is_putc(stream, c2) < 0) return -1;
	return w;
}

long is_igetl(IMDIO *stream)
{
	long c1, c2, c3, c4;
	if ((c1 = (long)is_getc(stream)) < 0) return -1;
	if ((c2 = (long)is_getc(stream)) < 0) return -1;
	if ((c3 = (long)is_getc(stream)) < 0) return -1;
	if ((c4 = (long)is_getc(stream)) < 0) return -1;
	return (c1 | (c2 << 8) | (c3 << 16) | (c4 << 24));
}

long is_iputl(IMDIO *stream, long l)
{
	long c1, c2, c3, c4;
	c1 = (l & 0x000000FF) >> 0;
	c2 = (l & 0x0000FF00) >> 8;
	c3 = (l & 0x00FF0000) >> 16;
	c4 = (l & 0xFF000000) >> 24;
	if (is_putc(stream, c1) < 0) return -1;
	if (is_putc(stream, c2) < 0) return -1;
	if (is_putc(stream, c3) < 0) return -1;
	if (is_putc(stream, c4) < 0) return -1;
	return l; 
}

int is_mgetw(IMDIO *stream)
{
	int c1, c2;
	if ((c1 = is_getc(stream)) < 0) return -1;
	if ((c2 = is_getc(stream)) < 0) return -1;
	return (c1 << 8) | c2;
}

int is_mputw(IMDIO *stream, int w)
{
	int c1 = (w >> 8) & 0xFF, c2 = (w & 0xFF);
	if (is_putc(stream, c1) < 0) return -1;
	if (is_putc(stream, c2) < 0) return -1;
	return w;
}

long is_mgetl(IMDIO *stream)
{
	long c1, c2, c3, c4;
	if ((c1 = (long)is_getc(stream)) < 0) return -1;
	if ((c2 = (long)is_getc(stream)) < 0) return -1;
	if ((c3 = (long)is_getc(stream)) < 0) return -1;
	if ((c4 = (long)is_getc(stream)) < 0) return -1;
	return ((c1 << 24) | (c2 << 16) | (c3 << 8) | c4);
}

long is_mputl(IMDIO *stream, long l)
{
	long c1, c2, c3, c4;
	c1 = (l & 0xFF000000) >> 24;
	c2 = (l & 0x00FF0000) >> 16;
	c3 = (l & 0x0000FF00) >> 8;
	c4 = (l & 0x000000FF);
	if (is_putc(stream, c1) < 0) return -1;
	if (is_putc(stream, c2) < 0) return -1;
	if (is_putc(stream, c3) < 0) return -1;
	if (is_putc(stream, c4) < 0) return -1;
	return l;
}

long is_getbits(IMDIO *stream, int count, int litend)
{
	long data, bitc, bitd, mask, c, i;
	bitc = stream->_bitc;
	bitd = stream->_bitd;
	for (data = 0, i = 0; count > 0; ) {
		if (bitc == 0) {
			bitd = is_getc(stream);
			bitc = 8;
			if (bitd < 0) {
				bitd = 0;
				bitc = 0;
				data = -1;
				break;
			}
		}
		c = (count >= bitc) ? bitc : count;
		if (litend == 0) {
			data <<= c;
			data |= (bitd >> (8 - c)) & 0xff;
			bitd = (bitd << c) & 0xff;
		}	else {
			mask = (1L << c) - 1;
			data |= (bitd & mask) << i;
			bitd >>= c;
			i += c;
		}
		count -= c;
		bitc -= c;
	}
	stream->_bitc = bitc;
	stream->_bitd = bitd;
	return data;
}

long is_putbits(IMDIO *stream, long data, int count, int litend)
{
	long bitc, bitd, mask, c;
	bitc = stream->_bitc;
	bitd = stream->_bitd;
	for (; count > 0; ) {
		c = 8 - bitc;
		if (count < c) c = count;
		if (litend == 0) {
			mask = (data >> (count - c)) & 0xff;
			bitd |= (mask << (8 - c)) >> bitc;
		}	else {
			mask = ((1L << c) - 1) & data;
			data >>= c;
			bitd |= mask << bitc;
		}
		bitc += c;
		count -= c;
		if (bitc >= 8) {
			if (is_putc(stream, bitd & 0xff) < 0) {
				return -1;
			}
			bitc = 0;
			bitd = 0;
		}
	}
	stream->_bitc = bitc;
	stream->_bitd = bitd;
	return 0;
}

void is_flushbits(IMDIO *stream, int isflushwrite, int litendian)
{
	if (isflushwrite) {
		if (stream->_bitc != 0) {
			is_putbits(stream, 0, 8 - stream->_bitc, litendian);
		}
	}
	stream->_bitc = 0;
	stream->_bitd = 0;
}

long is_igetbits(IMDIO *stream, int count)
{
	return is_getbits(stream, count, 1);
}

long is_mgetbits(IMDIO *stream, int count)
{
	return is_getbits(stream, count, 0);
}

long is_iputbits(IMDIO *stream, long data, int count)
{
	return is_putbits(stream, data, count, 1);
}

long is_mputbits(IMDIO *stream, long data, int count)
{
	return is_putbits(stream, data, count, 0);
}

void is_iflushbits(IMDIO *stream, int isflushwrite)
{
	is_flushbits(stream, isflushwrite, 1);
}

void is_mflushbits(IMDIO *stream, int isflushwrite)
{
	is_flushbits(stream, isflushwrite, 0);
}

void is_seekcur(IMDIO *stream, long skip)
{
	for (; skip > 0; skip--) is_getc(stream);
}


//---------------------------------------------------------------------
// IBITMAP PIXEL OPERATION
//---------------------------------------------------------------------
#define _is_bmline(bmp, y) ((unsigned char*)((bmp)->line[y]))

static inline IUINT32 _is_get3b(const void *p)
{
	return _ipixel_fetch_24(p, 0);
}

static inline IUINT32 _is_put3b(void *p, IUINT32 c)
{
	_ipixel_store_24(p, 0, c);
	return c;
}

#define _IS_HEADER1() do { \
	if (bmp == NULL) return 0;  \
	if (x<0 || y<0 || x>=(int)bmp->w || y>=(int)bmp->h) \
		return 0; \
}	while (0)

static inline IUINT32 _is_get8(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	return _is_bmline(bmp, y)[x];
}

static inline IUINT32 _is_get15(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	return ((IUINT16*)_is_bmline(bmp, y))[x];
}

static inline IUINT32 _is_get16(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	return ((IUINT16*)_is_bmline(bmp, y))[x];
}

static inline IUINT32 _is_get24(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	return _is_get3b(_is_bmline(bmp, y) + (x) * 3);
}

static inline IUINT32 _is_get32(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	return ((IUINT32*)_is_bmline(bmp, y))[x];
}

static inline IUINT32 _is_put8(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	_is_bmline(bmp, y)[x] = (unsigned char)(c & 0xFF);
	return c;
}

static inline IUINT32 _is_put15(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	((IUINT16*)_is_bmline(bmp, y))[x] = (IUINT16)(c & 0xFFFF);
	return c;
}

static inline IUINT32 _is_put16(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	((IUINT16*)_is_bmline(bmp, y))[x] = (IUINT16)(c & 0xFFFF);
	return c;
}

static inline IUINT32 _is_put24(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	_is_put3b(_is_bmline(bmp, y) + (x) * 3, c);
	return c;
}

static inline IUINT32 _is_put32(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	((IUINT32*)_is_bmline(bmp, y))[x] = c;
	return c;
}

static inline IUINT32 _is_getpx(const struct IBITMAP *bmp, int x, int y)
{
	_IS_HEADER1();
	if (bmp->bpp == 8) return _is_get8(bmp, x, y);
	else if (bmp->bpp == 15) return _is_get15(bmp, x, y);
	else if (bmp->bpp == 16) return _is_get16(bmp, x, y);
	else if (bmp->bpp == 24) return _is_get24(bmp, x, y);
	else if (bmp->bpp == 32) return _is_get32(bmp, x, y);
	return 0;
}

static inline IUINT32 _is_putpx(struct IBITMAP *bmp, int x, int y, IUINT32 c)
{
	_IS_HEADER1();
	if (bmp->bpp == 8) return _is_put8(bmp, x, y, c);
	else if (bmp->bpp == 15) return _is_put15(bmp, x, y, c);
	else if (bmp->bpp == 16) return _is_put16(bmp, x, y, c);
	else if (bmp->bpp == 24) return _is_put24(bmp, x, y, c);
	else if (bmp->bpp == 32) return _is_put32(bmp, x, y, c);
	return 0;
}

int _ibitmap_guess_pixfmt(const IBITMAP *bmp)
{
	int fmt;
	fmt = ibitmap_pixfmt_get(bmp);
	if (fmt < 0) {
		if (bmp->bpp == 8) return IPIX_FMT_C8;
		if (bmp->bpp == 15) return IPIX_FMT_X1R5G5B5;
		if (bmp->bpp == 16) return IPIX_FMT_R5G6B5;
		if (bmp->bpp == 24) return IPIX_FMT_R8G8B8;
		if (bmp->bpp == 32) return IPIX_FMT_A8R8G8B8;
	}
	return fmt;
}

IUINT32 _im_color_get(int fmt, IUINT32 c, const IRGB *pal)
{
	IINT32 r, g, b, a;
	if (ipixelfmt[fmt].type == IPIX_FMT_TYPE_INDEX) {
		return IRGBA_TO_A8R8G8B8(pal[c].r, pal[c].g, pal[c].b, 255);
	}
	IRGBA_DISEMBLE(fmt, c, r, g, b, a);
	return IRGBA_TO_A8R8G8B8(r, g, b, a);
}


#undef _IS_HEADER1

//---------------------------------------------------------------------
// local definition
//---------------------------------------------------------------------
typedef struct IBITMAP ibitmap_t;
long _is_perrno = 0;

void _is_perrno_set(long v) { _is_perrno = v; }
long _is_perrno_get(void) { return _is_perrno; }

//---------------------------------------------------------------------
// stream operation interface
//---------------------------------------------------------------------
#define IRWCHECK(s, v) do { if ((s)->_code != (v)) return -1; } while(0)
static int iproc_getc_mem(struct IMDIO *stream)
{
	IRWCHECK(stream, 0);
	if (stream->_pos < stream->_len)
		return ((unsigned char*)(stream->data))[stream->_pos++];
	return -1;
}

static int iproc_putc_mem(struct IMDIO *stream, int c)
{
	unsigned char ch = (unsigned char)c;
	IRWCHECK(stream, 0);
	if (stream->_pos == stream->_len) return -1;
	else if (stream->_pos < stream->_len) {
		((unsigned char*)(stream->data))[stream->_pos++] = ch;
	}
	if (stream->_pos > stream->_len) stream->_len = stream->_pos;
	return c;
}

static int iproc_getc_file(struct IMDIO *stream)
{
	IRWCHECK(stream, 1);
	return fgetc((FILE*)(stream->data));
}

static int iproc_putc_file(struct IMDIO *stream, int c)
{
	IRWCHECK(stream, 1);
	return fputc(c, (FILE*)(stream->data));
}

int is_init_mem(IMDIO *stream, const void *lptr, long size)
{
	stream->_code = 0;
	stream->_pos = 0;
	stream->_len = size;
	stream->_bitc = 0;
	stream->_bitd = 0;
	stream->_ungetc = -1;
	stream->data = (void*)lptr;
	stream->getc = iproc_getc_mem;
	stream->putc = iproc_putc_mem;
	return 0;
}

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

int is_open_file(IMDIO *stream, const char *filename, const char *rw)
{
	stream->_code = -1;
	stream->data = fopen(filename, rw);
	if (stream->data == NULL) return -1;
	stream->_code = 1;
	stream->_bitc = 0;
	stream->_bitd = 0;
	stream->_ungetc = -1;
	stream->getc = iproc_getc_file;
	stream->putc = iproc_putc_file;
	return 0;
}

int is_close_file(IMDIO *stream)
{
	IRWCHECK(stream, 1);
	fclose((FILE*)(stream->data));
	return 0;
}


void *is_read_marked_block(IMDIO *stream, const unsigned char *mark,
	int marksize, size_t *nbytes_readed)
{
	static const unsigned char dmark[2] = { 0, 0 };
	unsigned char *data;
	unsigned char *buffer;
	size_t block = 0;
	size_t nsize = 0;
	int nbuffered = 0;
	int index = 0;

	if (stream == NULL) 
		return NULL;

	buffer = (unsigned char*)malloc(marksize + 10);

	if (buffer == NULL) 
		return NULL;

	data = (unsigned char*)malloc(256);
	block = 256;

	if (data == NULL) {
		free(buffer);
		return NULL;
	}

	if (mark == NULL) mark = dmark;

	while (1) {
		int ch;
		ch = is_getc(stream);
		if (ch < 0) break;
		buffer[index] = (unsigned char)ch;
		data[nsize++] = (unsigned char)ch;

		if (nsize >= block) {
			unsigned char *tmp = (unsigned char*)malloc(block * 2);
			if (tmp != NULL) {
				memcpy(tmp, data, nsize);
				free(data);
				data = tmp;
				block = block * 2;
			}	else {
				nsize = 0;
			}
		}

		if (++nbuffered >= marksize && mark && marksize > 0) {
			int i, j, k;
			for (i = index, j = marksize - 1, k = 0; j >= 0; j--, k++) {
				if (buffer[i] != mark[j]) break;
				if (--i < 0) i = marksize - 1;
			}
			if (k == marksize) break;
		}
		if (++index >= marksize) index = 0;
	}

	free(buffer);

	buffer = NULL;
	if (nsize > 0) {
		buffer = (unsigned char*)malloc(nsize);
	}

	if (buffer == NULL || nsize == 0) {
		free(data);
		if (nbytes_readed) *nbytes_readed = 0;
		return NULL;
	}

	memcpy(buffer, data, nsize);
	if (nbytes_readed) *nbytes_readed = nsize;
	free(data);

	return buffer;
}



//=====================================================================
//
// bmp - Windows/OS2 IBITMAP Format
//
// decoding program - idcolor.h
// 
//=====================================================================
#define IBI_RGB          0
#define IBI_RLE8         1
#define IBI_RLE4         2
#define IBI_BITFIELDS    3

#define IOS2INFOHEADERSIZE  12
#define IWININFOHEADERSIZE  40


typedef struct _IBITMAPFILEHEADER
{
	IUINT32  bfType;
	IUINT32  bfSize;
	IUINT16 bfReserved1;
	IUINT16 bfReserved2;
	IUINT32  bfOffBits;
}	IBITMAPFILEHEADER;


typedef struct _IBITMAPINFOHEADER
{
	IUINT32  biWidth;
	IUINT32  biHeight;
	IUINT16 biBitCount;
	IUINT32  biCompression;
}	IBITMAPINFOHEADER;


//---------------------------------------------------------------------
// bmp - read_bmfileheader
//---------------------------------------------------------------------
static int iread_bmfileheader(IMDIO *stream, IBITMAPFILEHEADER *fileheader)
{
	fileheader->bfType = is_igetw(stream);
	fileheader->bfSize= is_igetl(stream);
	fileheader->bfReserved1= is_igetw(stream);
	fileheader->bfReserved2= is_igetw(stream);
	fileheader->bfOffBits= is_igetl(stream);

	if (fileheader->bfType != 0x4D42) return -1;

	return 0;
}

//---------------------------------------------------------------------
// bmp - read_bminfoheader
//---------------------------------------------------------------------
static int iread_bminfoheader(IMDIO *stream, IBITMAPINFOHEADER *infoheader, 
	int ihsize)
{
	long data[20];

	if (ihsize == IWININFOHEADERSIZE) {
		data[0] = is_igetl(stream);		/* IUINT32  biWidth......... */
		data[1] = is_igetl(stream);		/* IUINT32  biHeight........ */
		data[2] = is_igetw(stream);		/* IUINT16 biPlanes........ */
		data[3] = is_igetw(stream);		/* IUINT16 biBitCount...... */
		data[4] = is_igetl(stream);		/* IUINT32  biCompression... */
		data[5] = is_igetl(stream);		/* IUINT32  biSizeImage..... */
		data[6] = is_igetl(stream);		/* IUINT32  biXPelsPerMeters */
		data[7] = is_igetl(stream);		/* IUINT32  biYPelsPerMeters */
		data[8] = is_igetl(stream);		/* IUINT32  biClrUsed....... */
		data[9] = is_igetl(stream);		/* IUINT32  biClrImportant.. */

		infoheader->biWidth = data[0];
		infoheader->biHeight = data[1];
		infoheader->biBitCount = (IUINT16)data[3];
		infoheader->biCompression = data[4];
	}	
	else 
	if (ihsize == IOS2INFOHEADERSIZE) {
		data[0] = is_igetw(stream);		/* IUINT16 biWidth......... */
		data[1] = is_igetw(stream);		/* IUINT16 biHeight........ */
		data[2] = is_igetw(stream);		/* IUINT16 biPlanes........ */
		data[3] = is_igetw(stream);		/* IUINT16 biBitCount...... */

		infoheader->biWidth = data[0];
		infoheader->biHeight= data[1];
		infoheader->biBitCount = (IUINT16)data[3];
		infoheader->biCompression = 0;
	}
	else {
		return -1;
	}

	return 0;
}

//---------------------------------------------------------------------
// bmp - read_bmicolors
//---------------------------------------------------------------------
static void ibmp_read_bmicolors(int ncols, IRGB *pal, IMDIO *stream, 
	int win_flag)
{
	int i;
	for (i = 0; i < ncols; i++) {
		pal[i].b = is_getc(stream);
		pal[i].g = is_getc(stream);
		pal[i].r = is_getc(stream);
		if (win_flag) is_getc(stream);
	}
}

//---------------------------------------------------------------------
// bmp - read_image
//---------------------------------------------------------------------
static void ibmp_read_image(IMDIO *stream, ibitmap_t *bmp, const 
	IBITMAPINFOHEADER *infoheader)
{
	long line, start, length;
	long i, j, k, a, nbytes;
	IUINT32 n;
	unsigned char b[32];
	IRGB c;

	for (start = 0; start < (long)infoheader->biHeight; start++) {
		line = infoheader->biHeight - start - 1;
		length = (long)infoheader->biWidth;

		switch (infoheader->biBitCount) {

		case 1:
			for (i = 0; i < length; i++) {
				j = i % 32;
				if (j == 0) {
					n = is_mgetl(stream);
					for (k = 0; k < 32; k++) {
						b[31 - k] = (char)(n & 1);
						n = n >> 1;
					}
				}
				_is_put8(bmp, i, line, b[j]);
			}
			break;

		case 4:
			for (i = 0; i < length; i++) {
				j = i % 8;
				if (j == 0) {
					n = is_igetl(stream);
					for (k = 0; k < 4; k++) {
						b[k * 2 + 1] = (IUINT8)(((n & 0xFF) >> 0) & 0x0F);
						b[k * 2 + 0] = (IUINT8)(((n & 0xFF) >> 4) & 0x0F);
						n = n >> 8;
					}
				}
				_is_put8(bmp, i, line, b[j]);
			}
			break;

		case 8:
			for (i = 0; i < length; i++) {
				_is_put8(bmp, i, line, is_getc(stream));
			}
			break;

		case 24:
			for (nbytes = 0, i = 0; i < length; i++, nbytes += 3) {
				IUINT32 cc;
				c.b = is_getc(stream);
				c.g = is_getc(stream);
				c.r = is_getc(stream);
				cc = IRGBA_TO_R8G8B8(c.r, c.g, c.b, 255);
				_is_put24(bmp, i, line, cc);
			}
			nbytes = nbytes % 4;
			for (i = nbytes; nbytes != 0 && i < 4; i++) is_getc(stream);
			break;

		case 32:
			for (i = 0; i < length; i++) {
				c.b = is_getc(stream);
				c.g = is_getc(stream);
				c.r = is_getc(stream);
				a = is_getc(stream);
				if (bmp->bpp == 32) {
					IUINT32 cc = IRGBA_TO_A8R8G8B8(c.r, c.g, c.b, a);
					_is_put32(bmp, i, line, cc);
				}	else {
					IUINT32 cc = IRGBA_TO_A8R8G8B8(c.r, c.g, c.b, a);
					_is_put24(bmp, i, line, cc);
				}
			}
			break;
		}
	}
}

//---------------------------------------------------------------------
// bmp - read_bitfields_image
//---------------------------------------------------------------------
static void ibmp_read_bitfields_image(IMDIO *stream, ibitmap_t *bmp, 
	IBITMAPINFOHEADER *infoheader)
{
	long i, j;
	long bpp;
	int bytes_per_pixel;
	int red, grn, blu;
	IUINT32 buffer;

	bpp = bmp->bpp;
	bytes_per_pixel = (bpp + 7) / 8;

	for (j = (long)infoheader->biHeight - 1; j >= 0; j--) {
		for (i = 0; i < (long)infoheader->biWidth; i++) {

			buffer = 0;
			if (bytes_per_pixel == 1) buffer = is_getc(stream);
			else if (bytes_per_pixel == 2) buffer = is_igetw(stream);
			else if (bytes_per_pixel == 3) {
				buffer = is_getc(stream);
				buffer |= is_getc(stream) << 8;
				buffer |= is_getc(stream) << 16;
			}	
			else if (bytes_per_pixel == 4) {
				buffer = (IUINT32)is_igetl(stream);
			}

			if (bpp == 15) {
				red = (buffer >> 10) & 0x1f;
				grn = (buffer >> 5) & 0x1f;
				blu = (buffer) & 0x1f;
				buffer =	(red << 10) |
							(grn << 5) |
							(blu << 0);
			}
			else if (bpp == 16) {
				red = (buffer >> 11) & 0x1f;
				grn = (buffer >> 5) & 0x3f;
				blu = (buffer) & 0x1f;
				buffer =	(red << 11) |
							(grn << 5) |
							(blu << 0);
			}
			else {
				red = (buffer >> 16) & 0xff;
				grn = (buffer >> 8) & 0xff;
				blu = (buffer) & 0xff;
				buffer =	(red << 24) |
							(grn << 16) |
							(blu << 0);
			}

			if (bytes_per_pixel == 1) _is_put8(bmp, i, j, buffer);
			else if (bytes_per_pixel == 2) _is_put16(bmp, i, j, buffer);
			else if (bytes_per_pixel == 3) _is_put24(bmp, i, j, buffer);
			else _is_put32(bmp, i, j, buffer);
		}
	}
}

//---------------------------------------------------------------------
// bmp - read_rle_image
//---------------------------------------------------------------------
static void ibmp_read_rle_image(IMDIO *stream, ibitmap_t *bmp, 
	IBITMAPINFOHEADER *infoheader)
{
	unsigned char count, val, val0;
	unsigned char b[8];
	long line, j, k, pos, bpp;
	long eolflag, eopicflag;

	line = infoheader->biHeight - 1;
	bpp = (infoheader->biCompression == IBI_RLE8)? 8 : 4;
	val = 0;

	for (eopicflag = 0; eopicflag == 0; ) {
		for (eolflag = 0, pos = 0; eolflag == 0 && eopicflag == 0; ) {
			count = is_getc(stream);
			val0 = is_getc(stream);
			if (count > 0) {
				b[1] = val0 & 15;
				b[0] = (val0 >> 4) & 15;
				for (j = 0; j < count; j++, pos++) {
					val = (bpp == 8)? val0 : b[j % 2];
					_is_put8(bmp, pos, line, val);
				}
			}	
			else {
				if (val == 0) eolflag = 1;
				else if (val == 1) eopicflag = 1;
				else if (val == 2) {
					pos += is_getc(stream);
					line -= is_getc(stream);
				}	else {
					for (j = 0; j < val; j++, pos++) {
						if (bpp == 8) val = is_getc(stream);
						else {
							if ((j % 4) == 0) {
								val0 = is_igetw(stream);
								for (k = 0; k < 2; k++) {
									b[2 * k + 1] = val0 & 15;
									val0 = val0 >> 4;
									b[2 * k] = val0 & 15;
									val0 = val0 >> 4;
								}
							}
							val = b[j % 4];
						}
						_is_put8(bmp, pos, line, val); 
					}
					if (j % 2 == 1 && bpp == 8) val0 = is_getc(stream);
				}
			}
			if (pos >= (long)infoheader->biWidth) eolflag = 1;
		}
		if (--line < 0) eopicflag = 1;
	}
}

//---------------------------------------------------------------------
// bmp - iload_bmp_stream
//---------------------------------------------------------------------
struct IBITMAP *iload_bmp_stream(IMDIO *stream, IRGB *pal)
{
	IBITMAPFILEHEADER fileheader;
	IBITMAPINFOHEADER infoheader;
	struct IBITMAP *bmp;
	long want_palette = 1;
	long ncol;
	IUINT32 biSize;
	int bpp, dest_depth;
	IRGB tmppal[256];

	assert(stream);

	_is_perrno_set(0);

	if (!pal) {
		want_palette = 0;
		pal = tmppal;
	}

	if (iread_bmfileheader(stream, &fileheader) != 0) {
		_is_perrno_set(1);
		return NULL;
	}

	biSize = is_igetl(stream);
	
	if (iread_bminfoheader(stream, &infoheader, biSize) != 0) {
		_is_perrno_set(2);
		return NULL;
	}

	if (biSize == IWININFOHEADERSIZE) {
		ncol = (fileheader.bfOffBits - 54) / 4;
		if (infoheader.biCompression != IBI_BITFIELDS)
			ibmp_read_bmicolors(ncol, pal, stream, 1);
	}
	else if (biSize == IOS2INFOHEADERSIZE) {
		ncol = (fileheader.bfOffBits - 26) / 3;
		if (infoheader.biCompression != IBI_BITFIELDS)
			ibmp_read_bmicolors(ncol, pal, stream, 0);
	}	else {
		_is_perrno_set(3);
		return NULL;
	}

	if (infoheader.biBitCount == 24) bpp = 24;
	else if (infoheader.biBitCount == 16) bpp = 16;
	else if (infoheader.biBitCount == 32) bpp = 32;
	else bpp = 8;

	if (infoheader.biCompression == IBI_BITFIELDS) {
		IUINT32 redMask = is_igetl(stream);
		IUINT32 grnMask = is_igetl(stream);
		IUINT32 bluMask = is_igetl(stream);
		(void)grnMask;
		if ((bluMask == 0x001f) && (redMask == 0x7C00)) bpp = 15;
		else if ((bluMask == 0x001f) && (redMask == 0xF800)) bpp = 16;
		else if ((bluMask == 0x0000FF) && (redMask == 0xFF0000)) bpp = 32;
		else {
			_is_perrno_set(4);
			return NULL;
		}
	}

	bmp = ibitmap_create(infoheader.biWidth, infoheader.biHeight, bpp);
	dest_depth = bpp;

	if (!bmp) {
		_is_perrno_set(5);
		return NULL;
	}

	switch (infoheader.biCompression) 
	{
	case IBI_RGB:
		ibmp_read_image(stream, bmp, &infoheader);
		break;
	case IBI_RLE8:
	case IBI_RLE4:
		ibmp_read_rle_image(stream, bmp, &infoheader);
		break;
	case IBI_BITFIELDS:
		ibmp_read_bitfields_image(stream, bmp, &infoheader);
		break;
	default:
		ibitmap_release(bmp);
		bmp = NULL;
		_is_perrno_set(6);
		break;
	}

	if (dest_depth != bpp) {
		if ((bpp != 8) && (!want_palette)) pal = NULL;
	}

	ibitmap_pixfmt_set(bmp, ibitmap_pixfmt_guess(bmp));

	/* construct a fake palette if 8-bit mode is not involved */
	if ((bpp != 8) && (dest_depth != 8) && want_palette) {
		//_igenerate_332_palette(pal);
	}

	return bmp;
}

//---------------------------------------------------------------------
// bmp - isave_bmp_stream
//---------------------------------------------------------------------
int isave_bmp_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal)
{
	IUINT32 c, p;
	long i, j;
	IRGB tmppal[256];
	int bfSize;
	int biSizeImage;
	int depth;
	int bpp;
	int filler;
	int fmt;

	assert(bmp);
	assert(stream);

	depth = bmp->bpp;
	bpp = (depth == 8) ? 8 : 24;
	filler = 3 - ((bmp->w * (bpp / 8) - 1) & 3);

	if (!pal) {
		memcpy(tmppal, _ipaletted, 256 * sizeof(IRGB));
		pal = tmppal;
	}

	if (bpp == 8) {
		biSizeImage = (bmp->w + filler) * bmp->h;
		bfSize = (54 + 256 * 4 + biSizeImage);
	}	else {
		biSizeImage = (bmp->w * 3 + filler) * bmp->h;
		bfSize = 54 + biSizeImage;
	}

	_is_perrno_set(0);

	/* file_header */
	is_iputw(stream, 0x4D42);              /* bfType ("BM") */
	is_iputl(stream, bfSize);              /* bfSize */
	is_iputw(stream, 0);                   /* bfReserved1 */
	is_iputw(stream, 0);                   /* bfReserved2 */

	if (bpp == 8) is_iputl(stream, 54 + 256 * 4); 
	else is_iputl(stream, 54); 

	/* info_header */
	is_iputl(stream, 40);                  /* biSize */
	is_iputl(stream, (long)bmp->w);  /* biWidth */
	is_iputl(stream, (long)bmp->h); /* biHeight */
	is_iputw(stream, 1);                   /* biPlanes */
	is_iputw(stream, bpp);                 /* biBitCount */
	is_iputl(stream, 0);                   /* biCompression */
	is_iputl(stream, biSizeImage);         /* biSizeImage */
	is_iputl(stream, 0xB12);               /* (0xB12 = 72 dpi) */
	is_iputl(stream, 0xB12);               /* biYPelsPerMeter */

	if (bpp == 8) {
		is_iputl(stream, 256);              /* biClrUsed */
		is_iputl(stream, 256);              /* biClrImportant */
		for (i = 0; i < 256; i++) {
			is_putc(stream, pal[i].b);
			is_putc(stream, pal[i].g);
			is_putc(stream, pal[i].r);
			is_putc(stream, 0);
		}
	}	else {
		is_iputl(stream, 0);                /* biClrUsed */
		is_iputl(stream, 0);                /* biClrImportant */
	}

	fmt = _ibitmap_guess_pixfmt(bmp);

	/* image data */
	for (j = (long)bmp->h - 1; j >= 0; j--) {
		for (i = 0; i < (long)bmp->w; i++) {
			c = _is_getpx(bmp, i, j);
			p = _im_color_get(fmt, c, pal);
			if (bpp == 8) {
				is_putc(stream, (long)c);
			}	else {
				is_putc(stream, (int)((p >>  0) & 0xFF));
				is_putc(stream, (int)((p >>  8) & 0xFF));
				is_putc(stream, (int)((p >> 16) & 0xFF));
			}
		}
		for (i = 0; i < (long)filler; i++) is_putc(stream, 0);
	}

	return 0;
}

//---------------------------------------------------------------------
// bmp - isave_bmp_file
//---------------------------------------------------------------------
int isave_bmp_file(const char *file, struct IBITMAP *bmp, const IRGB *pal)
{
	IMDIO stream;
	int retval;

	if (is_open_file(&stream, file, "wb")) return -1;
	retval = isave_bmp_stream(&stream, bmp, pal);
	is_close_file(&stream);

	return retval;
}


//=====================================================================
//
// tga - Tagged Graphics (Truevision Inc.) Format
//
// decoding program - idcolor.h
// 
//=====================================================================


//---------------------------------------------------------------------
// tga - single_tga_readn
//---------------------------------------------------------------------
static IUINT32 single_tga_readn(IMDIO *stream, int bpp)
{
	IUINT32 r, g, b, a, c;

	if (bpp == 8) return is_getc(stream);
	if (bpp == 15) {
		c = is_igetw(stream);
		r = _ipixel_scale_5[(c >> 10) & 0x1F];
		g = _ipixel_scale_5[(c >>  5) & 0x1F];
		b = _ipixel_scale_5[(c >>  0) & 0x1F];
		return IRGBA_TO_X1R5G5B5(r, g, b, 255);
	}
	if (bpp == 16) {
		c = is_igetw(stream);
		r = _ipixel_scale_5[(c >> 11) & 0x1F];
		g = _ipixel_scale_6[(c >>  5) & 0x3F];
		b = _ipixel_scale_5[(c >>  0) & 0x1F];
		return IRGBA_TO_R5G6B5(r, g, b, 255);
	}
	if (bpp != 24 && bpp != 32) {
		return 0xffffffff;
	}

	b = is_getc(stream);
	g = is_getc(stream);
	r = is_getc(stream);
	a = (bpp == 32)? is_getc(stream) : 0;
	c = IRGBA_TO_A8R8G8B8(r, g, b, a);
	if (bpp == 24) c &= 0xffffff;

	return c;
}

//---------------------------------------------------------------------
// tga - raw_tga_readn
//---------------------------------------------------------------------
static void *raw_tga_readn(void *b, int w, int bpp, IMDIO *stream)
{
	unsigned char *lptr = (unsigned char*)b;
	IUINT32 c, n;

	n = (bpp + 7) >> 3;
	for (; w > 0; w--, lptr += n) {
		c = single_tga_readn(stream, bpp);
		if (n == 1) _ipixel_store_8(lptr, 0, c);
		else if (n == 2) _ipixel_store_16(lptr, 0, c);
		else if (n == 3) _ipixel_store_24(lptr, 0, c);
		else if (n == 4) _ipixel_store_32(lptr, 0, c);
	}

	return (void*)lptr;
}

//---------------------------------------------------------------------
// tga - iload_tga_stream
//---------------------------------------------------------------------
struct IBITMAP *iload_tga_stream(IMDIO *stream, IRGB *pal)
{
	unsigned char image_palette[256][3];
	unsigned char id_length, palette_type, image_type, palette_entry_size;
	unsigned char bpp, descriptor_bits;
	IUINT32 c, i, y, yc, n, x;
	IUINT16 palette_colors;
	IUINT16 image_width, image_height;
	unsigned char *lptr;
	struct IBITMAP *bmp;
	int compressed, want_palette, count;
	IRGB tmppal[256];
	
	assert(stream);

	_is_perrno_set(0);

	want_palette = 1;
	if (!pal) {
		want_palette = 0;
		pal = tmppal;
	}

	id_length = is_getc(stream);
	palette_type = is_getc(stream);
	image_type = is_getc(stream);
	is_igetw(stream);
	palette_colors  = is_igetw(stream);
	palette_entry_size = is_getc(stream);
	is_igetw(stream);
	is_igetw(stream);
	image_width = is_igetw(stream);
	image_height = is_igetw(stream);
	bpp = is_getc(stream);
	descriptor_bits = is_getc(stream);

	is_seekcur(stream, id_length);

	if (palette_type == 1) {
		for (i = 0; i < (IUINT32)palette_colors; i++) {
			if (palette_entry_size == 16) {
				c = is_igetw(stream);
				image_palette[i][0] = (IUINT8)_ipixel_scale_5[(c >>  0) & 31];
				image_palette[i][1] = (IUINT8)_ipixel_scale_5[(c >>  5) & 31];
				image_palette[i][2] = (IUINT8)_ipixel_scale_5[(c >> 10) & 31];
			}
			else if (palette_entry_size >= 24) {
				image_palette[i][0] = is_getc(stream);
				image_palette[i][1] = is_getc(stream);
				image_palette[i][2] = is_getc(stream);
				if (palette_entry_size == 32) is_getc(stream);

			}
		}
	}
	else if (palette_type != 0) {
		_is_perrno_set(1);
		return NULL;
	}

	/* 
	 * (image_type & 7): 1/colormapped, 2/truecolor, 3/grayscale
	 * (image_type & 8): 0/uncompressed, 8/compressed
	 */
	compressed = (image_type & 8);
	image_type = (image_type & 7);

	if ((image_type < 1) || (image_type > 3)) {
		_is_perrno_set(2);
		return NULL;
	}

	if (image_type == 1) {				/* paletted image */
		if ((palette_type != 1) || (bpp != 8)) {
			_is_perrno_set(11);
			return NULL;
		}
		for(i = 0; i < (IUINT32)palette_colors; i++) {
			pal[i].r = image_palette[i][2];
			pal[i].g = image_palette[i][1];
			pal[i].b = image_palette[i][0];
		}
	}
	else if (image_type == 2) {			/* truecolor image */
		if (palette_type != 0) {
			_is_perrno_set(21);
			return NULL;
		}
		if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
			_is_perrno_set(22);
			return NULL;
		}
		if (bpp == 16) bpp = 15;
	}
	else if (image_type == 3) {			/* grayscale image */
		if ((palette_type != 0) || (bpp != 8)) {
			_is_perrno_set(31);
			return NULL;
		}
		for (i = 0; i < 256; i++) {
			pal[i].r = (unsigned char)(i);
			pal[i].g = (unsigned char)(i);
			pal[i].b = (unsigned char)(i);
		}
	}

	n = (bpp + 7) >> 3;
	bmp = ibitmap_create(image_width, image_height, bpp);
	if (!bmp) {
		_is_perrno_set(50);
		return NULL;
	}
	for (y = 0; y < bmp->h; y++) {
		memset(bmp->line[y], 0, bmp->pitch);
	}

	for (y = image_height; y; y--) {
		yc = (descriptor_bits & 0x20) ? image_height - y : y - 1;
		lptr = (unsigned char*)(bmp->line[yc]);
		if (compressed == 0) {
			raw_tga_readn(lptr, image_width, bpp, stream);
		}	else {
			for (x = 0; x < (IUINT32)image_width; x += count) {
				count = is_getc(stream);
				if (count & 0x80) {
					count = (count & 0x7F) + 1;
					c = single_tga_readn(stream, bpp);
					for (i = 0; i < (IUINT32)count; i++, lptr += n) {
						if (n == 1) _ipixel_store_8(lptr, 0, c);
						else if (n == 2) _ipixel_store_16(lptr, 0, c);
						else if (n == 3) _ipixel_store_24(lptr, 0, c);
						else if (n == 4) _ipixel_store_32(lptr, 0, c);
					}
				}	else {
					count = count + 1;
					raw_tga_readn(lptr, count, bpp, stream);
					lptr = lptr + n * count;
				}
			}
		}
	}

	if (_is_perrno_get()) {
		ibitmap_release(bmp);
		return NULL;
	}

	ibitmap_pixfmt_set(bmp, ibitmap_pixfmt_guess(bmp));

	/* construct a fake palette if 8-bit mode is not involved */
	if ((bpp != 8) && want_palette) {
//		_igenerate_332_palette(pal);
	}
      
	return bmp;
}

//---------------------------------------------------------------------
// tga - isave_tga_stream
//---------------------------------------------------------------------
int isave_tga_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal)
{
	unsigned char image_palette[256][3];
	IUINT32 c, a, r, g, b, p;
	long x, y, depth, n;
	IRGB tmppal[256];
	int fmt;

	assert(bmp);
	assert(stream);

	if (!pal) {
		memcpy(tmppal, _ipaletted, 256 * sizeof(IRGB));
		pal = tmppal;
	}

	depth = bmp->bpp;

	if (depth == 15) depth = 16;
	_is_perrno_set(0);

	is_putc(stream, 0);                       /* id length (no id saved) */
	is_putc(stream, (depth == 8) ? 1 : 0);       /* palette type */
	is_putc(stream, (depth == 8) ? 1 : 2);       /* image type */
	is_iputw(stream, 0);                         /* first colour */
	is_iputw(stream, (depth == 8) ? 256 : 0);    /* number of colours */
	is_putc(stream, (depth == 8) ? 24 : 0);      /* palette entry size */
	is_iputw(stream, 0);                         /* left */
	is_iputw(stream, 0);                         /* top */
	is_iputw(stream, (int)bmp->w);         /* width */
	is_iputw(stream, (int)bmp->h);        /* height */
	is_putc(stream, depth);                      /* bits per pixel */
	is_putc(stream, (depth == 32) ? 8 : 0);      /* descriptor 
											(bottom to top, 8-bit alpha) */

	if (depth == 8) {
		for (y = 0; y < 256; y++) {
			image_palette[y][2] = pal[y].r;
			image_palette[y][1] = pal[y].g;
			image_palette[y][0] = pal[y].b;
			is_putc(stream, image_palette[y][0]);
			is_putc(stream, image_palette[y][1]);
			is_putc(stream, image_palette[y][2]);
		}
	}

	n = (bmp->bpp + 7) / 8;

	fmt = _ibitmap_guess_pixfmt(bmp);

	for (y = (long)bmp->h - 1; y >= 0; y--) {
		for (x = 0; x < (long)bmp->w; x++) {
			c = _is_getpx(bmp, x, y);
			p = _im_color_get(fmt, c, pal);
			a = (p >> 24) & 0xFF;
			r = (p >> 16) & 0xFF;
			g = (p >>  8) & 0xFF;
			b = (p >>  0) & 0xFF;
			if (n == 1) is_putc(stream, c);
			else if (n == 2) {
				c = ((r << 7) & 0x7C00) | ((g << 2) & 0x3E0);
				is_iputw(stream, c | ((b >> 3) & 0x1F));
			}
			else if (n == 3 || n == 4) {
				is_putc(stream, b);
				is_putc(stream, g);
				is_putc(stream, r);
				if (n == 4) is_putc(stream, a);
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------
// tga - isave_tga_file
//---------------------------------------------------------------------
int isave_tga_file(const char *file, struct IBITMAP *bmp, const IRGB *pal)
{
	IMDIO stream;
	int retval;

	if (is_open_file(&stream, file, "wb")) return -1;
	retval = isave_tga_stream(&stream, bmp, pal);
	is_close_file(&stream);

	return retval;
}

//---------------------------------------------------------------------
// Loader Definition:
// Add new loader to load other picture formats
//---------------------------------------------------------------------
static IPICLOADER iloader_table[256];

void iloader_table_init(void)
{
	static int inited = 0;
	int i;
	if (inited) return;
	for (i = 0; i < 256; i++) iloader_table[i] = NULL;
	inited = 1;
}


//---------------------------------------------------------------------
// setup a new loader: 
// using First Byte of the Picture to choose loader for different formats
// NULL for disable the loader
//---------------------------------------------------------------------
IPICLOADER ipic_loader(unsigned char firstbyte, IPICLOADER loader)
{
	IPICLOADER oldloader;
	iloader_table_init();
	oldloader = iloader_table[firstbyte];
	iloader_table[firstbyte] = loader;
	return oldloader;
}

//---------------------------------------------------------------------
// recognize data stream and load picture
//---------------------------------------------------------------------
struct IBITMAP *iload_picture(IMDIO *stream, IRGB *pal)
{
	unsigned char firstbyte;
	int ch;

	assert(stream);

	iloader_table_init();

	ch = is_getc(stream);
	if (ch < 0) return NULL;
	is_ungetc(stream, ch);

	firstbyte = (unsigned char)(ch & 0xff);
	if (iloader_table[firstbyte] != NULL) {
		return iloader_table[firstbyte](stream, pal);
	}

	if (ch == 'B') {
		return iload_bmp_stream(stream, pal);
	}	else 
	if (ch == 'G') {
		return iload_gif_stream(stream, pal);
	}	
	return iload_tga_stream(stream, pal);
}


//=====================================================================
// ORIGINAL OPERATION
//=====================================================================
static IRGB _idefault_pal[256];

void ipic_read_pal(void *_pal)
{
	memcpy(_pal, _idefault_pal, 256 * 4);
}

void ipic_write_pal(const void *_pal)
{
	memcpy(_idefault_pal, _pal, 256 * 4);
}

struct IBITMAP *ipic_convert(struct IBITMAP *src, int dfmt, const IRGB *pal)
{
	struct IBITMAP *bmp;
	int sfmt;
	int w, h;
	w = (int)src->w;
	h = (int)src->h;
	sfmt = _ibitmap_guess_pixfmt(src);
	if (sfmt == dfmt) {
		bmp = ibitmap_create(w, h, src->bpp);
		if (bmp == NULL) return NULL;
		ibitmap_blit(bmp, 0, 0, src, 0, 0, w, h, 0);
		return bmp;
	}
	bmp = ibitmap_convfmt(dfmt, src, pal, pal);
	return bmp;
}

struct IBITMAP *ipic_load_file(const char *file, long pos, IRGB *pal)
{
	IMDIO stream;
	struct IBITMAP *bmp;
	IRGB *p = pal == NULL? _ipaletted : pal;

	if (is_open_file(&stream, file, "rb")) {
		_is_perrno_set(-1);
		return NULL;
	}
	fseek((FILE*)(stream.data), pos, SEEK_SET);
	bmp = iload_picture(&stream, p);
	is_close_file(&stream);

	return bmp;
}

struct IBITMAP *ipic_load_mem(const void *ptr, long size, IRGB *pal)
{
	IMDIO stream;
	struct IBITMAP *bmp;
	IRGB *p = pal == NULL ? _ipaletted : pal;
	size = (size <= 0)? 0x7fffffffl : size;
	is_init_mem(&stream, ptr, size);
	bmp = iload_picture(&stream, p);
	return bmp;
}


//=====================================================================
//
// GIF Operation Interface
// Since lzw's patent has expired, we can decode/encode gif freely
//
//=====================================================================

//---------------------------------------------------------------------
// _igif_clear_table - clear lzw table
//---------------------------------------------------------------------
static void _igif_clear_table(IGIFDESC *gif)
{
	gif->empty_string = gif->cc + 2;
	gif->curr_bit_size = gif->bit_size + 1;
	gif->bit_overflow = 0;
}

//---------------------------------------------------------------------
// _igif_get_code - get input code with current bit size
//---------------------------------------------------------------------
static void _igif_get_code(IGIFDESC *gif)
{
	if (gif->bit_pos + gif->curr_bit_size > 8) {
		if (gif->data_pos >= gif->data_len) {
			gif->data_len = is_getc(gif->stream);
			gif->data_pos = 0; 
		}
		gif->entire = (is_getc(gif->stream) << 8) + gif->entire;
		gif->data_pos++;
	}
	if (gif->bit_pos + gif->curr_bit_size > 16) {
		if (gif->data_pos >= gif->data_len) {
			gif->data_len = is_getc(gif->stream); 
			gif->data_pos = 0;
		}
		gif->entire = (is_getc(gif->stream) << 16) + gif->entire;
		gif->data_pos++;
	}
	gif->code = (gif->entire >> gif->bit_pos) & ((1 << 
		gif->curr_bit_size) - 1);
	if (gif->bit_pos + gif->curr_bit_size > 8) 
		gif->entire >>= 8;
	if (gif->bit_pos + gif->curr_bit_size > 16)
		gif->entire >>= 8;
	gif->bit_pos = (gif->bit_pos + gif->curr_bit_size) % 8;
	if (gif->bit_pos == 0) {
		if (gif->data_pos >= gif->data_len) {
			gif->data_len = is_getc(gif->stream);
			gif->data_pos = 0;
		}
		gif->entire = is_getc(gif->stream);
		gif->data_pos++;
	}
}

//---------------------------------------------------------------------
// _igif_get_string - get string into gif->string
//---------------------------------------------------------------------
static void _igif_get_string(IGIFDESC *gif, int num)
{
	unsigned char *string;
	int i;
	string = gif->string;
	if (num < gif->cc) {
		gif->string_length = 1;
		string[0] = gif->newc[num];
	}	else {
		i = gif->str[num].length;
		gif->string_length = i;
		for (; i > 0; ) {
			i--;
			string[i] = gif->newc[num];
			num = gif->str[num].base;
		}
	}
}

//---------------------------------------------------------------------
// _igif_output_string - output string to bitmap
//---------------------------------------------------------------------
static void _igif_output_string(IGIFDESC *gif)
{
	unsigned char *string, **line, *ptr, mask;
	int imgx, imgy, x, y, len, last, i;
	if (gif->error || gif->bitmap == NULL) return;
	x = gif->x;
	y = gif->y;
	imgx = gif->image_x;
	imgy = gif->image_y;
	line = (unsigned char**)gif->bitmap->line;
	string = gif->string;
	last = gif->string_length;
	for (; last > 0; ) {
		len = gif->image_w + imgx - x;
		len = len > last ? last : len;
		if (gif->transparent < 0) {
			memcpy(line[y] + x, string, len);
			x += len;
			string += len;
		}	else {
			mask = (unsigned char)gif->transparent;
			ptr = line[y] + x;
			for (i = len; i > 0; i--) {
				if (string[0] != mask) *ptr = *string;
				string++;
				ptr++;
			}
			x += len;
		}
		last -= len;
		if (x >= imgx + gif->image_w) {
			x = imgx;
			y += gif->interlace;
			if (gif->interlace) {
				if (y >= imgy + gif->image_h) {
					if (gif->interlace == 8 && (y - imgy) % 8 == 0) {
						gif->interlace = 8;
						y = imgy + 4;
					}	else
					if (gif->interlace == 8 && (y - imgy) % 8 == 4) {
						gif->interlace = 4;
						y = imgy + 2;
					}	else
					if (gif->interlace == 4) {
						gif->interlace = 2;
						y = imgy + 1;
					}
				}
			}
		}
	}
	gif->x = x;
	gif->y = y;
}

//---------------------------------------------------------------------
// ipic_gif_open - open gif and load header
//---------------------------------------------------------------------
int ipic_gif_open(IGIFDESC *gif, IMDIO *stream, IRGB *pal, int customer)
{
	long c, i;

	assert(gif && stream);
	memset(gif, 0, sizeof(IGIFDESC));

	gif->stream = stream;

	c = is_mgetw(stream) << 8;
	c = c | is_getc(stream);
	gif->state = -1;
	if (c != 0x474946) {
		return -1;
	}

	is_seekcur(stream, 3);

	gif->width = is_igetw(stream);
	gif->height = is_igetw(stream);
	gif->flags = 0;
	gif->delay = 0;
	gif->hotx = 0;
	gif->hoty = 0;
	gif->frame = -1;

	gif->pal = pal;
	gif->bpp = 0;

	c = is_getc(stream);
	if (c & 128) 
		gif->bpp = (c & 7) + 1;

	gif->background = (unsigned char)is_getc(stream);
	gif->aspect = (unsigned char)is_getc(stream);

	if (gif->bpp && pal) {
		for (i = 0; i < (1 << gif->bpp); i++) {
			pal[i].r = is_getc(stream);
			pal[i].g = is_getc(stream);
			pal[i].b = is_getc(stream);
		}
	}	
	else if (gif->bpp) {
		is_seekcur(stream, (1 << gif->bpp) * 3);
	}

	if (customer == 0) {
		gif->bitmap = ibitmap_create(gif->width, gif->height * 2, 8);
		if (gif->bitmap == NULL) {
			return -2;
		}
		ibitmap_fill(gif->bitmap, 0, 0, gif->width, gif->height * 2,
			gif->background, 0);
		ibitmap_pixfmt_set(gif->bitmap, IPIX_FMT_C8);
	}	

	gif->state = 1;
	gif->error = 0;
	gif->transparent = -1;
	gif->flags = customer? IGIF_CUSTOMERBMP : 0;

	return 0;
}

//---------------------------------------------------------------------
// ipic_gif_close - close gif and free bitmap
//---------------------------------------------------------------------
int ipic_gif_close(IGIFDESC *gif)
{
	if ((gif->flags & IGIF_CUSTOMERBMP) == 0) {
		ibitmap_release(gif->bitmap);
		gif->bitmap = NULL;
	}
	if (gif->state == 2 || gif->state == 3) {
		is_putc(gif->stream, 0x3b);
	}
	gif->state = 0;
	return 0;
}

//---------------------------------------------------------------------
// ipic_gif_read_image_desc - read image desc
//---------------------------------------------------------------------
static int ipic_gif_read_image_desc(IGIFDESC *gif)
{
	unsigned char *string;
	IMDIO *stream;
	int dispose, depth, old, i;
	int retval = 0;

	if (gif->state != 1) return -1;
	stream = gif->stream;
	string = gif->string;
	
	gif->image_x = is_igetw(stream);
	gif->image_y = is_igetw(stream);
	gif->image_w = is_igetw(stream);
	gif->image_h = is_igetw(stream);

	ibitmap_blit(gif->bitmap, gif->image_x, gif->image_y, gif->bitmap,
		gif->image_x, gif->image_y + gif->height, gif->image_w, 
		gif->image_h, 0);

	if (gif->image_x < 0 || gif->image_y < 0 ||
		gif->image_x + gif->image_w > gif->width ||
		gif->image_y + gif->image_h > gif->height)
		gif->error = 1;

	i = is_getc(stream);
	if (i & 64) gif->interlace = 8;
	else gif->interlace = 1;

	if (i & 128) {
		depth = (i & 7) + 1;
		if (gif->pal) {
			for (i = 0; i < (1 << depth); i++) {
				gif->pal[i].r = is_getc(stream);
				gif->pal[i].g = is_getc(stream);
				gif->pal[i].b = is_getc(stream);
			}
		}	else {
			is_seekcur(stream, (1 << depth) * 3);
		}
	}

	gif->bit_size = is_getc(stream);
	gif->cc = 1 << gif->bit_size;

	for (i = 0; i < gif->cc; i++) {
		gif->str[i].base = -1;
		gif->str[i].length = 1;
		gif->newc[i] = (unsigned char)i;
	}

	gif->bit_pos = 0;
	gif->data_len = is_getc(stream);
	gif->data_pos = 0;
	gif->entire = is_getc(stream);
	gif->data_pos++;
	gif->string_length = 0;
	gif->x = gif->image_x;
	gif->y = gif->image_y;

	_igif_clear_table(gif);
	_igif_get_code(gif);
	if (gif->code == gif->cc) 
		_igif_get_code(gif);

	_igif_get_string(gif, gif->code);
	_igif_output_string(gif);
	old = gif->code;

	for (; ; ) {
		_igif_get_code(gif);
		if (gif->code == gif->cc + 1) {
			break;
		}
		if (gif->entire < 0) {
			retval = -1;
			gif->error = 2;
			break;
		}
		if (gif->code == gif->cc) {
			_igif_clear_table(gif);
			_igif_get_code(gif);
			_igif_get_string(gif, gif->code);
			_igif_output_string(gif);
			old = gif->code;
		}	else
		if (gif->code < gif->empty_string) {
			_igif_get_string(gif, gif->code);
			_igif_output_string(gif);
			if (gif->bit_overflow == 0) {
				i = gif->empty_string;
				gif->str[i].base = (short)old;
				gif->str[i].length = gif->str[old].length + 1;
				gif->newc[i] = string[0];
				gif->empty_string++;
				if (gif->empty_string == (1 << gif->curr_bit_size))
					gif->curr_bit_size++;
				if (gif->curr_bit_size == 13) {
					gif->curr_bit_size = 12;
					gif->bit_overflow = 1;
				}
			}
			old = gif->code;
		}	else {
			_igif_get_string(gif, old);
			string[gif->str[old].length] = string[0];
			gif->string_length++;
			if (gif->bit_overflow == 0) {
				i = gif->empty_string;
				gif->str[i].base = (short)old;
				gif->str[i].length = gif->str[old].length + 1;
				gif->newc[i] = string[0];
				gif->empty_string++;
				if (gif->empty_string == (1 << gif->curr_bit_size)) 
					gif->curr_bit_size++;
				if (gif->curr_bit_size == 13) {
					gif->curr_bit_size = 12;
					gif->bit_overflow = 1;
				}
			}
			_igif_output_string(gif);
			old = gif->code;
		}
	}

	dispose = (gif->method >> 2) & 7;
	if (dispose == 2) {
		i = gif->transparent >= 0? gif->transparent : gif->background;
		ibitmap_fill(gif->bitmap, gif->image_x, gif->image_y + gif->height,
			gif->image_w, gif->image_h, i, 0);
	}	else 
	if (dispose != 3) {
		ibitmap_blit(gif->bitmap, gif->image_x, gif->image_y + gif->height,
			gif->bitmap, gif->image_x, gif->image_y, 
			gif->image_w, gif->image_h, 0);
	}

	gif->frame++;

	return retval;
}

//---------------------------------------------------------------------
// ipic_gif_read_extension - read extension
//---------------------------------------------------------------------
int ipic_gif_read_extension(IGIFDESC *gif, int firstcmd)
{
	IMDIO *stream;
	int control, c;
	char ident[12];

	stream = gif->stream;
	gif->delay = 0;
	gif->transparent = -1;
	gif->method = 0;

	if (firstcmd < 0) control = is_getc(stream);
	else control = firstcmd;

	if (control == 0xf9) {
		c = is_getc(stream); 
		if (c != 4) return -1;

		gif->method = is_getc(stream);
		gif->delay = is_igetw(stream);
		
		gif->transparent = is_getc(stream);
		gif->bitmap->mask = (unsigned char)gif->transparent;

		if ((gif->method & 1) == 0)
			gif->transparent = -1;

		c = (gif->method >> 2) & 7;

		if (c == 20) {
		}
	}	else 
	if (control == 0xff) {
		c = is_getc(stream);
		if (c == 0x0b) {
			is_reader(stream, ident, 0x0b);
			if (memcmp(ident, "PIXIAGIF.CH", 0x0b) == 0) {
				c = is_getc(stream);
				if (c == 5) {
					c = is_getc(stream);
					gif->flags = (gif->flags & ~0xff) | (c & 0xff);
					gif->hotx = (short)is_igetw(stream);
					gif->hoty = (short)is_igetw(stream);
				}	else {
					is_ungetc(stream, c);
				}
			}
		}	else {
			is_ungetc(stream, c);
		}
	}

	for (; ; ) {
		c = is_getc(stream);
		if (c <= 0) break;
		is_seekcur(stream, c);
	}
	
	return 0;
}

//---------------------------------------------------------------------
// ipic_gif_read_frame - read single frame
//---------------------------------------------------------------------
int ipic_gif_read_frame(IGIFDESC *gif)
{
	int retval, type;
	if (gif->state != 1) return -1;

	for (retval = 0; gif->error == 0; ) {
		type = is_getc(gif->stream);
		if (type < 0 || type == 0x3b || gif->error) {
			retval = -2;
			break;
		}
		if (type == 0) continue;
		switch (type)
		{
		case 0x21:
			ipic_gif_read_extension(gif, -1);
			break;
		case 0x2c:
			ipic_gif_read_image_desc(gif);
			type = 0;
			break;
		case 0x01:
		case 0xf9:
		case 0xfe:
		case 0xff:
			ipic_gif_read_extension(gif, type);
			break;
		default:
			//printf("gif unknow %d\n", type);
			retval = -3;
			gif->error = 5;
			break;
		}
		if (type == 0) break;
	}
	return retval;
}

//---------------------------------------------------------------------
// load gif picture from stream
//---------------------------------------------------------------------
struct IBITMAP *iload_gif_stream(IMDIO *stream, IRGB *pal)
{
	struct IBITMAP *bmp;
	IGIFDESC *gif;
	gif = (IGIFDESC*)malloc(sizeof(IGIFDESC));
	if (gif == NULL) return NULL;
	if (ipic_gif_open(gif, stream, pal, 0) != 0) {
		free(gif);
		return NULL;
	}
	for (; ; ) {
		if (ipic_gif_read_frame(gif) < 0) break;
	}
	bmp = ibitmap_create(gif->width, gif->height, 8);
	if (bmp == NULL) {
		ibitmap_release(gif->bitmap);
		free(gif);
		return NULL;
	}
	ibitmap_blit(bmp, 0, 0, gif->bitmap, 0, 0, gif->width, 
		gif->height, 0);
	bmp->mask = (gif->transparent & 0xffff) | ((gif->flags & 0xffff) << 16);
	bmp->mode = (gif->hotx << 16) | (gif->hoty & 0xffff);
	ibitmap_release(gif->bitmap);
	free(gif);
	return bmp;
}

//---------------------------------------------------------------------
// open gif stream for saving
//---------------------------------------------------------------------
int ipic_gif_wopen(IGIFDESC *gif, IMDIO *stream, const IRGB *pal, 
	int w, int h, int background, int aspect, int flags, long hotxy)
{
	int i;

	assert(gif && stream);
	memset(gif, 0, sizeof(IGIFDESC));

	pal = pal ? pal : (const IRGB*)_ipaletted;

	gif->stream = stream;
	gif->pal = (IRGB*)pal;
	gif->width = w;
	gif->height = h;
	gif->bpp = 8;
	gif->background = background;
	gif->aspect = aspect <= 0 ? 0 : aspect;

	gif->image_x = 0;
	gif->image_y = 0;
	gif->image_w = w;
	gif->image_h = h;
	gif->interlace = 1;
	gif->flags = flags;
	gif->x = 0;
	gif->y = 0;
	gif->hotx = (short)((hotxy >> 16) & 0xffff);
	gif->hoty = (short)(hotxy & 0xffff);

	if ((gif->flags & IGIF_CUSTOMERBMP) == 0) {
		gif->bitmap = ibitmap_create(gif->width, gif->height * 2, 8);
		if (gif->bitmap == NULL) {
			return -1;
		}
		ibitmap_fill(gif->bitmap, 0, 0, gif->width, gif->height * 2, 
			gif->background, 0);
	}

	if (gif->flags & 0xff) flags |= IGIF_ANIMATE;

	is_writer(stream, "GIF", 3);
	is_writer(stream, (flags & IGIF_ANIMATE) ? "89a" : "87a", 3);
	is_iputw(stream, gif->width);
	is_iputw(stream, gif->height);
	is_putc(stream, 0xf7);
	is_putc(stream, gif->background);
	is_putc(stream, gif->aspect);

	for (i = 0; i < 256; i++) {
		is_putc(stream, pal[i].r);
		is_putc(stream, pal[i].g);
		is_putc(stream, pal[i].b);
	}

	gif->transparent = -1;
	gif->flags = flags;
	gif->error = 0;
	gif->delay = (flags & IGIF_ANIMATE) ? 0 : -1;
	gif->state = (flags & IGIF_ANIMATE) ? 3 : 2;

	if (gif->flags & 0xff) {
		is_putc(stream, 0x21);
		is_putc(stream, 0xff);
		is_putc(stream, 0x0b);
		is_writer(stream, "PIXIAGIF.CH", 0x0b);
		is_putc(stream, 5);
		is_putc(stream, gif->flags & 0xff);
		is_iputw(stream, gif->hotx);
		is_iputw(stream, gif->hoty);
		is_putc(stream, 0);
	}

	return 0;
}

//---------------------------------------------------------------------
// _igif_write_code - write bits into buffer
//---------------------------------------------------------------------
static void _igif_write_code(IGIFDESC *gif, int code, int bit_size)
{
	if (code < 0) {
		gif->entire &= (1 << gif->bit_pos) - 1;
		gif->string[gif->data_len] = (unsigned char)gif->entire;
		gif->entire >>= 8;
		gif->data_len++;
		is_putc(gif->stream, gif->data_len);
		is_writer(gif->stream, gif->string, gif->data_len);
		gif->data_len = 0;
		gif->bit_pos = 0;
		return;
	}

	gif->entire |= code << (gif->bit_pos);
	gif->bit_pos += bit_size;
	while (gif->bit_pos >= 8) {
		gif->string[gif->data_len] = (unsigned char)(gif->entire & 255);
		gif->entire >>= 8;
		gif->bit_pos -= 8;
		gif->data_len++;
		if (gif->data_len >= 252) {
			is_putc(gif->stream, gif->data_len);
			is_writer(gif->stream, gif->string, gif->data_len);
			gif->data_len = 0;
		}
	}
}

//---------------------------------------------------------------------
// _igif_get_next_color - get next color
//---------------------------------------------------------------------
static int _igif_get_next_color(IGIFDESC *gif)
{
	int x = gif->x;
	int y = gif->y;
	int c;

	if (--gif->string_length < 0) return -1;

	c = ((unsigned char*)gif->bitmap->line[y])[x];
	if (++x >= gif->image_x + gif->image_w) {
		x = gif->image_x;
		y += gif->interlace;
		if (gif->interlace) {
			if (y >= gif->image_y + gif->image_h) {
				if (gif->interlace == 8 && (y - gif->image_y) % 8 == 0) {
					gif->interlace = 8;
					y = gif->image_y + 4;
				}	else
				if (gif->interlace == 8 && (y - gif->image_y) % 8 == 4) {
					gif->interlace = 4;
					y = gif->image_y + 2;
				}	else
				if (gif->interlace == 4) {
					gif->interlace = 2;
					y = gif->image_y + 1;
				}
			}
		}
	}
	gif->x = x;
	gif->y = y;
	return c;
}

//---------------------------------------------------------------------
// ipic_gif_write_image_desc - encode image desc
//---------------------------------------------------------------------
static int ipic_gif_write_image_desc(IGIFDESC *gif, int palsize)
{
	IMDIO *stream;
	int palshift, mask, c, i, k;
	int string_buffer = 0;
	int color_num;
	int written;
	int hash;
	int length;
	int interlace;
	short *next;

	if (gif->state != 2 && gif->state != 3)
		return -1;

	stream = gif->stream;
	is_putc(stream, 0x2c);
	is_iputw(stream, gif->image_x);
	is_iputw(stream, gif->image_y);
	is_iputw(stream, gif->image_w);
	is_iputw(stream, gif->image_h);
	gif->x = gif->image_x;
	gif->y = gif->image_y;

	palsize = palsize < 0 ? 0 : palsize;
	for (palshift = 0; palshift < 8; palshift++)
		if ((1 << (palshift + 1)) >= palsize) break;
	palsize = palsize == 0 ? 0 : (1 << (palshift + 1));
	if (gif->interlace <= 1) gif->interlace = 1;
	else gif->interlace = 8;
	mask = palsize? 0x80 : 0;
	mask |= gif->interlace == 8 ? 0x40 : 0;
	mask |= palshift & 7;

	is_putc(stream, mask);
	if (palsize > 0) {
		if (gif->pal) {
			for (i = 0; i < palsize; i++) {
				is_putc(stream, gif->pal[i].r);
				is_putc(stream, gif->pal[i].g);
				is_putc(stream, gif->pal[i].b);
			}
		}	else {
			for (i = 0; i < palsize; i++) {
				is_putc(stream, i);
				is_putc(stream, i);
				is_putc(stream, i);
			}
		}
	}

	gif->string_length = gif->image_w * gif->image_h;
	interlace = gif->interlace;
	for (c = 0, k = 0; c >= 0; ) {
		c = _igif_get_next_color(gif);
		if (c > k) k = c;
	}

	for (c = 1; c <= 8; c++) 
		if ((1 << c) >= k) break;

	gif->bit_size = c;
	gif->cc = 1 << gif->bit_size;
	is_putc(stream, gif->bit_size);

	for (i = 0; i < gif->cc + 2; i++) {
		gif->str[i].base = -1;
		gif->str[i].length = 1;
		gif->newc[i] = (unsigned char)i;
	}

	next = (short*)(gif->string + 2048);
	for (i = 0; i < 1024; i++) next[i] = -1;

	gif->bit_pos = 0;
	gif->data_len = 0;
	gif->data_pos = 0;
	gif->string_length = gif->image_w * gif->image_h;
	gif->x = gif->image_x;
	gif->y = gif->image_y;
	gif->interlace = interlace;

	#define _igif_write(g, c) _igif_write_code(g, c, g->curr_bit_size)

	_igif_clear_table(gif);
	_igif_write(gif, gif->cc);

	for (length = 0; ; ) {

		c = _igif_get_next_color(gif);
		if (c < 0) break;
		length++;

		if (length == 1) {
			string_buffer = c;
			continue;
		}

		/* calculate hash entry */
		hash = ((string_buffer << (gif->bit_size - 1)) | c) & 0x3ff;
		length++;
		
		/* alias str[i].length as str[i].next to reduce memory */
		for (i = next[hash]; i >= 0; i = gif->str[i].length) {
			if (gif->str[i].base != string_buffer) continue;
			if (gif->newc[i] == c) break;
		}

		if (i >= 0) {
			string_buffer = i;
			written = 0;
		}	else {
			_igif_write(gif, string_buffer);
			color_num = gif->empty_string++;
			written = 1;

			/* alias str[i].length as str[i].next to reduce memory */
			if (color_num < 4096) {
				gif->str[color_num].base = string_buffer;
				gif->str[color_num].length = next[hash];
				next[hash] = color_num;
				gif->newc[color_num] = c;
			}
		
			string_buffer = c;
			length = 1;

			if (color_num >= (1 << gif->curr_bit_size))
				gif->curr_bit_size++;

			if (color_num >= 4094) {
				_igif_write(gif, string_buffer);
				_igif_write(gif, gif->cc);
				_igif_clear_table(gif);
				for (i = 0; i < 1024; i++) next[i] = -1;
				length = 0;
			}
		}
	}

	_igif_write(gif, string_buffer);

	_igif_write(gif, gif->cc + 1);
	_igif_write(gif, -1);

	#undef _igif_write

	is_putc(stream, 0);
	is_putc(stream, 0);

	written = written;

	return 0;
}

//---------------------------------------------------------------------
// ipic_gif_write_frame - write frame into gif
//---------------------------------------------------------------------
int ipic_gif_write_frame(IGIFDESC *gif, int delay, int mask, int palsize)
{
	IMDIO *stream;

	assert(gif);
	stream = gif->stream;

	if (gif->flags & IGIF_ANIMATE) {
		delay = delay < 0 ? 0 : delay;
		is_putc(stream, 0x21);
		is_putc(stream, 0xf9);
		is_putc(stream, 0x04);
		is_putc(stream, mask >= 0? 1 : 0);
		is_iputw(stream, delay);
		is_putc(stream, mask & 0xff);
		is_putc(stream, 0x00);
	}

	ipic_gif_write_image_desc(gif, palsize);

	return 0;
}

//---------------------------------------------------------------------
// save gif picture to stream
//---------------------------------------------------------------------
int isave_gif_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal)
{
	IGIFDESC *gif;
	long flags;
	long hotxy;

	assert(stream && bmp);

	gif = (IGIFDESC*)malloc(sizeof(IGIFDESC));
	if (gif == NULL) return 0;
	
	flags = (bmp->mask >> 16) & 0xffff;
	hotxy = bmp->mode;
	if (hotxy != 0 || (flags & 0xff)) flags |= IGIF_ANIMATE;

	if (ipic_gif_wopen(gif, stream, pal, bmp->w, bmp->h, 
		bmp->mask & 0xff, 0, flags, hotxy)) {
		free(gif);
		return -1;
	}

	gif->transparent = (short)(bmp->mask & 0xffff);
	/*gif->transparent = -1; */

	ibitmap_blit(gif->bitmap, 0, 0, bmp, 0, 0, bmp->w, bmp->h, 0);
	ipic_gif_write_frame(gif, 0, bmp->mask & 0xff, 0);
	ipic_gif_close(gif);

	free(gif);
	return 0;
}

//---------------------------------------------------------------------
// save gif picture to stream
//---------------------------------------------------------------------
int isave_gif_file(const char *file, struct IBITMAP *bmp, const IRGB *pal)
{
	IMDIO stream;
	if (is_open_file(&stream, file, "wb")) {
		return -1;
	}
	isave_gif_stream(&stream, bmp, pal);
	is_close_file(&stream);
	return 0;
}


