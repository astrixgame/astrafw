#pragma once

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Font data format
//
// Glyph bitmaps are packed 4bpp (two pixels per byte, MSN first).
// Alpha value 0x0 = fully transparent, 0xF = fully opaque.
//
// Font data C files are generated from a TTF via:
//   pip install freetype-py
//   python3 tools/gen_font.py path/to/Arial.ttf 20
// The generator outputs font_arial_20.c  — add it to SRCS in CMakeLists.txt.
// ---------------------------------------------------------------------------

typedef struct {
    uint32_t bitmap_index;  // byte offset into glyph_bitmap
    uint16_t adv_w;         // advance width * 16  (fixed-point 12.4, px << 4)
    uint8_t  box_w;         // bounding box width  (px)
    uint8_t  box_h;         // bounding box height (px)
    int8_t   ofs_x;         // left bearing  (px from cursor to left edge)
    int8_t   ofs_y;         // top bearing   (px from baseline to top of glyph, positive = above)
} font_glyph_dsc_t;

typedef struct {
    const uint8_t          *glyph_bitmap;  // packed 4bpp bitmap data
    const font_glyph_dsc_t *glyph_dsc;    // [0] = notdef glyph, [1..n] = codepoints
    uint32_t                range_start;   // first Unicode codepoint in contiguous range
    uint16_t                range_length;  // number of codepoints
    uint8_t                 bpp;           // always 4
    uint8_t                 line_height;   // vertical advance (px)
    uint8_t                 base_line;     // px below the baseline (descender)
} font_t;

// ---------------------------------------------------------------------------
// Renderer API
// ---------------------------------------------------------------------------

// Draw a UTF-8 string at pixel (x, y) where y is the TOP of the line.
// Text is composited over whatever is already in the framebuffer.
// Call display_flush() after to push to the panel.
void font_draw_string(const font_t *font, int x, int y,
                      const char *str, uint16_t color);

// Measure the pixel width a string would occupy without drawing it.
int font_measure_string(const font_t *font, const char *str);

// Line height of the font in pixels.
static inline int font_line_height(const font_t *font) { return font->line_height; }
