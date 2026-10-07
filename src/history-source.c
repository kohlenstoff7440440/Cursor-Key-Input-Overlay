#include "history-source.h"
#include "input-service.h"

#include <obs-module.h>
#include <plugin-support.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define HISTORY_SOURCE_ID "presshud_history"

/* --------------------------------------------------------- */
/* Layout                                                    */
/* --------------------------------------------------------- */

#define HISTORY_KEY_HEIGHT 400.0f
#define HISTORY_KEY_HEIGHT_PX 400
#define HISTORY_LAYOUT_SCALE (HISTORY_KEY_HEIGHT / 48.0f)
#define HISTORY_DEFAULT_COMBO_GAP (4.0f * HISTORY_LAYOUT_SCALE)
#define HISTORY_DEFAULT_WIDTH HISTORY_KEY_HEIGHT

#define HISTORY_MAX_COMBO_BOXES 8
#define HISTORY_COMBO_LABEL_SIZE 16
#define HISTORY_MAX_SIMULTANEOUS_PRIMARY_KEYS 4

#define HISTORY_MAX_ITEMS 20
#define HISTORY_DEFAULT_COUNT 3
#define HISTORY_DEFAULT_SIZE_PERCENT 45.0
#define HISTORY_DEFAULT_HISTORY_GAP 18.0f
#define HISTORY_DEFAULT_LATEST_HISTORY_GAP 30.0f
#define HISTORY_DEFAULT_GAP_LEVEL 5
#define HISTORY_MAX_GAP_LEVEL 10

/* --------------------------------------------------------- */
/* Recognize-key editor layout                               */
/* --------------------------------------------------------- */

#define HISTORY_EDITOR_KEY_UNIT 48.0f
#define HISTORY_EDITOR_KEY_GAP 3.0f
#define HISTORY_EDITOR_KEY_STEP (HISTORY_EDITOR_KEY_UNIT + HISTORY_EDITOR_KEY_GAP)

struct history_recognize_key_visual {
	int vk;

	const char *label;

	float x;
	float y;

	float width;
	float height;
};

static const struct history_recognize_key_visual history_recognize_layout[] = {
	/* Function row: every adjacent key uses the normal KEY_GAP. */
	{VK_ESCAPE, "Esc", 0.0f, 0.0f, 1.0f, 1.0f},
	{VK_F1, "F1", 1.0f, 0.0f, 1.0f, 1.0f},
	{VK_F2, "F2", 2.0f, 0.0f, 1.0f, 1.0f},
	{VK_F3, "F3", 3.0f, 0.0f, 1.0f, 1.0f},
	{VK_F4, "F4", 4.0f, 0.0f, 1.0f, 1.0f},
	{VK_F5, "F5", 5.0f, 0.0f, 1.0f, 1.0f},
	{VK_F6, "F6", 6.0f, 0.0f, 1.0f, 1.0f},
	{VK_F7, "F7", 7.0f, 0.0f, 1.0f, 1.0f},
	{VK_F8, "F8", 8.0f, 0.0f, 1.0f, 1.0f},
	{VK_F9, "F9", 9.0f, 0.0f, 1.0f, 1.0f},
	{VK_F10, "F10", 10.0f, 0.0f, 1.0f, 1.0f},
	{VK_F11, "F11", 11.0f, 0.0f, 1.0f, 1.0f},
	{VK_F12, "F12", 12.0f, 0.0f, 1.0f, 1.0f},
	{VK_SNAPSHOT, "PrtSc", 13.0f, 0.0f, 1.0f, 1.0f},
	{VK_SCROLL, "ScrLk", 14.0f, 0.0f, 1.0f, 1.0f},
	{VK_PAUSE, "Pause", 15.0f, 0.0f, 1.0f, 1.0f},

	/* Number row */
	{VK_OEM_3, "`", 0.0f, 1.0f, 1.0f, 1.0f},
	{'1', "1", 1.0f, 1.0f, 1.0f, 1.0f},
	{'2', "2", 2.0f, 1.0f, 1.0f, 1.0f},
	{'3', "3", 3.0f, 1.0f, 1.0f, 1.0f},
	{'4', "4", 4.0f, 1.0f, 1.0f, 1.0f},
	{'5', "5", 5.0f, 1.0f, 1.0f, 1.0f},
	{'6', "6", 6.0f, 1.0f, 1.0f, 1.0f},
	{'7', "7", 7.0f, 1.0f, 1.0f, 1.0f},
	{'8', "8", 8.0f, 1.0f, 1.0f, 1.0f},
	{'9', "9", 9.0f, 1.0f, 1.0f, 1.0f},
	{'0', "0", 10.0f, 1.0f, 1.0f, 1.0f},
	{VK_OEM_MINUS, "-", 11.0f, 1.0f, 1.0f, 1.0f},
	{VK_OEM_PLUS, "=", 12.0f, 1.0f, 1.0f, 1.0f},
	{VK_BACK, "Back", 13.0f, 1.0f, 2.0f, 1.0f},

	/* Q row */
	{VK_TAB, "Tab", 0.0f, 2.0f, 1.5f, 1.0f},
	{'Q', "Q", 1.5f, 2.0f, 1.0f, 1.0f},
	{'W', "W", 2.5f, 2.0f, 1.0f, 1.0f},
	{'E', "E", 3.5f, 2.0f, 1.0f, 1.0f},
	{'R', "R", 4.5f, 2.0f, 1.0f, 1.0f},
	{'T', "T", 5.5f, 2.0f, 1.0f, 1.0f},
	{'Y', "Y", 6.5f, 2.0f, 1.0f, 1.0f},
	{'U', "U", 7.5f, 2.0f, 1.0f, 1.0f},
	{'I', "I", 8.5f, 2.0f, 1.0f, 1.0f},
	{'O', "O", 9.5f, 2.0f, 1.0f, 1.0f},
	{'P', "P", 10.5f, 2.0f, 1.0f, 1.0f},
	{VK_OEM_4, "[", 11.5f, 2.0f, 1.0f, 1.0f},
	{VK_OEM_6, "]", 12.5f, 2.0f, 1.0f, 1.0f},
	{VK_OEM_5, "\\", 13.5f, 2.0f, 1.5f, 1.0f},

	/* Home row */
	{VK_CAPITAL, "Caps", 0.0f, 3.0f, 1.75f, 1.0f},
	{'A', "A", 1.75f, 3.0f, 1.0f, 1.0f},
	{'S', "S", 2.75f, 3.0f, 1.0f, 1.0f},
	{'D', "D", 3.75f, 3.0f, 1.0f, 1.0f},
	{'F', "F", 4.75f, 3.0f, 1.0f, 1.0f},
	{'G', "G", 5.75f, 3.0f, 1.0f, 1.0f},
	{'H', "H", 6.75f, 3.0f, 1.0f, 1.0f},
	{'J', "J", 7.75f, 3.0f, 1.0f, 1.0f},
	{'K', "K", 8.75f, 3.0f, 1.0f, 1.0f},
	{'L', "L", 9.75f, 3.0f, 1.0f, 1.0f},
	{VK_OEM_1, ";", 10.75f, 3.0f, 1.0f, 1.0f},
	{VK_OEM_7, "'", 11.75f, 3.0f, 1.0f, 1.0f},
	{VK_RETURN, "Enter", 12.75f, 3.0f, 2.25f, 1.0f},

	/* Shift row */
	{VK_LSHIFT, "Shift", 0.0f, 4.0f, 2.25f, 1.0f},
	{'Z', "Z", 2.25f, 4.0f, 1.0f, 1.0f},
	{'X', "X", 3.25f, 4.0f, 1.0f, 1.0f},
	{'C', "C", 4.25f, 4.0f, 1.0f, 1.0f},
	{'V', "V", 5.25f, 4.0f, 1.0f, 1.0f},
	{'B', "B", 6.25f, 4.0f, 1.0f, 1.0f},
	{'N', "N", 7.25f, 4.0f, 1.0f, 1.0f},
	{'M', "M", 8.25f, 4.0f, 1.0f, 1.0f},
	{VK_OEM_COMMA, ",", 9.25f, 4.0f, 1.0f, 1.0f},
	{VK_OEM_PERIOD, ".", 10.25f, 4.0f, 1.0f, 1.0f},
	{VK_OEM_2, "/", 11.25f, 4.0f, 1.0f, 1.0f},
	{VK_RSHIFT, "Shift", 12.25f, 4.0f, 2.75f, 1.0f},

	/* Bottom row */
	{VK_LCONTROL, "Ctrl", 0.0f, 5.0f, 1.25f, 1.0f},
	{VK_LWIN, "Win", 1.25f, 5.0f, 1.25f, 1.0f},
	{VK_LMENU, "Alt", 2.5f, 5.0f, 1.25f, 1.0f},
	{VK_SPACE, "Space", 3.75f, 5.0f, 6.25f, 1.0f},
	{VK_RMENU, "Alt", 10.0f, 5.0f, 1.25f, 1.0f},
	{VK_RWIN, "Win", 11.25f, 5.0f, 1.25f, 1.0f},
	{VK_APPS, "Menu", 12.5f, 5.0f, 1.25f, 1.0f},
	{VK_RCONTROL, "Ctrl", 13.75f, 5.0f, 1.25f, 1.0f},

	/* Navigation: normal gap after the main block. */
	{VK_INSERT, "Ins", 15.0f, 1.0f, 1.0f, 1.0f},
	{VK_HOME, "Home", 16.0f, 1.0f, 1.0f, 1.0f},
	{VK_PRIOR, "PgUp", 17.0f, 1.0f, 1.0f, 1.0f},
	{VK_DELETE, "Del", 15.0f, 2.0f, 1.0f, 1.0f},
	{VK_END, "End", 16.0f, 2.0f, 1.0f, 1.0f},
	{VK_NEXT, "PgDn", 17.0f, 2.0f, 1.0f, 1.0f},
	{VK_UP, "Up", 16.0f, 4.0f, 1.0f, 1.0f},
	{VK_LEFT, "Left", 15.0f, 5.0f, 1.0f, 1.0f},
	{VK_DOWN, "Down", 16.0f, 5.0f, 1.0f, 1.0f},
	{VK_RIGHT, "Right", 17.0f, 5.0f, 1.0f, 1.0f},

	/* Numpad: normal gap after the navigation block. */
	{VK_NUMLOCK, "Num", 18.0f, 1.0f, 1.0f, 1.0f},
	{VK_DIVIDE, "/", 19.0f, 1.0f, 1.0f, 1.0f},
	{VK_MULTIPLY, "*", 20.0f, 1.0f, 1.0f, 1.0f},
	{VK_SUBTRACT, "-", 21.0f, 1.0f, 1.0f, 1.0f},
	{VK_NUMPAD7, "7", 18.0f, 2.0f, 1.0f, 1.0f},
	{VK_NUMPAD8, "8", 19.0f, 2.0f, 1.0f, 1.0f},
	{VK_NUMPAD9, "9", 20.0f, 2.0f, 1.0f, 1.0f},
	{VK_ADD, "+", 21.0f, 2.0f, 1.0f, 2.0f},
	{VK_NUMPAD4, "4", 18.0f, 3.0f, 1.0f, 1.0f},
	{VK_NUMPAD5, "5", 19.0f, 3.0f, 1.0f, 1.0f},
	{VK_NUMPAD6, "6", 20.0f, 3.0f, 1.0f, 1.0f},
	{VK_NUMPAD1, "1", 18.0f, 4.0f, 1.0f, 1.0f},
	{VK_NUMPAD2, "2", 19.0f, 4.0f, 1.0f, 1.0f},
	{VK_NUMPAD3, "3", 20.0f, 4.0f, 1.0f, 1.0f},
	{KEY_INPUT_NUMPAD_ENTER, "Enter", 21.0f, 4.0f, 1.0f, 2.0f},
	{VK_NUMPAD0, "0", 18.0f, 5.0f, 2.0f, 1.0f},
	{VK_DECIMAL, ".", 20.0f, 5.0f, 1.0f, 1.0f},
};

#define HISTORY_RECOGNIZE_KEY_COUNT \
	(sizeof(history_recognize_layout) / sizeof(history_recognize_layout[0]))


/*
 * Keep the first three numeric values compatible with the
 * previous version:
 *   0 = old Bottom  -> Bottom - Left
 *   1 = old Top     -> Top - Left
 *   2 = old Left    -> Left - Bottom
 */
#define HISTORY_POSITION_BOTTOM_LEFT 0
#define HISTORY_POSITION_TOP_LEFT 1
#define HISTORY_POSITION_LEFT_BOTTOM 2

#define HISTORY_POSITION_BOTTOM_RIGHT 3
#define HISTORY_POSITION_TOP_RIGHT 4
#define HISTORY_POSITION_LEFT_TOP 5
#define HISTORY_POSITION_RIGHT_TOP 6
#define HISTORY_POSITION_RIGHT_BOTTOM 7

/* --------------------------------------------------------- */
/* Timing                                                    */
/* --------------------------------------------------------- */

