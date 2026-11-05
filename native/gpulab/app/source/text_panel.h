#ifndef TEXT_PANEL_H
#define TEXT_PANEL_H

/*
 * Portable C99 CPU text panel (synthetic public contract).
 *
 * Pixels are opaque 32-bit values; colors are copied exactly. No pixel
 * format, tiling, presentation or display result is implied.
 */

#include <stddef.h>
#include <stdint.h>

#define TP_MAX_DIMENSION 4096u
#define TP_MAX_LINES 22u
#define TP_MAX_LINE_LENGTH 80u
#define TP_CELL_WIDTH 6u
#define TP_CELL_HEIGHT 12u

typedef struct tp_line {
    const unsigned char *text; /* may be NULL only when length == 0 */
    size_t length;             /* explicit byte count, no terminator scan */
} tp_line;

typedef enum tp_result {
    TP_OK = 0,
    TP_INVALID
} tp_result;

/*
 * Renders up to 22 lines of printable ASCII into a caller-owned pixel array.
 *
 * Preconditions (all checked; any violation returns TP_INVALID and leaves
 * every destination pixel unchanged):
 *   - pixels != NULL
 *   - 1 <= width <= 4096, 1 <= height <= 4096
 *   - width <= stride_pixels <= 4096
 *   - capacity_pixels >= (height - 1) * stride_pixels + width
 *     (computed with overflow-safe size arithmetic)
 *   - line_count <= 22; lines != NULL when line_count > 0
 *   - each lines[i].length <= 80; lines[i].text != NULL when length > 0
 *   - every supplied byte is printable ASCII 0x20..0x7e
 *
 * Caller preconditions (NOT checked):
 *   - pixels refers to at least capacity_pixels accessible uint32_t values,
 *     lines to line_count descriptors, and each text to length bytes.
 *   - Line descriptors, text bytes and the pixel array do not overlap.
 *   - No validation of hostile or dangling pointers is performed.
 *
 * On success: for each row y < height, pixels[y*stride + x] for x < width
 * are set to background; stride padding and any other capacity pixels are
 * untouched. Line i is drawn at x = 0, y = i * 12 in 6x12 cells holding 5x7
 * glyphs (column 5 and row 7 blank). Set glyph bits receive foreground;
 * glyph pixels beyond the right/bottom bounds are clipped. Returns TP_OK.
 */
tp_result tp_render(uint32_t *pixels, size_t capacity_pixels,
                    size_t width, size_t height, size_t stride_pixels,
                    const tp_line *lines, size_t line_count,
                    uint32_t foreground, uint32_t background);

#endif /* TEXT_PANEL_H */

