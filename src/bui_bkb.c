/*
 * License for the BOLOS User Interface Library project, originally found here:
 * https://github.com/parkerhoyes/bolos-user-interface
 *
 * Copyright (C) 2016 Parker Hoyes <contact@parkerhoyes.com>
 *
 * This software is provided "as-is", without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 *    that you wrote the original software. If you use this software in a
 *    product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "bui_bkb.h"

#include <stdbool.h>
#include <stdint.h>

#include "os.h"

#include "bui.h"
#include "bui_font.h"

#define CEIL_DIV(x, y) (1 + (((x) - 1) / (y)))
#define NTH_BIT(n, i) (((n) >> (7 - (i))) & 1) // Only to be used with uint8_t

#define KEYS_ANIMATION_LEN 36 // The duration of the keys animation, in 10 ms increments
#define CURSOR_ANIMATION_INT 100 // Half the period of the cursor blink animation, in 10 ms increments

static const uint8_t bitmap_ellipsis_bitmap[] = {
	0x00, 0x2A, 0x00, 0x00, 0x00,
};
static const uint8_t bitmap_ellipsis_w = 5;
static const uint8_t bitmap_ellipsis_h = 8;

static const uint8_t bitmap_space_bitmap[] = {
	0x00, 0x3F, 0x10, 0x00, 0x00,
};
static const uint8_t bitmap_space_w = 5;
static const uint8_t bitmap_space_h = 8;

static const uint8_t bitmap_toggle_case_bitmap[] = {
	0x03, 0xA2, 0x38, 0xE2, 0x2E,
};
static const uint8_t bitmap_toggle_case_w = 5;
static const uint8_t bitmap_toggle_case_h = 8;

const char bui_bkb_layout_alphabetic[26] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z',
};
const char bui_bkb_layout_numeric[10] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};
const char bui_bkb_layout_alphanumeric[27] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', BUI_BKB_OPTION_NUMERICS,
};
const char bui_bkb_layout_hexadecimal[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};
const char bui_bkb_layout_symbols[32] = {
	'!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '@', '[',
	'\\', ']', '^', '_', '`', '{', '|', '}', '~',
};
const char bui_bkb_layout_standard[30] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', ' ', BUI_BKB_OPTION_NUMERICS, BUI_BKB_OPTION_SYMBOLS, BUI_BKB_OPTION_TOGGLE_CASE,
};

/*
 * Toggle the case of all characters in the specified string. All non-alphabetic characters in the string will not be
 * modified.
 *
 * Args:
 *     str: the string
 *     n: the length of the string, not including the null terminator if there is one
 */
static void bui_bkb_toggle_case(char *str, uint8_t n) {
	char *end = str + n;
	for (; str != end; str++) {
		if (*str >= 'A' && *str <= 'Z')
			*str += 'a' - 'A';
		else if (*str >= 'a' && *str <= 'z')
			*str -= 'a' - 'A';
	}
}

/*
 * Draw a key onto the keyboard at the specified position.
 *
 * Args:
 *     buffer: the display buffer
 *     key: the key to be drawn; may be a character displayable in bui_font_lucida_console_8, or an option key; the only
 *          whitespace character allowed is a space
 *     x: the x-coordinate of the top-left corner of the destination
 *     y: the y-coordinate of the top-left corner of the destination
 */
static void bui_bkb_draw_key(bui_bitmap_128x32_t *buffer, char key, int16_t x, int16_t y) {
	switch (key) {
	case BUI_BKB_OPTION_NUMERICS:
		key = '#';
		break;
	case BUI_BKB_OPTION_SYMBOLS:
		key = '@';
		break;
	case BUI_BKB_OPTION_TOGGLE_CASE:
		bui_draw_bitmap(buffer, bitmap_toggle_case_bitmap, bitmap_toggle_case_w, 0, 0, x, y, bitmap_toggle_case_w,
				bitmap_toggle_case_h);
		return;
	case ' ':
		bui_draw_bitmap(buffer, bitmap_space_bitmap, bitmap_space_w, 0, 0, x, y, bitmap_space_w, bitmap_space_h);
		return;
	}
	bui_font_draw_char(buffer, key, x, y, BUI_DIR_LEFT_TOP, bui_font_lucida_console_8);
}