#define HISTORY_DEFAULT_LATEST_HOLD_MS 1000
#define HISTORY_DEFAULT_LATEST_FADE_MS 350
#define HISTORY_DEFAULT_LATEST_FLASH_MS 120
#define HISTORY_DEFAULT_DISPLAY_MS 650
#define HISTORY_DEFAULT_SIMULTANEOUS_INPUT_WINDOW_MS 35

#define HISTORY_EVENT_READ_COUNT 64

/* --------------------------------------------------------- */

struct history_record {
	bool valid;
	uint64_t created_ms;

	size_t combo_count;
	char combo_labels[HISTORY_MAX_COMBO_BOXES][HISTORY_COMBO_LABEL_SIZE];
	float combo_box_x[HISTORY_MAX_COMBO_BOXES];
	float combo_box_width[HISTORY_MAX_COMBO_BOXES];
	float row_width;

	gs_texture_t *text_texture;
	uint8_t *text_base_pixels;
	uint8_t *text_frame_pixels;
	uint32_t text_texture_width;
	size_t text_pixel_bytes;
};

struct history_source {
	obs_source_t *source;

	bool distinguish_left_right_modifiers;
	bool recognized_keys[HISTORY_RECOGNIZE_KEY_COUNT];

	int history_position;
	size_t history_limit;
	uint64_t history_display_ms;
	float history_scale;

	float simultaneous_key_gap;
	float latest_history_gap;
	float history_key_gap;

	float key_background_opacity;
	float key_flash_opacity;
	float key_text_opacity;
	float history_background_opacity;
	float history_text_opacity;

	wchar_t key_font_face[LF_FACESIZE];
	int key_font_size;
	int key_font_weight;
	bool key_font_italic;
	bool key_font_underline;
	bool key_font_strikeout;

	uint32_t key_background_color;
	uint32_t key_text_color;
	uint32_t key_flash_color;
	uint32_t key_border_color;

	uint32_t history_background_color;
	uint32_t history_text_color;
	uint32_t history_border_color;

	float key_border_thickness;

	uint64_t key_hold_ms;
	uint64_t key_fade_ms;
	uint64_t key_flash_ms;

	uint64_t simultaneous_input_window_ms;
	uint64_t simultaneous_input_start_ms;

	uint64_t last_sequence;
	uint64_t last_key_down_ms;
	uint64_t last_key_flash_ms;

	float key_visibility;
	bool latest_archived;

	/* Physical modifier state */
	bool left_ctrl;
	bool right_ctrl;
	bool left_shift;
	bool right_shift;
	bool left_alt;
	bool right_alt;
	bool space;

	/* Simultaneous primary keys */
	size_t simultaneous_primary_count;
	int simultaneous_primary_keys[HISTORY_MAX_SIMULTANEOUS_PRIMARY_KEYS];

	/* Current displayed combo */
	size_t combo_count;
	char combo_labels[HISTORY_MAX_COMBO_BOXES][HISTORY_COMBO_LABEL_SIZE];
	float combo_box_x[HISTORY_MAX_COMBO_BOXES];
	float combo_box_width[HISTORY_MAX_COMBO_BOXES];
	float key_row_width;

	/* Latest text texture */
	gs_texture_t *text_texture;
	uint8_t *text_base_pixels;
	uint8_t *text_frame_pixels;
	uint32_t text_texture_width;
	size_t text_pixel_bytes;

	/* Previous inputs */
	struct history_record records[HISTORY_MAX_ITEMS];
	size_t history_size;
};

/* --------------------------------------------------------- */
/* General helpers                                           */
/* --------------------------------------------------------- */

static float history_clamp_float(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;

	if (value > maximum)
		return maximum;

	return value;
}

static uint32_t history_obs_rgba_to_argb(uint32_t color)
{
	uint32_t alpha = color & 0xFF000000;
	uint32_t red = (color & 0x000000FF) << 16;
	uint32_t green = color & 0x0000FF00;
	uint32_t blue = (color & 0x00FF0000) >> 16;

	return alpha | red | green | blue;
}

static uint32_t history_color_with_opacity(uint32_t color, float opacity)
{
	opacity = history_clamp_float(opacity, 0.0f, 1.0f);

	uint32_t original_alpha = (color >> 24) & 0xFF;
	uint32_t alpha = (uint32_t)((float)original_alpha * opacity);

	return (color & 0x00FFFFFF) | (alpha << 24);
}

static uint32_t history_blend_color(uint32_t from, uint32_t to, float progress)
{
	progress = history_clamp_float(progress, 0.0f, 1.0f);

	uint32_t from_a = (from >> 24) & 0xFF;
	uint32_t from_r = (from >> 16) & 0xFF;
	uint32_t from_g = (from >> 8) & 0xFF;
	uint32_t from_b = from & 0xFF;

	uint32_t to_a = (to >> 24) & 0xFF;
	uint32_t to_r = (to >> 16) & 0xFF;
	uint32_t to_g = (to >> 8) & 0xFF;
	uint32_t to_b = to & 0xFF;

	uint32_t a = (uint32_t)((float)from_a + ((float)to_a - (float)from_a) * progress);
	uint32_t r = (uint32_t)((float)from_r + ((float)to_r - (float)from_r) * progress);
	uint32_t g = (uint32_t)((float)from_g + ((float)to_g - (float)from_g) * progress);
	uint32_t b = (uint32_t)((float)from_b + ((float)to_b - (float)from_b) * progress);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

/* --------------------------------------------------------- */
/* Recognition helpers                                      */
/* --------------------------------------------------------- */

static void history_recognize_setting_name(size_t index, char *buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0 || index >= HISTORY_RECOGNIZE_KEY_COUNT)
		return;

	snprintf(buffer, buffer_size, "recognize_key_%04X",
		 (unsigned int)history_recognize_layout[index].vk);
}

static void history_load_recognized_keys(struct history_source *context, obs_data_t *settings)
{
	if (!context || !settings)
		return;

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];

		history_recognize_setting_name(i, setting_name, sizeof(setting_name));
		context->recognized_keys[i] = obs_data_get_bool(settings, setting_name);
	}
}

static int history_find_recognize_key_index(int key_code)
{
	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		if (history_recognize_layout[i].vk == key_code)
			return (int)i;
	}

	return -1;
}

static bool history_key_is_recognized(const struct history_source *context, int key_code)
{
	if (!context)
		return false;

	int index = history_find_recognize_key_index(key_code);

	if (index < 0)
		return false;

	return context->recognized_keys[index];
}

static float history_editor_key_width(float units)
{
	return (units * HISTORY_EDITOR_KEY_UNIT) +
	       ((units - 1.0f) * HISTORY_EDITOR_KEY_GAP);
}

static float history_editor_key_height(float units)
{
	return (units * HISTORY_EDITOR_KEY_UNIT) +
	       ((units - 1.0f) * HISTORY_EDITOR_KEY_GAP);
}

static float history_editor_key_x(const struct history_recognize_key_visual *key)
{
	if (!key)
		return 0.0f;

	return key->x * HISTORY_EDITOR_KEY_STEP;
}

static float history_editor_key_y(const struct history_recognize_key_visual *key)
{
	if (!key)
		return 0.0f;

	return key->y * HISTORY_EDITOR_KEY_STEP;
}

/* --------------------------------------------------------- */
/* Key names                                                 */
/* --------------------------------------------------------- */

static void history_get_key_label(int key_code, char *buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0)
		return;

	buffer[0] = '\0';

	if (key_code >= 'A' && key_code <= 'Z') {
		snprintf(buffer, buffer_size, "%c", (char)key_code);
		return;
	}

	if (key_code >= '0' && key_code <= '9') {
		snprintf(buffer, buffer_size, "%c", (char)key_code);
		return;
	}

	if (key_code >= VK_F1 && key_code <= VK_F12) {
		snprintf(buffer, buffer_size, "F%d", key_code - VK_F1 + 1);
		return;
	}

	switch (key_code) {
	case VK_ESCAPE:
		snprintf(buffer, buffer_size, "Esc");
		break;
	case VK_SNAPSHOT:
		snprintf(buffer, buffer_size, "PrtSc");
		break;
	case VK_SCROLL:
		snprintf(buffer, buffer_size, "ScrLk");
		break;
	case VK_PAUSE:
		snprintf(buffer, buffer_size, "Pause");
		break;
	case VK_OEM_3:
		snprintf(buffer, buffer_size, "`");
		break;
	case VK_OEM_MINUS:
		snprintf(buffer, buffer_size, "-");
		break;
	case VK_OEM_PLUS:
		snprintf(buffer, buffer_size, "=");
		break;
	case VK_BACK:
		snprintf(buffer, buffer_size, "Back");
		break;
	case VK_TAB:
		snprintf(buffer, buffer_size, "Tab");
		break;
	case VK_OEM_4:
		snprintf(buffer, buffer_size, "[");
		break;
	case VK_OEM_6:
		snprintf(buffer, buffer_size, "]");
		break;
	case VK_OEM_5:
		snprintf(buffer, buffer_size, "\\");
		break;
	case VK_CAPITAL:
		snprintf(buffer, buffer_size, "Caps");
		break;
	case VK_OEM_1:
		snprintf(buffer, buffer_size, ";");
		break;
	case VK_OEM_7:
		snprintf(buffer, buffer_size, "'");
		break;
	case VK_RETURN:
		snprintf(buffer, buffer_size, "Enter");
		break;
	case VK_OEM_COMMA:
		snprintf(buffer, buffer_size, ",");
		break;
	case VK_OEM_PERIOD:
		snprintf(buffer, buffer_size, ".");
		break;
	case VK_OEM_2:
		snprintf(buffer, buffer_size, "/");
		break;
	case VK_LWIN:
	case VK_RWIN:
		snprintf(buffer, buffer_size, "Win");
		break;
	case VK_APPS:
		snprintf(buffer, buffer_size, "Menu");
		break;
	case VK_INSERT:
		snprintf(buffer, buffer_size, "Ins");
		break;
	case VK_HOME:
		snprintf(buffer, buffer_size, "Home");
		break;
	case VK_PRIOR:
		snprintf(buffer, buffer_size, "PgUp");
		break;
	case VK_DELETE:
		snprintf(buffer, buffer_size, "Del");
		break;
	case VK_END:
		snprintf(buffer, buffer_size, "End");
		break;
	case VK_NEXT:
		snprintf(buffer, buffer_size, "PgDn");
		break;
	case VK_UP:
		snprintf(buffer, buffer_size, "\xE2\x86\x91");
		break;
	case VK_LEFT:
		snprintf(buffer, buffer_size, "\xE2\x86\x90");
		break;
	case VK_DOWN:
		snprintf(buffer, buffer_size, "\xE2\x86\x93");
		break;
	case VK_RIGHT:
		snprintf(buffer, buffer_size, "\xE2\x86\x92");
		break;
	case VK_NUMLOCK:
		snprintf(buffer, buffer_size, "Num");
		break;
	case VK_DIVIDE:
		snprintf(buffer, buffer_size, "/");
		break;
	case VK_MULTIPLY:
		snprintf(buffer, buffer_size, "*");
		break;
	case VK_SUBTRACT:
		snprintf(buffer, buffer_size, "-");
		break;
	case VK_ADD:
		snprintf(buffer, buffer_size, "+");
		break;
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
		snprintf(buffer, buffer_size, "%d", key_code - VK_NUMPAD0);
		break;
	case VK_DECIMAL:
		snprintf(buffer, buffer_size, ".");
		break;
	case KEY_INPUT_NUMPAD_ENTER:
		snprintf(buffer, buffer_size, "Enter");
		break;
	default:
		snprintf(buffer, buffer_size, "Key");
		break;
	}
}

/* --------------------------------------------------------- */
/* Modifier handling                                         */
/* --------------------------------------------------------- */

static bool history_key_is_modifier(int key_code)
{
	switch (key_code) {
	case VK_LCONTROL:
	case VK_RCONTROL:
	case VK_LSHIFT:
	case VK_RSHIFT:
	case VK_LMENU:
	case VK_RMENU:
	case VK_SPACE:
		return true;
	default:
		return false;
	}
}

static bool history_modifier_is_enabled(const struct history_source *context, int key_code)
{
	return history_key_is_recognized(context, key_code);
}

static bool history_primary_is_enabled(const struct history_source *context, int key_code)
{
	return history_key_is_recognized(context, key_code);
}

static void history_apply_modifier_event(struct history_source *context, int key_code, bool pressed)
{
	if (!context)
		return;

	switch (key_code) {
	case VK_LCONTROL:
		context->left_ctrl = pressed;
		break;
	case VK_RCONTROL:
		context->right_ctrl = pressed;
		break;
	case VK_LSHIFT:
		context->left_shift = pressed;
		break;
	case VK_RSHIFT:
		context->right_shift = pressed;
		break;
	case VK_LMENU:
		context->left_alt = pressed;
		break;
	case VK_RMENU:
		context->right_alt = pressed;
		break;
	case VK_SPACE:
		context->space = pressed;
		break;
	default:
		break;
	}
}


/* --------------------------------------------------------- */
/* Combo layout                                              */
/* --------------------------------------------------------- */

static float history_get_label_width(const char *label)
{
	if (!label)
		return HISTORY_DEFAULT_WIDTH;

	float cursor_width = 48.0f;

	if (strcmp(label, "Ctrl") == 0)
		cursor_width = 58.0f;
	else if (strcmp(label, "Shift") == 0)
		cursor_width = 64.0f;
	else if (strcmp(label, "Alt") == 0)
		cursor_width = 52.0f;
	else if (strcmp(label, "Space") == 0)
		cursor_width = 68.0f;
	else {
		size_t length = strlen(label);

		if (length <= 1)
			cursor_width = 48.0f;
		else if (length <= 3)
			cursor_width = 54.0f;
		else if (length <= 5)
			cursor_width = 64.0f;
		else if (length <= 7)
			cursor_width = 76.0f;
		else
			cursor_width = 88.0f;
	}

	return cursor_width * HISTORY_LAYOUT_SCALE;
}

static void history_clear_combo(struct history_source *context)
{
	if (!context)
		return;

	context->combo_count = 0;
	context->key_row_width = 0.0f;

	memset(context->combo_labels, 0, sizeof(context->combo_labels));
	memset(context->combo_box_x, 0, sizeof(context->combo_box_x));
	memset(context->combo_box_width, 0, sizeof(context->combo_box_width));
}

static void history_add_combo_box(struct history_source *context, const char *label)
{
	if (!context || !label)
		return;

	if (context->combo_count >= HISTORY_MAX_COMBO_BOXES)
		return;

	size_t index = context->combo_count;
	float width = history_get_label_width(label);
	float x = 0.0f;

	if (index > 0)
		x = context->key_row_width + context->simultaneous_key_gap;

	context->combo_box_x[index] = x;
	context->combo_box_width[index] = width;

	snprintf(context->combo_labels[index], HISTORY_COMBO_LABEL_SIZE, "%s", label);

	context->combo_count++;
	context->key_row_width = x + width;
}

/* --------------------------------------------------------- */
/* Font / raster helpers                                     */
/* --------------------------------------------------------- */

#define HISTORY_DEFAULT_FONT_SIZE 17
#define HISTORY_FONT_RASTER_SCALE (145.0f / 17.0f)
#define HISTORY_SMALL_FONT_RATIO (118.0f / 145.0f)

static bool history_ascii_contains_ignore_case(const char *text, const char *needle)
{
	if (!text || !needle || !*needle)
		return false;

	size_t needle_length = strlen(needle);

	for (const char *start = text; *start; start++) {
		size_t i = 0;

		while (i < needle_length && start[i] &&
		       tolower((unsigned char)start[i]) ==
			       tolower((unsigned char)needle[i])) {
			i++;
		}

		if (i == needle_length)
			return true;
	}

	return false;
}

static int history_font_weight_from_style(const char *style, uint32_t flags)
{
	int weight = FW_NORMAL;

	if (style && *style) {
		if (history_ascii_contains_ignore_case(style, "black") ||
		    history_ascii_contains_ignore_case(style, "heavy")) {
			weight = FW_BLACK;
		} else if (history_ascii_contains_ignore_case(style, "extra bold") ||
			   history_ascii_contains_ignore_case(style, "extrabold") ||
			   history_ascii_contains_ignore_case(style, "ultra bold") ||
			   history_ascii_contains_ignore_case(style, "ultrabold")) {
			weight = FW_EXTRABOLD;
		} else if (history_ascii_contains_ignore_case(style, "semi bold") ||
			   history_ascii_contains_ignore_case(style, "semibold") ||
			   history_ascii_contains_ignore_case(style, "demi bold") ||
			   history_ascii_contains_ignore_case(style, "demibold")) {
			weight = FW_SEMIBOLD;
		} else if (history_ascii_contains_ignore_case(style, "bold")) {
			weight = FW_BOLD;
		} else if (history_ascii_contains_ignore_case(style, "medium")) {
			weight = FW_MEDIUM;
		} else if (history_ascii_contains_ignore_case(style, "extra light") ||
			   history_ascii_contains_ignore_case(style, "extralight") ||
			   history_ascii_contains_ignore_case(style, "ultra light") ||
			   history_ascii_contains_ignore_case(style, "ultralight")) {
			weight = FW_EXTRALIGHT;
		} else if (history_ascii_contains_ignore_case(style, "light")) {
			weight = FW_LIGHT;
		} else if (history_ascii_contains_ignore_case(style, "thin")) {
			weight = FW_THIN;
		}
	}

	if ((flags & OBS_FONT_BOLD) != 0 && weight < FW_BOLD)
		weight = FW_BOLD;

	return weight;
}

static void history_load_key_font(struct history_source *context, obs_data_t *settings)
{
	if (!context || !settings)
		return;

	wcsncpy(context->key_font_face, L"Segoe UI", LF_FACESIZE - 1);
	context->key_font_face[LF_FACESIZE - 1] = L'\0';
	context->key_font_size = HISTORY_DEFAULT_FONT_SIZE;
	context->key_font_weight = FW_SEMIBOLD;
	context->key_font_italic = false;
	context->key_font_underline = false;
	context->key_font_strikeout = false;

	obs_data_t *font = obs_data_get_obj(settings, "key_font");

	if (!font)
		return;

	const char *face = obs_data_get_string(font, "face");
	const char *style = obs_data_get_string(font, "style");
	int64_t size = obs_data_get_int(font, "size");
	uint32_t flags = (uint32_t)obs_data_get_int(font, "flags");

	if (size > 0) {
		if (size > 200)
			size = 200;
		context->key_font_size = (int)size;
	}

	if (face && *face) {
		wchar_t wide_face[LF_FACESIZE];
		int result = MultiByteToWideChar(CP_UTF8, 0, face, -1,
					 wide_face, LF_FACESIZE);

		if (result > 0) {
			wide_face[LF_FACESIZE - 1] = L'\0';
			wcsncpy(context->key_font_face, wide_face, LF_FACESIZE - 1);
			context->key_font_face[LF_FACESIZE - 1] = L'\0';
		}
	}

	context->key_font_weight = history_font_weight_from_style(style, flags);
	context->key_font_italic =
		(flags & OBS_FONT_ITALIC) != 0 ||
		history_ascii_contains_ignore_case(style, "italic") ||
		history_ascii_contains_ignore_case(style, "oblique");
	context->key_font_underline = (flags & OBS_FONT_UNDERLINE) != 0;
	context->key_font_strikeout = (flags & OBS_FONT_STRIKEOUT) != 0;

	obs_data_release(font);
}

static HFONT history_create_font(const struct history_source *context, int size)
{
	if (size < 1)
		size = 1;

	const wchar_t *face = L"Segoe UI";
	int weight = FW_SEMIBOLD;
	BOOL italic = FALSE;
	BOOL underline = FALSE;
	BOOL strikeout = FALSE;

	if (context) {
		if (context->key_font_face[0] != L'\0')
			face = context->key_font_face;
		weight = context->key_font_weight;
		italic = context->key_font_italic ? TRUE : FALSE;
		underline = context->key_font_underline ? TRUE : FALSE;
		strikeout = context->key_font_strikeout ? TRUE : FALSE;
	}

	return CreateFontW(-size, 0, 0, 0, weight, italic, underline, strikeout,
			   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			   ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void history_draw_labels_to_dib(
	const struct history_source *context,
	HDC hdc,
	size_t combo_count,
	char labels[HISTORY_MAX_COMBO_BOXES][HISTORY_COMBO_LABEL_SIZE],
	float box_x[HISTORY_MAX_COMBO_BOXES],
	float box_width[HISTORY_MAX_COMBO_BOXES])
{
	if (!context || !hdc)
		return;

	int selected_size = context->key_font_size;
	if (selected_size < 1)
		selected_size = HISTORY_DEFAULT_FONT_SIZE;

	/*
	 * Font Size 17 reproduces the previous History default (145 px at
	 * this 400 px raster height).  Keep the previous long-label reduction
	 * ratio so existing History layouts retain their familiar appearance.
	 */
	int normal_size =
		(int)((float)selected_size * HISTORY_FONT_RASTER_SCALE + 0.5f);
	int small_size =
		(int)((float)normal_size * HISTORY_SMALL_FONT_RATIO + 0.5f);

	if (normal_size < 1)
		normal_size = 1;
	if (small_size < 1)
		small_size = 1;

	HFONT normal_font = history_create_font(context, normal_size);
	HFONT small_font = history_create_font(context, small_size);
	HGDIOBJ old_font = GetCurrentObject(hdc, OBJ_FONT);

	for (size_t i = 0; i < combo_count; i++) {
		const char *label = labels[i];

		RECT rect;
		rect.left = (LONG)box_x[i] + 12;
		rect.top = 12;
		rect.right = (LONG)(box_x[i] + box_width[i]) - 12;
		rect.bottom = HISTORY_KEY_HEIGHT_PX - 12;

		HFONT selected_font = strlen(label) >= 5 ? small_font : normal_font;
		SelectObject(hdc, selected_font);

		wchar_t wide_label[64];
		int wide_length = MultiByteToWideChar(
			CP_UTF8, 0, label, -1, wide_label,
			(int)(sizeof(wide_label) / sizeof(wide_label[0])));

		if (wide_length > 0) {
			DrawTextW(hdc, wide_label, -1, &rect,
				  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
	}

	SelectObject(hdc, old_font);

	if (normal_font)
		DeleteObject(normal_font);
	if (small_font)
		DeleteObject(small_font);
}

/* --------------------------------------------------------- */
/* Latest text texture                                       */
/* --------------------------------------------------------- */

static void history_destroy_latest_text_texture(struct history_source *context)
{
	if (!context)
		return;

	if (context->text_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->text_texture);
		obs_leave_graphics();

		context->text_texture = NULL;
	}

	if (context->text_base_pixels) {
		bfree(context->text_base_pixels);
		context->text_base_pixels = NULL;
	}

	if (context->text_frame_pixels) {
		bfree(context->text_frame_pixels);
		context->text_frame_pixels = NULL;
	}

	context->text_texture_width = 0;
	context->text_pixel_bytes = 0;
}

static void history_rebuild_latest_text_texture(struct history_source *context)
{
	if (!context)
		return;

	history_destroy_latest_text_texture(context);

	if (context->combo_count == 0 || context->key_row_width <= 0.0f)
		return;

	uint32_t texture_width = (uint32_t)(context->key_row_width + 0.999f);

	if (texture_width < 1)
		texture_width = 1;

	size_t pixel_bytes =
		(size_t)texture_width * (size_t)HISTORY_KEY_HEIGHT_PX * 4;

	uint8_t *base_pixels = bzalloc(pixel_bytes);
	uint8_t *frame_pixels = bzalloc(pixel_bytes);

	if (!base_pixels || !frame_pixels) {
		if (base_pixels)
			bfree(base_pixels);

		if (frame_pixels)
			bfree(frame_pixels);

		return;
	}

	HDC hdc = CreateCompatibleDC(NULL);

	if (!hdc) {
		bfree(base_pixels);
		bfree(frame_pixels);
		return;
	}

	BITMAPINFO bitmap_info;
	memset(&bitmap_info, 0, sizeof(bitmap_info));

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = (LONG)texture_width;
	bitmap_info.bmiHeader.biHeight = -HISTORY_KEY_HEIGHT_PX;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	void *bitmap_bits = NULL;

	HBITMAP bitmap =
		CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, &bitmap_bits, NULL, 0);

	if (!bitmap || !bitmap_bits) {
		if (bitmap)
			DeleteObject(bitmap);

		DeleteDC(hdc);
		bfree(base_pixels);
		bfree(frame_pixels);
		return;
	}

	HGDIOBJ old_bitmap = SelectObject(hdc, bitmap);

	memset(bitmap_bits, 0, pixel_bytes);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));

	history_draw_labels_to_dib(
		context,
		hdc,
		context->combo_count,
		context->combo_labels,
		context->combo_box_x,
		context->combo_box_width);

	GdiFlush();

	uint8_t *dib_pixels = (uint8_t *)bitmap_bits;
	size_t pixel_count = (size_t)texture_width * (size_t)HISTORY_KEY_HEIGHT_PX;

	for (size_t i = 0; i < pixel_count; i++) {
		uint8_t *pixel = dib_pixels + (i * 4);

		uint8_t blue = pixel[0];
		uint8_t green = pixel[1];
		uint8_t red = pixel[2];

		uint8_t alpha = red;

		if (green > alpha)
			alpha = green;

		if (blue > alpha)
			alpha = blue;

		base_pixels[(i * 4) + 0] = 255;
		base_pixels[(i * 4) + 1] = 255;
		base_pixels[(i * 4) + 2] = 255;
		base_pixels[(i * 4) + 3] = alpha;
	}

	SelectObject(hdc, old_bitmap);
	DeleteObject(bitmap);
	DeleteDC(hdc);

	context->text_base_pixels = base_pixels;
	context->text_frame_pixels = frame_pixels;
	context->text_texture_width = texture_width;
	context->text_pixel_bytes = pixel_bytes;

	const uint8_t *initial_data = frame_pixels;

	obs_enter_graphics();

	context->text_texture =
		gs_texture_create(
			texture_width,
			HISTORY_KEY_HEIGHT_PX,
			GS_BGRA,
			1,
			&initial_data,
			GS_DYNAMIC);

	obs_leave_graphics();
}

static void history_update_latest_text_texture(struct history_source *context, float opacity)
{
	if (!context ||
	    !context->text_texture ||
	    !context->text_base_pixels ||
	    !context->text_frame_pixels) {
		return;
	}

	opacity = history_clamp_float(opacity, 0.0f, 1.0f);

	uint8_t text_alpha = (uint8_t)((context->key_text_color >> 24) & 0xFF);
	uint8_t text_red = (uint8_t)((context->key_text_color >> 16) & 0xFF);
	uint8_t text_green = (uint8_t)((context->key_text_color >> 8) & 0xFF);
	uint8_t text_blue = (uint8_t)(context->key_text_color & 0xFF);

	size_t pixel_count =
		(size_t)context->text_texture_width * (size_t)HISTORY_KEY_HEIGHT_PX;

	for (size_t i = 0; i < pixel_count; i++) {
		size_t offset = i * 4;

		context->text_frame_pixels[offset + 0] = text_blue;
		context->text_frame_pixels[offset + 1] = text_green;
		context->text_frame_pixels[offset + 2] = text_red;

		uint8_t base_alpha = context->text_base_pixels[offset + 3];

		float final_alpha =
			(float)base_alpha * ((float)text_alpha / 255.0f) * opacity;

		context->text_frame_pixels[offset + 3] = (uint8_t)final_alpha;
	}

	gs_texture_set_image(
		context->text_texture,
		context->text_frame_pixels,
		context->text_texture_width * 4,
		false);
}

/* --------------------------------------------------------- */
/* History record textures                                   */
/* --------------------------------------------------------- */

static void history_destroy_record(struct history_record *record)
{
	if (!record)
		return;

	if (record->text_texture) {
		obs_enter_graphics();
		gs_texture_destroy(record->text_texture);
		obs_leave_graphics();

		record->text_texture = NULL;
	}

	if (record->text_base_pixels) {
		bfree(record->text_base_pixels);
		record->text_base_pixels = NULL;
	}

	if (record->text_frame_pixels) {
		bfree(record->text_frame_pixels);
		record->text_frame_pixels = NULL;
	}

	memset(record, 0, sizeof(*record));
}

static void history_create_record_texture(
	struct history_source *context,
	struct history_record *record)
{
	if (!context || !record || !record->valid)
		return;

	if (record->text_texture) {
		obs_enter_graphics();
		gs_texture_destroy(record->text_texture);
		obs_leave_graphics();

		record->text_texture = NULL;
	}

	if (record->text_base_pixels) {
		bfree(record->text_base_pixels);
		record->text_base_pixels = NULL;
	}

	if (record->text_frame_pixels) {
		bfree(record->text_frame_pixels);
		record->text_frame_pixels = NULL;
	}

	record->text_texture_width = 0;
	record->text_pixel_bytes = 0;

	if (record->combo_count == 0 || record->row_width <= 0.0f)
		return;

	uint32_t texture_width = (uint32_t)(record->row_width + 0.999f);

	if (texture_width < 1)
		texture_width = 1;

	size_t pixel_bytes =
		(size_t)texture_width * (size_t)HISTORY_KEY_HEIGHT_PX * 4;

	uint8_t *base_pixels = bzalloc(pixel_bytes);
	uint8_t *frame_pixels = bzalloc(pixel_bytes);

	if (!base_pixels || !frame_pixels) {
		if (base_pixels)
			bfree(base_pixels);

		if (frame_pixels)
			bfree(frame_pixels);

		return;
	}

	HDC hdc = CreateCompatibleDC(NULL);

	if (!hdc) {
		bfree(base_pixels);
		bfree(frame_pixels);
		return;
	}

	BITMAPINFO bitmap_info;
	memset(&bitmap_info, 0, sizeof(bitmap_info));

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = (LONG)texture_width;
	bitmap_info.bmiHeader.biHeight = -HISTORY_KEY_HEIGHT_PX;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	void *bitmap_bits = NULL;

	HBITMAP bitmap =
		CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, &bitmap_bits, NULL, 0);

	if (!bitmap || !bitmap_bits) {
		if (bitmap)
			DeleteObject(bitmap);

		DeleteDC(hdc);
		bfree(base_pixels);
		bfree(frame_pixels);
		return;
	}

	HGDIOBJ old_bitmap = SelectObject(hdc, bitmap);

	memset(bitmap_bits, 0, pixel_bytes);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));

	history_draw_labels_to_dib(
		context,
		hdc,
		record->combo_count,
		record->combo_labels,
		record->combo_box_x,
		record->combo_box_width);

	GdiFlush();

	uint8_t text_alpha = (uint8_t)((context->history_text_color >> 24) & 0xFF);
	uint8_t text_red = (uint8_t)((context->history_text_color >> 16) & 0xFF);
	uint8_t text_green = (uint8_t)((context->history_text_color >> 8) & 0xFF);
	uint8_t text_blue = (uint8_t)(context->history_text_color & 0xFF);

	uint8_t *dib_pixels = (uint8_t *)bitmap_bits;
	size_t pixel_count = (size_t)texture_width * (size_t)HISTORY_KEY_HEIGHT_PX;

	for (size_t i = 0; i < pixel_count; i++) {
		uint8_t *source_pixel = dib_pixels + (i * 4);

		uint8_t blue = source_pixel[0];
		uint8_t green = source_pixel[1];
		uint8_t red = source_pixel[2];

		uint8_t alpha = red;

		if (green > alpha)
			alpha = green;

		if (blue > alpha)
			alpha = blue;

		base_pixels[(i * 4) + 0] = text_blue;
		base_pixels[(i * 4) + 1] = text_green;
		base_pixels[(i * 4) + 2] = text_red;
		base_pixels[(i * 4) + 3] =
			(uint8_t)((float)alpha * ((float)text_alpha / 255.0f));

		frame_pixels[(i * 4) + 0] = base_pixels[(i * 4) + 0];
		frame_pixels[(i * 4) + 1] = base_pixels[(i * 4) + 1];
		frame_pixels[(i * 4) + 2] = base_pixels[(i * 4) + 2];
		frame_pixels[(i * 4) + 3] = base_pixels[(i * 4) + 3];
	}

	SelectObject(hdc, old_bitmap);
	DeleteObject(bitmap);
	DeleteDC(hdc);

	record->text_base_pixels = base_pixels;
	record->text_frame_pixels = frame_pixels;
	record->text_texture_width = texture_width;
	record->text_pixel_bytes = pixel_bytes;

	const uint8_t *texture_data = frame_pixels;

	obs_enter_graphics();

	record->text_texture =
		gs_texture_create(
			texture_width,
			HISTORY_KEY_HEIGHT_PX,
			GS_BGRA,
			1,
			&texture_data,
			GS_DYNAMIC);

	obs_leave_graphics();
}

static void history_update_record_text_texture(
	struct history_record *record,
	float opacity)
{
	if (!record ||
	    !record->text_texture ||
	    !record->text_base_pixels ||
	    !record->text_frame_pixels) {
		return;
	}

	opacity = history_clamp_float(opacity, 0.0f, 1.0f);

	size_t pixel_count =
		(size_t)record->text_texture_width * (size_t)HISTORY_KEY_HEIGHT_PX;

	for (size_t i = 0; i < pixel_count; i++) {
		size_t offset = i * 4;

		record->text_frame_pixels[offset + 0] =
			record->text_base_pixels[offset + 0];

		record->text_frame_pixels[offset + 1] =
			record->text_base_pixels[offset + 1];

		record->text_frame_pixels[offset + 2] =
			record->text_base_pixels[offset + 2];

		record->text_frame_pixels[offset + 3] =
			(uint8_t)((float)record->text_base_pixels[offset + 3] * opacity);
	}

	gs_texture_set_image(
		record->text_texture,
		record->text_frame_pixels,
		record->text_texture_width * 4,
		false);
}

static void history_relayout_record(
	const struct history_source *context,
	struct history_record *record)
{
	if (!context || !record || !record->valid)
		return;

	float row_width = 0.0f;

	for (size_t i = 0; i < record->combo_count; i++) {
		float width = history_get_label_width(record->combo_labels[i]);
		float x = i > 0 ? row_width + context->simultaneous_key_gap : 0.0f;

		record->combo_box_x[i] = x;
		record->combo_box_width[i] = width;
		row_width = x + width;
	}

	record->row_width = row_width;
}

static void history_rebuild_all_record_textures(struct history_source *context)
{
	if (!context)
		return;

	for (size_t i = 0; i < context->history_size; i++) {
		history_relayout_record(context, &context->records[i]);
		history_create_record_texture(context, &context->records[i]);
	}
}

/* --------------------------------------------------------- */
/* History queue                                             */
/* --------------------------------------------------------- */

static void history_clear_records(struct history_source *context)
{
	if (!context)
		return;

	for (size_t i = 0; i < context->history_size; i++)
		history_destroy_record(&context->records[i]);

	context->history_size = 0;
}

static void history_trim_records_to_limit(struct history_source *context)
{
	if (!context)
		return;

	while (context->history_size > context->history_limit) {
		size_t last = context->history_size - 1;

		history_destroy_record(&context->records[last]);
		context->history_size--;
	}

	if (context->history_limit == 0)
		history_clear_records(context);
}

static void history_remove_record_at(struct history_source *context, size_t index)
{
	if (!context || index >= context->history_size)
		return;

	history_destroy_record(&context->records[index]);

	for (size_t i = index + 1; i < context->history_size; i++)
		context->records[i - 1] = context->records[i];

	if (context->history_size > 0) {
		memset(
			&context->records[context->history_size - 1],
			0,
			sizeof(context->records[0]));

		context->history_size--;
	}
}

static float history_get_record_opacity(
	const struct history_source *context,
	const struct history_record *record,
	uint64_t now)
{
	if (!context || !record || !record->valid)
		return 0.0f;

	if (now < record->created_ms)
		return 1.0f;

	uint64_t elapsed = now - record->created_ms;

	if (elapsed <= context->history_display_ms)
		return 1.0f;

	uint64_t fade_elapsed = elapsed - context->history_display_ms;

	if (context->key_fade_ms == 0 ||
	    fade_elapsed >= context->key_fade_ms) {
		return 0.0f;
	}

	return history_clamp_float(
		1.0f -
			((float)fade_elapsed /
			 (float)context->key_fade_ms),
		0.0f,
		1.0f);
}

static void history_remove_expired_records(struct history_source *context, uint64_t now)
{
	if (!context)
		return;

	size_t index = 0;

	while (index < context->history_size) {
		struct history_record *record = &context->records[index];

		if (history_get_record_opacity(context, record, now) <= 0.0f) {
			history_remove_record_at(context, index);
			continue;
		}

		index++;
	}
}

static void history_push_current_combo(struct history_source *context, uint64_t created_ms)
{
	if (!context)
		return;

	if (context->latest_archived)
		return;

	if (context->combo_count == 0 || context->key_row_width <= 0.0f) {
		context->latest_archived = true;
		return;
	}

	context->latest_archived = true;

	if (context->history_limit == 0)
		return;

	if (context->history_size >= context->history_limit) {
		size_t last = context->history_size - 1;

		history_destroy_record(&context->records[last]);
		context->history_size--;
	}

	for (size_t i = context->history_size; i > 0; i--)
		context->records[i] = context->records[i - 1];

	memset(&context->records[0], 0, sizeof(context->records[0]));

	struct history_record *record = &context->records[0];

	record->valid = true;
	record->created_ms = created_ms;
	record->combo_count = context->combo_count;
	record->row_width = context->key_row_width;

	memcpy(record->combo_labels, context->combo_labels, sizeof(record->combo_labels));
	memcpy(record->combo_box_x, context->combo_box_x, sizeof(record->combo_box_x));
	memcpy(record->combo_box_width, context->combo_box_width, sizeof(record->combo_box_width));

	context->history_size++;

	history_create_record_texture(context, record);
}

/* --------------------------------------------------------- */
/* Simultaneous input                                        */
/* --------------------------------------------------------- */

static void history_clear_simultaneous_keys(struct history_source *context)
{
	if (!context)
		return;

	context->simultaneous_input_start_ms = 0;
	context->simultaneous_primary_count = 0;

	memset(
		context->simultaneous_primary_keys,
		0,
		sizeof(context->simultaneous_primary_keys));
}

static bool history_simultaneous_contains(
	const struct history_source *context,
	int key_code)
{
	if (!context)
		return false;

	for (size_t i = 0; i < context->simultaneous_primary_count; i++) {
		if (context->simultaneous_primary_keys[i] == key_code)
			return true;
	}

	return false;
}

static bool history_add_simultaneous_key(struct history_source *context, int key_code)
{
	if (!context)
		return false;

	if (history_simultaneous_contains(context, key_code))
		return false;

	if (context->simultaneous_primary_count >= HISTORY_MAX_SIMULTANEOUS_PRIMARY_KEYS)
		return false;

	size_t index = context->simultaneous_primary_count;

	context->simultaneous_primary_keys[index] = key_code;
	context->simultaneous_primary_count++;

	return true;
}

