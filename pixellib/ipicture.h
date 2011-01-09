//=====================================================================
//
// ipicture.h - general picture file operation
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

#ifndef __I_PICTURE_H__
#define __I_PICTURE_H__

#include "ibitmap.h"
#include "ibmbits.h"
#include "ibmcols.h"

#ifdef __cplusplus
extern "C" {
#endif


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
// Media Data Streaming Input/Output Interface
//---------------------------------------------------------------------
typedef struct IMDIO
{
	int (*getc)(struct IMDIO *stream);
	int (*putc)(struct IMDIO *stream, int c);
	long _code, _pos, _len, _ch, _bitc, _bitd;
	long _cnt, _ungetc;
	void *data;
}	IMDIO;

int is_getc(IMDIO *stream);
int is_putc(IMDIO *stream, int c);

int is_ungetc(IMDIO *stream, int c);

long is_reader(IMDIO *stream, void *buffer, long size);
long is_writer(IMDIO *stream, const void *buffer, long size);

int is_igetw(IMDIO *stream);
int is_iputw(IMDIO *stream, int w);
long is_igetl(IMDIO *stream);
long is_iputl(IMDIO *stream, long l);

int is_mgetw(IMDIO *stream);
int is_mputw(IMDIO *stream, int w);
long is_mgetl(IMDIO *stream);
long is_mputl(IMDIO *stream, long l);

long is_igetbits(IMDIO *stream, int count);
long is_iputbits(IMDIO *stream, long data, int count);
void is_iflushbits(IMDIO *stream, int isflushwrite);

long is_mgetbits(IMDIO *stream, int count);
long is_mputbits(IMDIO *stream, long data, int count);
void is_mflushbits(IMDIO *stream, int isflushwrite);

void is_seekcur(IMDIO *stream, long skip);


//---------------------------------------------------------------------
// stream operation
//---------------------------------------------------------------------
int is_init_mem(IMDIO *stream, const void *lptr, long size);
int is_open_file(IMDIO *stream, const char *filename, const char *rw);
int is_close_file(IMDIO *stream);

void _is_perrno_set(long v);
long _is_perrno_get(void);

void *is_read_marked_block(IMDIO *stream, const unsigned char *mark,
	int marksize, size_t *nbytes_readed);


//=====================================================================
//
// PICTURE MEDIA OPERATION
//
// BMP - Windows/OS2 IBITMAP Format, load/save supported
// TGA - Tagged Graphics (Truevision Inc) Format, load/save supported
// GIF - Graphics Interchange Format (CompuServe Inc), load supported
//
//=====================================================================


//---------------------------------------------------------------------
// Picture Loading / Saving - Original Interface
//---------------------------------------------------------------------
// load bmp picture from stream
struct IBITMAP *iload_bmp_stream(IMDIO *stream, IRGB *pal);

// load bmp picture from stream
struct IBITMAP *iload_tga_stream(IMDIO *stream, IRGB *pal);

// load single gif frame from stream
struct IBITMAP *iload_gif_stream(IMDIO *stream, IRGB *pal);

// save bmp picture to stream
int isave_bmp_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal);

// save tga picture to stream
int isave_tga_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal);

// save gif picture to stream
int isave_gif_stream(IMDIO *stream, struct IBITMAP *bmp, const IRGB *pal);



//---------------------------------------------------------------------
// Picture Loading With Format Detecting
//---------------------------------------------------------------------

// Loader Definition:
// Add new loader to load other picture formats
typedef struct IBITMAP *(*IPICLOADER)(IMDIO *stream, IRGB *pal);

// setup a new loader: 
// using First Byte of the Picture to choose loader for different formats
// NULL for disable the loader
// eg: FirstByte of BMP/GIF/PNG/JPG is 'B'/'G'/0x89/0xff
IPICLOADER ipic_loader(unsigned char firstbyte, IPICLOADER loader);

