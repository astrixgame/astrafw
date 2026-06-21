#include "font_render.h"
#include "../display_driver/display_driver.h"

// ---------------------------------------------------------------------------
// Per-glyph static render buffer.
// Sized for the largest expected glyph (Arial 48pt W/M: ~50×56px).
// 56 × 62 × 2 = 6944 bytes — allocated in BSS, not heap.
// ---------------------------------------------------------------------------

#define GLYPH_BUF_MAX_W 56
#define GLYPH_BUF_MAX_H 62
static uint16_t s_glyph_buf[GLYPH_BUF_MAX_W * GLYPH_BUF_MAX_H];

// ---------------------------------------------------------------------------
// RGB565 alpha blend (both fg and bg are byte-swapped for SPI)
// ---------------------------------------------------------------------------

static inline uint16_t swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint16_t blend_rgb565(uint16_t fg_s, uint16_t bg_s, uint8_t alpha) {
    if (alpha == 0)   return bg_s;
    if (alpha == 255) return fg_s;

    uint16_t f = swap16(fg_s);
    uint16_t b = swap16(bg_s);

    int fr = (f >> 11) & 0x1F,  fg = (f >> 5) & 0x3F,  fb = f & 0x1F;
    int br = (b >> 11) & 0x1F,  bg = (b >> 5) & 0x3F,  bb = b & 0x1F;

    int r  = (fr * alpha + br * (255 - alpha)) >> 8;
    int g  = (fg * alpha + bg * (255 - alpha)) >> 8;
    int bl = (fb * alpha + bb * (255 - alpha)) >> 8;

    return swap16((uint16_t)((r << 11) | (g << 5) | bl));
}

// ---------------------------------------------------------------------------
// UTF-8 decoder
// ---------------------------------------------------------------------------

static uint32_t utf8_next(const uint8_t **p) {
    uint8_t c = **p;
    uint32_t cp;
    if (c < 0x80) {
        cp = c; (*p)++;
    } else if ((c & 0xE0) == 0xC0) {
        cp  = (uint32_t)(c & 0x1F) << 6;
        cp |= (*p)[1] & 0x3F;
        *p += 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp  = (uint32_t)(c & 0x0F) << 12;
        cp |= (uint32_t)((*p)[1] & 0x3F) << 6;
        cp |=  (*p)[2] & 0x3F;
        *p += 3;
    } else {
        cp  = (uint32_t)(c & 0x07) << 18;
        cp |= (uint32_t)((*p)[1] & 0x3F) << 12;
        cp |= (uint32_t)((*p)[2] & 0x3F) << 6;
        cp |=  (*p)[3] & 0x3F;
        *p += 4;
    }
    return cp;
}

// ---------------------------------------------------------------------------
// Glyph lookup
// ---------------------------------------------------------------------------

static const font_glyph_dsc_t *get_glyph(const font_t *font, uint32_t cp) {
    if (cp >= font->range_start && cp < font->range_start + font->range_length)
        return &font->glyph_dsc[1 + (cp - font->range_start)];
    return &font->glyph_dsc[0]; // notdef
}

// ---------------------------------------------------------------------------
// Glyph draw — renders into s_glyph_buf, sends to panel via display_draw_bitmap
// ---------------------------------------------------------------------------

static void draw_glyph(const font_t *font, const font_glyph_dsc_t *g,
                        int cx, int baseline_y, uint16_t color) {
    if (g->box_w == 0 || g->box_h == 0) return;

    const uint8_t *bmp = font->glyph_bitmap + g->bitmap_index;
    int gx = cx + g->ofs_x;
    int gy = baseline_y - g->ofs_y;

    // Clip to screen bounds
    int col_start = 0, col_end = g->box_w;
    int row_start = 0, row_end = g->box_h;
    if (gx < 0)               col_start = -gx;
    if (gx + col_end > LCD_W) col_end   = LCD_W - gx;
    if (gy < 0)               row_start = -gy;
    if (gy + row_end > LCD_H) row_end   = LCD_H - gy;

    int draw_w = col_end - col_start;
    int draw_h = row_end - row_start;
    if (draw_w <= 0 || draw_h <= 0) return;
    if (draw_w > GLYPH_BUF_MAX_W || draw_h > GLYPH_BUF_MAX_H) return;

    uint16_t bg = display_get_bg_color();

    // Fill buffer with background color
    for (int i = 0; i < draw_w * draw_h; i++) s_glyph_buf[i] = bg;

    // Render visible glyph pixels with alpha blend
    for (int row = row_start; row < row_end; row++) {
        for (int col = col_start; col < col_end; col++) {
            int bit = row * g->box_w + col;
            uint8_t nibble = (bit & 1)
                ? (bmp[bit >> 1] & 0x0F)
                : (bmp[bit >> 1] >> 4);
            if (nibble == 0) continue;
            uint8_t alpha = (uint8_t)((nibble << 4) | nibble);
            int buf_idx = (row - row_start) * draw_w + (col - col_start);
            s_glyph_buf[buf_idx] = blend_rgb565(color, bg, alpha);
        }
    }

    display_draw_bitmap(gx + col_start, gy + row_start, draw_w, draw_h, s_glyph_buf);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void font_draw_string(const font_t *font, int x, int y,
                       const char *str, uint16_t color) {
    if (!font || !str) return;

    int            cx         = x;
    int            baseline_y = y + font->line_height - font->base_line;
    const uint8_t *p          = (const uint8_t *)str;

    while (*p) {
        uint32_t cp = utf8_next(&p);
        const font_glyph_dsc_t *g = get_glyph(font, cp);
        draw_glyph(font, g, cx, baseline_y, color);
        cx += g->adv_w >> 4;
    }
}

int font_measure_string(const font_t *font, const char *str) {
    if (!font || !str) return 0;

    int            w = 0;
    const uint8_t *p = (const uint8_t *)str;

    while (*p) {
        uint32_t cp = utf8_next(&p);
        w += get_glyph(font, cp)->adv_w >> 4;
    }
    return w;
}