static bool history_event_inside_window(
	const struct history_source *context,
	uint64_t timestamp_ms)
{
	if (!context)
		return false;

	if (context->simultaneous_input_window_ms == 0)
		return false;

	if (context->combo_count == 0)
		return false;

	if (context->simultaneous_input_start_ms == 0)
		return false;

	if (timestamp_ms < context->simultaneous_input_start_ms)
		return false;

	uint64_t elapsed =
		timestamp_ms - context->simultaneous_input_start_ms;

	return elapsed <= context->simultaneous_input_window_ms;
}

/* --------------------------------------------------------- */
/* Build latest combo                                        */
/* --------------------------------------------------------- */

static bool history_latest_is_right_anchored(
	const struct history_source *context)
{
	if (!context)
		return false;

	return context->history_position == HISTORY_POSITION_BOTTOM_RIGHT ||
	       context->history_position == HISTORY_POSITION_TOP_RIGHT ||
	       context->history_position == HISTORY_POSITION_LEFT_TOP ||
	       context->history_position == HISTORY_POSITION_LEFT_BOTTOM;
}

static void history_build_combo(struct history_source *context)
{
	if (!context)
		return;

	history_clear_combo(context);

	if (context->distinguish_left_right_modifiers) {
		if (context->left_ctrl && history_key_is_recognized(context, VK_LCONTROL))
			history_add_combo_box(context, "LCtrl");
		if (context->right_ctrl && history_key_is_recognized(context, VK_RCONTROL))
			history_add_combo_box(context, "RCtrl");
		if (context->left_shift && history_key_is_recognized(context, VK_LSHIFT))
			history_add_combo_box(context, "LShift");
		if (context->right_shift && history_key_is_recognized(context, VK_RSHIFT))
			history_add_combo_box(context, "RShift");
		if (context->left_alt && history_key_is_recognized(context, VK_LMENU))
			history_add_combo_box(context, "LAlt");
		if (context->right_alt && history_key_is_recognized(context, VK_RMENU))
			history_add_combo_box(context, "RAlt");
	} else {
		if ((context->left_ctrl && history_key_is_recognized(context, VK_LCONTROL)) ||
		    (context->right_ctrl && history_key_is_recognized(context, VK_RCONTROL)))
			history_add_combo_box(context, "Ctrl");

		if ((context->left_shift && history_key_is_recognized(context, VK_LSHIFT)) ||
		    (context->right_shift && history_key_is_recognized(context, VK_RSHIFT)))
			history_add_combo_box(context, "Shift");

		if ((context->left_alt && history_key_is_recognized(context, VK_LMENU)) ||
		    (context->right_alt && history_key_is_recognized(context, VK_RMENU)))
			history_add_combo_box(context, "Alt");
	}

	if (context->space && history_key_is_recognized(context, VK_SPACE))
		history_add_combo_box(context, "Space");

	if (history_latest_is_right_anchored(context)) {
		for (size_t i = context->simultaneous_primary_count; i > 0; i--) {
			int key_code = context->simultaneous_primary_keys[i - 1];
			if (!history_key_is_recognized(context, key_code))
				continue;

			char label[HISTORY_COMBO_LABEL_SIZE];
			history_get_key_label(key_code, label, sizeof(label));
			history_add_combo_box(context, label);
		}
	} else {
		for (size_t i = 0; i < context->simultaneous_primary_count; i++) {
			int key_code = context->simultaneous_primary_keys[i];
			if (!history_key_is_recognized(context, key_code))
				continue;

			char label[HISTORY_COMBO_LABEL_SIZE];
			history_get_key_label(key_code, label, sizeof(label));
			history_add_combo_box(context, label);
		}
	}

	history_rebuild_latest_text_texture(context);
}

/* --------------------------------------------------------- */
/* Recognize-key editor                                      */
/* --------------------------------------------------------- */

#define HISTORY_RECOGNIZE_EDITOR_WINDOW_CLASS L"PressHUDHistoryRecognizeEditor"
#define HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE 2000
#define HISTORY_RECOGNIZE_EDITOR_ALL_ID 1899
#define HISTORY_RECOGNIZE_EDITOR_NONE_ID 1900
#define HISTORY_RECOGNIZE_EDITOR_PADDING 12
#define HISTORY_RECOGNIZE_EDITOR_TOP_AREA 48

struct history_recognize_editor_state {
	struct history_source *context;
	HWND window;
	HWND buttons[HISTORY_RECOGNIZE_KEY_COUNT];
};

static void history_recognize_editor_get_key_text(size_t index, wchar_t *buffer, size_t buffer_count)
{
	if (!buffer || buffer_count == 0 || index >= HISTORY_RECOGNIZE_KEY_COUNT)
		return;

	buffer[0] = L'\0';

	const struct history_recognize_key_visual *key =
		&history_recognize_layout[index];

	switch (key->vk) {
	case VK_UP:
		buffer[0] = 0x2191;
		buffer[1] = L'\0';
		return;
	case VK_LEFT:
		buffer[0] = 0x2190;
		buffer[1] = L'\0';
		return;
	case VK_DOWN:
		buffer[0] = 0x2193;
		buffer[1] = L'\0';
		return;
	case VK_RIGHT:
		buffer[0] = 0x2192;
		buffer[1] = L'\0';
		return;
	default:
		break;
	}

	MultiByteToWideChar(CP_UTF8, 0, key->label, -1, buffer, (int)buffer_count);
}

static void history_save_recognized_keys(struct history_source *context)
{
	if (!context || !context->source)
		return;

	obs_data_t *settings = obs_source_get_settings(context->source);

	if (!settings)
		return;

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];

		history_recognize_setting_name(i, setting_name, sizeof(setting_name));
		obs_data_set_bool(settings, setting_name, context->recognized_keys[i]);
	}

	obs_source_update(context->source, settings);
	obs_data_release(settings);

	if (context->combo_count > 0)
		history_build_combo(context);
}

static void history_recognize_editor_invalidate_all(struct history_recognize_editor_state *state)
{
	if (!state)
		return;

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		if (state->buttons[i])
			InvalidateRect(state->buttons[i], NULL, TRUE);
	}
}

static void history_recognize_editor_set_all(struct history_recognize_editor_state *state, bool recognized)
{
	if (!state || !state->context)
		return;

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++)
		state->context->recognized_keys[i] = recognized;

	history_save_recognized_keys(state->context);
	history_recognize_editor_invalidate_all(state);
}

static void history_recognize_editor_draw_key_button(struct history_recognize_editor_state *state,
						    DRAWITEMSTRUCT *draw_item)
{
	if (!state || !state->context || !draw_item)
		return;

	int control_id = (int)draw_item->CtlID;

	if (control_id < HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE ||
	    control_id >= HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE +
				  (int)HISTORY_RECOGNIZE_KEY_COUNT)
		return;

	size_t index =
		(size_t)(control_id - HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE);

	bool recognized = state->context->recognized_keys[index];

	COLORREF fill_color =
		recognized ? GetSysColor(COLOR_BTNFACE) : RGB(48, 48, 48);

	HBRUSH fill_brush = CreateSolidBrush(fill_color);
	FillRect(draw_item->hDC, &draw_item->rcItem, fill_brush);
	DeleteObject(fill_brush);

	RECT edge_rect = draw_item->rcItem;
	DrawEdge(draw_item->hDC, &edge_rect,
		 (draw_item->itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED,
		 BF_RECT);

	wchar_t label[32];
	history_recognize_editor_get_key_text(index, label,
					     sizeof(label) / sizeof(label[0]));

	SetBkMode(draw_item->hDC, TRANSPARENT);
	SetTextColor(draw_item->hDC,
		     recognized ? GetSysColor(COLOR_BTNTEXT) : RGB(205, 205, 205));

	HFONT button_font =
		(HFONT)SendMessageW(draw_item->hwndItem, WM_GETFONT, 0, 0);

	HGDIOBJ old_font = NULL;

	if (button_font)
		old_font = SelectObject(draw_item->hDC, button_font);

	RECT text_rect = draw_item->rcItem;
	DrawTextW(draw_item->hDC, label, -1, &text_rect,
		  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
			  DT_END_ELLIPSIS);

	if (old_font)
		SelectObject(draw_item->hDC, old_font);

	if (draw_item->itemState & ODS_FOCUS) {
		RECT focus_rect = draw_item->rcItem;
		InflateRect(&focus_rect, -3, -3);
		DrawFocusRect(draw_item->hDC, &focus_rect);
	}
}

static LRESULT CALLBACK history_recognize_editor_window_proc(HWND window, UINT message,
							    WPARAM w_param, LPARAM l_param)
{
	struct history_recognize_editor_state *state =
		(struct history_recognize_editor_state *)
			GetWindowLongPtrW(window, GWLP_USERDATA);

	switch (message) {
	case WM_CREATE: {
		CREATESTRUCTW *create = (CREATESTRUCTW *)l_param;

		state = (struct history_recognize_editor_state *)create->lpCreateParams;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);

		if (state)
			state->window = window;

		return 0;
	}

	case WM_DRAWITEM:
		if (state) {
			history_recognize_editor_draw_key_button(
				state, (DRAWITEMSTRUCT *)l_param);
			return TRUE;
		}
		break;

	case WM_COMMAND: {
		if (!state || !state->context)
			break;

		int control_id = LOWORD(w_param);

		if (control_id == HISTORY_RECOGNIZE_EDITOR_ALL_ID &&
		    HIWORD(w_param) == BN_CLICKED) {
			history_recognize_editor_set_all(state, true);
			return 0;
		}

		if (control_id == HISTORY_RECOGNIZE_EDITOR_NONE_ID &&
		    HIWORD(w_param) == BN_CLICKED) {
			history_recognize_editor_set_all(state, false);
			return 0;
		}

		if (control_id < HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE ||
		    control_id >= HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE +
					  (int)HISTORY_RECOGNIZE_KEY_COUNT)
			break;

		if (HIWORD(w_param) != BN_CLICKED)
			break;

		size_t index =
			(size_t)(control_id - HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE);

		state->context->recognized_keys[index] =
			!state->context->recognized_keys[index];

		history_save_recognized_keys(state->context);

		if (state->buttons[index])
			InvalidateRect(state->buttons[index], NULL, TRUE);

		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(window);
		return 0;

	default:
		break;
	}

	return DefWindowProcW(window, message, w_param, l_param);
}

static bool history_recognize_editor_register_class(void)
{
	static ATOM editor_class = 0;

	if (editor_class != 0)
		return true;

	WNDCLASSEXW window_class;
	memset(&window_class, 0, sizeof(window_class));

	window_class.cbSize = sizeof(WNDCLASSEXW);
	window_class.lpfnWndProc = history_recognize_editor_window_proc;
	window_class.hInstance = GetModuleHandleW(NULL);
	window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	window_class.lpszClassName = HISTORY_RECOGNIZE_EDITOR_WINDOW_CLASS;

	editor_class = RegisterClassExW(&window_class);

	if (editor_class != 0)
		return true;

	return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static void history_recognize_editor_get_full_bounds(float *min_x, float *min_y,
						     float *max_x, float *max_y)
{
	if (!min_x || !min_y || !max_x || !max_y)
		return;

	bool found = false;

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		const struct history_recognize_key_visual *key =
			&history_recognize_layout[i];

		float x = history_editor_key_x(key);
		float y = history_editor_key_y(key);
		float right = x + history_editor_key_width(key->width);
		float bottom = y + history_editor_key_height(key->height);

		if (!found) {
			*min_x = x;
			*min_y = y;
			*max_x = right;
			*max_y = bottom;
			found = true;
			continue;
		}

		if (x < *min_x)
			*min_x = x;
		if (y < *min_y)
			*min_y = y;
		if (right > *max_x)
			*max_x = right;
		if (bottom > *max_y)
			*max_y = bottom;
	}

	if (!found) {
		*min_x = 0.0f;
		*min_y = 0.0f;
		*max_x = 1.0f;
		*max_y = 1.0f;
	}
}

static void history_recognize_editor_create_buttons(struct history_recognize_editor_state *state)
{
	if (!state || !state->window || !state->context)
		return;

	HFONT gui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	history_recognize_editor_get_full_bounds(
		&full_min_x, &full_min_y, &full_max_x, &full_max_y);

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		const struct history_recognize_key_visual *key =
			&history_recognize_layout[i];

		int x = HISTORY_RECOGNIZE_EDITOR_PADDING +
			(int)(history_editor_key_x(key) - full_min_x + 0.5f);

		int y = HISTORY_RECOGNIZE_EDITOR_TOP_AREA +
			(int)(history_editor_key_y(key) - full_min_y + 0.5f);

		int width = (int)(history_editor_key_width(key->width) + 0.5f);
		int height = (int)(history_editor_key_height(key->height) + 0.5f);

		wchar_t label[32];
		history_recognize_editor_get_key_text(
			i, label, sizeof(label) / sizeof(label[0]));

		HWND button = CreateWindowExW(
			0, L"BUTTON", label,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
			x, y, width, height, state->window,
			(HMENU)(INT_PTR)(HISTORY_RECOGNIZE_EDITOR_BUTTON_BASE + (int)i),
			GetModuleHandleW(NULL), NULL);

		state->buttons[i] = button;

		if (button)
			SendMessageW(button, WM_SETFONT, (WPARAM)gui_font, TRUE);
	}
}

static void history_show_recognize_editor(struct history_source *context)
{
	if (!context)
		return;

	if (!history_recognize_editor_register_class()) {
		MessageBoxW(NULL, L"Could not create recognize-key editor window.",
			    L"PressHUD", MB_OK | MB_ICONERROR);
		return;
	}

	struct history_recognize_editor_state state;
	memset(&state, 0, sizeof(state));
	state.context = context;

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	history_recognize_editor_get_full_bounds(
		&full_min_x, &full_min_y, &full_max_x, &full_max_y);

	int client_width =
		(int)(full_max_x - full_min_x + 0.5f) +
		(HISTORY_RECOGNIZE_EDITOR_PADDING * 2);

	int client_height =
		(int)(full_max_y - full_min_y + 0.5f) +
		HISTORY_RECOGNIZE_EDITOR_TOP_AREA +
		HISTORY_RECOGNIZE_EDITOR_PADDING;

	RECT window_rect = {0, 0, client_width, client_height};

	DWORD window_style =
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	AdjustWindowRect(&window_rect, window_style, FALSE);

	HWND window = CreateWindowExW(
		WS_EX_CONTROLPARENT, HISTORY_RECOGNIZE_EDITOR_WINDOW_CLASS,
		L"PressHUD History - Recognize Keys", window_style, CW_USEDEFAULT, CW_USEDEFAULT,
		window_rect.right - window_rect.left,
		window_rect.bottom - window_rect.top,
		NULL, NULL, GetModuleHandleW(NULL), &state);

	if (!window)
		return;

	HFONT gui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	HWND description = CreateWindowExW(
		0, L"STATIC", L"Light = Recognized    Dark = Ignored",
		WS_CHILD | WS_VISIBLE,
		HISTORY_RECOGNIZE_EDITOR_PADDING, 14, 380, 22,
		window, NULL, GetModuleHandleW(NULL), NULL);

	if (description)
		SendMessageW(description, WM_SETFONT, (WPARAM)gui_font, TRUE);

	int action_button_width = 130;
	int action_button_gap = 8;

	int none_x =
		client_width - HISTORY_RECOGNIZE_EDITOR_PADDING - action_button_width;

	int all_x =
		none_x - action_button_gap - action_button_width;

	HWND all_button = CreateWindowExW(
		0, L"BUTTON", L"Recognize All",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		all_x, 9, action_button_width, 28, window,
		(HMENU)(INT_PTR)HISTORY_RECOGNIZE_EDITOR_ALL_ID,
		GetModuleHandleW(NULL), NULL);

	if (all_button)
		SendMessageW(all_button, WM_SETFONT, (WPARAM)gui_font, TRUE);

	HWND none_button = CreateWindowExW(
		0, L"BUTTON", L"Ignore All",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		none_x, 9, action_button_width, 28, window,
		(HMENU)(INT_PTR)HISTORY_RECOGNIZE_EDITOR_NONE_ID,
		GetModuleHandleW(NULL), NULL);

	if (none_button)
		SendMessageW(none_button, WM_SETFONT, (WPARAM)gui_font, TRUE);

	history_recognize_editor_create_buttons(&state);

	ShowWindow(window, SW_SHOW);
	UpdateWindow(window);

	MSG message;

	while (IsWindow(window) && GetMessageW(&message, NULL, 0, 0) > 0) {
		if (!IsDialogMessageW(window, &message)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
}

static bool history_recognize_editor_clicked(obs_properties_t *properties,
					    obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(properties);
	UNUSED_PARAMETER(property);

	struct history_source *context = data;

	if (!context)
		return false;

	history_show_recognize_editor(context);
	return false;
}

/* --------------------------------------------------------- */
/* Properties                                                */
/* --------------------------------------------------------- */

static obs_properties_t *history_source_get_properties(void *data)
{
	obs_properties_t *properties = obs_properties_create();

	obs_properties_add_int_slider(
		properties, "simultaneous_input_window_ms",
		"Simultaneous Input Window (ms)", 0, 100, 5);

	obs_property_t *position = obs_properties_add_list(
		properties, "history_position", "Key-History Position",
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(position, "Bottom - Left", HISTORY_POSITION_BOTTOM_LEFT);
	obs_property_list_add_int(position, "Bottom - Right", HISTORY_POSITION_BOTTOM_RIGHT);
	obs_property_list_add_int(position, "Top - Left", HISTORY_POSITION_TOP_LEFT);
	obs_property_list_add_int(position, "Top - Right", HISTORY_POSITION_TOP_RIGHT);
	obs_property_list_add_int(position, "Left - Top", HISTORY_POSITION_LEFT_TOP);
	obs_property_list_add_int(position, "Left - Bottom", HISTORY_POSITION_LEFT_BOTTOM);
	obs_property_list_add_int(position, "Right - Top", HISTORY_POSITION_RIGHT_TOP);
	obs_property_list_add_int(position, "Right - Bottom", HISTORY_POSITION_RIGHT_BOTTOM);

	obs_property_t *key_font =
		obs_properties_add_font(properties, "key_font", "Key Font & Text Size");
	if (key_font) {
		obs_property_set_long_description(
			key_font,
			"Select the key text font and set its size in the font dialog. Size 17 matches the previous History default appearance.");
	}

	obs_properties_add_bool(properties, "distinguish_left_right_modifiers",
				"Distinguish Left / Right Ctrl, Shift, Alt");

	obs_properties_add_button2(properties, "recognize_keys_editor",
				   "Recognize Keys...",
				   history_recognize_editor_clicked, data);

	obs_properties_add_int_slider(properties, "simultaneous_key_gap",
				      "Simultaneous Key Gap", 0, HISTORY_MAX_GAP_LEVEL, 1);

	/* Latest Key */
	obs_properties_add_int_slider(properties, "key_hold_ms",
				      "Latest Key Hold Time (ms)", 0, 5000, 50);
	obs_properties_add_int_slider(properties, "key_fade_ms",
				      "Latest Key Fade Time (ms)", 50, 2000, 25);
	obs_properties_add_int_slider(properties, "key_flash_ms",
				      "Latest Key Flash Time (ms)", 20, 500, 10);

	obs_properties_add_int_slider(properties, "key_background_opacity_percent",
				      "Latest Key Background Opacity (%)", 0, 100, 1);
	obs_properties_add_int_slider(properties, "key_flash_opacity_percent",
				      "Latest Key Flash Opacity (%)", 0, 100, 1);
	obs_properties_add_int_slider(properties, "key_text_opacity_percent",
				      "Latest Key Text Opacity (%)", 0, 100, 1);
	obs_properties_add_int_slider(properties, "latest_history_gap",
				      "Latest Key-History Gap", 0, HISTORY_MAX_GAP_LEVEL, 1);

	obs_properties_add_color_alpha(properties, "key_background_color",
				       "Latest Key Background Color");
	obs_properties_add_color_alpha(properties, "key_text_color",
				       "Latest Key Text Color");
	obs_properties_add_color_alpha(properties, "key_flash_color",
				       "Latest Key Flash Color");
	obs_properties_add_color_alpha(properties, "key_border_color",
				       "Latest Key Border Color");

	/* History */
	obs_properties_add_int_slider(properties, "history_count",
				      "History Count", 0, HISTORY_MAX_ITEMS, 1);
	obs_properties_add_int_slider(properties, "history_display_ms",
				      "History Hold Time (ms)", 0, 60000, 50);
	obs_properties_add_float_slider(properties, "history_size_percent",
					"History Size (%)", 20.0, 100.0, 5.0);

	obs_properties_add_int_slider(properties, "history_background_opacity_percent",
				      "History Background Opacity (%)", 0, 100, 1);
	obs_properties_add_int_slider(properties, "history_text_opacity_percent",
				      "History Text Opacity (%)", 0, 100, 1);
	obs_properties_add_int_slider(properties, "history_key_gap",
				      "History Key Gap", 0, HISTORY_MAX_GAP_LEVEL, 1);

	obs_properties_add_color_alpha(properties, "history_background_color",
				       "History Background Color");
	obs_properties_add_color_alpha(properties, "history_text_color",
				       "History Text Color");
	obs_properties_add_color_alpha(properties, "history_border_color",
				       "History Border Color");

	obs_properties_add_float_slider(properties, "key_border_thickness",
					"Key Border Thickness", 0.0, 20.0, 1.0);

	return properties;
}

static void history_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "history_count", HISTORY_DEFAULT_COUNT);
	obs_data_set_default_int(settings, "history_position", HISTORY_POSITION_BOTTOM_LEFT);
	obs_data_set_default_int(settings, "history_display_ms", HISTORY_DEFAULT_DISPLAY_MS);
	obs_data_set_default_double(settings, "history_size_percent", HISTORY_DEFAULT_SIZE_PERCENT);

	obs_data_set_default_int(settings, "history_background_color", 0xFF181818);
	obs_data_set_default_int(settings, "history_text_color", 0xFF909090);
	obs_data_set_default_int(settings, "history_border_color", 0xFF404040);

	for (size_t i = 0; i < HISTORY_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];
		history_recognize_setting_name(i, setting_name, sizeof(setting_name));
		obs_data_set_default_bool(settings, setting_name, true);
	}

	obs_data_set_default_bool(settings, "distinguish_left_right_modifiers", false);

	obs_data_set_default_int(settings, "simultaneous_key_gap", HISTORY_DEFAULT_GAP_LEVEL);
	obs_data_set_default_int(settings, "latest_history_gap", HISTORY_DEFAULT_GAP_LEVEL);
	obs_data_set_default_int(settings, "history_key_gap", HISTORY_DEFAULT_GAP_LEVEL);

	obs_data_set_default_int(settings, "key_background_opacity_percent", 100);
	obs_data_set_default_int(settings, "key_flash_opacity_percent", 100);
	obs_data_set_default_int(settings, "key_text_opacity_percent", 100);
	obs_data_set_default_int(settings, "history_background_opacity_percent", 100);
	obs_data_set_default_int(settings, "history_text_opacity_percent", 100);

	obs_data_t *key_font = obs_data_create();
	obs_data_set_string(key_font, "face", "Segoe UI");
	obs_data_set_string(key_font, "style", "Semibold");
	obs_data_set_int(key_font, "size", HISTORY_DEFAULT_FONT_SIZE);
	obs_data_set_int(key_font, "flags", 0);
	obs_data_set_default_obj(settings, "key_font", key_font);
	obs_data_release(key_font);

	obs_data_set_default_int(settings, "simultaneous_input_window_ms",
				 HISTORY_DEFAULT_SIMULTANEOUS_INPUT_WINDOW_MS);
	obs_data_set_default_int(settings, "key_hold_ms", HISTORY_DEFAULT_LATEST_HOLD_MS);
	obs_data_set_default_int(settings, "key_fade_ms", HISTORY_DEFAULT_LATEST_FADE_MS);
	obs_data_set_default_int(settings, "key_flash_ms", HISTORY_DEFAULT_LATEST_FLASH_MS);

	obs_data_set_default_int(settings, "key_background_color", 0xFF303030);
	obs_data_set_default_int(settings, "key_text_color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "key_flash_color", 0xFF787878);
	obs_data_set_default_int(settings, "key_border_color", 0xFF808080);
	obs_data_set_default_double(settings, "key_border_thickness", 0.0);
}

static void history_source_update(void *data, obs_data_t *settings)
{
	struct history_source *context = data;

	if (!context || !settings)
		return;

	int64_t history_count = obs_data_get_int(settings, "history_count");
	if (history_count < 0)
		history_count = 0;
	if (history_count > HISTORY_MAX_ITEMS)
		history_count = HISTORY_MAX_ITEMS;
	context->history_limit = (size_t)history_count;

	int64_t history_position = obs_data_get_int(settings, "history_position");
	if (history_position < HISTORY_POSITION_BOTTOM_LEFT ||
	    history_position > HISTORY_POSITION_RIGHT_BOTTOM) {
		history_position = HISTORY_POSITION_BOTTOM_LEFT;
	}
	context->history_position = (int)history_position;

	int64_t history_display = obs_data_get_int(settings, "history_display_ms");
	if (history_display < 0)
		history_display = 0;
	if (history_display > 60000)
		history_display = 60000;
	context->history_display_ms = (uint64_t)history_display;

	double history_size = obs_data_get_double(settings, "history_size_percent");
	if (history_size <= 0.0)
		history_size = HISTORY_DEFAULT_SIZE_PERCENT;
	context->history_scale =
		history_clamp_float((float)(history_size / 100.0), 0.20f, 1.00f);

	context->history_background_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "history_background_color"));
	context->history_text_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "history_text_color"));
	context->history_border_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "history_border_color"));

	history_load_recognized_keys(context, settings);
	context->distinguish_left_right_modifiers =
		obs_data_get_bool(settings, "distinguish_left_right_modifiers");

	int64_t simultaneous_gap_level = obs_data_get_int(settings, "simultaneous_key_gap");
	int64_t latest_history_gap_level = obs_data_get_int(settings, "latest_history_gap");
	int64_t history_gap_level = obs_data_get_int(settings, "history_key_gap");

	if (simultaneous_gap_level < 0)
		simultaneous_gap_level = 0;
	if (simultaneous_gap_level > HISTORY_MAX_GAP_LEVEL)
		simultaneous_gap_level = HISTORY_MAX_GAP_LEVEL;
	if (latest_history_gap_level < 0)
		latest_history_gap_level = 0;
	if (latest_history_gap_level > HISTORY_MAX_GAP_LEVEL)
		latest_history_gap_level = HISTORY_MAX_GAP_LEVEL;
	if (history_gap_level < 0)
		history_gap_level = 0;
	if (history_gap_level > HISTORY_MAX_GAP_LEVEL)
		history_gap_level = HISTORY_MAX_GAP_LEVEL;

	context->simultaneous_key_gap =
		HISTORY_DEFAULT_COMBO_GAP * ((float)simultaneous_gap_level / (float)HISTORY_DEFAULT_GAP_LEVEL);
	context->latest_history_gap =
		HISTORY_DEFAULT_LATEST_HISTORY_GAP * ((float)latest_history_gap_level / (float)HISTORY_DEFAULT_GAP_LEVEL);
	context->history_key_gap =
		HISTORY_DEFAULT_HISTORY_GAP * ((float)history_gap_level / (float)HISTORY_DEFAULT_GAP_LEVEL);

	context->key_background_opacity = history_clamp_float(
		(float)obs_data_get_int(settings, "key_background_opacity_percent") / 100.0f,
		0.0f, 1.0f);
	context->key_flash_opacity = history_clamp_float(
		(float)obs_data_get_int(settings, "key_flash_opacity_percent") / 100.0f,
		0.0f, 1.0f);
	context->key_text_opacity = history_clamp_float(
		(float)obs_data_get_int(settings, "key_text_opacity_percent") / 100.0f,
		0.0f, 1.0f);
	context->history_background_opacity = history_clamp_float(
		(float)obs_data_get_int(settings, "history_background_opacity_percent") / 100.0f,
		0.0f, 1.0f);
	context->history_text_opacity = history_clamp_float(
		(float)obs_data_get_int(settings, "history_text_opacity_percent") / 100.0f,
		0.0f, 1.0f);

	history_load_key_font(context, settings);

	int64_t simultaneous_window =
		obs_data_get_int(settings, "simultaneous_input_window_ms");
	if (simultaneous_window < 0)
		simultaneous_window = 0;
	if (simultaneous_window > 100)
		simultaneous_window = 100;
	context->simultaneous_input_window_ms = (uint64_t)simultaneous_window;

	int64_t key_hold = obs_data_get_int(settings, "key_hold_ms");
	int64_t key_fade = obs_data_get_int(settings, "key_fade_ms");
	int64_t key_flash = obs_data_get_int(settings, "key_flash_ms");

	context->key_hold_ms =
		key_hold >= 0 ? (uint64_t)key_hold : HISTORY_DEFAULT_LATEST_HOLD_MS;
	context->key_fade_ms =
		key_fade > 0 ? (uint64_t)key_fade : HISTORY_DEFAULT_LATEST_FADE_MS;
	context->key_flash_ms =
		key_flash > 0 ? (uint64_t)key_flash : HISTORY_DEFAULT_LATEST_FLASH_MS;

	context->key_background_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "key_background_color"));
	context->key_text_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "key_text_color"));
	context->key_flash_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "key_flash_color"));
	context->key_border_color = history_obs_rgba_to_argb(
		(uint32_t)obs_data_get_int(settings, "key_border_color"));

	context->key_border_thickness =
		(float)obs_data_get_double(settings, "key_border_thickness");
	context->key_border_thickness =
		history_clamp_float(context->key_border_thickness, 0.0f, 20.0f);

	history_trim_records_to_limit(context);

	if (context->combo_count > 0)
		history_build_combo(context);

	if (context->history_size > 0)
		history_rebuild_all_record_textures(context);
}

/* --------------------------------------------------------- */
/* Source lifecycle                                          */
/* --------------------------------------------------------- */

static const char *history_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);

	return "PressHUD History";
}

static void *history_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct history_source *context =
		bzalloc(sizeof(struct history_source));

	if (!context)
		return NULL;

	context->source = source;
	context->latest_archived = true;

	if (!input_service_start()) {
		obs_log(
			LOG_ERROR,
			"Could not start input service for history source");
	}

	history_source_update(context, settings);

	context->last_sequence = input_service_get_latest_sequence();
	context->last_key_down_ms = 0;
	context->last_key_flash_ms = 0;
	context->key_visibility = 0.0f;

	context->left_ctrl = input_service_is_key_pressed(VK_LCONTROL);
	context->right_ctrl = input_service_is_key_pressed(VK_RCONTROL);
	context->left_shift = input_service_is_key_pressed(VK_LSHIFT);
	context->right_shift = input_service_is_key_pressed(VK_RSHIFT);
	context->left_alt = input_service_is_key_pressed(VK_LMENU);
	context->right_alt = input_service_is_key_pressed(VK_RMENU);
	context->space = input_service_is_key_pressed(VK_SPACE);

	history_clear_combo(context);
	history_clear_simultaneous_keys(context);

	obs_log(LOG_INFO, "PressHUD History source created");

	return context;
}

static void history_source_destroy(void *data)
{
	struct history_source *context = data;

	if (!context)
		return;

	history_clear_records(context);
	history_destroy_latest_text_texture(context);

	obs_log(LOG_INFO, "PressHUD History source destroyed");

	bfree(context);
}

/* --------------------------------------------------------- */
/* Source size                                               */
/* --------------------------------------------------------- */

static bool history_position_is_bottom(int position)
{
	return position == HISTORY_POSITION_BOTTOM_LEFT ||
	       position == HISTORY_POSITION_BOTTOM_RIGHT;
}

static bool history_position_is_top(int position)
{
	return position == HISTORY_POSITION_TOP_LEFT ||
	       position == HISTORY_POSITION_TOP_RIGHT;
}

static bool history_position_is_left(int position)
{
	return position == HISTORY_POSITION_LEFT_TOP ||
	       position == HISTORY_POSITION_LEFT_BOTTOM;
}

static bool history_position_is_right(int position)
{
	return position == HISTORY_POSITION_RIGHT_TOP ||
	       position == HISTORY_POSITION_RIGHT_BOTTOM;
}

static bool history_position_is_right_aligned(int position)
{
	return position == HISTORY_POSITION_BOTTOM_RIGHT ||
	       position == HISTORY_POSITION_TOP_RIGHT;
}

static bool history_position_is_top_aligned(int position)
{
	return position == HISTORY_POSITION_LEFT_TOP ||
	       position == HISTORY_POSITION_RIGHT_TOP;
}

static float history_get_latest_width(const struct history_source *context)
{
	if (!context)
		return HISTORY_DEFAULT_WIDTH;

	float width = context->key_row_width;

	if (width < HISTORY_DEFAULT_WIDTH)
		width = HISTORY_DEFAULT_WIDTH;

	return width;
}

static float history_get_reserved_record_width(const struct history_source *context)
{
	if (!context)
		return HISTORY_DEFAULT_WIDTH * (float)(HISTORY_DEFAULT_SIZE_PERCENT / 100.0);

	float native_width = HISTORY_DEFAULT_WIDTH;

	for (size_t i = 0; i < context->history_size; i++) {
		if (context->records[i].valid &&
		    context->records[i].row_width > native_width) {
			native_width = context->records[i].row_width;
		}
	}

	return native_width * context->history_scale;
}

static float history_get_reserved_stack_height(const struct history_source *context)
{
	if (!context || context->history_limit == 0)
		return 0.0f;

	float slot_height = HISTORY_KEY_HEIGHT * context->history_scale;
	float result = slot_height * (float)context->history_limit;

	if (context->history_limit > 1)
		result += context->history_key_gap * (float)(context->history_limit - 1);

	return result;
}

/*
 * Horizontal History uses ONLY records that actually exist.
 * Empty configured slots no longer reserve horizontal width.
 */
static float history_get_horizontal_history_area_width(
	const struct history_source *context)
{
	if (!context || context->history_size == 0)
		return 0.0f;

	float result = 0.0f;

	for (size_t i = 0; i < context->history_size; i++) {
		if (!context->records[i].valid)
			continue;

		result +=
			context->records[i].row_width *
			context->history_scale;
	}

	if (context->history_size > 1)
		result +=
			context->history_key_gap *
			(float)(context->history_size - 1);

	return result;
}

static void history_get_source_size(
	const struct history_source *context,
	float *width,
	float *height)
{
	if (!width || !height)
		return;

	if (!context) {
		*width = HISTORY_DEFAULT_WIDTH;
		*height = HISTORY_KEY_HEIGHT;
		return;
	}

	float latest_width = history_get_latest_width(context);

	/*
	 * Bottom/Top - Right and Left-position modes intentionally
	 * use a compact fixed anchor width. Wider combinations are
	 * drawn to negative X so their right edge stays fixed
	 * without reserving thousands of transparent pixels.
	 */
	if (context->history_limit == 0) {
		if (history_position_is_right_aligned(context->history_position) ||
		    history_position_is_left(context->history_position)) {
			*width = HISTORY_DEFAULT_WIDTH;
		} else {
			*width = latest_width;
		}

		*height = HISTORY_KEY_HEIGHT;
		return;
	}

	if (history_position_is_left(context->history_position)) {
		*width = HISTORY_DEFAULT_WIDTH;
		*height = HISTORY_KEY_HEIGHT;
		return;
	}

	if (history_position_is_right(context->history_position)) {
		float history_area_width =
			history_get_horizontal_history_area_width(context);

		*width = latest_width;

		if (history_area_width > 0.0f) {
			*width +=
				context->latest_history_gap +
				history_area_width;
		}

		*height = HISTORY_KEY_HEIGHT;
		return;
	}

	float record_width = history_get_reserved_record_width(context);
	float stack_height = history_get_reserved_stack_height(context);

	if (history_position_is_right_aligned(context->history_position)) {
		/*
		 * Compact right anchor. Wider rows extend left of x=0.
		 */
		*width = HISTORY_DEFAULT_WIDTH;
	} else {
		*width =
			latest_width > record_width
				? latest_width
				: record_width;
	}

	*height =
		HISTORY_KEY_HEIGHT +
		context->latest_history_gap +
		stack_height;
}

static uint32_t history_source_get_width(void *data)
{
	struct history_source *context = data;

	float width = HISTORY_DEFAULT_WIDTH;
	float height = HISTORY_KEY_HEIGHT;

	history_get_source_size(context, &width, &height);

	UNUSED_PARAMETER(height);

	if (width < 1.0f)
		width = 1.0f;

	return (uint32_t)(width + 0.999f);
}

static uint32_t history_source_get_height(void *data)
{
	struct history_source *context = data;

	float width = HISTORY_DEFAULT_WIDTH;
	float height = HISTORY_KEY_HEIGHT;

	history_get_source_size(context, &width, &height);

	UNUSED_PARAMETER(width);

	if (height < 1.0f)
		height = 1.0f;

	return (uint32_t)(height + 0.999f);
}

/* --------------------------------------------------------- */
/* Keyboard event processing                                 */
/* --------------------------------------------------------- */

static void history_begin_new_chord(
	struct history_source *context,
	uint64_t timestamp_ms)
{
	if (!context)
		return;

	history_push_current_combo(context, timestamp_ms);

	history_clear_simultaneous_keys(context);

	context->simultaneous_input_start_ms = timestamp_ms;
	context->latest_archived = false;
}

static void history_update_keyboard(struct history_source *context)
{
	if (!context)
		return;

	struct input_key_event events[HISTORY_EVENT_READ_COUNT];

	while (true) {
		size_t count =
			input_service_read_events(
				&context->last_sequence,
				events,
				HISTORY_EVENT_READ_COUNT);

		if (count == 0)
			break;

		for (size_t i = 0; i < count; i++) {
			struct input_key_event *event = &events[i];

			history_apply_modifier_event(
				context,
				event->key_code,
				event->pressed);

			if (!event->pressed)
				continue;

			bool inside_window =
				history_event_inside_window(
					context,
					event->timestamp_ms);

			if (history_key_is_modifier(event->key_code)) {
				if (!history_modifier_is_enabled(
					    context,
					    event->key_code)) {
					continue;
				}

				if (!inside_window)
					history_begin_new_chord(
						context,
						event->timestamp_ms);

				history_build_combo(context);
				context->latest_archived = false;

			} else {
				if (!history_primary_is_enabled(
					    context,
					    event->key_code)) {
					continue;
				}

				if (!inside_window) {
					history_begin_new_chord(
						context,
						event->timestamp_ms);
				} else if (
					history_simultaneous_contains(
						context,
						event->key_code)) {
					continue;
				}

				if (!history_add_simultaneous_key(
					    context,
					    event->key_code)) {
					continue;
				}

				history_build_combo(context);
				context->latest_archived = false;
			}

			context->last_key_down_ms = event->timestamp_ms;
			context->last_key_flash_ms = event->timestamp_ms;
		}

		if (count < HISTORY_EVENT_READ_COUNT)
			break;
	}

	uint64_t now = (uint64_t)GetTickCount64();

	history_remove_expired_records(context, now);

	if (context->last_key_down_ms == 0) {
		context->key_visibility = 0.0f;
		return;
	}

	uint64_t elapsed = now - context->last_key_down_ms;

	if (elapsed <= context->key_hold_ms) {
		context->key_visibility = 1.0f;
		return;
	}

	uint64_t fade_elapsed = elapsed - context->key_hold_ms;

	if (context->key_fade_ms == 0 ||
	    fade_elapsed >= context->key_fade_ms) {
		/*
		 * No automatic History insertion here.
		 *
		 * The current Latest is archived only when the next
		 * valid input starts a new chord. If no further input
		 * arrives, Latest simply disappears.
		 */
		context->key_visibility = 0.0f;

		/*
		 * Discard this Latest permanently when it expires without
		 * a following input. A much later input must not make the
		 * already-disappeared key reappear as History.
		 */
		context->latest_archived = true;
		return;
	}

	context->key_visibility =
		1.0f -
		((float)fade_elapsed / (float)context->key_fade_ms);

	context->key_visibility =
		history_clamp_float(context->key_visibility, 0.0f, 1.0f);
}

