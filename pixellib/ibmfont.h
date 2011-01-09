//=====================================================================
//
// ibmfont.h - builtin font operation
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __IBMFONT_H__
#define __IBMFONT_H__

#include "ibmcols.h"


#ifdef __cplusplus
extern "C" {
#endif


void ibitmap_draw_text(IBITMAP *dst, int x, int y, const char *string,
	const IRECT *clip, IUINT32 color, IUINT32 back, int additive);

void ibitmap_printf(IBITMAP *dst, int x, int y, const IRECT *clip,
	IUINT32 color, IUINT32 back, int additive, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif



