/**********************************************************************
 *
 * ibmbits.c - bitmap bits accessing
 *
 * Maintainer: skywind3000 (at) gmail.com, 2009, 2010, 2011, 2013
 * Homepage: https://github.com/skywind3000/pixellib
 *
 * FEATURES:
 *
 * - up to 64 pixel-formats supported
 * - fetching pixel from any pixel-formats to card(A8R8G8B8)
 * - storing card(A8R8G8B8) to any pixel-formats
 * - drawing scanline (copy/blend/additive) to any pixel-formats
 * - blit (with/without color key) between bitmaps
 * - converting between any format to another one
 * - compositing scanline with 35 operators
 * - many useful macros to access/process pixels
 * - all the routines can be replaced by calling **_set_**
 * - using 203KB static memory for look-up-tables
 * - self contained, C89 compatible
 *
 * INTERFACES:
 *
 * - ipixel_get_fetch: get function to fetch pixels into A8R8G8B8.
 * - ipixel_get_store: get function to store pixels from A8R8G8B8.
 * - ipixel_get_span_proc: get function to draw scanline with cover.
 * - ipixel_get_hline_proc: get function to draw horizontal line.
 * - ipixel_blit: blit between two bitmaps with same color depth.
 * - ipixel_blend: blend between two bitmaps in arbitrary pixel-formats.
 * - ipixel_convert: convert pixel-format.
 * - ipixel_fmt_init: initialize customized pixel-format with rgb masks.
 * - ipixel_fmt_cvt: convert between customized pixel-formats.
 * - ipixel_composite_get: get scanline compositor with 35 operators.
 * - ipixel_clip: valid rectangle clip.
 * - ipixel_set_dots: batch draw dots from the pos array to a bitmap.
 * - ipixel_palette_fit: fint best fit color in the palette.
 * - ipixel_palette_fetch: fetch from color index buffer to A8R8G8B8.
 * - ipixel_palette_store: store from A8R8G8B8 to color index buffer.
 * - ipixel_card_reverse: reverse A8R8G8B8 scanline.
 * - ipixel_card_shuffle: color components shuffle.
 * - ipixel_card_multi: A8R8G8B8 scanline color multiplication.
 * - ipixel_card_cover: A8R8G8B8 scanline coverage.
 *
 * HISTORY:
 *
 * 2009.09.12  skywind  create this file
 * 2009.12.07  skywind  optimize fetch and store
 * 2009.12.30  skywind  optimize 16 bits convertion using lut
 * 2010.01.05  skywind  implement scanline blending & additive
 * 2010.03.07  skywind  implement hline blending & additive
 * 2010.10.25  skywind  implement bliting and converting and cliping
 * 2011.03.04  skywind  implement card operations
 * 2011.08.10  skywind  implement compositing
 * 2013.09.11  skywind  free format converting
 *
 **********************************************************************/

#include "ibmbits.h"


/**********************************************************************
 * GLOBAL VARIABLES
 **********************************************************************/

/* pixel format declare */
const struct IPIXELFMT ipixelfmt[64] =
{
    { IPIX_FMT_A8R8G8B8, 32, 1, 0, 4, "IPIX_FMT_A8R8G8B8",
      0xff0000, 0xff00, 0xff, 0xff000000, 16, 8, 0, 24, 0, 0, 0, 0 },
    { IPIX_FMT_A8B8G8R8, 32, 1, 0, 4, "IPIX_FMT_A8B8G8R8",
      0xff, 0xff00, 0xff0000, 0xff000000, 0, 8, 16, 24, 0, 0, 0, 0 },
    { IPIX_FMT_R8G8B8A8, 32, 1, 0, 4, "IPIX_FMT_R8G8B8A8",
      0xff000000, 0xff0000, 0xff00, 0xff, 24, 16, 8, 0, 0, 0, 0, 0 },
    { IPIX_FMT_B8G8R8A8, 32, 1, 0, 4, "IPIX_FMT_B8G8R8A8",
      0xff00, 0xff0000, 0xff000000, 0xff, 8, 16, 24, 0, 0, 0, 0, 0 },
    { IPIX_FMT_X8R8G8B8, 32, 0, 1, 4, "IPIX_FMT_X8R8G8B8",
      0xff0000, 0xff00, 0xff, 0x0, 16, 8, 0, 0, 0, 0, 0, 8 },
    { IPIX_FMT_X8B8G8R8, 32, 0, 1, 4, "IPIX_FMT_X8B8G8R8",
      0xff, 0xff00, 0xff0000, 0x0, 0, 8, 16, 0, 0, 0, 0, 8 },
    { IPIX_FMT_R8G8B8X8, 32, 0, 1, 4, "IPIX_FMT_R8G8B8X8",
      0xff000000, 0xff0000, 0xff00, 0x0, 24, 16, 8, 0, 0, 0, 0, 8 },
    { IPIX_FMT_B8G8R8X8, 32, 0, 1, 4, "IPIX_FMT_B8G8R8X8",
      0xff00, 0xff0000, 0xff000000, 0x0, 8, 16, 24, 0, 0, 0, 0, 8 },
    { IPIX_FMT_P8R8G8B8, 32, 1, 5, 4, "IPIX_FMT_P8R8G8B8",
      0xff0000, 0xff00, 0xff, 0x0, 16, 8, 0, 0, 0, 0, 0, 8 },
    { IPIX_FMT_R8G8B8, 24, 0, 1, 3, "IPIX_FMT_R8G8B8",
      0xff0000, 0xff00, 0xff, 0x0, 16, 8, 0, 0, 0, 0, 0, 8 },
    { IPIX_FMT_B8G8R8, 24, 0, 1, 3, "IPIX_FMT_B8G8R8",
      0xff, 0xff00, 0xff0000, 0x0, 0, 8, 16, 0, 0, 0, 0, 8 },
    { IPIX_FMT_R5G6B5, 16, 0, 1, 2, "IPIX_FMT_R5G6B5",
      0xf800, 0x7e0, 0x1f, 0x0, 11, 5, 0, 0, 3, 2, 3, 8 },
    { IPIX_FMT_B5G6R5, 16, 0, 1, 2, "IPIX_FMT_B5G6R5",
      0x1f, 0x7e0, 0xf800, 0x0, 0, 5, 11, 0, 3, 2, 3, 8 },
    { IPIX_FMT_X1R5G5B5, 16, 0, 1, 2, "IPIX_FMT_X1R5G5B5",
      0x7c00, 0x3e0, 0x1f, 0x0, 10, 5, 0, 0, 3, 3, 3, 8 },
    { IPIX_FMT_X1B5G5R5, 16, 0, 1, 2, "IPIX_FMT_X1B5G5R5",
      0x1f, 0x3e0, 0x7c00, 0x0, 0, 5, 10, 0, 3, 3, 3, 8 },
    { IPIX_FMT_R5G5B5X1, 16, 0, 1, 2, "IPIX_FMT_R5G5B5X1",
      0xf800, 0x7c0, 0x3e, 0x0, 11, 6, 1, 0, 3, 3, 3, 8 },
    { IPIX_FMT_B5G5R5X1, 16, 0, 1, 2, "IPIX_FMT_B5G5R5X1",
      0x3e, 0x7c0, 0xf800, 0x0, 1, 6, 11, 0, 3, 3, 3, 8 },
    { IPIX_FMT_A1R5G5B5, 16, 1, 0, 2, "IPIX_FMT_A1R5G5B5",
      0x7c00, 0x3e0, 0x1f, 0x8000, 10, 5, 0, 15, 3, 3, 3, 7 },
    { IPIX_FMT_A1B5G5R5, 16, 1, 0, 2, "IPIX_FMT_A1B5G5R5",
      0x1f, 0x3e0, 0x7c00, 0x8000, 0, 5, 10, 15, 3, 3, 3, 7 },
    { IPIX_FMT_R5G5B5A1, 16, 1, 0, 2, "IPIX_FMT_R5G5B5A1",
      0xf800, 0x7c0, 0x3e, 0x1, 11, 6, 1, 0, 3, 3, 3, 7 },
    { IPIX_FMT_B5G5R5A1, 16, 1, 0, 2, "IPIX_FMT_B5G5R5A1",
      0x3e, 0x7c0, 0xf800, 0x1, 1, 6, 11, 0, 3, 3, 3, 7 },
    { IPIX_FMT_X4R4G4B4, 16, 0, 1, 2, "IPIX_FMT_X4R4G4B4",
      0xf00, 0xf0, 0xf, 0x0, 8, 4, 0, 0, 4, 4, 4, 8 },
    { IPIX_FMT_X4B4G4R4, 16, 0, 1, 2, "IPIX_FMT_X4B4G4R4",
      0xf, 0xf0, 0xf00, 0x0, 0, 4, 8, 0, 4, 4, 4, 8 },
    { IPIX_FMT_R4G4B4X4, 16, 0, 1, 2, "IPIX_FMT_R4G4B4X4",
      0xf000, 0xf00, 0xf0, 0x0, 12, 8, 4, 0, 4, 4, 4, 8 },
    { IPIX_FMT_B4G4R4X4, 16, 0, 1, 2, "IPIX_FMT_B4G4R4X4",
      0xf0, 0xf00, 0xf000, 0x0, 4, 8, 12, 0, 4, 4, 4, 8 },
    { IPIX_FMT_A4R4G4B4, 16, 1, 0, 2, "IPIX_FMT_A4R4G4B4",
      0xf00, 0xf0, 0xf, 0xf000, 8, 4, 0, 12, 4, 4, 4, 4 },
    { IPIX_FMT_A4B4G4R4, 16, 1, 0, 2, "IPIX_FMT_A4B4G4R4",
      0xf, 0xf0, 0xf00, 0xf000, 0, 4, 8, 12, 4, 4, 4, 4 },
    { IPIX_FMT_R4G4B4A4, 16, 1, 0, 2, "IPIX_FMT_R4G4B4A4",
      0xf000, 0xf00, 0xf0, 0xf, 12, 8, 4, 0, 4, 4, 4, 4 },
    { IPIX_FMT_B4G4R4A4, 16, 1, 0, 2, "IPIX_FMT_B4G4R4A4",
      0xf0, 0xf00, 0xf000, 0xf, 4, 8, 12, 0, 4, 4, 4, 4 },
    { IPIX_FMT_C8, 8, 0, 2, 1, "IPIX_FMT_C8",
      0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 8, 8, 8, 8 },
    { IPIX_FMT_G8, 8, 0, 3, 1, "IPIX_FMT_G8",
      0x0, 0xff, 0x0, 0x0, 0, 0, 0, 0, 8, 0, 8, 8 },
    { IPIX_FMT_A8, 8, 1, 4, 1, "IPIX_FMT_A8",
      0x0, 0x0, 0x0, 0xff, 0, 0, 0, 0, 8, 8, 8, 0 },
    { IPIX_FMT_R3G3B2, 8, 0, 1, 1, "IPIX_FMT_R3G3B2",
      0xe0, 0x1c, 0x3, 0x0, 5, 2, 0, 0, 5, 5, 6, 8 },
    { IPIX_FMT_B2G3R3, 8, 0, 1, 1, "IPIX_FMT_B2G3R3",
      0x7, 0x38, 0xc0, 0x0, 0, 3, 6, 0, 5, 5, 6, 8 },
    { IPIX_FMT_X2R2G2B2, 8, 0, 1, 1, "IPIX_FMT_X2R2G2B2",
      0x30, 0xc, 0x3, 0x0, 4, 2, 0, 0, 6, 6, 6, 8 },
    { IPIX_FMT_X2B2G2R2, 8, 0, 1, 1, "IPIX_FMT_X2B2G2R2",
      0x3, 0xc, 0x30, 0x0, 0, 2, 4, 0, 6, 6, 6, 8 },
    { IPIX_FMT_R2G2B2X2, 8, 0, 1, 1, "IPIX_FMT_R2G2B2X2",
      0xc0, 0x30, 0xc, 0x0, 6, 4, 2, 0, 6, 6, 6, 8 },
    { IPIX_FMT_B2G2R2X2, 8, 0, 1, 1, "IPIX_FMT_B2G2R2X2",
      0xc, 0x30, 0xc0, 0x0, 2, 4, 6, 0, 6, 6, 6, 8 },
    { IPIX_FMT_A2R2G2B2, 8, 1, 0, 1, "IPIX_FMT_A2R2G2B2",
      0x30, 0xc, 0x3, 0xc0, 4, 2, 0, 6, 6, 6, 6, 6 },
    { IPIX_FMT_A2B2G2R2, 8, 1, 0, 1, "IPIX_FMT_A2B2G2R2",
      0x3, 0xc, 0x30, 0xc0, 0, 2, 4, 6, 6, 6, 6, 6 },
    { IPIX_FMT_R2G2B2A2, 8, 1, 0, 1, "IPIX_FMT_R2G2B2A2",
      0xc0, 0x30, 0xc, 0x3, 6, 4, 2, 0, 6, 6, 6, 6 },
    { IPIX_FMT_B2G2R2A2, 8, 1, 0, 1, "IPIX_FMT_B2G2R2A2",
      0xc, 0x30, 0xc0, 0x3, 2, 4, 6, 0, 6, 6, 6, 6 },
    { IPIX_FMT_X4C4, 8, 0, 2, 1, "IPIX_FMT_X4C4",
      0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 8, 8, 8, 8 },
    { IPIX_FMT_X4G4, 8, 0, 3, 1, "IPIX_FMT_X4G4",
      0x0, 0xf, 0x0, 0x0, 0, 0, 0, 0, 8, 4, 8, 8 },
    { IPIX_FMT_X4A4, 8, 1, 4, 1, "IPIX_FMT_X4A4",
      0x0, 0x0, 0x0, 0xf, 0, 0, 0, 0, 8, 8, 8, 4 },
    { IPIX_FMT_C4X4, 8, 0, 2, 1, "IPIX_FMT_C4X4",
      0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 8, 8, 8, 8 },
    { IPIX_FMT_G4X4, 8, 0, 3, 1, "IPIX_FMT_G4X4",
      0x0, 0xf0, 0x0, 0x0, 0, 4, 0, 0, 8, 4, 8, 8 },
    { IPIX_FMT_A4X4, 8, 1, 4, 1, "IPIX_FMT_A4X4",
      0x0, 0x0, 0x0, 0xf0, 0, 0, 0, 4, 8, 8, 8, 4 },
    { IPIX_FMT_C4, 4, 0, 2, 0, "IPIX_FMT_C4",
      0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 8, 8, 8, 8 },
    { IPIX_FMT_G4, 4, 0, 3, 0, "IPIX_FMT_G4",
      0x0, 0xf, 0x0, 0x0, 0, 0, 0, 0, 8, 4, 8, 8 },
    { IPIX_FMT_A4, 4, 1, 4, 0, "IPIX_FMT_A4",
      0x0, 0x0, 0x0, 0xf, 0, 0, 0, 0, 8, 8, 8, 4 },
    { IPIX_FMT_R1G2B1, 4, 0, 1, 0, "IPIX_FMT_R1G2B1",
      0x8, 0x6, 0x1, 0x0, 3, 1, 0, 0, 7, 6, 7, 8 },
    { IPIX_FMT_B1G2R1, 4, 0, 1, 0, "IPIX_FMT_B1G2R1",
      0x1, 0x6, 0x8, 0x0, 0, 1, 3, 0, 7, 6, 7, 8 },
    { IPIX_FMT_A1R1G1B1, 4, 1, 0, 0, "IPIX_FMT_A1R1G1B1",
      0x4, 0x2, 0x1, 0x8, 2, 1, 0, 3, 7, 7, 7, 7 },
    { IPIX_FMT_A1B1G1R1, 4, 1, 0, 0, "IPIX_FMT_A1B1G1R1",
      0x1, 0x2, 0x4, 0x8, 0, 1, 2, 3, 7, 7, 7, 7 },
    { IPIX_FMT_R1G1B1A1, 4, 1, 0, 0, "IPIX_FMT_R1G1B1A1",
      0x8, 0x4, 0x2, 0x1, 3, 2, 1, 0, 7, 7, 7, 7 },
    { IPIX_FMT_B1G1R1A1, 4, 1, 0, 0, "IPIX_FMT_B1G1R1A1",
      0x2, 0x4, 0x8, 0x1, 1, 2, 3, 0, 7, 7, 7, 7 },
    { IPIX_FMT_X1R1G1B1, 4, 0, 1, 0, "IPIX_FMT_X1R1G1B1",
      0x4, 0x2, 0x1, 0x0, 2, 1, 0, 0, 7, 7, 7, 8 },
    { IPIX_FMT_X1B1G1R1, 4, 0, 1, 0, "IPIX_FMT_X1B1G1R1",
      0x1, 0x2, 0x4, 0x0, 0, 1, 2, 0, 7, 7, 7, 8 },
    { IPIX_FMT_R1G1B1X1, 4, 0, 1, 0, "IPIX_FMT_R1G1B1X1",
      0x8, 0x4, 0x2, 0x0, 3, 2, 1, 0, 7, 7, 7, 8 },
    { IPIX_FMT_B1G1R1X1, 4, 0, 1, 0, "IPIX_FMT_B1G1R1X1",
      0x2, 0x4, 0x8, 0x0, 1, 2, 3, 0, 7, 7, 7, 8 },
    { IPIX_FMT_C1, 1, 0, 2, 0, "IPIX_FMT_C1",
      0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 8, 8, 8, 8 },
    { IPIX_FMT_G1, 1, 0, 3, 0, "IPIX_FMT_G1",
      0x0, 0x1, 0x0, 0x0, 0, 0, 0, 0, 8, 7, 8, 8 },
    { IPIX_FMT_A1, 1, 1, 4, 0, "IPIX_FMT_A1",
      0x0, 0x0, 0x0, 0x1, 0, 0, 0, 0, 8, 8, 8, 7 },
};


/* lookup table for scaling 1 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_1[2] = { 0, 255 };

/* lookup table for scaling 2 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_2[4] = { 0, 85, 170, 255 };

/* lookup table for scaling 3 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_3[8] = { 0, 36, 72, 109, 145, 182, 218, 255 };

/* lookup table for scaling 4 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_4[16] = 
{
    0, 16, 32, 49, 65, 82, 98, 115, 
    139, 156, 172, 189, 205, 222, 238, 255
};

/* lookup table for scaling 5 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_5[32] =
{
   0,   8,   16,  24,  32,  41,  49,  57,
   65,  74,  82,  90,  98,  106, 115, 123,
   131, 139, 148, 156, 164, 172, 180, 189,
   197, 205, 213, 222, 230, 238, 246, 255
};

/* lookup table for scaling 6 bit colors up to 8 bits */
const IUINT32 _ipixel_scale_6[64] =
{
   0,   4,   8,   12,  16,  20,  24,  28,
   32,  36,  40,  44,  48,  52,  56,  60,
   64,  68,  72,  76,  80,  85,  89,  93,
   97,  101, 105, 109, 113, 117, 121, 125,
   129, 133, 137, 141, 145, 149, 153, 157,
   161, 165, 170, 174, 178, 182, 186, 190,
   194, 198, 202, 206, 210, 214, 218, 222,
   226, 230, 234, 238, 242, 246, 250, 255
};

/* default color index */
static iColorIndex _ipixel_static_index[2];

iColorIndex *_ipixel_src_index = &_ipixel_static_index[0];
iColorIndex *_ipixel_dst_index = &_ipixel_static_index[1];

/* default palette */
IRGB _ipaletted[256];

/* 8 bits min/max saturation table */
const unsigned char IMINMAX256[770] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,
   14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
   29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
   44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
   59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,
   74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,
   89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
  119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
  134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
  149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
  164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178,
  179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193,
  194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208,
  209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
  224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
  239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253,
  254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255,
};

const unsigned char *iclip256 = &IMINMAX256[256];

unsigned char ipixel_blend_lut[2048 * 2];

unsigned char _ipixel_mullut[256][256];   /* [x][y] = x * y / 255 */
unsigned char _ipixel_divlut[256][256];   /* [x][y] = y * 255 / x */

unsigned char _ipixel_fmt_scale[9][256];   /* component scale for 0-8 bits */

/* memcpy hook */
void *(*ipixel_memcpy_hook)(void *dst, const void *src, size_t size) = NULL;

#if defined(__BORLANDC__) && !defined(__MSDOS__)
	#pragma warn -8004  
	#pragma warn -8057
#endif


/**********************************************************************
 * 256 PALETTE INTERFACE
 **********************************************************************/

/* find best fit color */
int ipixel_palette_fit(const IRGB *pal, int r, int g, int b, int palsize)
{ 
	static IUINT32 diff_lookup[512 * 3] = { 0 };
	long lowest = 0x7FFFFFFF, bestfit = 0;
	long coldiff, i, k;
	IRGB *rgb;

	/* calculate color difference lookup table:
	 * COLOR DIFF TABLE
	 * table1: diff_lookup[i | i = 256->511, n=0->(+255)] = (n * 30) ^ 2
	 * table2: diff_lookup[i | i = 256->1,   n=0->(-255)] = (n * 30) ^ 2
	 * result: f(n) = (n * 30) ^ 2 = diff_lookup[256 + n]
	 */
	if (diff_lookup[0] == 0) {
		for (i = 0; i < 256; i++) {
			k = i * i;
			diff_lookup[ 256 + i] = diff_lookup[ 256 - i] = k * 30 * 30;
			diff_lookup[ 768 + i] = diff_lookup[ 768 - i] = k * 59 * 59;
			diff_lookup[1280 + i] = diff_lookup[1280 - i] = k * 11 * 11;
		}
		diff_lookup[0] = 1;
	}

	/* range correction */
	r = r & 255;
	g = g & 255;
	b = b & 255;

	/*
	 * vector:   c1 = [r1, g1, b1], c2 = [r2, g2, b2]
	 * distance: dc = length(c1 - c2)
	 * coldiff:  dc^2 = (r1 - r2)^2 + (g1 - g2)^2 + (b1 - b2)^2
	 */
	for (i = palsize, rgb = (IRGB*)pal; i > 0; rgb++, i--) {
		coldiff  = diff_lookup[ 768 + rgb->g - g];
		if (coldiff >= lowest) continue;

		coldiff += diff_lookup[ 256 + rgb->r - r];
		if (coldiff >= lowest) continue;

		coldiff += diff_lookup[1280 + rgb->b - b];
		if (coldiff >= lowest) continue;

		bestfit = (int)(rgb - (IRGB*)pal); /* faster than `bestfit = i;' */
		if (coldiff == 0) return bestfit;
		lowest = coldiff;
	}

	return bestfit;
} 

/* convert palette to index */
int ipalette_to_index(iColorIndex *index, const IRGB *pal, int palsize)
{
	IUINT32 r, g, b, a;
	int i;
	index->color = palsize;
	for (i = 0; i < palsize; i++) {
		r = pal[i].r;
		g = pal[i].g;
		b = pal[i].b;
		a = 255;
		index->rgba[i] = IRGBA_TO_PIXEL(A8R8G8B8, r, g, b, a);
	}
	for (i = 0; i < 0x8000; i++) {
		IRGBA_FROM_PIXEL(X1R5G5B5, i, r, g, b, a);
		index->ent[i] = ipixel_palette_fit(pal, r, g, b, palsize);
	}
	return 0;
}


/* get raw color */
IUINT32 ipixel_assemble(int pixfmt, int r, int g, int b, int a)
{
	IUINT32 c;
	IRGBA_ASSEMBLE(pixfmt, c, r, g, b, a);
	return c;
}

/* get r, g, b, a from color */
void ipixel_desemble(int pixfmt, IUINT32 c, IINT32 *r, IINT32 *g, 
	IINT32 *b, IINT32 *a)
{
	IINT32 R, G, B, A;
	IRGBA_DISEMBLE(pixfmt, c, R, G, B, A);
	*r = R;
	*g = G;
	*b = B;
	*a = A;
}


/* stand along memcpy */
void *ipixel_memcpy(void *dst, const void *src, size_t size)
{
	if (ipixel_memcpy_hook) {
		return ipixel_memcpy_hook(dst, src, size);
	}

#ifndef IPIXEL_NO_MEMCPY
	return memcpy(dst, src, size);
#else
	unsigned char *dd = (unsigned char*)dst;
	const unsigned char *ss = (const unsigned char*)src;
	for (; size >= 8; size -= 8) {
		*(IUINT32*)(dd + 0) = *(const IUINT32*)(ss + 0);
		*(IUINT32*)(dd + 4) = *(const IUINT32*)(ss + 4);
		dd += 8;
		ss += 8;
	}
	for (; size > 0; size--) *dd++ = *ss++;
	return dst;
#endif
}


/**********************************************************************
 * Fast 1/2 byte -> 4 byte
 **********************************************************************/
void ipixel_lut_2_to_4(int sfmt, int dfmt, IUINT32 *table)
{
	IUINT32 c;

	if (ipixelfmt[sfmt].pixelbyte != 2 || ipixelfmt[dfmt].pixelbyte != 4) {
		assert(0);
		return;
	}

	for (c = 0; c < 256; c++) {
		IUINT32 r1, g1, b1, a1;
		IUINT32 r2, g2, b2, a2;
		IUINT32 c1, c2;
#if IWORDS_BIG_ENDIAN
		c1 = c << 8;
		c2 = c;
#else
		c1 = c;
		c2 = c << 8;
#endif
		IRGBA_DISEMBLE(sfmt, c1, r1, g1, b1, a1);
		IRGBA_DISEMBLE(sfmt, c2, r2, g2, b2, a2);
		IRGBA_ASSEMBLE(dfmt, c1, r1, g1, b1, a1);
		IRGBA_ASSEMBLE(dfmt, c2, r2, g2, b2, a2);
		table[c +   0] = c1;
		table[c + 256] = c2;
	}
}

void ipixel_lut_1_to_4(int sfmt, int dfmt, IUINT32 *table)
{
	IUINT32 c;

	if (ipixelfmt[sfmt].pixelbyte != 1 || ipixelfmt[dfmt].pixelbyte != 4) {
		assert(0);
		return;
	}

	for (c = 0; c < 256; c++) {
		IUINT32 c1, r1, g1, b1, a1;
		c1 = c;
		IRGBA_DISEMBLE(sfmt, c1, r1, g1, b1, a1);
		IRGBA_ASSEMBLE(dfmt, c1, r1, g1, b1, a1);
		table[c] = c1;
	}
}

void ipixel_lut_conv_2(IUINT32 *dst, const IUINT16 *src, int w, 
	const IUINT32 *lut)
{
	const IUINT8 *input = (const IUINT8*)src;
	IUINT32 c1, c2, c3, c4;
	ILINS_LOOP_DOUBLE(
		{
			c1 = lut[*input++ +   0];
			c2 = lut[*input++ + 256];
			*dst++ = c1 | c2;
		},
		{
			c1 = lut[*input++ +   0];
			c2 = lut[*input++ + 256];
			c3 = lut[*input++ +   0];
			c4 = lut[*input++ + 256];
			c1 |= c2;
			c3 |= c4;
			*dst++ = c1;
			*dst++ = c3;
		},
		w);
}

void ipixel_lut_conv_1(IUINT32 *dst, const IUINT16 *src, int w, 
	const IUINT32 *lut)
{
	const IUINT8 *input = (const IUINT8*)src;
	IUINT32 c1, c2;
	ILINS_LOOP_DOUBLE(
		{
			*dst++ = lut[*input++];
		},
		{
			c1 = lut[*input++];
			c2 = lut[*input++];
			*dst++ = c1;
			*dst++ = c2;
		},
		w);
}


/**********************************************************************
 * DEFAULT FETCH PROC
 **********************************************************************/
#define IFETCH_PROC(fmt, bpp) \
static void _ifetch_proc_##fmt(const void *bits, int x, \
    int w, IUINT32 *buffer, const iColorIndex *_ipixel_src_index) \
{ \
    IUINT32 c1, r1, g1, b1, a1, c2, r2, g2, b2, a2; \
    int y = x + 1; \
    ILINS_LOOP_DOUBLE( \
        { \
            c1 = _ipixel_fetch(bpp, bits, x); \
            x++; \
            y++; \
            IRGBA_FROM_PIXEL(fmt, c1, r1, g1, b1, a1); \
            c1 = IRGBA_TO_PIXEL(A8R8G8B8, r1, g1, b1, a1); \
            *buffer++ = c1; \
        }, \
        { \
            c1 = _ipixel_fetch(bpp, bits, x); \
            c2 = _ipixel_fetch(bpp, bits, y); \
            x += 2; \
            y += 2; \
            IRGBA_FROM_PIXEL(fmt, c1, r1, g1, b1, a1); \
            IRGBA_FROM_PIXEL(fmt, c2, r2, g2, b2, a2); \
            c1 = IRGBA_TO_PIXEL(A8R8G8B8, r1, g1, b1, a1); \
            c2 = IRGBA_TO_PIXEL(A8R8G8B8, r2, g2, b2, a2); \
            *buffer++ = c1; \
            *buffer++ = c2; \
        }, \
        w); \
    _ipixel_src_index = _ipixel_src_index; \
}


/**********************************************************************
 * DEFAULT STORE PROC
 **********************************************************************/
#define ISTORE_PROC(fmt, bpp) \
static void _istore_proc_##fmt(void *bits, const IUINT32 *values, \
    int x, int w, const iColorIndex *_ipixel_dst_index) \
{ \
    IUINT32 c1, r1, g1, b1, a1, c2, r2, g2, b2, a2; \
    int y = x + 1; \
    ILINS_LOOP_DOUBLE( \
        { \
            c1 = *values++; \
            IRGBA_FROM_PIXEL(A8R8G8B8, c1, r1, g1, b1, a1); \
            c1 = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
            _ipixel_store(bpp, bits, x, c1); \
            x++; \
            y++; \
        }, \
        { \
            c1 = *values++; \
            c2 = *values++; \
            IRGBA_FROM_PIXEL(A8R8G8B8, c1, r1, g1, b1, a1); \
            IRGBA_FROM_PIXEL(A8R8G8B8, c2, r2, g2, b2, a2); \
            c1 = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
            c2 = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
            _ipixel_store(bpp, bits, x, c1); \
            _ipixel_store(bpp, bits, y, c2); \
            x += 2; \
            y += 2; \
        }, \
        w); \
    _ipixel_dst_index = _ipixel_dst_index; \
	c1 = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2 + c2; \
}


/**********************************************************************
 * DEFAULT PIXEL FETCH PROC
 **********************************************************************/
#define IFETCH_PIXEL(fmt, bpp) \
static IUINT32 _ifetch_pixel_##fmt(const void *bits, \
    int offset, const iColorIndex *_ipixel_src_index) \
{ \
    IUINT32 c, r, g, b, a; \
    c = _ipixel_fetch(bpp, bits, offset); \
    IRGBA_FROM_PIXEL(fmt, c, r, g, b, a); \
    return IRGBA_TO_PIXEL(A8R8G8B8, r, g, b, a); \
}



/**********************************************************************
 * FETCHING PROCEDURES
 **********************************************************************/
static void _ifetch_proc_A8R8G8B8(const void *bits, int x, 
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	ipixel_memcpy(buffer, (const IUINT32*)bits + x, w * sizeof(IUINT32));
}

static void _ifetch_proc_A8B8G8R8(const void *bits, int x,
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	const IUINT32 *pixel = (const IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*buffer++ = ((*pixel & 0xff00ff00) |
				((*pixel & 0xff0000) >> 16) | ((*pixel & 0xff) << 16));
			pixel++;
		},
		{
			*buffer++ = ((*pixel & 0xff00ff00) |
				((*pixel & 0xff0000) >> 16) | ((*pixel & 0xff) << 16));
			pixel++;
			*buffer++ = ((*pixel & 0xff00ff00) |
				((*pixel & 0xff0000) >> 16) | ((*pixel & 0xff) << 16));
			pixel++;
		},
		w);
}

static void _ifetch_proc_R8G8B8A8(const void *bits, int x, 
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	const IUINT32 *pixel = (const IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*buffer++ = ((*pixel & 0xff) << 24) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
		},
		{
			*buffer++ = ((*pixel & 0xff) << 24) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
			*buffer++ = ((*pixel & 0xff) << 24) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
		},
		w);
}

IFETCH_PROC(B8G8R8A8, 32)

static void _ifetch_proc_X8R8G8B8(const void *bits, int x,
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	const IUINT32 *pixel = (const IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*buffer++ = *pixel++ | 0xff000000;
		},
		{
			*buffer++ = *pixel++ | 0xff000000;
			*buffer++ = *pixel++ | 0xff000000;
		},
		w);
}

IFETCH_PROC(X8B8G8R8, 32)

static void _ifetch_proc_R8G8B8X8(const void *bits, int x, 
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	const IUINT32 *pixel = (const IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*buffer++ = (0xff000000) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
		},
		{
			*buffer++ = (0xff000000) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
			*buffer++ = (0xff000000) |
				((*pixel & 0xffffff00) >> 8);
			pixel++;
		},
		w);
}

IFETCH_PROC(B8G8R8X8, 32)
IFETCH_PROC(P8R8G8B8, 32)

static void _ifetch_proc_R8G8B8(const void *bits, int x,
	int w, IUINT32 *buffer, const iColorIndex *idx)
{
	const IUINT8 *pixel = (const IUINT8*)bits + x * 3;
	ILINS_LOOP_DOUBLE( 
		{
			*buffer++ = IFETCH24(pixel) | 0xff000000;
			pixel += 3;
		},
		{
			*buffer++ = IFETCH24(pixel) | 0xff000000;
			pixel += 3;
			*buffer++ = IFETCH24(pixel) | 0xff000000;
			pixel += 3;
		},
		w);
}


IFETCH_PROC(B8G8R8, 24)

IFETCH_PROC(R5G6B5, 16)
IFETCH_PROC(B5G6R5, 16)
IFETCH_PROC(X1R5G5B5, 16)
IFETCH_PROC(X1B5G5R5, 16)
IFETCH_PROC(R5G5B5X1, 16)
IFETCH_PROC(B5G5R5X1, 16)
IFETCH_PROC(A1R5G5B5, 16)
IFETCH_PROC(A1B5G5R5, 16)
IFETCH_PROC(R5G5B5A1, 16)
IFETCH_PROC(B5G5R5A1, 16)
IFETCH_PROC(X4R4G4B4, 16)
IFETCH_PROC(X4B4G4R4, 16)
IFETCH_PROC(R4G4B4X4, 16)
IFETCH_PROC(B4G4R4X4, 16)
IFETCH_PROC(A4R4G4B4, 16)
IFETCH_PROC(A4B4G4R4, 16)
IFETCH_PROC(R4G4B4A4, 16)
IFETCH_PROC(B4G4R4A4, 16)
IFETCH_PROC(C8, 8)
IFETCH_PROC(G8, 8)
IFETCH_PROC(A8, 8)
IFETCH_PROC(R3G3B2, 8)
IFETCH_PROC(B2G3R3, 8)
IFETCH_PROC(X2R2G2B2, 8)
IFETCH_PROC(X2B2G2R2, 8)
IFETCH_PROC(R2G2B2X2, 8)
IFETCH_PROC(B2G2R2X2, 8)
IFETCH_PROC(A2R2G2B2, 8)
IFETCH_PROC(A2B2G2R2, 8)
IFETCH_PROC(R2G2B2A2, 8)
IFETCH_PROC(B2G2R2A2, 8)
IFETCH_PROC(X4C4, 8)
IFETCH_PROC(X4G4, 8)
IFETCH_PROC(X4A4, 8)
IFETCH_PROC(C4X4, 8)
IFETCH_PROC(G4X4, 8)
IFETCH_PROC(A4X4, 8)
IFETCH_PROC(C4, 4)
IFETCH_PROC(G4, 4)
IFETCH_PROC(A4, 4)
IFETCH_PROC(R1G2B1, 4)
IFETCH_PROC(B1G2R1, 4)
IFETCH_PROC(A1R1G1B1, 4)
IFETCH_PROC(A1B1G1R1, 4)
IFETCH_PROC(R1G1B1A1, 4)
IFETCH_PROC(B1G1R1A1, 4)
IFETCH_PROC(X1R1G1B1, 4)
IFETCH_PROC(X1B1G1R1, 4)
IFETCH_PROC(R1G1B1X1, 4)
IFETCH_PROC(B1G1R1X1, 4)
IFETCH_PROC(C1, 1)
IFETCH_PROC(G1, 1)
IFETCH_PROC(A1, 1)


/**********************************************************************
 * STORING PROCEDURES
 **********************************************************************/
static void _istore_proc_A8R8G8B8(void *bits, 
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	ipixel_memcpy(((IUINT32*)bits) + x, values, w * sizeof(IUINT32));
}

static void _istore_proc_A8B8G8R8(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT32 *pixel = (IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*pixel++ = (values[0] & 0xff00ff00) |
				((values[0] & 0xff0000) >> 16) | ((values[0] & 0xff) << 16);
			values++;
		},
		{
			*pixel++ = (values[0] & 0xff00ff00) |
				((values[0] & 0xff0000) >> 16) | ((values[0] & 0xff) << 16);
			values++;
			*pixel++ = (values[0] & 0xff00ff00) |
				((values[0] & 0xff0000) >> 16) | ((values[0] & 0xff) << 16);
			values++;
		},
		w);
}

static void _istore_proc_R8G8B8A8(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT32 *pixel = (IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*pixel++ = ((values[0] & 0xffffff) << 8) |
				((values[0] & 0xff000000) >> 24);
			values++;
		},
		{
			*pixel++ = ((values[0] & 0xffffff) << 8) |
				((values[0] & 0xff000000) >> 24);
			values++;
			*pixel++ = ((values[0] & 0xffffff) << 8) |
				((values[0] & 0xff000000) >> 24);
			values++;
		},
		w);
}

ISTORE_PROC(B8G8R8A8, 32)

static void _istore_proc_X8R8G8B8(void *bits, 
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT32 *pixel = (IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*pixel++ = values[0] & 0xffffff;
			values++;
		},
		{
			*pixel++ = values[0] & 0xffffff;
			values++;
			*pixel++ = values[0] & 0xffffff;
			values++;
		},
		w);
}

ISTORE_PROC(X8B8G8R8, 32)

static void _istore_proc_R8G8B8X8(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT32 *pixel = (IUINT32*)bits + x;
	ILINS_LOOP_DOUBLE( 
		{
			*pixel++ = ((values[0] & 0xffffff) << 8);
			values++;
		},
		{
			*pixel++ = ((values[0] & 0xffffff) << 8);
			values++;
			*pixel++ = ((values[0] & 0xffffff) << 8);
			values++;
		},
		w);
}

ISTORE_PROC(B8G8R8X8, 32)
ISTORE_PROC(P8R8G8B8, 32)


static void _istore_proc_R8G8B8(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT8 *pixel = (IUINT8*)bits + x * 3;
	IUINT32 c;
	int i;
	for (i = w; i > 0; i--) {
		c = *values++;
		ISTORE24(pixel, c);
		pixel += 3;
	}
}

static void _istore_proc_B8G8R8(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT8 *pixel = (IUINT8*)bits + x * 3;
	IUINT32 c;
	int i;
	for (i = w; i > 0; i--) {
		c = *values++;
		c = (c & 0x00ff00) | ((c & 0xff0000) >> 16) | ((c & 0xff) << 16);
		ISTORE24(pixel, c);
		pixel += 3;
	}
}

static void _istore_proc_R5G6B5(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT16 *pixel = (IUINT16*)bits + x;
	IUINT32 c1, c2, r1, g1, b1, r2, g2, b2;
	ILINS_LOOP_DOUBLE( 
		{
			c1 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			*pixel++ = (IUINT16) (((IUINT16)(r1 & 0xf8) << 8) |
								  ((IUINT16)(g1 & 0xfc) << 3) |
								  ((IUINT16)(b1 & 0xf8) >> 3));
		},
		{
			c1 = *values++;
			c2 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			ISPLIT_RGB(c2, r2, g2, b2);
			*pixel++ = (IUINT16) (((IUINT16)(r1 & 0xf8) << 8) |
								  ((IUINT16)(g1 & 0xfc) << 3) |
								  ((IUINT16)(b1 & 0xf8) >> 3));
			*pixel++ = (IUINT16) (((IUINT16)(r2 & 0xf8) << 8) |
								  ((IUINT16)(g2 & 0xfc) << 3) |
								  ((IUINT16)(b2 & 0xf8) >> 3));
		},
		w);
}

static void _istore_proc_B5G6R5(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT16 *pixel = (IUINT16*)bits + x;
	IUINT32 c1, c2, r1, g1, b1, r2, g2, b2;
	ILINS_LOOP_DOUBLE( 
		{
			c1 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			*pixel++ = (IUINT16) (((IUINT16)(b1 & 0xf8) << 8) |
								  ((IUINT16)(g1 & 0xfc) << 3) |
								  ((IUINT16)(r1 & 0xf8) >> 3));
		},
		{
			c1 = *values++;
			c2 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			ISPLIT_RGB(c2, r2, g2, b2);
			*pixel++ = (IUINT16) (((IUINT16)(b1 & 0xf8) << 8) |
								  ((IUINT16)(g1 & 0xfc) << 3) |
								  ((IUINT16)(r1 & 0xf8) >> 3));
			*pixel++ = (IUINT16) (((IUINT16)(b2 & 0xf8) << 8) |
								  ((IUINT16)(g2 & 0xfc) << 3) |
								  ((IUINT16)(r2 & 0xf8) >> 3));
		},
		w);
}

static void _istore_proc_X1R5G5B5(void *bits, 
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT16 *pixel = (IUINT16*)bits + x;
	IUINT32 c1, c2, r1, g1, b1, r2, g2, b2;
	ILINS_LOOP_DOUBLE( 
		{
			c1 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			*pixel++ = (IUINT16) (((IUINT16)(r1 & 0xf8) << 7) |
								  ((IUINT16)(g1 & 0xf8) << 2) |
								  ((IUINT16)(b1 & 0xf8) >> 3));
		},
		{
			c1 = *values++;
			c2 = *values++;
			ISPLIT_RGB(c1, r1, g1, b1);
			ISPLIT_RGB(c2, r2, g2, b2);
			*pixel++ = (IUINT16) (((IUINT16)(r1 & 0xf8) << 7) |
								  ((IUINT16)(g1 & 0xf8) << 2) |
								  ((IUINT16)(b1 & 0xf8) >> 3));
			*pixel++ = (IUINT16) (((IUINT16)(r2 & 0xf8) << 7) |
								  ((IUINT16)(g2 & 0xf8) << 2) |
								  ((IUINT16)(b2 & 0xf8) >> 3));
		},
		w);
}

ISTORE_PROC(X1B5G5R5, 16)
ISTORE_PROC(R5G5B5X1, 16)
ISTORE_PROC(B5G5R5X1, 16)

ISTORE_PROC(A1R5G5B5, 16)
ISTORE_PROC(A1B5G5R5, 16)
ISTORE_PROC(R5G5B5A1, 16)
ISTORE_PROC(B5G5R5A1, 16)
ISTORE_PROC(X4R4G4B4, 16)
ISTORE_PROC(X4B4G4R4, 16)
ISTORE_PROC(R4G4B4X4, 16)
ISTORE_PROC(B4G4R4X4, 16)

ISTORE_PROC(A4R4G4B4, 16)
ISTORE_PROC(A4B4G4R4, 16)

static void _istore_proc_R4G4B4A4(void *bits,
	const IUINT32 *values, int x, int w, const iColorIndex *idx)
{
	IUINT16 *pixel = (IUINT16*)bits + x;
	IUINT32 c, a, r, g, b;
	int i;
	for (i = w; i > 0; i--) {
		c = *values++;
		ISPLIT_ARGB(c, a, r, g, b);
		*pixel++ = ((IUINT16)( 
			((IUINT16)((r) & 0xf0) << 8) | 
			((IUINT16)((g) & 0xf0) << 4) | 
			((IUINT16)((b) & 0xf0) >> 0) | 
			((IUINT16)((a) & 0xf0) >> 4)));
	}
}

ISTORE_PROC(B4G4R4A4, 16)
ISTORE_PROC(C8, 8)
ISTORE_PROC(G8, 8)
ISTORE_PROC(A8, 8)
ISTORE_PROC(R3G3B2, 8)
ISTORE_PROC(B2G3R3, 8)
ISTORE_PROC(X2R2G2B2, 8)
ISTORE_PROC(X2B2G2R2, 8)
ISTORE_PROC(R2G2B2X2, 8)
ISTORE_PROC(B2G2R2X2, 8)
ISTORE_PROC(A2R2G2B2, 8)
ISTORE_PROC(A2B2G2R2, 8)
ISTORE_PROC(R2G2B2A2, 8)
ISTORE_PROC(B2G2R2A2, 8)
ISTORE_PROC(X4C4, 8)
ISTORE_PROC(X4G4, 8)
ISTORE_PROC(X4A4, 8)
ISTORE_PROC(C4X4, 8)
ISTORE_PROC(G4X4, 8)
ISTORE_PROC(A4X4, 8)
ISTORE_PROC(C4, 4)
ISTORE_PROC(G4, 4)
ISTORE_PROC(A4, 4)
ISTORE_PROC(R1G2B1, 4)
ISTORE_PROC(B1G2R1, 4)
ISTORE_PROC(A1R1G1B1, 4)
ISTORE_PROC(A1B1G1R1, 4)
ISTORE_PROC(R1G1B1A1, 4)
ISTORE_PROC(B1G1R1A1, 4)
ISTORE_PROC(X1R1G1B1, 4)
ISTORE_PROC(X1B1G1R1, 4)
ISTORE_PROC(R1G1B1X1, 4)
ISTORE_PROC(B1G1R1X1, 4)
ISTORE_PROC(C1, 1)
ISTORE_PROC(G1, 1)
ISTORE_PROC(A1, 1)


/**********************************************************************
 * PIXEL FETCHING PROCEDURES
 **********************************************************************/

static IUINT32 _ifetch_pixel_A8R8G8B8(const void *bits, 
    int offset, const iColorIndex *_ipixel_src_index) 
{ 
    return _ipixel_fetch(32, bits, offset); 
}

static IUINT32 _ifetch_pixel_A8B8G8R8(const void *bits,
	int offset, const iColorIndex *idx)
{
	IUINT32 pixel = ((const IUINT32*)(bits))[offset];
	return ((pixel & 0xff00ff00) | ((pixel & 0xff0000) >> 16) | 
		((pixel & 0xff) << 16));
}

static IUINT32 _ifetch_pixel_R8G8B8A8(const void *bits,
	int offset, const iColorIndex *idx)
{
	IUINT32 pixel = ((const IUINT32*)(bits))[offset];
	return ((pixel & 0xff) << 24) | ((pixel & 0xffffff00) >> 8);
}

static IUINT32 _ifetch_pixel_B8G8R8A8(const void *bits,
	int offset, const iColorIndex *idx)
{
	IUINT32 pixel = ((const IUINT32*)(bits))[offset];
	return		((pixel & 0x000000ff) << 24) |
				((pixel & 0x0000ff00) <<  8) |
				((pixel & 0x00ff0000) >>  8) |
				((pixel & 0xff000000) >> 24);
}


static IUINT32 _ifetch_pixel_X8R8G8B8(const void *bits,
	int offset, const iColorIndex *idx)
{
	return ((const IUINT32*)(bits))[offset] | 0xff000000;
}

static IUINT32 _ifetch_pixel_X8B8G8R8(const void *bits,
	int offset, const iColorIndex *idx)
{
	IUINT32 pixel = ((const IUINT32*)(bits))[offset];
	return ((pixel & 0x0000ff00) | ((pixel & 0xff0000) >> 16) | 
		((pixel & 0xff) << 16)) | 0xff000000;
}

IFETCH_PIXEL(R8G8B8X8, 32)
IFETCH_PIXEL(B8G8R8X8, 32)
IFETCH_PIXEL(P8R8G8B8, 32)

IFETCH_PIXEL(R8G8B8, 24)
IFETCH_PIXEL(B8G8R8, 24)
IFETCH_PIXEL(R5G6B5, 16)
IFETCH_PIXEL(B5G6R5, 16)
IFETCH_PIXEL(X1R5G5B5, 16)
IFETCH_PIXEL(X1B5G5R5, 16)
IFETCH_PIXEL(R5G5B5X1, 16)
IFETCH_PIXEL(B5G5R5X1, 16)
IFETCH_PIXEL(A1R5G5B5, 16)
IFETCH_PIXEL(A1B5G5R5, 16)
IFETCH_PIXEL(R5G5B5A1, 16)
IFETCH_PIXEL(B5G5R5A1, 16)
IFETCH_PIXEL(X4R4G4B4, 16)
IFETCH_PIXEL(X4B4G4R4, 16)
IFETCH_PIXEL(R4G4B4X4, 16)
IFETCH_PIXEL(B4G4R4X4, 16)
IFETCH_PIXEL(A4R4G4B4, 16)
IFETCH_PIXEL(A4B4G4R4, 16)
IFETCH_PIXEL(R4G4B4A4, 16)
IFETCH_PIXEL(B4G4R4A4, 16)
IFETCH_PIXEL(C8, 8)
IFETCH_PIXEL(G8, 8)
IFETCH_PIXEL(A8, 8)
IFETCH_PIXEL(R3G3B2, 8)
IFETCH_PIXEL(B2G3R3, 8)
IFETCH_PIXEL(X2R2G2B2, 8)
IFETCH_PIXEL(X2B2G2R2, 8)
IFETCH_PIXEL(R2G2B2X2, 8)
IFETCH_PIXEL(B2G2R2X2, 8)
IFETCH_PIXEL(A2R2G2B2, 8)
IFETCH_PIXEL(A2B2G2R2, 8)
IFETCH_PIXEL(R2G2B2A2, 8)
IFETCH_PIXEL(B2G2R2A2, 8)
IFETCH_PIXEL(X4C4, 8)
IFETCH_PIXEL(X4G4, 8)
IFETCH_PIXEL(X4A4, 8)
IFETCH_PIXEL(C4X4, 8)
IFETCH_PIXEL(G4X4, 8)
IFETCH_PIXEL(A4X4, 8)
IFETCH_PIXEL(C4, 4)
IFETCH_PIXEL(G4, 4)
IFETCH_PIXEL(A4, 4)
IFETCH_PIXEL(R1G2B1, 4)
IFETCH_PIXEL(B1G2R1, 4)
IFETCH_PIXEL(A1R1G1B1, 4)
IFETCH_PIXEL(A1B1G1R1, 4)
IFETCH_PIXEL(R1G1B1A1, 4)
IFETCH_PIXEL(B1G1R1A1, 4)
IFETCH_PIXEL(X1R1G1B1, 4)
IFETCH_PIXEL(X1B1G1R1, 4)
IFETCH_PIXEL(R1G1B1X1, 4)
IFETCH_PIXEL(B1G1R1X1, 4)
IFETCH_PIXEL(C1, 1)
IFETCH_PIXEL(G1, 1)
IFETCH_PIXEL(A1, 1)



/**********************************************************************
 * FETCHING STORING LOOK UP TABLE
 **********************************************************************/
struct iPixelAccessProc
{
    iFetchProc fetch, fetch_builtin;
    iStoreProc store, store_builtin;
    iFetchPixelProc fetchpixel, fetchpixel_builtin;
};

#define ITABLE_ITEM(fmt) { \
		_ifetch_proc_##fmt, _ifetch_proc_##fmt, \
		_istore_proc_##fmt, _istore_proc_##fmt, \
		_ifetch_pixel_##fmt, _ifetch_pixel_##fmt }


/* builtin look table */
static struct iPixelAccessProc ipixel_access_proc[IPIX_FMT_COUNT] =
{
	ITABLE_ITEM(A8R8G8B8),
	ITABLE_ITEM(A8B8G8R8),
	ITABLE_ITEM(R8G8B8A8),
	ITABLE_ITEM(B8G8R8A8),
	ITABLE_ITEM(X8R8G8B8),
	ITABLE_ITEM(X8B8G8R8),
	ITABLE_ITEM(R8G8B8X8),
	ITABLE_ITEM(B8G8R8X8),
	ITABLE_ITEM(P8R8G8B8),
	ITABLE_ITEM(R8G8B8),
	ITABLE_ITEM(B8G8R8),
	ITABLE_ITEM(R5G6B5),
	ITABLE_ITEM(B5G6R5),
	ITABLE_ITEM(X1R5G5B5),
	ITABLE_ITEM(X1B5G5R5),
	ITABLE_ITEM(R5G5B5X1),
	ITABLE_ITEM(B5G5R5X1),
	ITABLE_ITEM(A1R5G5B5),
	ITABLE_ITEM(A1B5G5R5),
	ITABLE_ITEM(R5G5B5A1),
	ITABLE_ITEM(B5G5R5A1),
	ITABLE_ITEM(X4R4G4B4),
	ITABLE_ITEM(X4B4G4R4),
	ITABLE_ITEM(R4G4B4X4),
	ITABLE_ITEM(B4G4R4X4),
	ITABLE_ITEM(A4R4G4B4),
	ITABLE_ITEM(A4B4G4R4),
	ITABLE_ITEM(R4G4B4A4),
	ITABLE_ITEM(B4G4R4A4),
	ITABLE_ITEM(C8),
	ITABLE_ITEM(G8),
	ITABLE_ITEM(A8),
	ITABLE_ITEM(R3G3B2),
	ITABLE_ITEM(B2G3R3),
	ITABLE_ITEM(X2R2G2B2),
	ITABLE_ITEM(X2B2G2R2),
	ITABLE_ITEM(R2G2B2X2),
	ITABLE_ITEM(B2G2R2X2),
	ITABLE_ITEM(A2R2G2B2),
	ITABLE_ITEM(A2B2G2R2),
	ITABLE_ITEM(R2G2B2A2),
	ITABLE_ITEM(B2G2R2A2),
	ITABLE_ITEM(X4C4),
	ITABLE_ITEM(X4G4),
	ITABLE_ITEM(X4A4),
	ITABLE_ITEM(C4X4),
	ITABLE_ITEM(G4X4),
	ITABLE_ITEM(A4X4),
	ITABLE_ITEM(C4),
	ITABLE_ITEM(G4),
	ITABLE_ITEM(A4),
	ITABLE_ITEM(R1G2B1),
	ITABLE_ITEM(B1G2R1),
	ITABLE_ITEM(A1R1G1B1),
	ITABLE_ITEM(A1B1G1R1),
	ITABLE_ITEM(R1G1B1A1),
	ITABLE_ITEM(B1G1R1A1),
	ITABLE_ITEM(X1R1G1B1),
	ITABLE_ITEM(X1B1G1R1),
	ITABLE_ITEM(R1G1B1X1),
	ITABLE_ITEM(B1G1R1X1),
	ITABLE_ITEM(C1),
	ITABLE_ITEM(G1),
	ITABLE_ITEM(A1),
};


#undef ITABLE_ITEM
#undef IFETCH_PROC
#undef ISTORE_PROC
#undef IFETCH_PIXEL


/* set procedure */
void ipixel_set_proc(int pixfmt, int type, void *proc)
{
	assert(pixfmt >= 0 && pixfmt < IPIX_FMT_COUNT);
	if (pixfmt < 0 || pixfmt >= IPIX_FMT_COUNT) return;
	if (type == IPIXEL_PROC_TYPE_FETCH) {
		if (proc != NULL) {
			ipixel_access_proc[pixfmt].fetch = (iFetchProc)proc;
		}	else {
			ipixel_access_proc[pixfmt].fetch = 
				ipixel_access_proc[pixfmt].fetch_builtin;
		}
	}
	else if (type == IPIXEL_PROC_TYPE_STORE) {
		if (proc != NULL) {
			ipixel_access_proc[pixfmt].store = (iStoreProc)proc;
		}	else {
			ipixel_access_proc[pixfmt].store = 
				ipixel_access_proc[pixfmt].store_builtin;
		}
	}
	else if (type == IPIXEL_PROC_TYPE_FETCHPIXEL) {
		if (proc != NULL) {
			ipixel_access_proc[pixfmt].fetchpixel = (iFetchPixelProc)proc;
		}	else {
			ipixel_access_proc[pixfmt].fetchpixel = 
				ipixel_access_proc[pixfmt].fetchpixel_builtin;
		}
	}
}



/**********************************************************************
 * FETCHING STORING LOOK UP TABLE
 **********************************************************************/
#define IFETCH_LUT_2(sfmt) \
IUINT32 _ipixel_cvt_lut_##sfmt[256 * 2]; \
static void _ifetch_proc_lut_##sfmt(const void *bits, int x, \
    int w, IUINT32 *buffer, const iColorIndex *idx) \
{ \
	const IUINT8 *input = (const IUINT8*)bits + (x << 1); \
	IUINT32 c1, c2, c3, c4; \
	ILINS_LOOP_DOUBLE( \
		{ \
			c1 = _ipixel_cvt_lut_##sfmt[*input++ +   0]; \
			c2 = _ipixel_cvt_lut_##sfmt[*input++ + 256]; \
			*buffer++ = c1 | c2; \
		}, \
		{ \
			c1 = _ipixel_cvt_lut_##sfmt[*input++ +   0]; \
			c2 = _ipixel_cvt_lut_##sfmt[*input++ + 256]; \
			c3 = _ipixel_cvt_lut_##sfmt[*input++ +   0]; \
			c4 = _ipixel_cvt_lut_##sfmt[*input++ + 256]; \
			c1 |= c2; \
			c3 |= c4; \
			*buffer++ = c1; \
			*buffer++ = c3; \
		}, \
		w); \
} \
static IUINT32 _ifetch_pixel_lut_##sfmt(const void *bits, \
    int offset, const iColorIndex *idx) \
{ \
	const IUINT8 *input = (const IUINT8*)bits + (offset << 1); \
	IUINT32 c1, c2; \
	c1 = _ipixel_cvt_lut_##sfmt[*input++ +   0]; \
	c2 = _ipixel_cvt_lut_##sfmt[*input++ + 256]; \
	return c1 | c2; \
}


#define IFETCH_LUT_1(sfmt) \
IUINT32 _ipixel_cvt_lut_##sfmt[256]; \
static void _ifetch_proc_lut_##sfmt(const void *bits, int x, \
    int w, IUINT32 *buffer, const iColorIndex *idx) \
{ \
	const IUINT8 *input = (const IUINT8*)bits + x; \
	IUINT32 c1, c2; \
	ILINS_LOOP_DOUBLE( \
		{ \
			c1 = _ipixel_cvt_lut_##sfmt[*input++]; \
			*buffer++ = c1; \
		}, \
		{ \
			c1 = _ipixel_cvt_lut_##sfmt[*input++]; \
			c2 = _ipixel_cvt_lut_##sfmt[*input++]; \
			*buffer++ = c1; \
			*buffer++ = c2; \
		}, \
		w); \
} \
static IUINT32 _ifetch_pixel_lut_##sfmt(const void *bits, \
    int offset, const iColorIndex *idx) \
{ \
	const IUINT8 *input = (const IUINT8*)bits + offset; \
	return _ipixel_cvt_lut_##sfmt[*input]; \
}

#define IFETCH_LUT_MAIN(sfmt, nbytes) \
	IFETCH_LUT_##nbytes(sfmt)


IFETCH_LUT_MAIN(R5G6B5, 2)
IFETCH_LUT_MAIN(B5G6R5, 2)
IFETCH_LUT_MAIN(X1R5G5B5, 2)
IFETCH_LUT_MAIN(X1B5G5R5, 2)
IFETCH_LUT_MAIN(R5G5B5X1, 2)
IFETCH_LUT_MAIN(B5G5R5X1, 2)
IFETCH_LUT_MAIN(A1R5G5B5, 2)
IFETCH_LUT_MAIN(A1B5G5R5, 2)
IFETCH_LUT_MAIN(R5G5B5A1, 2)
IFETCH_LUT_MAIN(B5G5R5A1, 2)
IFETCH_LUT_MAIN(X4R4G4B4, 2)
IFETCH_LUT_MAIN(X4B4G4R4, 2)
IFETCH_LUT_MAIN(R4G4B4X4, 2)
IFETCH_LUT_MAIN(B4G4R4X4, 2)
IFETCH_LUT_MAIN(A4R4G4B4, 2)
IFETCH_LUT_MAIN(A4B4G4R4, 2)
IFETCH_LUT_MAIN(R4G4B4A4, 2)
IFETCH_LUT_MAIN(B4G4R4A4, 2)
IFETCH_LUT_MAIN(R3G3B2, 1)
IFETCH_LUT_MAIN(B2G3R3, 1)
IFETCH_LUT_MAIN(X2R2G2B2, 1)
IFETCH_LUT_MAIN(X2B2G2R2, 1)
IFETCH_LUT_MAIN(R2G2B2X2, 1)
IFETCH_LUT_MAIN(B2G2R2X2, 1)
IFETCH_LUT_MAIN(A2R2G2B2, 1)
IFETCH_LUT_MAIN(A2B2G2R2, 1)
IFETCH_LUT_MAIN(R2G2B2A2, 1)
IFETCH_LUT_MAIN(B2G2R2A2, 1)


struct iPixelAccessLutTable
{
	int fmt;
	iFetchProc fetch;
	iFetchPixelProc pixel;
};