static void history_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct history_source *context = data;

	if (!context)
		return;

	history_update_keyboard(context);
}

/* --------------------------------------------------------- */
/* Rendering helpers                                         */
/* --------------------------------------------------------- */

static void history_draw_box(
	float x,
	float y,
	float width,
	float height,
	uint32_t color_value)
{
	gs_effect_t *solid_effect =
		obs_get_base_effect(OBS_EFFECT_SOLID);

	if (!solid_effect)
		return;

	gs_eparam_t *color =
		gs_effect_get_param_by_name(solid_effect, "color");

	if (!color)
		return;

	gs_effect_set_color(color, color_value);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);

	while (gs_effect_loop(solid_effect, "Solid"))
		gs_draw_sprite(NULL, 0, (uint32_t)width, (uint32_t)height);

	gs_matrix_pop();
}

static void history_draw_outline(
	float x,
	float y,
	float width,
	float height,
	float thickness,
	uint32_t color_value)
{
	if (thickness <= 0.0f)
		return;

	if (thickness > height / 2.0f)
		thickness = height / 2.0f;

	history_draw_box(x, y, width, thickness, color_value);
	history_draw_box(x, y + height - thickness, width, thickness, color_value);

	history_draw_box(
		x,
		y + thickness,
		thickness,
		height - (thickness * 2.0f),
		color_value);

	history_draw_box(
		x + width - thickness,
		y + thickness,
		thickness,
		height - (thickness * 2.0f),
		color_value);
}

static uint32_t history_get_latest_key_color(
	const struct history_source *context,
	float *opacity)
{
	if (!context) {
		if (opacity)
			*opacity = 1.0f;
		return 0xFF303030;
	}

	if (opacity)
		*opacity = context->key_background_opacity;

	if (context->last_key_flash_ms == 0 || context->key_flash_ms == 0)
		return context->key_background_color;

	uint64_t now = (uint64_t)GetTickCount64();
	uint64_t elapsed = now - context->last_key_flash_ms;

	if (elapsed >= context->key_flash_ms)
		return context->key_background_color;

	float progress = (float)elapsed / (float)context->key_flash_ms;

	if (opacity) {
		*opacity = context->key_flash_opacity +
			(context->key_background_opacity - context->key_flash_opacity) * progress;
	}

	return history_blend_color(
		context->key_flash_color,
		context->key_background_color,
		progress);
}

static void history_draw_latest_text(
	struct history_source *context,
	float x,
	float y)
{
	if (!context || !context->text_texture)
		return;

	history_update_latest_text_texture(
		context,
		context->key_visibility * context->key_text_opacity);

	gs_effect_t *effect =
		obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (!effect)
		return;

	gs_eparam_t *image =
		gs_effect_get_param_by_name(effect, "image");

	if (!image)
		return;

	gs_effect_set_texture(image, context->text_texture);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);

	while (gs_effect_loop(effect, "Draw")) {
		gs_draw_sprite(
			context->text_texture,
			0,
			context->text_texture_width,
			HISTORY_KEY_HEIGHT_PX);
	}

	gs_matrix_pop();
}

static void history_draw_record_texture(
	struct history_record *record,
	float opacity)
{
	if (!record || !record->text_texture)
		return;

	history_update_record_text_texture(record, opacity);

	gs_effect_t *effect =
		obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (!effect)
		return;

	gs_eparam_t *image =
		gs_effect_get_param_by_name(effect, "image");

	if (!image)
		return;

	gs_effect_set_texture(image, record->text_texture);

	while (gs_effect_loop(effect, "Draw")) {
		gs_draw_sprite(
			record->text_texture,
			0,
			record->text_texture_width,
			HISTORY_KEY_HEIGHT_PX);
	}
}

static void history_draw_latest(
	struct history_source *context,
	float x,
	float y)
{
	if (!context)
		return;

	if (context->key_visibility <= 0.0f || context->combo_count == 0)
		return;

	float key_fill_opacity = context->key_background_opacity;
	uint32_t key_color = history_get_latest_key_color(context, &key_fill_opacity);
	key_color = history_color_with_opacity(
		key_color, context->key_visibility * key_fill_opacity);

	uint32_t border_color =
		history_color_with_opacity(
			context->key_border_color,
			context->key_visibility * context->key_background_opacity);

	for (size_t i = 0; i < context->combo_count; i++) {
		history_draw_box(
			x + context->combo_box_x[i],
			y,
			context->combo_box_width[i],
			HISTORY_KEY_HEIGHT,
			key_color);

		if (context->key_border_thickness > 0.0f) {
			history_draw_outline(
				x + context->combo_box_x[i],
				y,
				context->combo_box_width[i],
				HISTORY_KEY_HEIGHT,
				context->key_border_thickness,
				border_color);
		}
	}

	history_draw_latest_text(context, x, y);
}

static void history_draw_record(
	struct history_source *context,
	struct history_record *record,
	float x,
	float y,
	float opacity)
{
	if (!context || !record || !record->valid)
		return;

	float scale = context->history_scale;

	if (scale <= 0.0f || opacity <= 0.0f)
		return;

	uint32_t background_color =
		history_color_with_opacity(
			context->history_background_color,
			opacity * context->history_background_opacity);

	uint32_t border_color =
		history_color_with_opacity(
			context->history_border_color,
			opacity * context->history_background_opacity);

	gs_matrix_push();

	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);

	for (size_t i = 0; i < record->combo_count; i++) {
		history_draw_box(
			record->combo_box_x[i],
			0.0f,
			record->combo_box_width[i],
			HISTORY_KEY_HEIGHT,
			background_color);

		if (context->key_border_thickness > 0.0f) {
			history_draw_outline(
				record->combo_box_x[i],
				0.0f,
				record->combo_box_width[i],
				HISTORY_KEY_HEIGHT,
				context->key_border_thickness,
				border_color);
		}
	}

	history_draw_record_texture(record, opacity * context->history_text_opacity);

	gs_matrix_pop();
}

/* --------------------------------------------------------- */
/* Render                                                    */
/* --------------------------------------------------------- */

static void history_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct history_source *context = data;

	if (!context)
		return;

	float source_width = HISTORY_DEFAULT_WIDTH;
	float source_height = HISTORY_KEY_HEIGHT;

	history_get_source_size(
		context,
		&source_width,
		&source_height);

	uint64_t now = (uint64_t)GetTickCount64();

	float latest_width = history_get_latest_width(context);

	/*
	 * No History slots.
	 *
	 * Right-aligned and Left-position modes use a compact
	 * 400 px right anchor. Wider combinations extend to
	 * negative X instead of creating a huge transparent slot.
	 */
	if (context->history_limit == 0) {
		float latest_x = 0.0f;

		if (history_position_is_right_aligned(context->history_position) ||
		    history_position_is_left(context->history_position)) {
			latest_x =
				HISTORY_DEFAULT_WIDTH -
				latest_width;
		}

		history_draw_latest(
			context,
			latest_x,
			0.0f);

		return;
	}

	float slot_height =
		HISTORY_KEY_HEIGHT *
		context->history_scale;

	/*
	 * Bottom - Left / Bottom - Right
	 */
	if (history_position_is_bottom(context->history_position)) {
		bool align_right =
			history_position_is_right_aligned(
				context->history_position);

		float right_anchor =
			HISTORY_DEFAULT_WIDTH;

		float latest_x =
			align_right
				? right_anchor - latest_width
				: 0.0f;

		history_draw_latest(
			context,
			latest_x,
			0.0f);

		float origin_y =
			HISTORY_KEY_HEIGHT +
			context->latest_history_gap;

		for (size_t i = 0; i < context->history_size; i++) {
			struct history_record *record =
				&context->records[i];

			float opacity =
				history_get_record_opacity(
					context,
					record,
					now);

			float record_width =
				record->row_width *
				context->history_scale;

			float x =
				align_right
					? right_anchor - record_width
					: 0.0f;

			float y =
				origin_y +
				((float)i *
				 (slot_height +
				  context->history_key_gap));

			history_draw_record(
				context,
				record,
				x,
				y,
				opacity);
		}

		return;
	}

	/*
	 * Top - Left / Top - Right
	 */
	if (history_position_is_top(context->history_position)) {
		bool align_right =
			history_position_is_right_aligned(
				context->history_position);

		float right_anchor =
			HISTORY_DEFAULT_WIDTH;

		float stack_height =
			history_get_reserved_stack_height(
				context);

		float latest_y =
			stack_height +
			context->latest_history_gap;

		float latest_x =
			align_right
				? right_anchor - latest_width
				: 0.0f;

		history_draw_latest(
			context,
			latest_x,
			latest_y);

		for (size_t i = 0; i < context->history_size; i++) {
			struct history_record *record =
				&context->records[i];

			float opacity =
				history_get_record_opacity(
					context,
					record,
					now);

			float record_width =
				record->row_width *
				context->history_scale;

			float x =
				align_right
					? right_anchor - record_width
					: 0.0f;

			size_t slot_index =
				context->history_limit -
				1 -
				i;

			float y =
				(float)slot_index *
				(slot_height +
				 context->history_key_gap);

			history_draw_record(
				context,
				record,
				x,
				y,
				opacity);
		}

		return;
	}

	bool align_top =
		history_position_is_top_aligned(
			context->history_position);

	float history_y =
		align_top
			? 0.0f
			: source_height - slot_height;

	/*
	 * Left - Top / Left - Bottom
	 *
	 * Latest keeps a compact fixed right anchor at x=400.
	 * History starts immediately to the LEFT of the actual
	 * Latest combo, with only context->latest_history_gap between
	 * them. No empty History slots reserve width.
	 *
	 * Newest History is closest to Latest:
	 *
	 *   oldest ... newest | Latest
	 */
	if (history_position_is_left(context->history_position)) {
		float right_anchor =
			HISTORY_DEFAULT_WIDTH;

		float latest_x =
			right_anchor -
			latest_width;

		history_draw_latest(
			context,
			latest_x,
			0.0f);

		float cursor_x =
			latest_x -
			context->latest_history_gap;

		for (size_t i = 0; i < context->history_size; i++) {
			struct history_record *record =
				&context->records[i];

			float record_width =
				record->row_width *
				context->history_scale;

			cursor_x -= record_width;

			float opacity =
				history_get_record_opacity(
					context,
					record,
					now);

			history_draw_record(
				context,
				record,
				cursor_x,
				history_y,
				opacity);

			cursor_x -= context->history_key_gap;
		}

		return;
	}

	/*
	 * Right - Top / Right - Bottom
	 *
	 * Latest stays on the left. Horizontal bounds use only
	 * History records that actually exist, so empty configured
	 * slots do not create transparent space.
	 */
	history_draw_latest(
		context,
		0.0f,
		0.0f);

	float cursor_x =
		latest_width;

	if (context->history_size > 0)
		cursor_x += context->latest_history_gap;

	for (size_t i = 0; i < context->history_size; i++) {
		struct history_record *record =
			&context->records[i];

		float opacity =
			history_get_record_opacity(
				context,
				record,
				now);

		history_draw_record(
			context,
			record,
			cursor_x,
			history_y,
			opacity);

		cursor_x +=
			(record->row_width *
			 context->history_scale) +
			context->history_key_gap;
	}
}

/* --------------------------------------------------------- */
/* OBS source definition                                     */
/* --------------------------------------------------------- */

static struct obs_source_info history_source_info = {
	.id = HISTORY_SOURCE_ID,

	.type = OBS_SOURCE_TYPE_INPUT,

	.output_flags =
		OBS_SOURCE_VIDEO |
		OBS_SOURCE_CUSTOM_DRAW,

	.get_name = history_source_get_name,
	.create = history_source_create,
	.destroy = history_source_destroy,

	.update = history_source_update,
	.get_defaults = history_source_get_defaults,
	.get_properties = history_source_get_properties,

	.get_width = history_source_get_width,
	.get_height = history_source_get_height,

	.video_tick = history_source_video_tick,
	.video_render = history_source_video_render,
};

void history_source_register(void)
{
	obs_register_source(&history_source_info);
}