// recognize data stream and load picture
struct IBITMAP *iload_picture(IMDIO *stream, IRGB *pal);


//---------------------------------------------------------------------
// Picture Saving - Standard Interface
//---------------------------------------------------------------------
// save bmp picture to file
int isave_bmp_file(const char *file, struct IBITMAP *bmp, const IRGB *pal);

// save tga picture to file
int isave_tga_file(const char *file, struct IBITMAP *bmp, const IRGB *pal);

// save gif picture to file
int isave_gif_file(const char *file, struct IBITMAP *bmp, const IRGB *pal);


//---------------------------------------------------------------------
// ORIGINAL OPERATION
//---------------------------------------------------------------------
struct IBITMAP *ipic_load_file(const char *file, long pos, IRGB *pal);
struct IBITMAP *ipic_load_mem(const void *ptr, long size, IRGB *pal);

struct IBITMAP *ipic_convert(struct IBITMAP *src, int fmt, const IRGB *pal);


//---------------------------------------------------------------------
// GIF INTERFACE (LZW PATENT HAS EXPIRED NOW)
//---------------------------------------------------------------------
struct IGIFDESC			/* gif descriptor */
{
	int width;			/* screen width */
	int height;			/* screen height */
	int bpp;			/* color depth */
	int state;			/* decode state */
	int aspect;			/* aspect */
	int error;			/* decode/encode error */
	int empty_string;	/* lzw - empty_string pointer */
	int curr_bit_size;	/* lzw - current bit size */
	int bit_overflow;	/* lzw - is table size greater than 4096 */
	int bit_pos;		/* lzw - loading bit position */
	int data_pos;		/* lzw - loading data position */
	int data_len;		/* lzw - loading data length */
	int entire;			/* lzw - loading data entire */
	int code;			/* lzw - code loaded */
	int cc;				/* lzw - table size */
	int string_length;	/* lzw - string length */
	int bit_size;		/* lzw - bit size */
	int image_x;		/* image descriptor - image x */
	int image_y;		/* image descriptor - image y */
	int image_w;		/* image descriptor - image width */
	int image_h;		/* image descriptor - image height */
	int interlace;		/* image descriptor - interlace */
	int x;				/* image descriptor - current x */
	int y;				/* image descriptor - current y */
	int flags;			/* flags */
	int transparent;	/* extension - color transparent */
	int method;			/* extension - rander method */
	long delay;			/* extension - time delay */
	long frame;			/* extension - frame count */
	int hotx;			/* extension - hotpoint x */
	int hoty;			/* extension - hotpoint y */
	IRGB *pal;			/* current palette */
	IMDIO *stream;	/* input / output stream */
	struct IBITMAP *bitmap;	/* bitmap */
	unsigned char background;		/* background color */
	unsigned char string[4096];		/* decode string */
	unsigned char newc[4096];		/* dict new char */
	struct { short base, length; } str[4096];   /* lzw table */
};

typedef struct IGIFDESC IGIFDESC;

#define IGIF_ANIMATE		256
#define IGIF_CUSTOMERBMP	512
#define IGIF_EXTMASK		255


// load gif header, allocate bitmap if customer is false
int ipic_gif_open(IGIFDESC *gif, IMDIO *stream, IRGB *pal, int customer);

// close gif and free bitmap
int ipic_gif_close(IGIFDESC *gif);

// returns delay time, below zero for eof or error
int ipic_gif_read_frame(IGIFDESC *gif);

// write gif header, allocate bitmap if IGIF_CUSTOMERBMP flag is 0
int ipic_gif_wopen(IGIFDESC *gif, IMDIO *stream, const IRGB *pal, 
	int w, int h, int background, int aspect, int flags, long hotxy);

// write frame into gif stream
int ipic_gif_write_frame(IGIFDESC *gif, int delay, int mask, int palsize);



#ifdef __cplusplus
}
#endif

#endif