#define ITABLE_ITEM(fmt) \
	{ IPIX_FMT_##fmt, _ifetch_proc_lut_##fmt, _ifetch_pixel_lut_##fmt }

static struct iPixelAccessLutTable ipixel_access_lut[28] =
{
	ITABLE_ITEM(R5G6B5),
	ITABLE_ITEM(B5G6R5),
	ITABLE_ITEM(X1R5G5B5),
	ITABLE_ITEM(X1B5G5R5),
	ITABLE_ITEM(R5G5B5X1),
	ITABLE_ITEM(B5G5R5X1),
	ITABLE_ITEM(A1R5G5B5),
	ITABLE_ITEM(A1B5G5R5),
	ITABLE_ITEM(R5G5B5A1),
	ITABLE_ITEM(B5G5R5A1),
	ITABLE_ITEM(X4R4G4B4),
	ITABLE_ITEM(X4B4G4R4),
	ITABLE_ITEM(R4G4B4X4),
	ITABLE_ITEM(B4G4R4X4),
	ITABLE_ITEM(A4R4G4B4),
	ITABLE_ITEM(A4B4G4R4),
	ITABLE_ITEM(R4G4B4A4),
	ITABLE_ITEM(B4G4R4A4),
	ITABLE_ITEM(R3G3B2),
	ITABLE_ITEM(B2G3R3),
	ITABLE_ITEM(X2R2G2B2),
	ITABLE_ITEM(X2B2G2R2),
	ITABLE_ITEM(R2G2B2X2),
	ITABLE_ITEM(B2G2R2X2),
	ITABLE_ITEM(A2R2G2B2),
	ITABLE_ITEM(A2B2G2R2),
	ITABLE_ITEM(R2G2B2A2),
	ITABLE_ITEM(B2G2R2A2),
};

static const int ipixel_access_lut_fmt[IPIX_FMT_COUNT] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 
	9, 10, 11, 12, 13, 14, 15, 16, 17, -1, -1, -1, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1 
};


#undef ITABLE_ITEM
#undef IFETCH_LUT_2
#undef IFETCH_LUT_1
#undef IFETCH_LUT_MAIN

static int ipixel_lut_inited = 0;

void ipixel_lut_init(void)
{
	int i, j, k;

	if (ipixel_lut_inited != 0) return;

	#define IPIXEL_LUT_INIT(fmt, nbytes) \
		ipixel_lut_##nbytes##_to_4(IPIX_FMT_##fmt, IPIX_FMT_A8R8G8B8, \
			_ipixel_cvt_lut_##fmt)

	IPIXEL_LUT_INIT(R5G6B5, 2);
	IPIXEL_LUT_INIT(B5G6R5, 2);
	IPIXEL_LUT_INIT(X1R5G5B5, 2);
	IPIXEL_LUT_INIT(X1B5G5R5, 2);
	IPIXEL_LUT_INIT(R5G5B5X1, 2);
	IPIXEL_LUT_INIT(B5G5R5X1, 2);
	IPIXEL_LUT_INIT(A1R5G5B5, 2);
	IPIXEL_LUT_INIT(A1B5G5R5, 2);
	IPIXEL_LUT_INIT(R5G5B5A1, 2);
	IPIXEL_LUT_INIT(B5G5R5A1, 2);
	IPIXEL_LUT_INIT(X4R4G4B4, 2);
	IPIXEL_LUT_INIT(X4B4G4R4, 2);
	IPIXEL_LUT_INIT(R4G4B4X4, 2);
	IPIXEL_LUT_INIT(B4G4R4X4, 2);
	IPIXEL_LUT_INIT(A4R4G4B4, 2);
	IPIXEL_LUT_INIT(A4B4G4R4, 2);
	IPIXEL_LUT_INIT(R4G4B4A4, 2);
	IPIXEL_LUT_INIT(B4G4R4A4, 2);
	IPIXEL_LUT_INIT(R3G3B2, 1);
	IPIXEL_LUT_INIT(B2G3R3, 1);
	IPIXEL_LUT_INIT(X2R2G2B2, 1);
	IPIXEL_LUT_INIT(X2B2G2R2, 1);
	IPIXEL_LUT_INIT(R2G2B2X2, 1);
	IPIXEL_LUT_INIT(B2G2R2X2, 1);
	IPIXEL_LUT_INIT(A2R2G2B2, 1);
	IPIXEL_LUT_INIT(A2B2G2R2, 1);
	IPIXEL_LUT_INIT(R2G2B2A2, 1);
	IPIXEL_LUT_INIT(B2G2R2A2, 1);

	#undef IPIXEL_LUT_INIT
	
	for (i = 0; i < 2048; i++) {
		IUINT32 da = _ipixel_scale_5[i >> 6];
		IUINT32 sa = _ipixel_scale_6[i & 63];
		IUINT32 FA = da + ((255 - da) * sa) / 255;
		IUINT32 SA = (FA != 0)? ((sa * 255) / FA) : 0;
		ipixel_blend_lut[i * 2 + 0] = (unsigned char)SA;
		ipixel_blend_lut[i * 2 + 1] = (unsigned char)FA;
	}

	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++) {
			k = i * j;
			_ipixel_mullut[i][j] = (IUINT8)_ipixel_fast_div_255(k);
		}
	}
	for (i = 0; i < 256; i++) {
		_ipixel_divlut[0][i] = 0;
		for (j = 1; j < i; j++) {
			_ipixel_divlut[i][j] = (IUINT8)((j * 255) / i);
		}
		for (j = i; j < 256; j++) {
			_ipixel_divlut[i][j] = 255;
		}
	}

	ipixel_lut_inited = 1;
}

/* get color fetching procedure */
iFetchProc ipixel_get_fetch(int pixfmt, int access_mode)
{
	assert(pixfmt >= 0 && pixfmt < IPIX_FMT_COUNT);
	if (pixfmt < 0 || pixfmt >= IPIX_FMT_COUNT) return NULL;
	if (access_mode == IPIXEL_ACCESS_MODE_NORMAL) {
		int id = ipixel_access_lut_fmt[pixfmt];
		if (ipixel_lut_inited == 0) ipixel_lut_init();
		if (id >= 0) return ipixel_access_lut[id].fetch;
		return ipixel_access_proc[pixfmt].fetch;
	}
	if (access_mode == IPIXEL_ACCESS_MODE_ACCURATE) {
		return ipixel_access_proc[pixfmt].fetch;
	}
	return ipixel_access_proc[pixfmt].fetch_builtin;
}

/* get color storing procedure */
iStoreProc ipixel_get_store(int pixfmt, int access_mode)
{
	assert(pixfmt >= 0 && pixfmt < IPIX_FMT_COUNT);
	if (pixfmt < 0 || pixfmt >= IPIX_FMT_COUNT) return NULL;
	if (access_mode == IPIXEL_ACCESS_MODE_NORMAL) {
		if (ipixel_lut_inited == 0) ipixel_lut_init();
		return ipixel_access_proc[pixfmt].store;
	}
	if (access_mode == IPIXEL_ACCESS_MODE_ACCURATE) {
		return ipixel_access_proc[pixfmt].store;
	}
	return ipixel_access_proc[pixfmt].store_builtin;
}

/* get color pixel fetching procedure */
iFetchPixelProc ipixel_get_fetchpixel(int pixfmt, int access_mode)
{
	assert(pixfmt >= 0 && pixfmt < IPIX_FMT_COUNT);
	if (pixfmt < 0 || pixfmt >= IPIX_FMT_COUNT) return NULL;
	if (access_mode == IPIXEL_ACCESS_MODE_NORMAL) {
		int id = ipixel_access_lut_fmt[pixfmt];
		if (ipixel_lut_inited == 0) ipixel_lut_init();
		if (id >= 0) return ipixel_access_lut[id].pixel;
		return ipixel_access_proc[pixfmt].fetchpixel;
	}
	if (access_mode == IPIXEL_ACCESS_MODE_ACCURATE) {
		return ipixel_access_proc[pixfmt].fetchpixel;
	}
	return ipixel_access_proc[pixfmt].fetchpixel_builtin;
}


/* returns pixel format names */
const char *ipixelfmt_name(int fmt)
{
	return ipixelfmt[fmt].name;
}


/**********************************************************************
 * SPAN DRAWING
 **********************************************************************/
/* span blending for 8, 16, 24, 32 bits */
#define IPIXEL_SPAN_DRAW_PROC_N(fmt, bpp, nbytes, mode) \
static void ipixel_span_draw_proc_##fmt##_0(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_1(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				r1 = _imul_y_div_255(r1, cc); \
				g1 = _imul_y_div_255(g1, cc); \
				b1 = _imul_y_div_255(b1, cc); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_2(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			if (a1 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst += nbytes; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
}

/* span blending for 8 bits without palette */
#define IPIXEL_SPAN_DRAW_PROC_1(fmt, bpp, nbytes, mode) \
static void ipixel_span_draw_proc_##fmt##_0(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + (offset); \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (a1 > 0) { \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_1(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + (offset); \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (a1 > 0) { \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				r1 = _imul_y_div_255(r1, cc); \
				g1 = _imul_y_div_255(g1, cc); \
				b1 = _imul_y_div_255(b1, cc); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_2(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + (offset); \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	if (cover == NULL) { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 > 0) { \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	}	else { \
		for (inc = w; inc > 0; inc--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			if (a1 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				r1 = dst[0]; \
				cc = _ipixel_cvt_lut_##fmt[r1]; \
				IRGBA_FROM_PIXEL(A8R8G8B8, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
			card++; \
			dst++; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
}

/* span blending for 8/4/1 bits with or without palette */
#define IPIXEL_SPAN_DRAW_PROC_X(fmt, bpp, nbytes, mode, init) \
static void ipixel_span_draw_proc_##fmt##_0(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	unsigned char *dst = (unsigned char*)bits; \
	init; \
	if (cover == NULL) { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			else if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	}	else { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_1(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	unsigned char *dst = (unsigned char*)bits; \
	init; \
	if (cover == NULL) { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 == 255) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			else if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	}	else { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			r2 = a1 + cc; \
			if (r2 == 510) { \
				cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			else if (r2 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				r1 = _imul_y_div_255(r1, cc); \
				g1 = _imul_y_div_255(g1, cc); \
				b1 = _imul_y_div_255(b1, cc); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_span_draw_proc_##fmt##_2(void *bits, \
	int offset, int w, const IUINT32 *card, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	IUINT32 cc, r1, g1, b1, a1, r2, g2, b2, a2, inc; \
	unsigned char *dst = (unsigned char*)bits; \
	init; \
	if (cover == NULL) { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			if (a1 > 0) { \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	}	else { \
		for (inc = offset; w > 0; inc++, w--) { \
			_ipixel_load_card(card, r1, g1, b1, a1); \
			cc = *cover++; \
			if (r1 > 0 && cc > 0) { \
				a1 = _imul_y_div_255(a1, cc); \
				cc = _ipixel_fetch(bpp, dst, inc); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, inc, cc); \
			} \
			card++; \
		} \
	} \
}

/* span blending for 4/1 bits without palette */
#define IPIXEL_SPAN_DRAW_PROC_BITS(fmt, bpp, nbytes, mode) \
		IPIXEL_SPAN_DRAW_PROC_X(fmt, bpp, nbytes, mode, {}) 

/* span blending for 8/4/1 bits with palette */
#define IPIXEL_SPAN_DRAW_PROC_PAL(fmt, bpp, nbytes, mode) \
		IPIXEL_SPAN_DRAW_PROC_X(fmt, bpp, nbytes, mode, \
			const iColorIndex *_ipixel_dst_index = _ipixel_src_index)

/* span blending main */
#define IPIXEL_SPAN_DRAW_MAIN(type, fmt, bpp, nbytes, mode) \
	IPIXEL_SPAN_DRAW_PROC_##type(fmt, bpp, nbytes, mode) 

/* span blending procedures declare */
IPIXEL_SPAN_DRAW_MAIN(N, A8R8G8B8, 32, 4, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, A8B8G8R8, 32, 4, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, R8G8B8A8, 32, 4, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, B8G8R8A8, 32, 4, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, X8R8G8B8, 32, 4, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X8B8G8R8, 32, 4, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, R8G8B8X8, 32, 4, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, B8G8R8X8, 32, 4, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, P8R8G8B8, 32, 4, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, R8G8B8, 24, 3, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, B8G8R8, 24, 3, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, R5G6B5, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, B5G6R5, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X1R5G5B5, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X1B5G5R5, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, R5G5B5X1, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, B5G5R5X1, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, A1R5G5B5, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, A1B5G5R5, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, R5G5B5A1, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, B5G5R5A1, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, X4R4G4B4, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X4B4G4R4, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, R4G4B4X4, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, B4G4R4X4, 16, 2, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, A4R4G4B4, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, A4B4G4R4, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, R4G4B4A4, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(N, B4G4R4A4, 16, 2, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(PAL, C8, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, G8, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, A8, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(1, R3G3B2, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, B2G3R3, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, X2R2G2B2, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, X2B2G2R2, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, R2G2B2X2, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, B2G2R2X2, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(1, A2R2G2B2, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(1, A2B2G2R2, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(1, R2G2B2A2, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(1, B2G2R2A2, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(PAL, X4C4, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X4G4, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, X4A4, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(PAL, C4X4, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, G4X4, 8, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(N, A4X4, 8, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(PAL, C4, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, G4, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, A4, 4, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(BITS, R1G2B1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, B1G2R1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, A1R1G1B1, 4, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(BITS, A1B1G1R1, 4, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(BITS, R1G1B1A1, 4, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(BITS, B1G1R1A1, 4, 1, NORMAL_FAST)
IPIXEL_SPAN_DRAW_MAIN(BITS, X1R1G1B1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, X1B1G1R1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, R1G1B1X1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, B1G1R1X1, 4, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(PAL, C1, 1, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, G1, 1, 1, STATIC)
IPIXEL_SPAN_DRAW_MAIN(BITS, A1, 1, 1, NORMAL_FAST)


/* draw span over in A8R8G8B8 or X8R8G8B8 */
static void ipixel_span_draw_proc_over_32(void *bits,
	int offset, int w, const IUINT32 *card, const IUINT8 *cover,
	const iColorIndex *_ipixel_src_index)
{
	IUINT32 *dst = ((IUINT32*)bits) + offset;
	IUINT32 alpha, cc;
	int inc;
	if (cover == NULL) {
		for (inc = w; inc > 0; inc--) {
			alpha = card[0] >> 24;
			if (alpha == 255) {
				dst[0] = card[0];
			}
			else if (alpha > 0) {
				IBLEND_PARGB(dst[0], card[0]);
			}
			card++;
			dst++;
		}
	}	else {
		for (inc = w; inc > 0; inc--) {
			alpha = card[0] >> 24;
			cc = cover[0];
			if (cc + alpha == 510) {
				dst[0] = card[0];
			}
			else if (cc && alpha) {
				IBLEND_PARGB_COVER(dst[0], card[0], cc);
			}
			card++;
			dst++;
			cover++;
		}
	}
}


#undef IPIXEL_SPAN_DRAW_MAIN
#undef IPIXEL_SPAN_DRAW_PROC_N
#undef IPIXEL_SPAN_DRAW_PROC_X
#undef IPIXEL_SPAN_DRAW_PROC_1
#undef IPIXEL_SPAN_DRAW_PROC_BITS
#undef IPIXEL_SPAN_DRAW_PROC_PAL


struct iPixelSpanDrawProc
{
	iSpanDrawProc blend, srcover, additive;
	iSpanDrawProc blend_builtin, srcover_builtin, additive_builtin;
};

#define ITABLE_ITEM(fmt) { \
	ipixel_span_draw_proc_##fmt##_0, ipixel_span_draw_proc_##fmt##_1, \
	ipixel_span_draw_proc_##fmt##_2, ipixel_span_draw_proc_##fmt##_0, \
	ipixel_span_draw_proc_##fmt##_1, ipixel_span_draw_proc_##fmt##_2 }

static struct iPixelSpanDrawProc ipixel_span_proc_list[IPIX_FMT_COUNT] =
{
	ITABLE_ITEM(A8R8G8B8),
	ITABLE_ITEM(A8B8G8R8),
	ITABLE_ITEM(R8G8B8A8),
	ITABLE_ITEM(B8G8R8A8),
	ITABLE_ITEM(X8R8G8B8),
	ITABLE_ITEM(X8B8G8R8),
	ITABLE_ITEM(R8G8B8X8),
	ITABLE_ITEM(B8G8R8X8),
	ITABLE_ITEM(P8R8G8B8),
	ITABLE_ITEM(R8G8B8),
	ITABLE_ITEM(B8G8R8),
	ITABLE_ITEM(R5G6B5),
	ITABLE_ITEM(B5G6R5),
	ITABLE_ITEM(X1R5G5B5),
	ITABLE_ITEM(X1B5G5R5),
	ITABLE_ITEM(R5G5B5X1),
	ITABLE_ITEM(B5G5R5X1),
	ITABLE_ITEM(A1R5G5B5),
	ITABLE_ITEM(A1B5G5R5),
	ITABLE_ITEM(R5G5B5A1),
	ITABLE_ITEM(B5G5R5A1),
	ITABLE_ITEM(X4R4G4B4),
	ITABLE_ITEM(X4B4G4R4),
	ITABLE_ITEM(R4G4B4X4),
	ITABLE_ITEM(B4G4R4X4),
	ITABLE_ITEM(A4R4G4B4),
	ITABLE_ITEM(A4B4G4R4),
	ITABLE_ITEM(R4G4B4A4),
	ITABLE_ITEM(B4G4R4A4),
	ITABLE_ITEM(C8),
	ITABLE_ITEM(G8),
	ITABLE_ITEM(A8),
	ITABLE_ITEM(R3G3B2),
	ITABLE_ITEM(B2G3R3),
	ITABLE_ITEM(X2R2G2B2),
	ITABLE_ITEM(X2B2G2R2),
	ITABLE_ITEM(R2G2B2X2),
	ITABLE_ITEM(B2G2R2X2),
	ITABLE_ITEM(A2R2G2B2),
	ITABLE_ITEM(A2B2G2R2),
	ITABLE_ITEM(R2G2B2A2),
	ITABLE_ITEM(B2G2R2A2),
	ITABLE_ITEM(X4C4),
	ITABLE_ITEM(X4G4),
	ITABLE_ITEM(X4A4),
	ITABLE_ITEM(C4X4),
	ITABLE_ITEM(G4X4),
	ITABLE_ITEM(A4X4),
	ITABLE_ITEM(C4),
	ITABLE_ITEM(G4),
	ITABLE_ITEM(A4),
	ITABLE_ITEM(R1G2B1),
	ITABLE_ITEM(B1G2R1),
	ITABLE_ITEM(A1R1G1B1),
	ITABLE_ITEM(A1B1G1R1),
	ITABLE_ITEM(R1G1B1A1),
	ITABLE_ITEM(B1G1R1A1),
	ITABLE_ITEM(X1R1G1B1),
	ITABLE_ITEM(X1B1G1R1),
	ITABLE_ITEM(R1G1B1X1),
	ITABLE_ITEM(B1G1R1X1),
	ITABLE_ITEM(C1),
	ITABLE_ITEM(G1),
	ITABLE_ITEM(A1),
};

#undef ITABLE_ITEM

static iSpanDrawProc ipixel_span_draw_over = ipixel_span_draw_proc_over_32;

iSpanDrawProc ipixel_get_span_proc(int fmt, int op, int builtin)
{
	assert(fmt >= 0 && fmt < IPIX_FMT_COUNT);
	if (fmt < -1 || fmt >= IPIX_FMT_COUNT) {
		abort();
		return NULL;
	}
	if (ipixel_lut_inited == 0) ipixel_lut_init();
	if (builtin) {
		if (fmt < 0) return ipixel_span_draw_proc_over_32;
		if (op == 0) return ipixel_span_proc_list[fmt].blend_builtin;
		else if (op == 1) return ipixel_span_proc_list[fmt].srcover_builtin;
		else return ipixel_span_proc_list[fmt].additive_builtin;
	}	else {
		if (fmt < 0) return ipixel_span_draw_over;
		if (op == 0) return ipixel_span_proc_list[fmt].blend;
		else if (op == 1) return ipixel_span_proc_list[fmt].srcover;
		else return ipixel_span_proc_list[fmt].additive;
	}
}

void ipixel_set_span_proc(int fmt, int op, iSpanDrawProc proc)
{
	assert(fmt >= 0 && fmt < IPIX_FMT_COUNT);
	if (fmt < -1 || fmt >= IPIX_FMT_COUNT) {
		abort();
		return;
	}
	if (ipixel_lut_inited == 0) ipixel_lut_init();
	if (fmt < 0) {
		if (proc != NULL) {
			ipixel_span_draw_over = proc;
		}	else {
			ipixel_span_draw_over = ipixel_span_draw_proc_over_32;
		}
	}	
	else {
		if (op == 0) {
			if (proc != NULL) {
				ipixel_span_proc_list[fmt].blend = proc;
			}	else {
				ipixel_span_proc_list[fmt].blend = 
					ipixel_span_proc_list[fmt].blend_builtin;
			}
		}	
		else if (op == 1) {
			if (proc != NULL) {
				ipixel_span_proc_list[fmt].srcover = proc;
			}	else {
				ipixel_span_proc_list[fmt].srcover = 
					ipixel_span_proc_list[fmt].srcover_builtin;
			}
		}
		else {
			if (proc != NULL) {
				ipixel_span_proc_list[fmt].additive = proc;
			}	else {
				ipixel_span_proc_list[fmt].additive =
					ipixel_span_proc_list[fmt].additive_builtin;
			}
		}
	}
}



/**********************************************************************
 * CARD operations
 **********************************************************************/

/* reverse card */
void ipixel_card_reverse(IUINT32 *card, int size)
{
	IUINT32 *p1, *p2;
	IUINT32 value;
	for (p1 = card, p2 = card + size - 1; p1 < p2; p1++, p2--) {
		value = *p1;
		*p1 = *p2;
		*p2 = value;
	}
}


/* multi card */
void ipixel_card_multi_default(IUINT32 *card, int size, IUINT32 color)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, f;
	IRGBA_FROM_A8R8G8B8(color, r1, g1, b1, a1);
	if ((color & 0xffffff) == 0xffffff) f = 1;
	else f = 0;
	if (color == 0xffffffff) {
		return;
	}
	else if (color == 0) {
		memset(card, 0, sizeof(IUINT32) * size);
	}
	else if (f) {
		IUINT8 *src = (IUINT8*)card;
		if (a1 == 0) {
			for (; size > 0; size--) {
			#if IWORDS_BIG_ENDIAN
				src[0] = 0;
			#else
				src[3] = 0;
			#endif
				src += sizeof(IUINT32);
			}
			return;
		}
		a1 = _ipixel_norm(a1);
		for (; size > 0; size--) {
		#if IWORDS_BIG_ENDIAN
			a2 = src[0];
			src[0] = (IUINT8)((a2 * a1) >> 8);
		#else
			a2 = src[3];
			src[3] = (IUINT8)((a2 * a1) >> 8);
		#endif
			src += sizeof(IUINT32);
		}
	}
	else {
		IUINT8 *src = (IUINT8*)card;
		a1 = _ipixel_norm(a1);
		r1 = _ipixel_norm(r1);
		g1 = _ipixel_norm(g1);
		b1 = _ipixel_norm(b1);
		for (; size > 0; src += sizeof(IUINT32), size--) {
			_ipixel_load_card(src, r2, g2, b2, a2);
			r2 = (r1 * r2) >> 8;
			g2 = (g1 * g2) >> 8;
			b2 = (b1 * b2) >> 8;
			a2 = (a1 * a2) >> 8;
			*((IUINT32*)src) = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}


void (*ipixel_card_multi_proc)(IUINT32 *card, int size, IUINT32 color) =
	ipixel_card_multi_default;

/* multi card */
void ipixel_card_multi(IUINT32 *card, int size, IUINT32 color)
{
	ipixel_card_multi_proc(card, size, color);
}


/* mask card */
void ipixel_card_mask_default(IUINT32 *card, int size, const IUINT32 *mask)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; size > 0; card++, mask++, size--) {
		_ipixel_load_card(mask, r1, g1, b1, a1);
		_ipixel_load_card(card, r2, g2, b2, a2);
		r2 = _imul_y_div_255(r2, r1);
		g2 = _imul_y_div_255(g2, g1);
		b2 = _imul_y_div_255(b2, b1);
		a2 = _imul_y_div_255(a2, a1);
		*card = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
	}
}

void (*ipixel_card_mask_proc)(IUINT32 *card, int size, const IUINT32 *mask) =
	ipixel_card_mask_default;

/* mask card */
void ipixel_card_mask(IUINT32 *card, int size, const IUINT32 *mask)
{
	ipixel_card_mask_proc(card, size, mask);
}

/* cover multi */
void ipixel_card_cover_default(IUINT32 *card, int size, const IUINT8 *cover)
{
	IINT32 cc, aa;
	for (; size > 0; card++, size--) {
		cc = *cover++;
		if (cc == 0) {
			((IUINT8*)card)[_ipixel_card_alpha] = 0;
		}
		else {
			aa = ((IUINT8*)card)[_ipixel_card_alpha];
			if (aa == 0) continue;
			aa *= cc;
			((IUINT8*)card)[_ipixel_card_alpha] = 
				(IUINT8)_ipixel_fast_div_255(aa);
		}
	}
}

void (*ipixel_card_cover_proc)(IUINT32 *card, int size, const IUINT8 *cover) 
	= ipixel_card_cover_default;

/* mask cover */
void ipixel_card_cover(IUINT32 *card, int size, const IUINT8 *cover)
{
	ipixel_card_cover_proc(card, size, cover);
}

void ipixel_card_over_default(IUINT32 *dst, int size, const IUINT32 *card,
	const IUINT8 *cover)
{
	IUINT32 *endup = dst + size;
	if (cover == NULL) {
		for (; dst < endup; card++, dst++) {
			IBLEND_PARGB(dst[0], card[0]);
		}
	}	else {
		for (; dst < endup; cover++, card++, dst++) {
			IBLEND_PARGB_COVER(dst[0], card[0], cover[0]);
		}
	}
}

void (*ipixel_card_over_proc)(IUINT32*, int, const IUINT32*, const IUINT8*) =
	ipixel_card_over_default;

/* card composite: src over */
void ipixel_card_over(IUINT32 *dst, int size, const IUINT32 *card, 
	const IUINT8 *cover)
{
	ipixel_card_over_proc(dst, size, card, cover);
}

/* default card shuffle */
static void ipixel_card_shuffle_default(IUINT32 *card, int w, 
	int b0, int b1, int b2, int b3)
{
	IUINT8 *src = (IUINT8*)card;
	union { IUINT8 quad[4]; IUINT32 color; } cc;
	for (; w > 0; src += 4, w--) {
		cc.color = *((IUINT32*)src);
		src[0] = cc.quad[b0];
		src[1] = cc.quad[b1];
		src[2] = cc.quad[b2];
		src[3] = cc.quad[b3];
	}
}

static void (*ipixel_card_shuffle_proc)(IUINT32*, int, int, int, int, int) = 
	ipixel_card_shuffle_default;

void ipixel_card_shuffle(IUINT32 *card, int w, int a, int b, int c, int d)
{
	ipixel_card_shuffle_proc(card, w, a, b, c, d);
}


/* card proc set */
void ipixel_card_set_proc(int id, void *proc)
{
	if (id == 0) {
		if (proc == NULL) ipixel_card_multi_proc = ipixel_card_multi_default;
		else {
			ipixel_card_multi_proc = 
				(void (*)(IUINT32 *, int, IUINT32))proc;
		}
	}
	else if (id == 1) {
		if (proc == NULL) ipixel_card_mask_proc = ipixel_card_mask_default;
		else {
			ipixel_card_mask_proc = 
				(void (*)(IUINT32 *, int, const IUINT32 *))proc;
		}
	}
	else if (id == 2) {
		if (proc == NULL) ipixel_card_cover_proc = ipixel_card_cover_default;
		else {
			ipixel_card_cover_proc = 
				(void (*)(IUINT32 *, int, const IUINT8 *))proc;
		}
	}
	else if (id == 3) {
		if (proc == NULL) ipixel_card_over_proc = ipixel_card_over_default;
		else {
			ipixel_card_over_proc = 
				(void (*)(IUINT32*, int, const IUINT32*, const IUINT8*))proc;
		}
	}
	else if (id == 4) {
		if (proc == NULL) 
			ipixel_card_shuffle_proc = ipixel_card_shuffle_default;
		else
			ipixel_card_shuffle_proc = 
				(void (*)(IUINT32*, int, int, int, int, int))proc;
	}
}



/**********************************************************************
 * MACRO: HLINE ROUTINE
 **********************************************************************/
/* hline filling: 8/16/24/32 bits without palette */
#define IPIXEL_HLINE_DRAW_PROC_N(fmt, bpp, nbytes, mode) \
static void ipixel_hline_draw_proc_##fmt##_0(void *bits, \
	int offset, int w, IUINT32 color, const IUINT8 *cover, \
	const iColorIndex *idx) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx, cz; \
	IRGBA_FROM_A8R8G8B8(color, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	cz = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
	if (cover == NULL) { \
		if (a1 == 255) { \
			_ipixel_fill(bpp, dst, 0, w, cz); \
		} \
		else if (a1 > 0) { \
			for (; w > 0; dst += nbytes, w--) { \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; dst += nbytes, w--) { \
				a1 = *cover++; \
				if (a1 == 255) { \
					_ipixel_store(bpp, dst, 0, cz); \
				} \
				else if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; dst += nbytes, w--) { \
				cx = *cover++; \
				if (cx > 0) { \
					cx = (cx * a1) >> 8; \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_##mode(r1, g1, b1, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_hline_draw_proc_##fmt##_1(void *bits, \
	int offset, int w, IUINT32 color, const IUINT8 *cover, \
	const iColorIndex *idx) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx, cz; \
	IUINT32 r3, g3, b3; \
	IRGBA_FROM_A8R8G8B8(color, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	cz = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
	if (cover == NULL) { \
		if (a1 == 255) { \
			_ipixel_fill(bpp, dst, 0, w, cz); \
		} \
		else if (a1 > 0) { \
			for (; w > 0; dst += nbytes, w--) { \
				cc = _ipixel_fetch(bpp, dst, 0); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, 0, cc); \
			} \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; dst += nbytes, w--) { \
				a1 = *cover++; \
				if (a1 == 255) { \
					_ipixel_store(bpp, dst, 0, cz); \
				} \
				else if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					r3 = _imul_y_div_255(r1, a1); \
					g3 = _imul_y_div_255(g1, a1); \
					b3 = _imul_y_div_255(b1, a1); \
					IBLEND_SRCOVER(r3, g3, b3, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; dst += nbytes, w--) { \
				cx = *cover++; \
				if (cx > 0) { \
					r3 = _imul_y_div_255(r1, cx); \
					g3 = _imul_y_div_255(g1, cx); \
					b3 = _imul_y_div_255(b1, cx); \
					cx = (cx * a1) >> 8; \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_SRCOVER(r3, g3, b3, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_hline_draw_proc_##fmt##_2(void *bits, \
	int offset, int w, IUINT32 color, const IUINT8 *cover, \
	const iColorIndex *idx) \
{ \
	unsigned char *dst = ((unsigned char*)bits) + offset * nbytes; \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx; \
	IRGBA_FROM_A8R8G8B8(color, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	if (cover == NULL) { \
		r2 = g2 = b2 = a2 = 0; \
		IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, a2, b2); \
		r1 = r2; g1 = g2; b1 = b2; a1 = a2; \
		for (; w > 0; dst += nbytes, w--) { \
			cc = _ipixel_fetch(bpp, dst, 0); \
			IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
			r2 = ICLIP_256(r1 + r2); \
			g2 = ICLIP_256(g1 + g2); \
			b2 = ICLIP_256(b1 + b2); \
			a2 = ICLIP_256(a1 + a2); \
			cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
			_ipixel_store(bpp, dst, 0, cc); \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; dst += nbytes, w--) { \
				a1 = *cover++; \
				if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; dst += nbytes, w--) { \
				cx = *cover++; \
				if (cx > 0) { \
					cx = (cx * a1) >> 8; \
					cc = _ipixel_fetch(bpp, dst, 0); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_ADDITIVE(r1, g1, b1, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, 0, cc); \
				} \
			} \
		} \
	} \
}

/* hline filling: 8/4/1 bits with or without palette */
#define IPIXEL_HLINE_DRAW_PROC_X(fmt, bpp, nbytes, mode, init) \
static void ipixel_hline_draw_proc_##fmt##_0(void *bits, \
	int offset, int w, IUINT32 col, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits); \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx, cz; \
	init; \
	IRGBA_FROM_A8R8G8B8(col, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	cz = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
	if (cover == NULL) { \
		if (a1 == 255) { \
			_ipixel_fill(bpp, dst, offset, w, cz); \
		} \
		else if (a1 > 0) { \
			for (; w > 0; offset++, w--) { \
				cc = _ipixel_fetch(bpp, dst, offset); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, offset, cc); \
			} \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; offset++, w--) { \
				a1 = *cover++; \
				if (a1 == 255) { \
					_ipixel_store(bpp, dst, offset, cz); \
				} \
				else if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_##mode(r1, g1, b1, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; offset++, w--) { \
				cx = (*cover++ * a1) >> 8; \
				if (cx == 255) { \
					cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
				else if (cx > 0) { \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_##mode(r1, g1, b1, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_hline_draw_proc_##fmt##_1(void *bits, \
	int offset, int w, IUINT32 col, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits); \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx, cz; \
	IUINT32 r3, g3, b3; \
	init; \
	IRGBA_FROM_A8R8G8B8(col, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	cz = IRGBA_TO_PIXEL(fmt, r1, g1, b1, a1); \
	if (cover == NULL) { \
		if (a1 == 255) { \
			_ipixel_fill(bpp, dst, offset, w, cz); \
		} \
		else if (a1 > 0) { \
			for (; w > 0; offset++, w--) { \
				cc = _ipixel_fetch(bpp, dst, offset); \
				IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
				IBLEND_SRCOVER(r1, g1, b1, a1, r2, g2, b2, a2); \
				cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
				_ipixel_store(bpp, dst, offset, cc); \
			} \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; offset++, w--) { \
				a1 = *cover++; \
				if (a1 == 255) { \
					_ipixel_store(bpp, dst, offset, cz); \
				} \
				else if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					r3 = _imul_y_div_255(r1, a1); \
					g3 = _imul_y_div_255(g1, a1); \
					b3 = _imul_y_div_255(b1, a1); \
					IBLEND_SRCOVER(r3, g3, b3, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; offset++, w--) { \
				cx = *cover++; \
				if (cx + a1 == 255 + 256) { \
					cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
				else if (cx > 0) { \
					r3 = _imul_y_div_255(r1, cx); \
					g3 = _imul_y_div_255(g1, cx); \
					b3 = _imul_y_div_255(b1, cx); \
					cx = (cx * a1) >> 8; \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_SRCOVER(r1, g1, b1, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
} \
static void ipixel_hline_draw_proc_##fmt##_2(void *bits, \
	int offset, int w, IUINT32 col, const IUINT8 *cover, \
	const iColorIndex *_ipixel_src_index) \
{ \
	unsigned char *dst = ((unsigned char*)bits); \
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, cc, cx; \
	init; \
	IRGBA_FROM_A8R8G8B8(col, r1, g1, b1, a1); \
	if (a1 == 0) return; \
	if (cover == NULL) { \
		r2 = g2 = b2 = a2 = 0; \
		IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, a2, b2); \
		r1 = r2; g1 = g2; b1 = b2; a1 = a2; \
		for (; w > 0; dst += nbytes, w--) { \
			cc = _ipixel_fetch(bpp, dst, offset); \
			IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
			r2 = ICLIP_256(r1 + r2); \
			g2 = ICLIP_256(g1 + g2); \
			b2 = ICLIP_256(b1 + b2); \
			a2 = ICLIP_256(a1 + a2); \
			cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
			_ipixel_store(bpp, dst, offset, cc); \
		} \
	}	else { \
		if (a1 == 255) { \
			for (; w > 0; offset++, w--) { \
				a1 = *cover++; \
				if (a1 > 0) { \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_ADDITIVE(r1, g1, b1, a1, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r2, g2, b2, a2); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		}	\
		else if (a1 > 0) { \
			a1 = _ipixel_norm(a1); \
			for (; w > 0; offset++, w--) { \
				cx = *cover++; \
				if (cx > 0) { \
					cx = (cx * a1) >> 8; \
					cc = _ipixel_fetch(bpp, dst, offset); \
					IRGBA_FROM_PIXEL(fmt, cc, r2, g2, b2, a2); \
					IBLEND_ADDITIVE(r1, g1, b1, cx, r2, g2, b2, a2); \
					cc = IRGBA_TO_PIXEL(fmt, r1, g1, b1, 255); \
					_ipixel_store(bpp, dst, offset, cc); \
				} \
			} \
		} \
	} \
	cc = a1 + a2 + r1 + r2 + g1 + g2 + b1 + b2; \
}

/* hline filling: 8/4/1 bits without palette */
#define IPIXEL_HLINE_DRAW_PROC_BITS(fmt, bpp, nbytes, mode) \
		IPIXEL_HLINE_DRAW_PROC_X(fmt, bpp, nbytes, mode, {})

/* hline filling: 8/4/1 bits with palette */
#define IPIXEL_HLINE_DRAW_PROC_PAL(fmt, bpp, nbytes, mode) \
		IPIXEL_HLINE_DRAW_PROC_X(fmt, bpp, nbytes, mode,  \
				const iColorIndex *_ipixel_dst_index = _ipixel_src_index) 

/* hline filling: main macro */
#define IPIXEL_HLINE_DRAW_MAIN(type, fmt, bpp, nbytes, mode) \
	IPIXEL_HLINE_DRAW_PROC_##type(fmt, bpp, nbytes, mode) 


#if 1
IPIXEL_HLINE_DRAW_MAIN(N, A8R8G8B8, 32, 4, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, A8B8G8R8, 32, 4, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R8G8B8A8, 32, 4, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, B8G8R8A8, 32, 4, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, X8R8G8B8, 32, 4, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X8B8G8R8, 32, 4, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, R8G8B8X8, 32, 4, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B8G8R8X8, 32, 4, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, P8R8G8B8, 32, 4, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R8G8B8, 24, 3, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B8G8R8, 24, 3, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, R5G6B5, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B5G6R5, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X1R5G5B5, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X1B5G5R5, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, R5G5B5X1, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B5G5R5X1, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, A1R5G5B5, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, A1B5G5R5, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R5G5B5A1, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, B5G5R5A1, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, X4R4G4B4, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X4B4G4R4, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, R4G4B4X4, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B4G4R4X4, 16, 2, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, A4R4G4B4, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, A4B4G4R4, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R4G4B4A4, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, B4G4R4A4, 16, 2, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(PAL, C8, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, G8, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, A8, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R3G3B2, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B2G3R3, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X2R2G2B2, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X2B2G2R2, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, R2G2B2X2, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, B2G2R2X2, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, A2R2G2B2, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, A2B2G2R2, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, R2G2B2A2, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(N, B2G2R2A2, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(PAL, X4C4, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X4G4, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, X4A4, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(PAL, C4X4, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, G4X4, 8, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(N, A4X4, 8, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(PAL, C4, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, G4, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, A4, 4, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(BITS, R1G2B1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, B1G2R1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, A1R1G1B1, 4, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(BITS, A1B1G1R1, 4, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(BITS, R1G1B1A1, 4, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(BITS, B1G1R1A1, 4, 1, NORMAL_FAST)
IPIXEL_HLINE_DRAW_MAIN(BITS, X1R1G1B1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, X1B1G1R1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, R1G1B1X1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, B1G1R1X1, 4, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(PAL, C1, 1, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, G1, 1, 1, STATIC)
IPIXEL_HLINE_DRAW_MAIN(BITS, A1, 1, 1, NORMAL_FAST)
#endif

#undef IPIXEL_HLINE_DRAW_MAIN
#undef IPIXEL_HLINE_DRAW_PROC_PAL
#undef IPIXEL_HLINE_DRAW_PROC_BITS
#undef IPIXEL_HLINE_DRAW_PROC_X
#undef IPIXEL_HLINE_DRAW_PROC_N


struct iPixelHLineDrawProc
{
	iHLineDrawProc blend, srcover, additive;
	iHLineDrawProc blend_builtin, srcover_builtin, additive_builtin;
};

#define ITABLE_ITEM(fmt) { \
	ipixel_hline_draw_proc_##fmt##_0, ipixel_hline_draw_proc_##fmt##_1, \
	ipixel_hline_draw_proc_##fmt##_2, ipixel_hline_draw_proc_##fmt##_0, \
	ipixel_hline_draw_proc_##fmt##_1, ipixel_hline_draw_proc_##fmt##_2 }

static struct iPixelHLineDrawProc ipixel_hline_proc_list[IPIX_FMT_COUNT] =
{
	ITABLE_ITEM(A8R8G8B8),
	ITABLE_ITEM(A8B8G8R8),
	ITABLE_ITEM(R8G8B8A8),
	ITABLE_ITEM(B8G8R8A8),
	ITABLE_ITEM(X8R8G8B8),
	ITABLE_ITEM(X8B8G8R8),
	ITABLE_ITEM(R8G8B8X8),
	ITABLE_ITEM(B8G8R8X8),
	ITABLE_ITEM(P8R8G8B8),
	ITABLE_ITEM(R8G8B8),
	ITABLE_ITEM(B8G8R8),
	ITABLE_ITEM(R5G6B5),
	ITABLE_ITEM(B5G6R5),
	ITABLE_ITEM(X1R5G5B5),
	ITABLE_ITEM(X1B5G5R5),
	ITABLE_ITEM(R5G5B5X1),
	ITABLE_ITEM(B5G5R5X1),
	ITABLE_ITEM(A1R5G5B5),
	ITABLE_ITEM(A1B5G5R5),
	ITABLE_ITEM(R5G5B5A1),
	ITABLE_ITEM(B5G5R5A1),
	ITABLE_ITEM(X4R4G4B4),
	ITABLE_ITEM(X4B4G4R4),
	ITABLE_ITEM(R4G4B4X4),
	ITABLE_ITEM(B4G4R4X4),
	ITABLE_ITEM(A4R4G4B4),
	ITABLE_ITEM(A4B4G4R4),
	ITABLE_ITEM(R4G4B4A4),
	ITABLE_ITEM(B4G4R4A4),
	ITABLE_ITEM(C8),
	ITABLE_ITEM(G8),
	ITABLE_ITEM(A8),
	ITABLE_ITEM(R3G3B2),
	ITABLE_ITEM(B2G3R3),
	ITABLE_ITEM(X2R2G2B2),
	ITABLE_ITEM(X2B2G2R2),
	ITABLE_ITEM(R2G2B2X2),
	ITABLE_ITEM(B2G2R2X2),
	ITABLE_ITEM(A2R2G2B2),
	ITABLE_ITEM(A2B2G2R2),
	ITABLE_ITEM(R2G2B2A2),
	ITABLE_ITEM(B2G2R2A2),
	ITABLE_ITEM(X4C4),
	ITABLE_ITEM(X4G4),
	ITABLE_ITEM(X4A4),
	ITABLE_ITEM(C4X4),
	ITABLE_ITEM(G4X4),
	ITABLE_ITEM(A4X4),
	ITABLE_ITEM(C4),
	ITABLE_ITEM(G4),
	ITABLE_ITEM(A4),
	ITABLE_ITEM(R1G2B1),
	ITABLE_ITEM(B1G2R1),
	ITABLE_ITEM(A1R1G1B1),
	ITABLE_ITEM(A1B1G1R1),
	ITABLE_ITEM(R1G1B1A1),
	ITABLE_ITEM(B1G1R1A1),
	ITABLE_ITEM(X1R1G1B1),
	ITABLE_ITEM(X1B1G1R1),
	ITABLE_ITEM(R1G1B1X1),
	ITABLE_ITEM(B1G1R1X1),
	ITABLE_ITEM(C1),
	ITABLE_ITEM(G1),
	ITABLE_ITEM(A1),
};

#undef ITABLE_ITEM


/* get a hline drawing function with given pixel format */
iHLineDrawProc ipixel_get_hline_proc(int fmt, int op, int builtin)
{
	assert(fmt >= 0 && fmt < IPIX_FMT_COUNT);
	if (fmt < 0 || fmt >= IPIX_FMT_COUNT) {
		abort();
		return NULL;
	}
	if (ipixel_lut_inited == 0) ipixel_lut_init();
	if (builtin) {
		if (op == 0) 
			return ipixel_hline_proc_list[fmt].blend_builtin;
		else if (op == 1)
			return ipixel_hline_proc_list[fmt].srcover_builtin;
		else
			return ipixel_hline_proc_list[fmt].additive_builtin;
	}	else {
		if (op == 0) 
			return ipixel_hline_proc_list[fmt].blend;
		else if (op == 1)
			return ipixel_hline_proc_list[fmt].srcover;
		else 
			return ipixel_hline_proc_list[fmt].additive;
	}
}

/* set a hline drawing function */
void ipixel_set_hline_proc(int fmt, int op, iHLineDrawProc proc)
{
	assert(fmt >= 0 && fmt < IPIX_FMT_COUNT);
	if (fmt < 0 || fmt >= IPIX_FMT_COUNT) {
		abort();
		return;
	}
	if (ipixel_lut_inited == 0) ipixel_lut_init();
	if (op == 0) {
		if (proc != NULL) {
			ipixel_hline_proc_list[fmt].blend = proc;
		}	else {
			ipixel_hline_proc_list[fmt].blend = 
				ipixel_hline_proc_list[fmt].blend_builtin;
		}
	}	
	else if (op == 1) {
		if (proc != NULL) {
			ipixel_hline_proc_list[fmt].srcover = proc;
		}	else {
			ipixel_hline_proc_list[fmt].srcover = 
				ipixel_hline_proc_list[fmt].srcover_builtin;
		}
	}
	else {
		if (proc != NULL) {
			ipixel_hline_proc_list[fmt].additive = proc;
		}	else {
			ipixel_hline_proc_list[fmt].additive = 
				ipixel_hline_proc_list[fmt].additive_builtin;
		}
	}
}


/* ipixel_blend - blend between two formats 
 * you must provide a working memory pointer to workmem. if workmem eq NULL,
 * this function will do nothing but returns how many bytes needed in workmem
 * dfmt        - dest pixel format
 * dbits       - dest pixel buffer
 * dpitch      - dest row stride
 * dx          - dest x offset
 * sfmt        - source pixel format
 * sbits       - source pixel buffer
 * spitch      - source row stride
 * sx          - source x offset
 * w           - width
 * h           - height
 * color       - const color
 * op          - blending operator (IPIXEL_BLEND_OP_BLEND, ADD, COPY)
 * flip        - flip (IPIXEL_FLIP_NONE, HFLIP, VFLIP)
 * dindex      - dest index
 * sindex      - source index
 * workmem     - working memory
 * this function need some memory to work with. to avoid allocating, 
 * you must provide a memory block whose size is (w * 4) to it.
 */
long ipixel_blend(int dfmt, void *dbits, long dpitch, int dx, int sfmt, 
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 color,
	int op, int flip, const iColorIndex *dindex, 
	const iColorIndex *sindex, void *workmem)
{
	IUINT32 *buffer = (IUINT32*)workmem;
	IUINT8 *dline = (IUINT8*)dbits;
	const IUINT8 *sline = (const IUINT8*)sbits;
	iSpanDrawProc drawspan = NULL;
	iFetchProc fetch;
	iStoreProc store;
	int k;

	if (workmem == NULL) {
		return w * sizeof(IUINT32);
	}

	fetch = ipixel_get_fetch(sfmt, IPIXEL_ACCESS_MODE_NORMAL);
	store = ipixel_get_store(dfmt, IPIXEL_ACCESS_MODE_NORMAL);

	if (op == IPIXEL_BLEND_OP_BLEND) {
		drawspan = ipixel_get_span_proc(dfmt, 0, 0);
	}
	else if (op == IPIXEL_BLEND_OP_SRCOVER) {
		drawspan = ipixel_get_span_proc(dfmt, 1, 0);
	}
	else if (op == IPIXEL_BLEND_OP_ADD) {
		drawspan = ipixel_get_span_proc(dfmt, 2, 0);
	}

	if ((flip & IPIXEL_FLIP_VFLIP) != 0) {
		sline = sline + spitch * (h - 1);
		spitch = -spitch;
	}

	if (sfmt == IPIX_FMT_P8R8G8B8 && dfmt == IPIX_FMT_P8R8G8B8) {
		if (op == IPIXEL_BLEND_OP_BLEND && color == 0xffffffff) {
			sfmt = IPIX_FMT_A8R8G8B8;
			dfmt = IPIX_FMT_A8R8G8B8;
			drawspan = ipixel_get_span_proc(-1, 0, 0);
		}
	}

	if (sfmt == IPIX_FMT_A8R8G8B8 && (flip & IPIXEL_FLIP_HFLIP) == 0 &&
		color == 0xffffffff) {
		if (drawspan != NULL) {
			for (k = 0; k < h; sline += spitch, dline += dpitch, k++) {
				const IUINT32 *src = ((const IUINT32*)sline) + sx;
				IUINT32 *dst = ((IUINT32*)dline);
				drawspan(dst, dx, w, src, NULL, dindex);
			}
		}	else {
			for (k = 0; k < h; sline += spitch, dline += dpitch, k++) {
				const IUINT32 *src = ((const IUINT32*)sline) + sx;
				IUINT32 *dst = ((IUINT32*)dline);
				store(dst, src, dx, w, dindex);
			}
		}
		return 0;
	}

	#define IPIXEL_BLEND_LOOP(work) do { \
			for (k = 0; k < h; sline += spitch, dline += dpitch, k++) { \
				const IUINT32 *src = ((const IUINT32*)sline); \
				IUINT32 *dst = ((IUINT32*)dline); \
				fetch(src, sx, w, buffer, sindex); \
				work; \
			} \
		}	while (0)

	if ((flip & IPIXEL_FLIP_HFLIP) == 0) {
		if (drawspan != NULL) {
			if (color == 0xffffffff) {
				IPIXEL_BLEND_LOOP( {
					drawspan(dst, dx, w, buffer, NULL, dindex);
				});
			}	else {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_multi_proc(buffer, w, color);
					drawspan(dst, dx, w, buffer, NULL, dindex);
				});
			}
		}	else {
			if (color == 0xffffffff) {
				IPIXEL_BLEND_LOOP( {
					store(dst, buffer, dx, w, dindex);
				});
			}	else {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_multi_proc(buffer, w, color);
					store(dst, buffer, dx, w, dindex);
				});
			}
		}
	}	else {
		if (drawspan != NULL) {
			if (color == 0xffffffff) {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_reverse(buffer, w);
					drawspan(dst, dx, w, buffer, NULL, dindex);
				});
			}	else {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_reverse(buffer, w);
					ipixel_card_multi_proc(buffer, w, color);
					drawspan(dst, dx, w, buffer, NULL, dindex);
				});
			}
		}	else {
			if (color == 0xffffffff) {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_reverse(buffer, w);
					store(dst, buffer, dx, w, dindex);
				});
			}	else {
				IPIXEL_BLEND_LOOP( {
					ipixel_card_reverse(buffer, w);
					ipixel_card_multi_proc(buffer, w, color);
					store(dst, buffer, dx, w, dindex);
				});
			}
		}
	}

	#undef IPIXEL_BLEND_LOOP

	return 0;
}



/**********************************************************************
 * MACRO: BLITING ROUTINE
 **********************************************************************/
/* normal blit in 32/16/8 bits */
#define IPIXEL_BLIT_PROC_N(nbits, nbytes, INTTYPE) \
static int ipixel_blit_proc_##nbits(void *dbits, long dpitch, int dx,  \
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 mask, \
	int flip) \
{ \
	int y, x; \
	if (flip & IPIXEL_FLIP_VFLIP) { \
		sbits = (const IUINT8*)sbits + spitch * (h - 1); \
		spitch = -spitch; \
	} \
	if ((flip & IPIXEL_FLIP_HFLIP) == 0) { \
		long size = w * nbytes; \
		for (y = 0; y < h; y++) { \
			ipixel_memcpy((INTTYPE*)dbits + dx, (const INTTYPE*)sbits + sx, size); \
			dbits = (IUINT8*)dbits + dpitch; \
			sbits = (const IUINT8*)sbits + spitch; \
		} \
	}	else { \
		for (y = 0; y < h; y++) { \
			const INTTYPE *src = (const INTTYPE*)sbits + sx + w - 1; \
			INTTYPE *dst = (INTTYPE*)dbits + dx; \
			for (x = w; x > 0; x--) *dst++ = *src--; \
			dbits = (IUINT8*)dbits + dpitch; \
			sbits = (const IUINT8*)sbits + spitch; \
		} \
	} \
	return 0; \
} 

/* normal blit in 24/4/1 bits */
#define IPIXEL_BLIT_PROC_BITS(nbits) \
static int ipixel_blit_proc_##nbits(void *dbits, long dpitch, int dx, \
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 mask, \
	int flip) \
{ \
	int y, x1, x2, sx0, sxd, endx; \
	if (flip & IPIXEL_FLIP_VFLIP) { \
		sbits = (const IUINT8*)sbits + spitch * (h - 1); \
		spitch = -spitch; \
	} \
	if (flip & IPIXEL_FLIP_HFLIP) { \
		sx0 = sx + w - 1; \
		sxd = -1; \
	}	else { \
		sx0 = sx; \
		sxd = 1; \
	} \
	endx = dx + w; \
	for (y = 0; y < h; y++) { \
		IUINT32 cc; \
		for (x1 = dx, x2 = sx0; x1 < endx; x1++, x2 += sxd) { \
			cc = _ipixel_fetch(nbits, sbits, x2); \
			_ipixel_store(nbits, dbits, x1, cc); \
		} \
		dbits = (IUINT8*)dbits + dpitch; \
		sbits = (const IUINT8*)sbits + spitch; \
	} \
	return 0; \
}


/* mask blit in 32/16/8 bits */
#define IPIXEL_BLIT_MASK_PROC_N(nbits, nbytes, INTTYPE) \
static int ipixel_blit_mask_proc_##nbits(void *dbits, long dpitch, \
	int dx, const void *sbits, long spitch, int sx, int w, int h, \
	IUINT32 mask, int flip) \
{ \
	INTTYPE cmask = (INTTYPE)mask; \
	int y; \
	if (flip & IPIXEL_FLIP_VFLIP) { \
		sbits = (const IUINT8*)sbits + spitch * (h - 1); \
		spitch = -spitch; \
	} \
	if ((flip & IPIXEL_FLIP_HFLIP) == 0) { \
		for (y = 0; y < h; y++) { \
			const INTTYPE *src = (const INTTYPE*)sbits + sx; \
			INTTYPE *dst = (INTTYPE*)dbits + dx; \
			INTTYPE *dstend = dst + w; \
			for (; dst < dstend; src++, dst++) { \
				if (src[0] != cmask) dst[0] = src[0]; \
			} \
			dbits = (IUINT8*)dbits + dpitch; \
			sbits = (const IUINT8*)sbits + spitch; \
		} \
	}	else { \
		for (y = 0; y < h; y++) { \
			const INTTYPE *src = (const INTTYPE*)sbits + sx + w - 1; \
			INTTYPE *dst = (INTTYPE*)dbits + dx; \
			INTTYPE *dstend = dst + w; \
			for (; dst < dstend; src--, dst++) { \
				if (src[0] != cmask) dst[0] = src[0]; \
			} \
			dbits = (IUINT8*)dbits + dpitch; \
			sbits = (const IUINT8*)sbits + spitch; \
		} \
	} \
	return 0; \
}

/* mask blit in 24/4/1 bits */
#define IPIXEL_BLIT_MASK_PROC_BITS(nbits) \
static int ipixel_blit_mask_proc_##nbits(void *dbits, long dpitch, \
	int dx, const void *sbits, long spitch, int sx, int w, int h, \
	IUINT32 mask, int flip) \
{ \
	int y, x1, x2, sx0, sxd, endx; \
	if (flip & IPIXEL_FLIP_VFLIP) { \
		sbits = (const IUINT8*)sbits + spitch * (h - 1); \
		spitch = -spitch; \
	} \
	if (flip & IPIXEL_FLIP_HFLIP) { \
		sx0 = sx + w - 1; \
		sxd = -1; \
	}	else { \
		sx0 = sx; \
		sxd = 1; \
	} \
	endx = dx + w; \
	for (y = 0; y < h; y++) { \
		IUINT32 cc; \
		for (x1 = dx, x2 = sx0; x1 < endx; x1++, x2 += sxd) { \
			cc = _ipixel_fetch(nbits, sbits, x2); \
			if (cc != mask) _ipixel_store(nbits, dbits, x1, cc); \
		} \
		dbits = (IUINT8*)dbits + dpitch; \
		sbits = (const IUINT8*)sbits + spitch; \
	} \
	return 0; \
}


/* normal bliter */
IPIXEL_BLIT_PROC_N(32, 4, IUINT32);
IPIXEL_BLIT_PROC_N(16, 2, IUINT16);
IPIXEL_BLIT_PROC_N(8, 1, IUINT8);

IPIXEL_BLIT_PROC_BITS(24);
IPIXEL_BLIT_PROC_BITS(4);
IPIXEL_BLIT_PROC_BITS(1);

/* mask bliter */
IPIXEL_BLIT_MASK_PROC_N(32, 4, IUINT32);
IPIXEL_BLIT_MASK_PROC_N(16, 2, IUINT16);
IPIXEL_BLIT_MASK_PROC_N(8, 1, IUINT8);

IPIXEL_BLIT_MASK_PROC_BITS(24);
IPIXEL_BLIT_MASK_PROC_BITS(4);
IPIXEL_BLIT_MASK_PROC_BITS(1);


#undef IPIXEL_BLIT_PROC_N
#undef IPIXEL_BLIT_PROC_BITS
#undef IPIXEL_BLIT_MASK_PROC_N
#undef IPIXEL_BLIT_MASK_PROC_BITS



/* blit driver desc */
struct iPixelBlitProc
{
	iBlitProc normal, normal_default;
	iBlitProc mask, mask_default;
};

#define ITABLE_ITEM(bpp) { \
	ipixel_blit_proc_##bpp, ipixel_blit_proc_##bpp, \
	ipixel_blit_mask_proc_##bpp, ipixel_blit_mask_proc_##bpp }


/* blit procedure look up table */
static struct iPixelBlitProc ipixel_blit_proc_list[6] =
{
	ITABLE_ITEM(32),
	ITABLE_ITEM(24),
	ITABLE_ITEM(16),
	ITABLE_ITEM(8),
	ITABLE_ITEM(4),
	ITABLE_ITEM(1),
};

static const int ipixel_lookup_bpp[33] = {
	-1, 5, -1, -1, 4, -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, 
	2, 2, -1, -1, -1, -1, -1, -1, -1, 1, -1, -1, -1, -1, -1,
	-1, -1, 0,
};


/* get normal blit procedure */
/* if ismask equals to zero, returns normal bliter */
/* if ismask doesn't equal to zero, returns transparent bliter */
iBlitProc ipixel_blit_get(int bpp, int istransparent, int isdefault)
{
	int index;
	if (bpp < 0 || bpp > 32) return NULL;
	index = ipixel_lookup_bpp[bpp];
	if (index < 0) return NULL;
	if (isdefault) {
		if (istransparent) return ipixel_blit_proc_list[index].mask_default;
		return ipixel_blit_proc_list[index].normal_default;
	}
	if (istransparent) return ipixel_blit_proc_list[index].mask;
	return ipixel_blit_proc_list[index].normal;
}

/* set normal blit procedure */
/* if ismask equals to zero, set normal bliter */
/* if ismask doesn't equal to zero, set transparent bliter */
void ipixel_set_blit_proc(int bpp, int istransparent, iBlitProc proc)
{
	int index;
	if (bpp < 0 || bpp > 32) return;
	index = ipixel_lookup_bpp[bpp];
	if (index < 0) return;
	if (istransparent == 0) {
		if (proc != NULL) {
			ipixel_blit_proc_list[index].normal = proc;
		}	else {
			ipixel_blit_proc_list[index].normal = 
				ipixel_blit_proc_list[index].normal_default;
		}
	}	else {
		if (proc != NULL) {
			ipixel_blit_proc_list[index].mask = proc;
		}	else {
			ipixel_blit_proc_list[index].mask = 
				ipixel_blit_proc_list[index].mask_default;
		}
	}
}



/* ipixel_blit - bliting (copy pixel from one rectangle to another)
 * it will only copy pixels in the same depth (1/4/8/16/24/32).
 * no color format will be convert (using ipixel_convert to do it)
 * bpp    - color depth of the two bitmap
 * dst    - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * src    - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * for transparent bliting, set mode with IPIXEL_BLIT_MASK, bliter will 
 * skip the colors equal to 'mask' parameter.
 */
void ipixel_blit(int bpp, void *dst, long dpitch, int dx, const void *src, 
	long spitch, int sx, int w, int h, IUINT32 mask, int mode)
{
	int transparent, flip, index, retval;
	iBlitProc bliter;

	transparent = (mask & IPIXEL_BLIT_MASK)? 1 : 0;
	flip = mode & (IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP);
	
	assert(bpp >= 0 && bpp <= 32);

	index = ipixel_lookup_bpp[bpp];

	if (transparent) bliter = ipixel_blit_proc_list[index].mask;
	else bliter = ipixel_blit_proc_list[index].normal;

	/* using current bliter */
	bliter = ipixel_blit_get(bpp, transparent, 0);

	if (bliter) {
		retval = bliter(dst, dpitch, dx, src, spitch, sx, w, h, mask, flip);
		if (retval == 0) return; /* return for success */
	}

	/* using default bliter */
	if (transparent) bliter = ipixel_blit_proc_list[index].mask_default;
	else bliter = ipixel_blit_proc_list[index].normal_default;
	
	bliter(dst, dpitch, dx, src, spitch, sx, w, h, mask, flip);
}


/**********************************************************************
 * CONVERTING
 **********************************************************************/
static iPixelCvt ipixel_cvt_table[IPIX_FMT_COUNT][IPIX_FMT_COUNT][8];
static int ipixel_cvt_inited = 0;

/* initialize converting procedure table */
static void ipixel_cvt_init(void)
{
	int dfmt, sfmt, i;
	if (ipixel_cvt_inited) return;
	for (dfmt = 0; dfmt < IPIX_FMT_COUNT; dfmt++) {
		for (sfmt = 0; sfmt < IPIX_FMT_COUNT; sfmt++) {
			for (i = 0; i < 8; i++) ipixel_cvt_table[dfmt][sfmt][i] = NULL;
		}
	}
	ipixel_cvt_inited = 1;
}

/* get converting procedure */
iPixelCvt ipixel_cvt_get(int dfmt, int sfmt, int index)
{
	if (ipixel_cvt_inited == 0) ipixel_cvt_init();
	if (dfmt < 0 || dfmt >= IPIX_FMT_COUNT) return NULL;
	if (sfmt < 0 || sfmt >= IPIX_FMT_COUNT) return NULL;
	if (index < 0 || index >= 8) return NULL;
	return ipixel_cvt_table[dfmt][sfmt][index];
}

/* set converting procedure */
void ipixel_cvt_set(int dfmt, int sfmt, int index, iPixelCvt proc)
{
	if (ipixel_cvt_inited == 0) ipixel_cvt_init();
	if (dfmt < 0 || dfmt >= IPIX_FMT_COUNT) return;
	if (sfmt < 0 || sfmt >= IPIX_FMT_COUNT) return;
	if (index < 0 || index >= 8) return;
	ipixel_cvt_table[dfmt][sfmt][index] = proc;
}

/* ipixel_slow: default slow converter */
long ipixel_cvt_slow(int dfmt, void *dbits, long dpitch, int dx, int sfmt, 
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 mask, 
	int mode, const iColorIndex *dindex, const iColorIndex *sindex)
{
	const iColorIndex *_ipixel_dst_index = dindex;
	const iColorIndex *_ipixel_src_index = sindex;
	int flip, sbpp, dbpp, i, j;
	int transparent;

	flip = mode & (IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP);
	transparent = (mode & IPIXEL_BLIT_MASK)? 1 : 0;

	sbpp = ipixelfmt[sfmt].bpp;
	dbpp = ipixelfmt[dfmt].bpp;

	if (flip & IPIXEL_FLIP_VFLIP) { 
		sbits = (const IUINT8*)sbits + spitch * (h - 1); 
		spitch = -spitch; 
	} 

	for (j = 0; j < h; j++) {
		IUINT32 cc = 0, r, g, b, a;
		int incx, x1, x2 = dx;

		if ((flip & IPIXEL_FLIP_HFLIP) == 0) x1 = sx, incx = 1;
		else x1 = sx + w - 1, incx = -1;

		for (i = w; i > 0; x1 += incx, x2++, i--) {
			switch (sbpp) {
				case  1: cc = _ipixel_fetch(1, sbits, x1); break;
				case  4: cc = _ipixel_fetch(4, sbits, x1); break;
				case  8: cc = _ipixel_fetch(8, sbits, x1); break;
				case 16: cc = _ipixel_fetch(16, sbits, x1); break;
				case 24: cc = _ipixel_fetch(24, sbits, x1); break;
				case 32: cc = _ipixel_fetch(32, sbits, x1); break;
			}

			if (transparent && cc == mask) 
				continue;

			IRGBA_DISEMBLE(sfmt, cc, r, g, b, a);
			IRGBA_ASSEMBLE(dfmt, cc, r, g, b, a);

			switch (dbpp) {
				case  1: _ipixel_store(1, dbits, x2, cc); break;
				case  4: _ipixel_store(4, dbits, x2, cc); break;
				case  8: _ipixel_store(8, dbits, x2, cc); break;
				case 16: _ipixel_store(16, dbits, x2, cc); break;
				case 24: _ipixel_store(24, dbits, x2, cc); break;
				case 32: _ipixel_store(32, dbits, x2, cc); break;
			}
		}

		sbits = (const IUINT8*)sbits + spitch;
		dbits = (IUINT8*)dbits + dpitch;
	}

	return 0;
}


/* ipixel_convert: convert pixel format 
 * you must provide a working memory pointer to mem. if mem eq NULL,
 * this function will do nothing but returns how many bytes needed in mem
 * dfmt   - dest color format
 * dbits  - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * sfmt   - source color format
 * sbits  - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * didx   - dest color index
 * sidx   - source color index
 * mem    - work memory
 * for transparent converting, set mode with IPIXEL_BLIT_MASK, it will 
 * skip the colors equal to 'mask' parameter.
 */
long ipixel_convert(int dfmt, void *dbits, long dpitch, int dx, int sfmt, 
	const void *sbits, long spitch, int sx, int w, int h, IUINT32 mask, 
	int mode, const iColorIndex *didx, const iColorIndex *sidx, void *mem)
{
	iPixelCvt cvt = NULL;
	int flip, index;

	if (mem == NULL) {
		return w * sizeof(IUINT32);
	}

	if (ipixel_cvt_inited == 0) ipixel_cvt_init();

	assert(dfmt >= 0 && dfmt < IPIX_FMT_COUNT);
	assert(sfmt >= 0 && sfmt < IPIX_FMT_COUNT);

	flip = mode & (IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP);
	index = (mode & IPIXEL_BLIT_MASK)? 1 : 0;

	if (didx == NULL) didx = _ipixel_dst_index;
	if (sidx == NULL) sidx = _ipixel_src_index;

	cvt = ipixel_cvt_table[dfmt][sfmt][index];

	/* using converting procedure */
	if (cvt != NULL) {
		int retval = cvt(dbits, dpitch, dx, sbits, spitch, sx, w, h, 
			mask, flip, didx, sidx);
		if (retval == 0) return 0;
	}

	/* using bliting procedure when no convertion needed */
	if (sfmt == dfmt && ipixelfmt[sfmt].type != IPIX_FMT_TYPE_INDEX) {
		ipixel_blit(ipixelfmt[sfmt].bpp, dbits, dpitch, dx, sbits, 
			spitch, sx, w, h, mask, mode);
		return 0;
	}

	/* without transparent color key using ipixel_blend */
	if ((mode & IPIXEL_BLIT_MASK) == 0) {
		return ipixel_blend(dfmt, dbits, dpitch, dx, sfmt, sbits, spitch, sx,
			w, h, 0xffffffff, IPIXEL_BLEND_OP_COPY, flip, didx, sidx, mem);
	}

	/* using ipixel_cvt_slow to proceed other convertion */
	ipixel_cvt_slow(dfmt, dbits, dpitch, dx, sfmt, sbits, spitch, sx,
		w, h, mask, mode, didx, sidx);

	return 0;
}


/**********************************************************************
 * FREE FORMAT CONVERT
 **********************************************************************/

static iPixelFmtReader ipixel_fmt_reader[4] = { NULL, NULL, NULL, NULL };
static iPixelFmtWriter ipixel_fmt_writer[4] = { NULL, NULL, NULL, NULL };

/* default free format reader */
static int ipixel_fmt_reader_default(const iPixelFmt *fmt, 
	const void *bits, int x, int w, IUINT32 *card);

/* default free format writer */
static int ipixel_fmt_writer_default(const iPixelFmt *fmt,
	void *bits, int x, int w, const IUINT32 *card);


/* initialize _ipixel_fmt_scale */
static void ipixel_fmt_make_lut(void) 
{
	static int inited = 0;
	if (inited == 0) {
		int i;
		for (i = 0; i < 9; i++) {
			IUINT8 *table = _ipixel_fmt_scale[i];
			IUINT32 maxvalue = (1 << i) - 1;
			int k = 0;
			memset(table, 255, 256);
			if (maxvalue > 0) {
				for (k = 0; k <= maxvalue; k++) {
					table[k] = k * 255 / maxvalue;
				}
			}
		}
		inited = 1;
	}
}


/* ipixel_fmt_init: init pixel format structure
 * depth: color bits, one of 8, 16, 24, 32
 * rmask: mask for red, eg:   0x00ff0000
 * gmask: mask for green, eg: 0x0000ff00
 * bmask: mask for blue, eg:  0x000000ff
 * amask: mask for alpha, eg: 0xff000000
 */
int ipixel_fmt_init(iPixelFmt *fmt, int depth, 
	IUINT32 rmask, IUINT32 gmask, IUINT32 bmask, IUINT32 amask)
{
	int pixelbyte = (depth + 7) / 8;
	int i;
	fmt->bpp = pixelbyte * 8;
	if (depth < 8 || depth > 32) {
		return -1;
	}
	ipixel_fmt_make_lut();
	for (i = 0; i < IPIX_FMT_COUNT; i++) {
		if (ipixelfmt[i].amask == amask &&
			ipixelfmt[i].rmask == rmask &&
			ipixelfmt[i].gmask == gmask &&
			ipixelfmt[i].bmask == bmask) {
			if (fmt->bpp == ipixelfmt[i].bpp) {
				fmt[0] = ipixelfmt[i];
				return 0;
			}
		}
	}
	fmt->format = IPIX_FMT_UNKNOWN;
	fmt->bpp = pixelbyte * 8;
	fmt->rmask = rmask;
	fmt->gmask = gmask;
	fmt->bmask = bmask;
	fmt->amask = amask;
	fmt->alpha = (amask == 0)? 0 : 1;
	fmt->type = (fmt->alpha)? IPIX_FMT_TYPE_ARGB : IPIX_FMT_TYPE_RGB;
	fmt->pixelbyte = pixelbyte;
	fmt->name = "IPIX_FMT_UNKNOWN";
	for (i = 0; i < 4; i++) {
		IUINT32 mask;
		int shift = 0;
		int loss = 8;
		switch (i) {
		case 0: mask = fmt->rmask; break;
		case 1: mask = fmt->gmask; break;
		case 2: mask = fmt->bmask; break;
		case 3: mask = fmt->amask; break;
		}
		if (mask != 0) {
			int zeros = 0;
			int ones = 0;
			for (zeros = 0; (mask & 1) == 0; zeros++, mask >>= 1);
			for (ones = 0; (mask & 1) == 1; ones++, mask >>= 1);
			shift = zeros;
			if (ones <= 8) {
				loss = 8 - ones;
			}	else {
				return -2;
			}
		}
		switch (i) {
		case 0: fmt->rloss = loss; fmt->rshift = shift; break;
		case 1: fmt->gloss = loss; fmt->gshift = shift; break;
		case 2: fmt->bloss = loss; fmt->bshift = shift; break;
		case 3: fmt->aloss = loss; fmt->ashift = shift; break;
		}
	}
	return 0;
}


/* set free format reader */
void ipixel_fmt_set_reader(int depth, iPixelFmtReader reader)
{
	int index = ((depth + 7) / 8) - 1;
	if (index >= 0 && index < 4) {
		ipixel_fmt_make_lut();
		ipixel_fmt_reader[index] = reader;
	}
}


/* set free format writer */
void ipixel_fmt_set_writer(int depth, iPixelFmtWriter writer)
{
	int index = ((depth + 7) / 8) - 1;
	if (index >= 0 && index < 4) {
		ipixel_fmt_writer[index] = writer;
	}
}


/* get free format reader */
iPixelFmtReader ipixel_fmt_get_reader(int depth, int isdefault)
{
	int index = ((depth + 7) / 8) - 1;
	int inited = 0;
	if (inited == 0) {
		ipixel_fmt_make_lut();
		inited = 1;
	}
	if (index < 0 || index >= 4) return NULL;
	if (ipixel_fmt_reader[index] == NULL || isdefault != 0) {
		return ipixel_fmt_reader_default;
	}
	return ipixel_fmt_reader[index];
}


/* get free format writer */
iPixelFmtWriter ipixel_fmt_get_writer(int depth, int isdefault)
{
	int index = ((depth + 7) / 8) - 1;
	if (index < 0 || index >= 4) return NULL;
	if (ipixel_fmt_writer[index] == NULL || isdefault != 0) {
		return ipixel_fmt_writer_default;
	}
	return ipixel_fmt_writer[index];
}


/* free format defaut writer */
static int ipixel_fmt_writer_default(const iPixelFmt *fmt,
	void *bits, int x, int w, const IUINT32 *card)
{
	int rshift = fmt->rshift;
	int gshift = fmt->gshift;
	int bshift = fmt->bshift;
	int ashift = fmt->ashift;
	int rloss = fmt->rloss;
	int gloss = fmt->gloss;
	int bloss = fmt->bloss;
	int aloss = fmt->aloss;
	IUINT32 r, g, b, a, cc;
	IUINT8 *dst = ((IUINT8*)bits) + fmt->pixelbyte * x;
	switch (fmt->pixelbyte) {
	case 1: 
		for (; w > 0; x++, card++, w--) {
			_ipixel_load_card(card, r, g, b, a);
			r = (r >> rloss) << rshift;
			g = (g >> gloss) << gshift;
			b = (b >> bloss) << bshift;
			a = (a >> aloss) << ashift;
			cc = r | g | b | a;
			_ipixel_store(8, dst, 0, cc);
			dst += 1;
		}
		break;
	case 2:
		for (; w > 0; x++, card++, w--) {
			_ipixel_load_card(card, r, g, b, a);
			r = (r >> rloss) << rshift;
			g = (g >> gloss) << gshift;
			b = (b >> bloss) << bshift;
			a = (a >> aloss) << ashift;
			cc = r | g | b | a;
			_ipixel_store(16, dst, 0, cc);
			dst += 2;
		}
		break;
	case 3:
		for (; w > 0; x++, card++, w--) {
			_ipixel_load_card(card, r, g, b, a);
			r = (r >> rloss) << rshift;
			g = (g >> gloss) << gshift;
			b = (b >> bloss) << bshift;
			a = (a >> aloss) << ashift;
			cc = r | g | b | a;
			_ipixel_store(24, dst, 0, cc);
			dst += 3;
		}
		break;
	case 4:
		for (; w > 0; x++, card++, w--) {
			_ipixel_load_card(card, r, g, b, a);
			r = (r >> rloss) << rshift;
			g = (g >> gloss) << gshift;
			b = (b >> bloss) << bshift;
			a = (a >> aloss) << ashift;
			cc = r | g | b | a;
			_ipixel_store(32, dst, 0, cc);
			dst += 4;
		}
		break;
	}
	return 0;
}


/* free format default reader */
static int ipixel_fmt_reader_default(const iPixelFmt *fmt, 
	const void *bits, int x, int w, IUINT32 *card)
{
	IUINT32 rmask = fmt->rmask;
	IUINT32 gmask = fmt->gmask;
	IUINT32 bmask = fmt->bmask;
	IUINT32 amask = fmt->amask;
	int rshift = fmt->rshift;
	int gshift = fmt->gshift;
	int bshift = fmt->bshift;
	int ashift = fmt->ashift;
	int rloss = fmt->rloss;
	int gloss = fmt->gloss;
	int bloss = fmt->bloss;
	int aloss = fmt->aloss;
	const IUINT8 *rscale = &_ipixel_fmt_scale[8 - rloss][0];
	const IUINT8 *gscale = &_ipixel_fmt_scale[8 - gloss][0];
	const IUINT8 *bscale = &_ipixel_fmt_scale[8 - bloss][0];
	const IUINT8 *ascale = &_ipixel_fmt_scale[8 - aloss][0];
	const IUINT8 *src = ((const IUINT8*)bits) + fmt->pixelbyte * x;
	IUINT32 r, g, b, a, cc;
	switch (fmt->pixelbyte) {
	case 1:
		for (; w > 0; x++, card++, w--) {
			cc = _ipixel_fetch(8, src, 0);
			r = rscale[(cc & rmask) >> rshift];
			g = gscale[(cc & gmask) >> gshift];
			b = bscale[(cc & bmask) >> bshift];
			a = ascale[(cc & amask) >> ashift];
			src += 1;
			card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
		}
		break;
	case 2:
		for (; w > 0; x++, card++, w--) {
			cc = _ipixel_fetch(16, src, 0);
			r = rscale[(cc & rmask) >> rshift];
			g = gscale[(cc & gmask) >> gshift];
			b = bscale[(cc & bmask) >> bshift];
			a = ascale[(cc & amask) >> ashift];
			src += 2;
			card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
		}
		break;
	case 3:
		for (; w > 0; x++, card++, w--) {
			cc = _ipixel_fetch(24, src, 0);
			r = rscale[(cc & rmask) >> rshift];
			g = gscale[(cc & gmask) >> gshift];
			b = bscale[(cc & bmask) >> bshift];
			a = ascale[(cc & amask) >> ashift];
			src += 3;
			card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
		}
		break;
	case 4:
		for (; w > 0; x++, card++, w--) {
			cc = _ipixel_fetch(32, src, 0);
			r = rscale[(cc & rmask) >> rshift];
			g = gscale[(cc & gmask) >> gshift];
			b = bscale[(cc & bmask) >> bshift];
			a = ascale[(cc & amask) >> ashift];
			src += 4;
			card[0] = IRGBA_TO_A8R8G8B8(r, g, b, a);
		}
		break;
	}
	return 0;
}


/* ipixel_slow: default slow converter */
long ipixel_fmt_slow(const iPixelFmt *dfmt, void *dbits, long dpitch, 
	int dx, const iPixelFmt *sfmt, const void *sbits, long spitch, 
	int sx, int w, int h, IUINT32 mask, int mode)
{
	int flip, sbpp, dbpp, i, j;
	int transparent;

	flip = mode & (IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP);
	transparent = (mode & IPIXEL_BLIT_MASK)? 1 : 0;

	sbpp = sfmt->bpp;
	dbpp = dfmt->bpp;

	if (flip & IPIXEL_FLIP_VFLIP) { 
		sbits = (const IUINT8*)sbits + spitch * (h - 1); 
		spitch = -spitch; 
	} 

	for (j = 0; j < h; j++) {
		IUINT32 cc = 0, r, g, b, a;
		int incx, x1, x2 = dx;

		if ((flip & IPIXEL_FLIP_HFLIP) == 0) x1 = sx, incx = 1;
		else x1 = sx + w - 1, incx = -1;

		for (i = w; i > 0; x1 += incx, x2++, i--) {
			switch (sbpp) {
				case  1: cc = _ipixel_fetch(1, sbits, x1); break;
				case  4: cc = _ipixel_fetch(4, sbits, x1); break;
				case  8: cc = _ipixel_fetch(8, sbits, x1); break;
				case 16: cc = _ipixel_fetch(16, sbits, x1); break;
				case 24: cc = _ipixel_fetch(24, sbits, x1); break;
				case 32: cc = _ipixel_fetch(32, sbits, x1); break;
			}

			if (transparent && cc == mask) 
				continue;

			IPIXEL_FMT_TO_RGBA(sfmt, cc, r, g, b, a);
			IPIXEL_FMT_FROM_RGBA(dfmt, cc, r, g, b, a);

			switch (dbpp) {
				case  1: _ipixel_store(1, dbits, x2, cc); break;
				case  4: _ipixel_store(4, dbits, x2, cc); break;
				case  8: _ipixel_store(8, dbits, x2, cc); break;
				case 16: _ipixel_store(16, dbits, x2, cc); break;
				case 24: _ipixel_store(24, dbits, x2, cc); break;
				case 32: _ipixel_store(32, dbits, x2, cc); break;
			}
		}

		sbits = (const IUINT8*)sbits + spitch;
		dbits = (IUINT8*)dbits + dpitch;
	}

	return 0;
}


/* ipixel_fmt_cvt: free format convert
 * you must provide a working memory pointer to mem. if mem eq NULL,
 * this function will do nothing but returns how many bytes needed in mem
 * dfmt   - dest pixel format structure
 * dbits  - dest bits
 * dpitch - dest pitch (row stride)
 * dx     - dest x offset
 * sfmt   - source pixel format structure
 * sbits  - source bits
 * spitch - source pitch (row stride)
 * sx     - source x offset
 * w      - width
 * h      - height
 * mask   - mask color (colorkey), no effect without IPIXEL_BLIT_MASK
 * mode   - IPIXEL_FLIP_HFLIP | IPIXEL_FLIP_VFLIP | IPIXEL_BLIT_MASK ..
 * mem    - work memory
 * for transparent converting, set mode with IPIXEL_BLIT_MASK, it will 
 * skip the colors equal to 'mask' parameter.
 */
long ipixel_fmt_cvt(const iPixelFmt *dfmt, void *dbits, long dpitch, 
	int dx, const iPixelFmt *sfmt, const void *sbits, long spitch, int sx,
	int w, int h, IUINT32 mask, int mode, void *mem)
{
	if (mem == NULL) {
		return (long)(w * sizeof(IUINT32));
	}

	if (dfmt->format != IPIX_FMT_UNKNOWN && 
		sfmt->format != IPIX_FMT_UNKNOWN) {
		return ipixel_convert(dfmt->format, dbits, dpitch, dx,
				sfmt->format, sbits, spitch, sx, w, h, mask, mode, 
				NULL, NULL, mem);
	}

	if (dfmt->bpp == sfmt->bpp &&
		dfmt->type == sfmt->type &&
		dfmt->alpha == sfmt->alpha) {
		if (dfmt->rmask == sfmt->rmask &&
			dfmt->gmask == sfmt->gmask &&
			dfmt->bmask == sfmt->bmask &&
			dfmt->amask == sfmt->amask) {
			ipixel_blit(sfmt->bpp, dbits, dpitch, dx, sbits, spitch, 
					sx, w, h, mask, mode);
			return 0;
		}
	}

	if ((mode & IPIXEL_BLIT_MASK) == 0) {
		iPixelFmtReader reader = ipixel_fmt_get_reader(sfmt->bpp, 0);
		iPixelFmtWriter writer = ipixel_fmt_get_writer(dfmt->bpp, 0);
		iFetchProc fetch = NULL;
		iStoreProc store = NULL;
		if (sfmt->format >= 0 && sfmt->format < IPIX_FMT_COUNT) 
			fetch = ipixel_get_fetch(sfmt->format, 0);
		if (dfmt->format >= 0 && dfmt->format < IPIX_FMT_COUNT)
			store = ipixel_get_store(dfmt->format, 0);
		if (reader != NULL && writer != NULL) {
			IUINT32 *card = (IUINT32*)mem;
			int j;
			if (mode & IPIXEL_FLIP_VFLIP) { 
				sbits = (const IUINT8*)sbits + spitch * (h - 1); 
				spitch = -spitch; 
			} 
			for (j = 0; j < h; j++) {
				if (fetch) {
					fetch(sbits, sx, w, card, NULL);
				}	else {
					reader(sfmt, sbits, sx, w, card);
				}
				if (mode & IPIXEL_FLIP_HFLIP) {
					ipixel_card_reverse(card, w);
				}
				if (store) {
					store(dbits, card, dx, w, NULL);
				}	else {
					writer(dfmt, dbits, dx, w, card);
				}
				sbits = (const IUINT8*)sbits + spitch;
				dbits = (IUINT8*)dbits + dpitch;
			}
			return 0;
		}
	}
	return ipixel_fmt_slow(dfmt, dbits, dpitch, dx, sfmt, sbits, spitch,
			sx, w, h, mask, mode);
}


/**********************************************************************
 * CLIPPING
 **********************************************************************/

/*
 * ipixel_clip - clip the rectangle from the src clip and dst clip then
 * caculate a new rectangle which is shared between dst and src cliprect:
 * clipdst  - dest clip array (left, top, right, bottom)
 * clipsrc  - source clip array (left, top, right, bottom)
 * (x, y)   - dest position
 * rectsrc  - source rect
 * mode     - check IPIXEL_FLIP_HFLIP or IPIXEL_FLIP_VFLIP
 * return zero for successful, return non-zero if there is no shared part
 */
int ipixel_clip(const int *clipdst, const int *clipsrc, int *x, int *y,
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
    
    hflip = (mode & IPIXEL_FLIP_HFLIP)? 1 : 0;
    vflip = (mode & IPIXEL_FLIP_VFLIP)? 1 : 0;

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
 * COMPOSITE
 **********************************************************************/
static iPixelComposite ipixel_composite_table[40][2];
static int ipixel_composite_inited = 0;

static void ipixel_comp_src(IUINT32 *dst, const IUINT32 *src, int w)
{
	ipixel_memcpy(dst, src, w * sizeof(IUINT32));
}

static void ipixel_comp_dst(IUINT32 *dst, const IUINT32 *src, int w)
{
}

static void ipixel_comp_clear(IUINT32 *dst, const IUINT32 *src, int w)
{
	memset(dst, 0, sizeof(IUINT32) * w);
}

static void ipixel_comp_blend(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		a1 = ((const IUINT8*)src)[_ipixel_card_alpha];
		if (a1 == 0) continue;
		else if (a1 == 255) dst[0] = src[0];
		else {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);
			IBLEND_NORMAL_FAST(r1, g1, b1, a1, r2, g2, b2, a2);
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

static void ipixel_comp_add(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		_ipixel_load_card(src, r1, g1, b1, a1);
		_ipixel_load_card(dst, r2, g2, b2, a2);
		r2 = ICLIP_256(r1 + r2);
		g2 = ICLIP_256(g1 + g2);
		b2 = ICLIP_256(b1 + b2);
		a2 = ICLIP_256(a1 + a2);
		dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
	}
}

static void ipixel_comp_sub(IUINT32 *dst, const IUINT32 *src, int w)
{
	IINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		_ipixel_load_card(src, r1, g1, b1, a1);
		_ipixel_load_card(dst, r2, g2, b2, a2);
		r2 = ICLIP_256(r2 - r1);
		g2 = ICLIP_256(g2 - g1);
		b2 = ICLIP_256(b2 - b1);
		a2 = ICLIP_256(a2 - a1);
		dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
	}
}

static void ipixel_comp_sub_inv(IUINT32 *dst, const IUINT32 *src, int w)
{
	IINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		_ipixel_load_card(src, r1, g1, b1, a1);
		_ipixel_load_card(dst, r2, g2, b2, a2);
		r2 = ICLIP_256(r1 - r2);
		g2 = ICLIP_256(g1 - g2);
		b2 = ICLIP_256(b1 - b2);
		a2 = ICLIP_256(a1 - a2);
		dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
	}
}


#define IPIXEL_COMPOSITE_NORMAL(name, opname) \
static void ipixel_comp_##name(IUINT32 *dst, const IUINT32 *src, int w)\
{ \
	IUINT32 sr, sg, sb, sa, dr, dg, db, da; \
	for (; w > 0; dst++, src++, w--) { \
		_ipixel_load_card(src, sr, sg, sb, sa); \
		_ipixel_load_card(dst, dr, dg, db, da); \
		sr = _ipixel_mullut[sa][sr]; \
		sg = _ipixel_mullut[sa][sg]; \
		sb = _ipixel_mullut[sa][sb]; \
		dr = _ipixel_mullut[da][dr]; \
		dg = _ipixel_mullut[da][dg]; \
		db = _ipixel_mullut[da][db]; \
		IBLEND_OP_##opname(sr, sg, sb, sa, dr, dg, db, da); \
		dr = _ipixel_divlut[da][dr]; \
		dg = _ipixel_divlut[da][dg]; \
		db = _ipixel_divlut[da][db]; \
		dst[0] = IRGBA_TO_A8R8G8B8(dr, dg, db, da); \
	} \
}

#define IPIXEL_COMPOSITE_PREMUL(name, opname) \
static void ipixel_comp_##name(IUINT32 *dst, const IUINT32 *src, int w)\
{ \
	IUINT32 sr, sg, sb, sa, dr, dg, db, da; \
	for (; w > 0; dst++, src++, w--) { \
		_ipixel_load_card(src, sr, sg, sb, sa); \
		_ipixel_load_card(dst, dr, dg, db, da); \
		IBLEND_OP_##opname(sr, sg, sb, sa, dr, dg, db, da); \
		dst[0] = IRGBA_TO_A8R8G8B8(dr, dg, db, da); \
	} \
}

IPIXEL_COMPOSITE_NORMAL(xor, XOR);
IPIXEL_COMPOSITE_NORMAL(plus, PLUS);
IPIXEL_COMPOSITE_NORMAL(src_atop, SRC_ATOP);
IPIXEL_COMPOSITE_NORMAL(src_in, SRC_IN);
IPIXEL_COMPOSITE_NORMAL(src_out, SRC_OUT);
IPIXEL_COMPOSITE_NORMAL(src_over, SRC_OVER);
IPIXEL_COMPOSITE_NORMAL(dst_atop, DST_ATOP);
IPIXEL_COMPOSITE_NORMAL(dst_in, DST_IN);
IPIXEL_COMPOSITE_NORMAL(dst_out, DST_OUT);
IPIXEL_COMPOSITE_NORMAL(dst_over, DST_OVER);

IPIXEL_COMPOSITE_PREMUL(pre_xor, XOR);
IPIXEL_COMPOSITE_PREMUL(pre_plus, PLUS);
IPIXEL_COMPOSITE_PREMUL(pre_src_atop, SRC_ATOP);
IPIXEL_COMPOSITE_PREMUL(pre_src_in, SRC_IN);
IPIXEL_COMPOSITE_PREMUL(pre_src_out, SRC_OUT);
IPIXEL_COMPOSITE_PREMUL(pre_src_over, SRC_OVER);
IPIXEL_COMPOSITE_PREMUL(pre_dst_atop, DST_ATOP);
IPIXEL_COMPOSITE_PREMUL(pre_dst_in, DST_IN);
IPIXEL_COMPOSITE_PREMUL(pre_dst_out, DST_OUT);
IPIXEL_COMPOSITE_PREMUL(pre_dst_over, DST_OVER);

#undef IPIXEL_COMPOSITE_NORMAL
#undef IPIXEL_COMPOSITE_PREMUL

static void ipixel_comp_preblend(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 c1, c2, a;
	for (; w > 0; dst++, src++, w--) {
		c1 = src[0];
		a = src[0] >> 24;
		if (a == 255) {
			dst[0] = src[0];
		}
		else if (a > 0) {
			c2 = dst[0];
			IBLEND_PARGB(c2, c1);
			dst[0] = c2;
		}
	}
}

static void ipixel_comp_allanon(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 c1, c2, c3, c4;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			c1 = src[0] & 0x00ff00ff;
			c2 = (src[0] >> 8) & 0x00ff00ff;
			c3 = dst[0] & 0x00ff00ff;
			c4 = (dst[0] >> 8) & 0x00ff00ff;
			c1 = (c1 + c3) >> 1;
			c2 = (c2 + c4) >> 1;
			dst[0] = (c1 & 0x00ff00ff) | ((c2 & 0x00ff00ff) << 8);
		}
	}
}

static void ipixel_comp_tint(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);
			r1 = r1 * r2;
			g1 = g1 * g2;
			b1 = b1 * b2;
			r1 = _ipixel_fast_div_255(r1);
			g1 = _ipixel_fast_div_255(g1);
			b1 = _ipixel_fast_div_255(b1);
			dst[0] = IRGBA_TO_A8R8G8B8(r1, g1, b1, a2);
		}
	}
	r1 = a2 + a1;
}

static void ipixel_comp_diff(IUINT32 *dst, const IUINT32 *src, int w)
{
	IINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);
			r1 -= r2;
			g1 -= g2;
			b1 -= b2;
			r2 = (r1 < 0)? -r1 : r1;
			g2 = (g1 < 0)? -g1 : g1;
			b2 = (b1 < 0)? -b1 : b1;
			if (a1 > a2) a2 = a1;
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

static void ipixel_comp_darken(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);
			if (a1 < a2) a2 = a1;
			if (r1 < r2) r2 = r1;
			if (g1 < g2) g2 = g1;
			if (b1 < b2) b2 = b1;
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

static void ipixel_comp_lighten(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);
			if (a1 > a2) a2 = a1;
			if (r1 > r2) r2 = r1;
			if (g1 > g2) g2 = g1;
			if (b1 > b2) b2 = b1;
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

static void ipixel_comp_screen(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, res1, res2;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);

			#define IPIXEL_SCREEN_VALUE(b, t) do { \
				res1 = 0xFF - b; res2 = 0xff - t; \
				res1 = 0xff - ((res1 * res2) >> 8); \
				b = res1; } while (0)

			IPIXEL_SCREEN_VALUE(r2, r1);
			IPIXEL_SCREEN_VALUE(g2, g1);
			IPIXEL_SCREEN_VALUE(b2, b1);

			#undef IPIXEL_SCREEN_VALUE

			if (a1 > a2) a2 = a1;
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

static void ipixel_comp_overlay(IUINT32 *dst, const IUINT32 *src, int w)
{
	IUINT32 r1, g1, b1, a1, r2, g2, b2, a2, tmp_screen, tmp_mult, res;
	for (; w > 0; dst++, src++, w--) {
		if ((src[0] >> 24) != 0) {
			_ipixel_load_card(src, r1, g1, b1, a1);
			_ipixel_load_card(dst, r2, g2, b2, a2);

		#define IPIXEL_OVERLAY_VALUE(b, t) do { \
				tmp_screen = 0xff - (((0xff - (int)b) * (0xff - t)) >> 8); \
				tmp_mult   = (b * t) >> 8; \
				res = (b * tmp_screen + (0xff - b) * tmp_mult) >> 8; \
				b = res; \
			}	while (0)

			IPIXEL_OVERLAY_VALUE(r2, r1);
			IPIXEL_OVERLAY_VALUE(g2, g1);
			IPIXEL_OVERLAY_VALUE(b2, b1);

		#undef IPIXEL_OVERLAY_VALUE

			if (a1 > a2) a2 = a1;
			dst[0] = IRGBA_TO_A8R8G8B8(r2, g2, b2, a2);
		}
	}
}

/* initialize compositors */
static void ipixel_composite_init(void)
{
	if (ipixel_composite_inited) return;
	#define ipixel_composite_install(opname, name) do { \
		ipixel_composite_table[IPIXEL_OP_##opname][0] = ipixel_comp_##name; \
		ipixel_composite_table[IPIXEL_OP_##opname][1] = ipixel_comp_##name; \
	}	while (0) 

	ipixel_composite_install(SRC, src);
	ipixel_composite_install(DST, dst);
	ipixel_composite_install(CLEAR, clear);
	ipixel_composite_install(BLEND, blend);
	ipixel_composite_install(ADD, add);
	ipixel_composite_install(SUB, sub);
	ipixel_composite_install(SUB_INV, sub_inv);

	ipixel_composite_install(XOR, xor);
	ipixel_composite_install(PLUS, plus);
	ipixel_composite_install(SRC_ATOP, src_atop);
	ipixel_composite_install(SRC_IN, src_in);
	ipixel_composite_install(SRC_OUT, src_out);
	ipixel_composite_install(SRC_OVER, src_over);
	ipixel_composite_install(DST_ATOP, dst_atop);
	ipixel_composite_install(DST_IN, dst_in);
	ipixel_composite_install(DST_OUT, dst_out);
	ipixel_composite_install(DST_OVER, dst_over);

	ipixel_composite_install(PREMUL_XOR, pre_xor);
	ipixel_composite_install(PREMUL_PLUS, pre_plus);
	ipixel_composite_install(PREMUL_SRC_ATOP, pre_src_atop);
	ipixel_composite_install(PREMUL_SRC_IN, pre_src_in);
	ipixel_composite_install(PREMUL_SRC_OUT, pre_src_out);
	ipixel_composite_install(PREMUL_SRC_OVER, pre_src_over);
	ipixel_composite_install(PREMUL_DST_ATOP, pre_dst_atop);
	ipixel_composite_install(PREMUL_DST_IN, pre_dst_in);
	ipixel_composite_install(PREMUL_DST_OUT, pre_dst_out);
	ipixel_composite_install(PREMUL_DST_OVER, pre_dst_over);

	ipixel_composite_install(PREMUL_BLEND, preblend);
	ipixel_composite_install(ALLANON, allanon);
	ipixel_composite_install(TINT, tint);
	ipixel_composite_install(DIFF, diff);
	ipixel_composite_install(DARKEN, darken);
	ipixel_composite_install(LIGHTEN, lighten);
	ipixel_composite_install(SCREEN, screen);
	ipixel_composite_install(OVERLAY, overlay);

	#undef ipixel_composite_install

	ipixel_lut_init();
	ipixel_composite_inited = 1;
}


/* get compositor */
iPixelComposite ipixel_composite_get(int op, int isdefault)
{
	if (ipixel_composite_inited == 0) ipixel_composite_init();
	if (op < 0 || op > IPIXEL_OP_OVERLAY) return NULL;
	return ipixel_composite_table[op][isdefault ? 1 : 0];
}

/* set compositor */
void ipixel_composite_set(int op, iPixelComposite composite)
{
	if (ipixel_composite_inited == 0) ipixel_composite_init();
	if (op < 0 || op > IPIXEL_OP_OVERLAY) return;
	if (composite == NULL) composite = ipixel_composite_table[op][1];
	ipixel_composite_table[op][0] = composite;
}

/* composite operator names */
const char *ipixel_composite_opnames[] = {
	"IPIXEL_OP_SRC",
	"IPIXEL_OP_DST",
	"IPIXEL_OP_CLEAR",
	"IPIXEL_OP_BLEND",
	"IPIXEL_OP_ADD",
	"IPIXEL_OP_SUB",
	"IPIXEL_OP_SUB_INV",
	"IPIXEL_OP_XOR",
	"IPIXEL_OP_PLUS",
	"IPIXEL_OP_SRC_ATOP",
	"IPIXEL_OP_SRC_IN",
	"IPIXEL_OP_SRC_OUT",
	"IPIXEL_OP_SRC_OVER",
	"IPIXEL_OP_DST_ATOP",
	"IPIXEL_OP_DST_IN",
	"IPIXEL_OP_DST_OUT",
	"IPIXEL_OP_DST_OVER",
	"IPIXEL_OP_PREMUL_XOR",
	"IPIXEL_OP_PREMUL_PLUS",
	"IPIXEL_OP_PREMUL_SRC_OVER",
	"IPIXEL_OP_PREMUL_SRC_IN",
	"IPIXEL_OP_PREMUL_SRC_OUT",
	"IPIXEL_OP_PREMUL_SRC_ATOP",
	"IPIXEL_OP_PREMUL_DST_OVER",
	"IPIXEL_OP_PREMUL_DST_IN",
	"IPIXEL_OP_PREMUL_DST_OUT",
	"IPIXEL_OP_PREMUL_DST_ATOP",
	"IPIXEL_OP_PREMUL_BLEND",
	"IPIXEL_OP_ALLANON",
	"IPIXEL_OP_TINT",
	"IPIXEL_OP_DIFF",
	"IPIXEL_OP_DARKEN",
	"IPIXEL_OP_LIGHTEN",
	"IPIXEL_OP_SCREEN",
	"IPIXEL_OP_OVERLAY",
};

/* get composite operator names */
const char *ipixel_composite_opname(int op)
{
	if (op < 0 || op > IPIXEL_OP_OVERLAY) return "UNKNOW";
	return ipixel_composite_opnames[op];
}


/**********************************************************************
 * Palette & Others
 **********************************************************************/

/* fetch card from IPIX_FMT_C8 */
void ipixel_palette_fetch(const unsigned char *src, int w, IUINT32 *card,
	const IRGB *palette)
{
	IUINT32 r, g, b;
	for (; w > 0; src++, card++, w--) {
		r = palette[*src].r;
		g = palette[*src].g;
		b = palette[*src].b;
		card[0] = IRGBA_TO_A8R8G8B8(r, g, b, 255);
	}
}

/* store card into IPIX_FMT_C8 */
void ipixel_palette_store(unsigned char *dst, int w, const IUINT32 *card,
	const IRGB *palette, int palsize)
{
	IUINT32 r, g, b;
	for (; w > 0; dst++, card++, w--) {
		ISPLIT_RGB(card[0], r, g, b);
		dst[0] = ipixel_palette_fit(palette, r, g, b, palsize);
	}
}

/* batch draw dots */
int ipixel_set_dots(int bpp, void *bits, long pitch, int w, int h,
	IUINT32 color, const short *xylist, int count, const int *clip)
{
	IUINT8 *image = (IUINT8*)bits;
	int cl = 0, cr = w, ct = 0, cb = h, i;
	int pixelbyte = (bpp + 7) / 8;
	if (clip) {
		cl = clip[0];
		ct = clip[1];
		cr = clip[2];
		cb = clip[3];
		if (cl < 0) cl = 0;
		if (ct < 0) ct = 0;
		if (cr > w) cr = w;
		if (cb > h) cb = h;
	}
	cr--;    /* use range [cl, cr] */
	cb--;
	switch (pixelbyte) {
	case 1:
		for (i = count; i > 0; xylist += 2, i--) {
			int x = xylist[0];
			int y = xylist[1];
			if (((x - cl) | (y - ct) | (cr - x) | (cb - y)) >= 0) {
				IUINT8 *line = image + pitch * y;
				_ipixel_store(8, line, x, color);
			}
		}
		break;
	case 2:
		for (i = count; i > 0; xylist += 2, i--) {
			int x = xylist[0];
			int y = xylist[1];
			if (((x - cl) | (y - ct) | (cr - x) | (cb - y)) >= 0) {
				IUINT8 *line = image + pitch * y;
				_ipixel_store(16, line, x, color);
			}
		}
		break;
	case 3:
		for (i = count; i > 0; xylist += 2, i--) {
			int x = xylist[0];
			int y = xylist[1];
			if (((x - cl) | (y - ct) | (cr - x) | (cb - y)) >= 0) {
				IUINT8 *line = image + pitch * y;
				_ipixel_store(24, line, x, color);
			}
		}
		break;
	case 4:
		for (i = count; i > 0; xylist += 2, i--) {
			int x = xylist[0];
			int y = xylist[1];
			if (((x - cl) | (y - ct) | (cr - x) | (cb - y)) >= 0) {
				IUINT8 *line = image + pitch * y;
				_ipixel_store(24, line, x, color);
			}
		}
		break;
	}
	return 0;
}


/* vim: set ts=4 sw=4 tw=0 noet :*/