void bui_bkb_init(bui_bkb_bkb_t *bkb, const char *layout, uint8_t layout_size, char *type_buff, uint8_t type_buff_size,
		uint8_t type_buff_cap, bool animations) {
	if (layout_size != 0)
		os_memcpy(bkb->layout, layout, layout_size);
	bkb->layout_size = layout_size;
	bkb->type_buff = type_buff;
	bkb->type_buff_size = type_buff_size;
	bkb->type_buff_cap = type_buff_cap;
	bkb->bits_typed = 0;
	bkb->bits_typed_size = 0;
	bkb->option = '\0';
	bkb->keys_tick = animations ? KEYS_ANIMATION_LEN : 255;
	bkb->cursor_tick = 0;
}

int bui_bkb_choose(bui_bkb_bkb_t *bkb, bui_dir_e side) {
	// Handle full type buffer
	if (bkb->type_buff_size == bkb->type_buff_cap) {
		if (side == BUI_DIR_LEFT) { // If backspace key was chosen
			bkb->type_buff_size -= 1;
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return 0x2FF;
		}
		return 0x1FF; // No character was chosen
	}

	// Get currently active layout
	const char *layout;
	uint8_t layout_size;
	switch (bkb->option) {
	case '\0':
		layout = bkb->layout;
		layout_size = bkb->layout_size;
		break;
	case BUI_BKB_OPTION_NUMERICS:
		layout = bui_bkb_layout_numeric;
		layout_size = sizeof(bui_bkb_layout_numeric);
		break;
	case BUI_BKB_OPTION_SYMBOLS:
		layout = bui_bkb_layout_symbols;
		layout_size = sizeof(bui_bkb_layout_symbols);
		break;
	}

	// Add choice to sequence
	if (side == BUI_DIR_RIGHT)
		bkb->bits_typed |= 0x80 >> bkb->bits_typed_size;
	bkb->bits_typed_size += 1;

	// Find chosen character
	uint8_t charsn = layout_size;
	if (bkb->type_buff_size != 0 && bkb->option == '\0') // Handle backspace key, if applicable
		charsn += 1;
	uint8_t charsi = 0;
	for (uint8_t i = 0; i < bkb->bits_typed_size; i++) {
		if (NTH_BIT(bkb->bits_typed, i) == 0) {
			charsn = CEIL_DIV(charsn, 2);
		} else {
			charsi += CEIL_DIV(charsn, 2);
			charsn /= 2;
		}
	}

	// If a single key has been chosen, apply choice
	if (charsn == 1) {
		bkb->bits_typed = 0;
		bkb->bits_typed_size = 0;
		if (charsi == layout_size) { // If backspace key was chosen
			bkb->type_buff_size -= 1;
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return 0x2FF;
		}
		char ch = layout[charsi];
		switch (ch) {
		case BUI_BKB_OPTION_NUMERICS:
			bkb->option = BUI_BKB_OPTION_NUMERICS;
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return 0x1FF; // No character was chosen
		case BUI_BKB_OPTION_SYMBOLS:
			bkb->option = BUI_BKB_OPTION_SYMBOLS;
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return 0x1FF; // No character was chosen
		case BUI_BKB_OPTION_TOGGLE_CASE:
			bui_bkb_toggle_case(bkb->layout, bkb->layout_size);
			bkb->option = '\0';
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return 0x1FF; // No character was chosen
		default:
			bkb->type_buff[bkb->type_buff_size++] = ch;
			bkb->option = '\0';
			if (bkb->keys_tick != 255)
				bkb->keys_tick = KEYS_ANIMATION_LEN; // Finish animation
			return ch;
		}
	}

	if (bkb->keys_tick != 255)
		bkb->keys_tick = 0; // Restart animation
	return 0x1FF; // No character was chosen
}

bool bui_bkb_tick(bui_bkb_bkb_t *bkb, uint32_t elapsed) {
	if (elapsed == 0)
		return false;
	bool change = false;
	if (bkb->keys_tick < KEYS_ANIMATION_LEN) {
		if (elapsed >= KEYS_ANIMATION_LEN - bkb->keys_tick)
			bkb->keys_tick = KEYS_ANIMATION_LEN;
		else
			bkb->keys_tick += elapsed;
		change = true;
	}
	bool last_cursor = bkb->cursor_tick < CURSOR_ANIMATION_INT;
	bkb->cursor_tick += elapsed % (CURSOR_ANIMATION_INT * 2);
	bkb->cursor_tick %= CURSOR_ANIMATION_INT * 2;
	if (last_cursor != (bool) (bkb->cursor_tick < CURSOR_ANIMATION_INT))
		change = true;
	return change;
}

void bui_bkb_draw(const bui_bkb_bkb_t *bkb, bui_bitmap_128x32_t *buffer) {
	// Locate textbox
	uint8_t textbox_size = bkb->type_buff_cap + 1; // The number of slots in the textbox
	if (textbox_size > 20)
		textbox_size = 20;
	// The index in bkb->type_buff of the first character displayed in the textbox (which must be replaced with an
	// ellipsis textbox_ellipsis is true)
	uint8_t textbox_i = bkb->type_buff_size <= 19 ? 0 : bkb->type_buff_size - 19;
	// The x-coordinate of the leftmost pixel at the leftmost slot of the textbox
	uint8_t textbox_x = 64 - textbox_size * 6 / 2;
	// True if an ellipsis should be displayed in the first slot instead of the first character, false otherwise
	bool textbox_ellipsis = bkb->type_buff_size > 19;
	// The slot index of the cursor
	uint8_t textbox_cursor_i = bkb->type_buff_size;
	if (textbox_cursor_i > 19)
		textbox_cursor_i = 19;

	// Draw textbox slots
	for (uint8_t i = 0; i < textbox_size; i++) {
		bui_fill_rect(buffer, textbox_x + i * 6, 31, 5, 1, true);
	}

	// Draw textbox contents
	for (uint8_t i = 0; i <= textbox_cursor_i; i++) {
		if (i == 0 && textbox_ellipsis) {
			bui_draw_bitmap(buffer, bitmap_ellipsis_bitmap, bitmap_ellipsis_w, 0, 0, textbox_x, 22, bitmap_ellipsis_w,
					bitmap_ellipsis_h);
		} else if (i < textbox_cursor_i) {
			bui_font_draw_char(buffer, bkb->type_buff[textbox_i + i], textbox_x + i * 6, 22, BUI_DIR_LEFT_TOP,
					bui_font_lucida_console_8);
		} else { // i == textbox_cursor_i
			if (bkb->keys_tick == 255 || bkb->cursor_tick < 10)
				bui_fill_rect(buffer, textbox_x + textbox_cursor_i * 6 + 2, 22, 1, 7, true); // Draw cursor
		}
	}

	// Draw center arrow icons
	bui_draw_bitmap(buffer, bui_bitmap_left_bitmap, bui_bitmap_left_w, 0, 0, 58, 5, bui_bitmap_left_w,
			bui_bitmap_left_h);
	bui_draw_bitmap(buffer, bui_bitmap_right_bitmap, bui_bitmap_right_w, 0, 0, 66, 5, bui_bitmap_right_w,
			bui_bitmap_right_h);

	// Draw keyboard "keys"
	if (bkb->type_buff_size != bkb->type_buff_cap) { // If the textbox is not full
		// Get currently active layout
		const char *layout;
		uint8_t layout_size;
		switch (bkb->option) {
		case '\0':
			layout = bkb->layout;
			layout_size = bkb->layout_size;
			break;
		case BUI_BKB_OPTION_NUMERICS:
			layout = bui_bkb_layout_numeric;
			layout_size = sizeof(bui_bkb_layout_numeric);
			break;
		case BUI_BKB_OPTION_SYMBOLS:
			layout = bui_bkb_layout_symbols;
			layout_size = sizeof(bui_bkb_layout_symbols);
			break;
		}

		// Find currently and previously displayed sections
		uint8_t prev_charsi = 0;
		uint8_t prev_charsn = layout_size;
		if (bkb->type_buff_size != 0 && bkb->option == '\0') // Handle backspace key, if applicable
			prev_charsn += 1;
		for (uint8_t i = 0; i + 1 < bkb->bits_typed_size; i++) {
			if (NTH_BIT(bkb->bits_typed, i) == 0) {
				prev_charsn = CEIL_DIV(prev_charsn, 2);
			} else {
				prev_charsi += CEIL_DIV(prev_charsn, 2);
				prev_charsn /= 2;
			}
		}
		uint8_t charsi = prev_charsi;
		uint8_t charsn = prev_charsn;
		if (bkb->bits_typed_size != 0) {
			if (NTH_BIT(bkb->bits_typed, bkb->bits_typed_size - 1) == 0) {
				charsn = CEIL_DIV(charsn, 2);
			} else {
				charsi += CEIL_DIV(charsn, 2);
				charsn /= 2;
			}
		}
		uint8_t lefti = charsi;
		uint8_t leftn = CEIL_DIV(charsn, 2);
		uint8_t prev_lefti = prev_charsi;
		uint8_t prev_leftn = CEIL_DIV(prev_charsn, 2);
		uint8_t righti = lefti + leftn;
		uint8_t rightn = charsn / 2;
		uint8_t prev_righti = prev_lefti + prev_leftn;

		// Draw keys on left side
		for (uint8_t i = 0; i < leftn; i++) {
			uint8_t x = 1 + 6 * (i % 9);
			uint8_t y = i < 9 ? 0 : 9;
			// If currently animating and the current character was previously on the right side
			if (bkb->keys_tick < KEYS_ANIMATION_LEN && lefti + i >= prev_righti) {
				uint8_t oldi = lefti + i - prev_righti;
				uint8_t prev_x = 74 + 6 * (oldi % 9);
				uint8_t prev_y = 0; // The first half of the group of keys on the right half are always on the top row
				x = (int) prev_x + ((int) x - (int) prev_x) * (int) bkb->keys_tick / KEYS_ANIMATION_LEN;
				y = (int) prev_y + ((int) y - (int) prev_y) * (int) bkb->keys_tick / KEYS_ANIMATION_LEN;
			}
			bui_bkb_draw_key(buffer, layout[lefti + i], x, y);
		}

		// Draw keys on right side
		for (uint8_t i = 0; i < rightn; i++) {
			uint8_t x = 74 + 6 * (i % 9);
			uint8_t y = i < 9 ? 0 : 9;
			if (bkb->keys_tick < KEYS_ANIMATION_LEN) { // If currently animating
				uint8_t prev_x;
				uint8_t prev_y;
				if (righti + i < prev_righti) { // If the current character was previously on the left side
					uint8_t oldi = righti + i - prev_lefti;
					prev_x = 1 + 6 * (oldi % 9);
					prev_y = oldi < 9 ? 0 : 9;
				} else { // The current character was previously on the right side
					uint8_t oldi = righti + i - prev_righti;
					prev_x = 74 + 6 * (oldi % 9);
					prev_y = oldi < 9 ? 0 : 9;
				}
				x = (int) prev_x + ((int) x - (int) prev_x) * (int) bkb->keys_tick / KEYS_ANIMATION_LEN;
				y = (int) prev_y + ((int) y - (int) prev_y) * (int) bkb->keys_tick / KEYS_ANIMATION_LEN;
			}
			if (righti + i == layout_size) {
				// Draw backspace key
				bui_draw_bitmap(buffer, bui_bitmap_left_filled_bitmap, bui_bitmap_left_filled_w, 0, 0, x + 1, y,
						bui_bitmap_left_filled_w, bui_bitmap_left_filled_h);
			} else {
				// Draw normal key
				bui_bkb_draw_key(buffer, layout[righti + i], x, y);
			}
		}
	} else {
		// Draw backspace key
		bui_draw_bitmap(buffer, bui_bitmap_left_filled_bitmap, bui_bitmap_left_filled_w, 0, 0, 1, 0,
				bui_bitmap_left_filled_w, bui_bitmap_left_filled_h);
	}
}

void bui_bkb_set_type_buff(bui_bkb_bkb_t *bkb, char *type_buff, uint8_t type_buff_size, uint8_t type_buff_cap) {
	bkb->type_buff = type_buff;
	bkb->type_buff_size = type_buff_size;
	bkb->type_buff_cap = type_buff_cap;
}

uint8_t bui_bkb_get_type_buff_size(const bui_bkb_bkb_t *bkb) {
	return bkb->type_buff_size;
}
