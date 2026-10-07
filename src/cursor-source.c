#include "cursor-source.h"
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

#define CURSOR_SOURCE_ID "presshud_cursor"

/* --------------------------------------------------------- */
/* Layout                                                    */
/* --------------------------------------------------------- */

#define KEY_BOX_HEIGHT 48.0f
#define KEY_BOX_GAP 4.0f

#define DEFAULT_MOUSE_BOX_WIDTH 22.0f
#define DEFAULT_MOUSE_BOX_HEIGHT 18.0f
#define DEFAULT_MOUSE_BOX_GAP 4.0f

#define MOUSE_BORDER_THICKNESS 2.0f
#define KEY_BORDER_THICKNESS 0.0f

#define MOUSE_KEY_GAP 4.0f

#define CURSOR_OFFSET_X 18.0f

/* --------------------------------------------------------- */
/* Recognize-key editor layout                               */
/* --------------------------------------------------------- */

#define CURSOR_EDITOR_KEY_UNIT 48.0f
#define CURSOR_EDITOR_KEY_GAP 3.0f
#define CURSOR_EDITOR_KEY_STEP (CURSOR_EDITOR_KEY_UNIT + CURSOR_EDITOR_KEY_GAP)

struct cursor_recognize_key_visual {
	int vk;

	const char *label;

	float x;
	float y;

	float width;
	float height;
};

static const struct cursor_recognize_key_visual cursor_recognize_layout[] = {
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

#define CURSOR_RECOGNIZE_KEY_COUNT \
	(sizeof(cursor_recognize_layout) / sizeof(cursor_recognize_layout[0]))

/* --------------------------------------------------------- */
/* Timing                                                    */
/* --------------------------------------------------------- */

#define DEFAULT_KEY_HOLD_MS 1000
#define DEFAULT_KEY_FADE_MS 350
#define DEFAULT_KEY_FLASH_MS 120
#define DEFAULT_MOUSE_FADE_MS 300

#define DEFAULT_SIMULTANEOUS_INPUT_WINDOW_MS 35
#define MAX_SIMULTANEOUS_PRIMARY_KEYS 4

#define EVENT_READ_COUNT 64

/* --------------------------------------------------------- */
/* Key combo                                                 */
/* --------------------------------------------------------- */

#define MAX_COMBO_BOXES 8
#define COMBO_LABEL_SIZE 16

#define KEY_TEXT_RENDER_SCALE 4

#define KEY_TEXT_LOGICAL_WIDTH 800
#define KEY_TEXT_LOGICAL_HEIGHT 48

#define KEY_TEXT_TEXTURE_WIDTH \
	(KEY_TEXT_LOGICAL_WIDTH * KEY_TEXT_RENDER_SCALE)

#define KEY_TEXT_TEXTURE_HEIGHT \
	(KEY_TEXT_LOGICAL_HEIGHT * KEY_TEXT_RENDER_SCALE)

#define KEY_TEXT_PIXEL_COUNT \
	(KEY_TEXT_TEXTURE_WIDTH * KEY_TEXT_TEXTURE_HEIGHT * 4)

/* --------------------------------------------------------- */

struct mouse_visual {
	bool pressed;
	bool previous_pressed;

	uint64_t released_ms;

	float intensity;
};

struct cursor_source {
	obs_source_t *source;

	uint32_t width;
	uint32_t height;

	int target_monitor;

	bool show_left_mouse_button;
	bool show_right_mouse_button;
	bool show_mouse_outline_when_inactive;

	bool distinguish_left_right_modifiers;
	bool recognized_keys[CURSOR_RECOGNIZE_KEY_COUNT];

	float overlay_scale;
	float key_background_opacity;
	float key_flash_opacity;
	float key_text_opacity;

	wchar_t key_font_face[LF_FACESIZE];
	int key_font_size;
	int key_font_weight;
	bool key_font_italic;
	bool key_font_underline;
	bool key_font_strikeout;

	float mouse_box_width;
	float mouse_box_height;
	float mouse_box_gap;

	uint32_t key_background_color;
	uint32_t key_text_color;
	uint32_t key_text_border_color;
	uint32_t key_flash_color;
	uint32_t key_border_color;

	uint32_t mouse_border_color;
	uint32_t mouse_active_color;

	float key_text_border_thickness;
	float key_border_thickness;
	float mouse_border_thickness;

	float overlay_x;
	float overlay_y;

	float cursor_offset_x;
	float cursor_offset_y;

	bool cursor_on_target_monitor;

	uint64_t last_sequence;
	uint64_t last_mouse_sequence;

	uint64_t last_key_down_ms;
	uint64_t last_key_flash_ms;

	uint64_t key_hold_ms;
	uint64_t key_fade_ms;
	uint64_t key_flash_ms;
	uint64_t mouse_fade_ms;

	uint64_t simultaneous_input_window_ms;
	uint64_t simultaneous_input_start_ms;

	size_t simultaneous_primary_count;

	int simultaneous_primary_keys[MAX_SIMULTANEOUS_PRIMARY_KEYS];

	float key_visibility;

	/* Physical modifier state */
	bool left_ctrl;
	bool right_ctrl;

	bool left_shift;
	bool right_shift;

	bool left_alt;
	bool right_alt;

	bool space;

	/* Current displayed combo */
	size_t combo_count;

	char combo_labels[MAX_COMBO_BOXES][COMBO_LABEL_SIZE];

	float combo_box_x[MAX_COMBO_BOXES];

	float combo_box_width[MAX_COMBO_BOXES];

	float key_row_width;

	/* Dynamic text texture */
	gs_texture_t *key_text_texture;

	uint8_t key_text_base_pixels[KEY_TEXT_PIXEL_COUNT];

	uint8_t key_text_frame_pixels[KEY_TEXT_PIXEL_COUNT];

	uint8_t key_text_border_alpha[KEY_TEXT_TEXTURE_WIDTH * KEY_TEXT_TEXTURE_HEIGHT];

	uint8_t key_text_border_work_alpha[KEY_TEXT_TEXTURE_WIDTH * KEY_TEXT_TEXTURE_HEIGHT];

	struct mouse_visual left_mouse;
	struct mouse_visual right_mouse;
};

struct monitor_lookup {
	int target_index;
	int current_index;

	bool found;

	HMONITOR monitor;
	RECT rect;
};

struct primary_monitor_lookup {
	int current_index;
	int primary_index;

	bool found;
};

/* --------------------------------------------------------- */
/* General helpers                                           */
/* --------------------------------------------------------- */

static float clamp_float(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;

	if (value > maximum)
		return maximum;

	return value;
}

static float get_overlay_scale_from_settings(obs_data_t *settings)
{
	if (!settings)
		return 1.5f;

	double percent = obs_data_get_double(settings, "overlay_scale_percent");

	if (percent <= 0.0)
		percent = 100.0;

	/*
	 * New 100% = old 150%
	 */
	float scale = (float)((percent / 100.0) * 1.5);

	return clamp_float(scale, 0.75f, 3.0f);
}

static uint32_t obs_rgba_to_argb(uint32_t color)
{
	uint32_t alpha = color & 0xFF000000;

	uint32_t red = (color & 0x000000FF) << 16;

	uint32_t green = color & 0x0000FF00;

	uint32_t blue = (color & 0x00FF0000) >> 16;

	return alpha | red | green | blue;
}

static void load_timing_settings(struct cursor_source *context, obs_data_t *settings)
{
	if (!context || !settings)
		return;

	int64_t key_hold = obs_data_get_int(settings, "key_hold_ms");

	int64_t key_fade = obs_data_get_int(settings, "key_fade_ms");

	int64_t key_flash = obs_data_get_int(settings, "key_flash_ms");

	int64_t mouse_fade = obs_data_get_int(settings, "mouse_fade_ms");

	int64_t simultaneous_window = obs_data_get_int(settings, "simultaneous_input_window_ms");

	context->key_hold_ms = key_hold >= 0 ? (uint64_t)key_hold : DEFAULT_KEY_HOLD_MS;

	context->key_fade_ms = key_fade > 0 ? (uint64_t)key_fade : DEFAULT_KEY_FADE_MS;

	context->key_flash_ms = key_flash > 0 ? (uint64_t)key_flash : DEFAULT_KEY_FLASH_MS;

	context->mouse_fade_ms = mouse_fade > 0 ? (uint64_t)mouse_fade : DEFAULT_MOUSE_FADE_MS;

	if (simultaneous_window < 0) {

		simultaneous_window = DEFAULT_SIMULTANEOUS_INPUT_WINDOW_MS;
	}

	if (simultaneous_window > 100)
		simultaneous_window = 100;

	context->simultaneous_input_window_ms = (uint64_t)simultaneous_window;
}

static void update_canvas_size(struct cursor_source *context)
{
	if (!context)
		return;

	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info)) {
		context->width = video_info.base_width;

		context->height = video_info.base_height;
	}

	if (context->width == 0)
		context->width = 1920;

	if (context->height == 0)
		context->height = 1080;
}

static void cursor_recognize_setting_name(size_t index, char *buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0 || index >= CURSOR_RECOGNIZE_KEY_COUNT)
		return;

	snprintf(buffer, buffer_size, "recognize_key_%04X",
		 (unsigned int)cursor_recognize_layout[index].vk);
}

static void cursor_load_recognized_keys(struct cursor_source *context, obs_data_t *settings)
{
	if (!context || !settings)
		return;

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];

		cursor_recognize_setting_name(i, setting_name, sizeof(setting_name));
		context->recognized_keys[i] = obs_data_get_bool(settings, setting_name);
	}
}

static int cursor_find_recognize_key_index(int key_code)
{
	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		if (cursor_recognize_layout[i].vk == key_code)
			return (int)i;
	}

	return -1;
}

static bool cursor_key_is_recognized(const struct cursor_source *context, int key_code)
{
	if (!context)
		return false;

	int index = cursor_find_recognize_key_index(key_code);

	if (index < 0)
		return false;

	return context->recognized_keys[index];
}

static float cursor_editor_key_width(float units)
{
	return (units * CURSOR_EDITOR_KEY_UNIT) +
	       ((units - 1.0f) * CURSOR_EDITOR_KEY_GAP);
}

static float cursor_editor_key_height(float units)
{
	return (units * CURSOR_EDITOR_KEY_UNIT) +
	       ((units - 1.0f) * CURSOR_EDITOR_KEY_GAP);
}

static float cursor_editor_key_x(const struct cursor_recognize_key_visual *key)
{
	if (!key)
		return 0.0f;

	return key->x * CURSOR_EDITOR_KEY_STEP;
}

static float cursor_editor_key_y(const struct cursor_recognize_key_visual *key)
{
	if (!key)
		return 0.0f;

	return key->y * CURSOR_EDITOR_KEY_STEP;
}

/* --------------------------------------------------------- */
/* Monitor handling                                          */
/* --------------------------------------------------------- */

static BOOL CALLBACK monitor_lookup_callback(HMONITOR monitor, HDC hdc, LPRECT monitor_rect, LPARAM parameter)
{
	UNUSED_PARAMETER(hdc);
	UNUSED_PARAMETER(monitor_rect);

	struct monitor_lookup *lookup = (struct monitor_lookup *)parameter;

	if (!lookup)
		return FALSE;

	if (lookup->current_index == lookup->target_index) {

		lookup->monitor = monitor;

		MONITORINFO info;

		info.cbSize = sizeof(MONITORINFO);

		if (GetMonitorInfoW(monitor, &info)) {

			lookup->rect = info.rcMonitor;

			lookup->found = true;
		}

		return FALSE;
	}

	lookup->current_index++;

	return TRUE;
}

static bool get_monitor_rect(int monitor_index, RECT *rect, HMONITOR *monitor_handle)
{
	if (!rect)
		return false;

	struct monitor_lookup lookup = {
		.target_index = monitor_index,

		.current_index = 0,

		.found = false,

		.monitor = NULL,
	};

	EnumDisplayMonitors(NULL, NULL, monitor_lookup_callback, (LPARAM)&lookup);

	if (!lookup.found || !lookup.monitor) {
		return false;
	}

	*rect = lookup.rect;

	if (monitor_handle) {
		*monitor_handle = lookup.monitor;
	}

	return true;
}

static BOOL CALLBACK primary_monitor_callback(HMONITOR monitor, HDC hdc, LPRECT monitor_rect, LPARAM parameter)
{
	UNUSED_PARAMETER(hdc);
	UNUSED_PARAMETER(monitor_rect);

	struct primary_monitor_lookup *lookup = (struct primary_monitor_lookup *)parameter;

	MONITORINFO info;

	info.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfoW(monitor, &info)) {

		if (info.dwFlags & MONITORINFOF_PRIMARY) {

			lookup->primary_index = lookup->current_index;

			lookup->found = true;

			return FALSE;
		}
	}

	lookup->current_index++;

	return TRUE;
}

static int get_primary_monitor_index(void)
{
	struct primary_monitor_lookup lookup = {
		.current_index = 0,
		.primary_index = 0,
		.found = false,
	};

	EnumDisplayMonitors(NULL, NULL, primary_monitor_callback, (LPARAM)&lookup);

	if (!lookup.found)
		return 0;

	return lookup.primary_index;
}

static BOOL CALLBACK monitor_property_callback(HMONITOR monitor, HDC hdc, LPRECT monitor_rect, LPARAM parameter)
{
	UNUSED_PARAMETER(hdc);
	UNUSED_PARAMETER(monitor_rect);

	obs_property_t *list = (obs_property_t *)parameter;

	if (!list)
		return FALSE;

	MONITORINFO info;

	info.cbSize = sizeof(MONITORINFO);

	if (!GetMonitorInfoW(monitor, &info)) {
		return TRUE;
	}

	int index = (int)obs_property_list_item_count(list);

	LONG width = info.rcMonitor.right - info.rcMonitor.left;

	LONG height = info.rcMonitor.bottom - info.rcMonitor.top;

	char description[256];

	if (info.dwFlags & MONITORINFOF_PRIMARY) {

		snprintf(description, sizeof(description), "Display %d (Primary) - %ldx%ld @ %ld,%ld", index + 1, width,
			 height, info.rcMonitor.left, info.rcMonitor.top);

	} else {

		snprintf(description, sizeof(description), "Display %d - %ldx%ld @ %ld,%ld", index + 1, width, height,
			 info.rcMonitor.left, info.rcMonitor.top);
	}

	obs_property_list_add_int(list, description, index);

	return TRUE;
}

/* --------------------------------------------------------- */
/* Recognize-key editor                                      */
/* --------------------------------------------------------- */

#define CURSOR_RECOGNIZE_EDITOR_WINDOW_CLASS L"PressHUDCursorRecognizeEditor"
#define CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE 2000
#define CURSOR_RECOGNIZE_EDITOR_ALL_ID 1899
#define CURSOR_RECOGNIZE_EDITOR_NONE_ID 1900
#define CURSOR_RECOGNIZE_EDITOR_PADDING 12
#define CURSOR_RECOGNIZE_EDITOR_TOP_AREA 48

struct cursor_recognize_editor_state {
	struct cursor_source *context;
	HWND window;
	HWND buttons[CURSOR_RECOGNIZE_KEY_COUNT];
};

static void cursor_recognize_editor_get_key_text(size_t index, wchar_t *buffer, size_t buffer_count)
{
	if (!buffer || buffer_count == 0 || index >= CURSOR_RECOGNIZE_KEY_COUNT)
		return;

	buffer[0] = L'\0';

	const struct cursor_recognize_key_visual *key =
		&cursor_recognize_layout[index];

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

static void cursor_save_recognized_keys(struct cursor_source *context)
{
	if (!context || !context->source)
		return;

	obs_data_t *settings = obs_source_get_settings(context->source);

	if (!settings)
		return;

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];

		cursor_recognize_setting_name(i, setting_name, sizeof(setting_name));
		obs_data_set_bool(settings, setting_name, context->recognized_keys[i]);
	}

	obs_source_update(context->source, settings);
	obs_data_release(settings);
}

static void cursor_recognize_editor_invalidate_all(struct cursor_recognize_editor_state *state)
{
	if (!state)
		return;

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		if (state->buttons[i])
			InvalidateRect(state->buttons[i], NULL, TRUE);
	}
}

static void cursor_recognize_editor_set_all(struct cursor_recognize_editor_state *state, bool recognized)
{
	if (!state || !state->context)
		return;

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++)
		state->context->recognized_keys[i] = recognized;

	cursor_save_recognized_keys(state->context);
	cursor_recognize_editor_invalidate_all(state);
}

static void cursor_recognize_editor_draw_key_button(struct cursor_recognize_editor_state *state,
						    DRAWITEMSTRUCT *draw_item)
{
	if (!state || !state->context || !draw_item)
		return;

	int control_id = (int)draw_item->CtlID;

	if (control_id < CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE ||
	    control_id >= CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE +
				  (int)CURSOR_RECOGNIZE_KEY_COUNT)
		return;

	size_t index =
		(size_t)(control_id - CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE);

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
	cursor_recognize_editor_get_key_text(index, label,
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

static LRESULT CALLBACK cursor_recognize_editor_window_proc(HWND window, UINT message,
							    WPARAM w_param, LPARAM l_param)
{
	struct cursor_recognize_editor_state *state =
		(struct cursor_recognize_editor_state *)
			GetWindowLongPtrW(window, GWLP_USERDATA);

	switch (message) {
	case WM_CREATE: {
		CREATESTRUCTW *create = (CREATESTRUCTW *)l_param;

		state = (struct cursor_recognize_editor_state *)create->lpCreateParams;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);

		if (state)
			state->window = window;

		return 0;
	}

	case WM_DRAWITEM:
		if (state) {
			cursor_recognize_editor_draw_key_button(
				state, (DRAWITEMSTRUCT *)l_param);
			return TRUE;
		}
		break;

	case WM_COMMAND: {
		if (!state || !state->context)
			break;

		int control_id = LOWORD(w_param);

		if (control_id == CURSOR_RECOGNIZE_EDITOR_ALL_ID &&
		    HIWORD(w_param) == BN_CLICKED) {
			cursor_recognize_editor_set_all(state, true);
			return 0;
		}

		if (control_id == CURSOR_RECOGNIZE_EDITOR_NONE_ID &&
		    HIWORD(w_param) == BN_CLICKED) {
			cursor_recognize_editor_set_all(state, false);
			return 0;
		}

		if (control_id < CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE ||
		    control_id >= CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE +
					  (int)CURSOR_RECOGNIZE_KEY_COUNT)
			break;

		if (HIWORD(w_param) != BN_CLICKED)
			break;

		size_t index =
			(size_t)(control_id - CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE);

		state->context->recognized_keys[index] =
			!state->context->recognized_keys[index];

		cursor_save_recognized_keys(state->context);

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

static bool cursor_recognize_editor_register_class(void)
{
	static ATOM editor_class = 0;

	if (editor_class != 0)
		return true;

	WNDCLASSEXW window_class;
	memset(&window_class, 0, sizeof(window_class));

	window_class.cbSize = sizeof(WNDCLASSEXW);
	window_class.lpfnWndProc = cursor_recognize_editor_window_proc;
	window_class.hInstance = GetModuleHandleW(NULL);
	window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	window_class.lpszClassName = CURSOR_RECOGNIZE_EDITOR_WINDOW_CLASS;

	editor_class = RegisterClassExW(&window_class);

	if (editor_class != 0)
		return true;

	return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static void cursor_recognize_editor_get_full_bounds(float *min_x, float *min_y,
						     float *max_x, float *max_y)
{
	if (!min_x || !min_y || !max_x || !max_y)
		return;

	bool found = false;

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		const struct cursor_recognize_key_visual *key =
			&cursor_recognize_layout[i];

		float x = cursor_editor_key_x(key);
		float y = cursor_editor_key_y(key);
		float right = x + cursor_editor_key_width(key->width);
		float bottom = y + cursor_editor_key_height(key->height);

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

static void cursor_recognize_editor_create_buttons(struct cursor_recognize_editor_state *state)
{
	if (!state || !state->window || !state->context)
		return;

	HFONT gui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	cursor_recognize_editor_get_full_bounds(
		&full_min_x, &full_min_y, &full_max_x, &full_max_y);

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		const struct cursor_recognize_key_visual *key =
			&cursor_recognize_layout[i];

		int x = CURSOR_RECOGNIZE_EDITOR_PADDING +
			(int)(cursor_editor_key_x(key) - full_min_x + 0.5f);

		int y = CURSOR_RECOGNIZE_EDITOR_TOP_AREA +
			(int)(cursor_editor_key_y(key) - full_min_y + 0.5f);

		int width = (int)(cursor_editor_key_width(key->width) + 0.5f);
		int height = (int)(cursor_editor_key_height(key->height) + 0.5f);

		wchar_t label[32];
		cursor_recognize_editor_get_key_text(
			i, label, sizeof(label) / sizeof(label[0]));

		HWND button = CreateWindowExW(
			0, L"BUTTON", label,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
			x, y, width, height, state->window,
			(HMENU)(INT_PTR)(CURSOR_RECOGNIZE_EDITOR_BUTTON_BASE + (int)i),
			GetModuleHandleW(NULL), NULL);

		state->buttons[i] = button;

		if (button)
			SendMessageW(button, WM_SETFONT, (WPARAM)gui_font, TRUE);
	}
}

static void cursor_show_recognize_editor(struct cursor_source *context)
{
	if (!context)
		return;

	if (!cursor_recognize_editor_register_class()) {
		MessageBoxW(NULL, L"Could not create recognize-key editor window.",
			    L"PressHUD", MB_OK | MB_ICONERROR);
		return;
	}

	struct cursor_recognize_editor_state state;
	memset(&state, 0, sizeof(state));
	state.context = context;

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	cursor_recognize_editor_get_full_bounds(
		&full_min_x, &full_min_y, &full_max_x, &full_max_y);

	int client_width =
		(int)(full_max_x - full_min_x + 0.5f) +
		(CURSOR_RECOGNIZE_EDITOR_PADDING * 2);

	int client_height =
		(int)(full_max_y - full_min_y + 0.5f) +
		CURSOR_RECOGNIZE_EDITOR_TOP_AREA +
		CURSOR_RECOGNIZE_EDITOR_PADDING;

	RECT window_rect = {0, 0, client_width, client_height};

	DWORD window_style =
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	AdjustWindowRect(&window_rect, window_style, FALSE);

	HWND window = CreateWindowExW(
		WS_EX_CONTROLPARENT, CURSOR_RECOGNIZE_EDITOR_WINDOW_CLASS,
		L"PressHUD Cursor - Recognize Keys", window_style, CW_USEDEFAULT, CW_USEDEFAULT,
		window_rect.right - window_rect.left,
		window_rect.bottom - window_rect.top,
		NULL, NULL, GetModuleHandleW(NULL), &state);

	if (!window)
		return;

	HFONT gui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	HWND description = CreateWindowExW(
		0, L"STATIC", L"Light = Recognized    Dark = Ignored",
		WS_CHILD | WS_VISIBLE,
		CURSOR_RECOGNIZE_EDITOR_PADDING, 14, 380, 22,
		window, NULL, GetModuleHandleW(NULL), NULL);

	if (description)
		SendMessageW(description, WM_SETFONT, (WPARAM)gui_font, TRUE);

	int action_button_width = 130;
	int action_button_gap = 8;

	int none_x =
		client_width - CURSOR_RECOGNIZE_EDITOR_PADDING - action_button_width;

	int all_x =
		none_x - action_button_gap - action_button_width;

	HWND all_button = CreateWindowExW(
		0, L"BUTTON", L"Recognize All",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		all_x, 9, action_button_width, 28, window,
		(HMENU)(INT_PTR)CURSOR_RECOGNIZE_EDITOR_ALL_ID,
		GetModuleHandleW(NULL), NULL);

	if (all_button)
		SendMessageW(all_button, WM_SETFONT, (WPARAM)gui_font, TRUE);

	HWND none_button = CreateWindowExW(
		0, L"BUTTON", L"Ignore All",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		none_x, 9, action_button_width, 28, window,
		(HMENU)(INT_PTR)CURSOR_RECOGNIZE_EDITOR_NONE_ID,
		GetModuleHandleW(NULL), NULL);

	if (none_button)
		SendMessageW(none_button, WM_SETFONT, (WPARAM)gui_font, TRUE);

	cursor_recognize_editor_create_buttons(&state);

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

static bool cursor_recognize_editor_clicked(obs_properties_t *properties,
					    obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(properties);
	UNUSED_PARAMETER(property);

	struct cursor_source *context = data;

	if (!context)
		return false;

	cursor_show_recognize_editor(context);
	return false;
}

static void render_combo_text_pixels(struct cursor_source *context);

/* --------------------------------------------------------- */
/* Key font                                                  */
/* --------------------------------------------------------- */

static bool cursor_ascii_contains_ignore_case(const char *text, const char *needle)
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

static int cursor_font_weight_from_style(const char *style, uint32_t flags)
{
	int weight = FW_NORMAL;

	if (style && *style) {
		if (cursor_ascii_contains_ignore_case(style, "black") ||
		    cursor_ascii_contains_ignore_case(style, "heavy")) {
			weight = FW_BLACK;
		} else if (cursor_ascii_contains_ignore_case(style, "extra bold") ||
			   cursor_ascii_contains_ignore_case(style, "extrabold") ||
			   cursor_ascii_contains_ignore_case(style, "ultra bold") ||
			   cursor_ascii_contains_ignore_case(style, "ultrabold")) {
			weight = FW_EXTRABOLD;
		} else if (cursor_ascii_contains_ignore_case(style, "semi bold") ||
			   cursor_ascii_contains_ignore_case(style, "semibold") ||
			   cursor_ascii_contains_ignore_case(style, "demi bold") ||
			   cursor_ascii_contains_ignore_case(style, "demibold")) {
			weight = FW_SEMIBOLD;
		} else if (cursor_ascii_contains_ignore_case(style, "bold")) {
			weight = FW_BOLD;
		} else if (cursor_ascii_contains_ignore_case(style, "medium")) {
			weight = FW_MEDIUM;
		} else if (cursor_ascii_contains_ignore_case(style, "extra light") ||
			   cursor_ascii_contains_ignore_case(style, "extralight") ||
			   cursor_ascii_contains_ignore_case(style, "ultra light") ||
			   cursor_ascii_contains_ignore_case(style, "ultralight")) {
			weight = FW_EXTRALIGHT;
		} else if (cursor_ascii_contains_ignore_case(style, "light")) {
			weight = FW_LIGHT;
		} else if (cursor_ascii_contains_ignore_case(style, "thin")) {
			weight = FW_THIN;
		}
	}

	if ((flags & OBS_FONT_BOLD) && weight < FW_BOLD)
		weight = FW_BOLD;

	return weight;
}

static void cursor_load_key_font(struct cursor_source *context,
				 obs_data_t *settings)
{
	if (!context || !settings)
		return;

	wcsncpy(context->key_font_face, L"Segoe UI", LF_FACESIZE - 1);
	context->key_font_face[LF_FACESIZE - 1] = L'\0';
	context->key_font_size = 17;
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
			wcsncpy(context->key_font_face, wide_face,
				LF_FACESIZE - 1);
			context->key_font_face[LF_FACESIZE - 1] = L'\0';
		}
	}

	context->key_font_weight =
		cursor_font_weight_from_style(style, flags);
	context->key_font_italic =
		(flags & OBS_FONT_ITALIC) != 0 ||
		cursor_ascii_contains_ignore_case(style, "italic") ||
		cursor_ascii_contains_ignore_case(style, "oblique");
	context->key_font_underline = (flags & OBS_FONT_UNDERLINE) != 0;
	context->key_font_strikeout = (flags & OBS_FONT_STRIKEOUT) != 0;

	obs_data_release(font);
}

/* --------------------------------------------------------- */
/* OBS properties                                            */
/* --------------------------------------------------------- */

static void cursor_add_section_header(obs_properties_t *properties,
                                      const char *spacing_name,
                                      const char *header_name,
                                      const char *title)
{
	if (!properties || !spacing_name || !header_name || !title)
		return;

	/*
	 * A blank OBS_TEXT_INFO row gives the section some breathing room.
	 * Giving the header a long description forces OBS to render the
	 * title in the left-hand label column instead of the control column.
	 */
	obs_properties_add_text(properties, spacing_name, " ", OBS_TEXT_INFO);

	obs_property_t *header =
		obs_properties_add_text(properties, header_name, title, OBS_TEXT_INFO);

	if (header)
		obs_property_set_long_description(header, " ");
}

static obs_properties_t *cursor_source_get_properties(void *data)
{
	obs_properties_t *properties = obs_properties_create();

	obs_property_t *monitor_list =
		obs_properties_add_list(properties, "monitor", "Target Monitor",
					OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	EnumDisplayMonitors(NULL, NULL, monitor_property_callback, (LPARAM)monitor_list);


	obs_properties_add_float_slider(properties, "overlay_scale_percent",
					"Overlay Size (%)", 50.0, 200.0, 5.0);

	obs_properties_add_int_slider(properties, "cursor_offset_x",
				      "Horizontal Offset (px)", -50, 50, 1);

	obs_properties_add_int_slider(properties, "cursor_offset_y",
				      "Vertical Offset (px)", -50, 50, 1);

	cursor_add_section_header(properties, "keyboard_section_spacing",
				  "keyboard_section", "Keyboard");

	obs_properties_add_int_slider(properties, "key_background_opacity_percent",
				      "Key Background Opacity (%)", 0, 100, 1);

	obs_properties_add_int_slider(properties, "key_flash_opacity_percent",
				      "Key Flash Opacity (%)", 0, 100, 1);

	obs_properties_add_int_slider(properties, "key_text_opacity_percent",
				      "Key Text Opacity (%)", 0, 100, 1);

	obs_property_t *key_font =
		obs_properties_add_font(properties, "key_font", "Key Font & Text Size");

	if (key_font) {
		obs_property_set_long_description(
			key_font,
			"Select the key text font and size. The Size value in the font dialog directly controls the overlay text size.");
	}

	obs_properties_add_int_slider(properties, "simultaneous_input_window_ms",
				      "Simultaneous Input Window (ms)", 0, 100, 5);

	obs_properties_add_int_slider(properties, "key_hold_ms",
				      "Key Hold Time (ms)", 0, 5000, 50);

	obs_properties_add_int_slider(properties, "key_fade_ms",
				      "Key Fade Time (ms)", 50, 2000, 25);

	obs_properties_add_int_slider(properties, "key_flash_ms",
				      "Key Flash Time (ms)", 20, 500, 10);

	obs_properties_add_color_alpha(properties, "key_background_color",
				       "Key Background Color");

	obs_properties_add_color_alpha(properties, "key_text_color",
				       "Key Text Color");

	obs_properties_add_color_alpha(properties, "key_text_border_color",
				       "Key Text Border Color");

	obs_properties_add_float_slider(properties, "key_text_border_thickness",
					"Key Text Border Thickness", 0.0, 2.0, 0.25);

	obs_properties_add_color_alpha(properties, "key_flash_color",
				       "Key Flash Color");

	obs_properties_add_color_alpha(properties, "key_border_color",
				       "Key Border Color");

	obs_properties_add_float_slider(properties, "key_border_thickness",
					"Key Border Thickness", 0.0, 6.0, 0.5);

	obs_properties_add_bool(properties, "distinguish_left_right_modifiers",
				"Distinguish Left / Right Ctrl, Shift, Alt");

	obs_properties_add_button2(properties, "recognize_keys_editor",
				   "Recognize Keys...",
				   cursor_recognize_editor_clicked, data);

	cursor_add_section_header(properties, "mouse_section_spacing",
				  "mouse_section", "Mouse");

	obs_properties_add_bool(properties, "show_left_mouse_button",
				"Show Left Mouse Button");

	obs_properties_add_bool(properties, "show_right_mouse_button",
				"Show Right Mouse Button");

	obs_properties_add_bool(properties, "show_mouse_outline_when_inactive",
				"Show Mouse Outline When Inactive");

	obs_properties_add_int_slider(properties, "mouse_fade_ms",
				      "Mouse Fade Time (ms)", 50, 2000, 25);

	obs_properties_add_color_alpha(properties, "mouse_border_color",
				       "Mouse Border Color");

	obs_properties_add_color_alpha(properties, "mouse_active_color",
				       "Mouse Active Color");

	obs_properties_add_float_slider(properties, "mouse_box_width",
					"Mouse Button Width (px)", 10.0, 100.0, 1.0);

	obs_properties_add_float_slider(properties, "mouse_box_height",
					"Mouse Button Height (px)", 10.0, 100.0, 1.0);

	obs_properties_add_float_slider(properties, "mouse_box_gap",
					"Mouse Button Gap (px)", 0.0, 50.0, 1.0);

	obs_properties_add_float_slider(properties, "mouse_border_thickness",
					"Mouse Border Thickness", 0.5, 6.0, 0.5);

	return properties;
}

static void cursor_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "monitor", get_primary_monitor_index());


	obs_data_set_default_double(settings, "overlay_scale_percent", 100.0);

	obs_data_set_default_int(settings, "cursor_offset_x", 0);
	obs_data_set_default_int(settings, "cursor_offset_y", 0);

	obs_data_set_default_int(settings, "key_background_opacity_percent", 100);
	obs_data_set_default_int(settings, "key_flash_opacity_percent", 100);
	obs_data_set_default_int(settings, "key_text_opacity_percent", 100);

	obs_data_t *key_font = obs_data_create();
	obs_data_set_string(key_font, "face", "Segoe UI");
	obs_data_set_string(key_font, "style", "Semibold");
	obs_data_set_int(key_font, "size", 17);
	obs_data_set_int(key_font, "flags", 0);
	obs_data_set_default_obj(settings, "key_font", key_font);
	obs_data_release(key_font);


	obs_data_set_default_int(settings, "simultaneous_input_window_ms",
				 DEFAULT_SIMULTANEOUS_INPUT_WINDOW_MS);

	obs_data_set_default_int(settings, "key_hold_ms", DEFAULT_KEY_HOLD_MS);
	obs_data_set_default_int(settings, "key_fade_ms", DEFAULT_KEY_FADE_MS);
	obs_data_set_default_int(settings, "key_flash_ms", DEFAULT_KEY_FLASH_MS);

	obs_data_set_default_int(settings, "key_background_color", 0xFF303030);
	obs_data_set_default_int(settings, "key_text_color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "key_text_border_color", 0xFF000000);
	obs_data_set_default_int(settings, "key_flash_color", 0xFF787878);
	obs_data_set_default_int(settings, "key_border_color", 0xFF808080);

	obs_data_set_default_double(settings, "key_text_border_thickness", 0.0);

	obs_data_set_default_double(settings, "key_border_thickness",
				    KEY_BORDER_THICKNESS);

	obs_data_set_default_bool(settings, "distinguish_left_right_modifiers",
				  false);

	for (size_t i = 0; i < CURSOR_RECOGNIZE_KEY_COUNT; i++) {
		char setting_name[64];

		cursor_recognize_setting_name(i, setting_name, sizeof(setting_name));
		obs_data_set_default_bool(settings, setting_name, true);
	}

	obs_data_set_default_bool(settings, "show_left_mouse_button", true);
	obs_data_set_default_bool(settings, "show_right_mouse_button", true);
	obs_data_set_default_bool(settings, "show_mouse_outline_when_inactive",
				  false);

	obs_data_set_default_int(settings, "mouse_fade_ms", DEFAULT_MOUSE_FADE_MS);

	obs_data_set_default_int(settings, "mouse_border_color", 0xFF969696);
	obs_data_set_default_int(settings, "mouse_active_color", 0xFFDCDCDC);

	obs_data_set_default_double(settings, "mouse_box_width",
				    DEFAULT_MOUSE_BOX_WIDTH);
	obs_data_set_default_double(settings, "mouse_box_height",
				    DEFAULT_MOUSE_BOX_HEIGHT);
	obs_data_set_default_double(settings, "mouse_box_gap",
				    DEFAULT_MOUSE_BOX_GAP);

	obs_data_set_default_double(settings, "mouse_border_thickness",
				    MOUSE_BORDER_THICKNESS);
}

static void cursor_source_update(void *data, obs_data_t *settings)
{
	struct cursor_source *context = data;

	if (!context || !settings)
		return;

	context->target_monitor = (int)obs_data_get_int(settings, "monitor");

	context->overlay_scale = get_overlay_scale_from_settings(settings);

	context->cursor_offset_x =
		clamp_float((float)obs_data_get_int(settings, "cursor_offset_x"),
			    -50.0f, 50.0f);

	context->cursor_offset_y =
		clamp_float((float)obs_data_get_int(settings, "cursor_offset_y"),
			    -50.0f, 50.0f);

	context->key_background_opacity =
		clamp_float((float)obs_data_get_int(
			settings, "key_background_opacity_percent") / 100.0f,
			0.0f, 1.0f);

	context->key_flash_opacity =
		clamp_float((float)obs_data_get_int(
			settings, "key_flash_opacity_percent") / 100.0f,
			0.0f, 1.0f);

	context->key_text_opacity =
		clamp_float((float)obs_data_get_int(
			settings, "key_text_opacity_percent") / 100.0f,
			0.0f, 1.0f);

	cursor_load_key_font(context, settings);


	load_timing_settings(context, settings);

	context->key_background_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "key_background_color"));

	context->key_text_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "key_text_color"));

	context->key_text_border_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "key_text_border_color"));

	context->key_text_border_thickness =
		clamp_float((float)obs_data_get_double(
				    settings, "key_text_border_thickness"),
			    0.0f, 2.0f);

	context->key_flash_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "key_flash_color"));

	context->key_border_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "key_border_color"));

	context->key_border_thickness =
		clamp_float((float)obs_data_get_double(
				    settings, "key_border_thickness"),
			    0.0f, 6.0f);

	context->distinguish_left_right_modifiers =
		obs_data_get_bool(settings, "distinguish_left_right_modifiers");

	cursor_load_recognized_keys(context, settings);

	context->show_left_mouse_button =
		obs_data_get_bool(settings, "show_left_mouse_button");

	context->show_right_mouse_button =
		obs_data_get_bool(settings, "show_right_mouse_button");

	context->show_mouse_outline_when_inactive =
		obs_data_get_bool(settings, "show_mouse_outline_when_inactive");

	context->mouse_border_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "mouse_border_color"));

	context->mouse_active_color =
		obs_rgba_to_argb((uint32_t)obs_data_get_int(
			settings, "mouse_active_color"));

	context->mouse_box_width =
		clamp_float((float)obs_data_get_double(settings, "mouse_box_width"),
			    10.0f, 100.0f);

	context->mouse_box_height =
		clamp_float((float)obs_data_get_double(settings, "mouse_box_height"),
			    10.0f, 100.0f);

	context->mouse_box_gap =
		clamp_float((float)obs_data_get_double(settings, "mouse_box_gap"),
			    0.0f, 50.0f);

	context->mouse_border_thickness =
		clamp_float((float)obs_data_get_double(
				    settings, "mouse_border_thickness"),
			    0.5f, 6.0f);

	if (!context->show_left_mouse_button) {
		context->left_mouse.intensity = 0.0f;
		context->left_mouse.released_ms = 0;
	}

	if (!context->show_right_mouse_button) {
		context->right_mouse.intensity = 0.0f;
		context->right_mouse.released_ms = 0;
	}

	if (context->combo_count > 0)
		render_combo_text_pixels(context);

	context->cursor_on_target_monitor = false;
}

/* --------------------------------------------------------- */
/* Key names                                                 */
/* --------------------------------------------------------- */

static void get_key_label(int key_code, char *buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0) {
		return;
	}

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
		snprintf(buffer, buffer_size, "LWin");
		break;

	case VK_RWIN:
		snprintf(buffer, buffer_size, "RWin");
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
/* Modifier tracking                                         */
/* --------------------------------------------------------- */

static bool key_is_display_modifier(int key_code)
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

static bool modifier_is_enabled(const struct cursor_source *context, int key_code)
{
	return cursor_key_is_recognized(context, key_code);
}

static bool primary_key_is_enabled(const struct cursor_source *context, int key_code)
{
	return cursor_key_is_recognized(context, key_code);
}

static void apply_modifier_event(struct cursor_source *context, int key_code, bool pressed)
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

static bool ctrl_is_active(const struct cursor_source *context)
{
	return context->left_ctrl || context->right_ctrl;
}

static bool shift_is_active(const struct cursor_source *context)
{
	return context->left_shift || context->right_shift;
}

static bool alt_is_active(const struct cursor_source *context)
{
	return context->left_alt || context->right_alt;
}

/* --------------------------------------------------------- */
/* Combo layout                                              */
/* --------------------------------------------------------- */

static float get_label_box_width(const char *label)
{
	if (!label)
		return 48.0f;

	if (strcmp(label, "Ctrl") == 0) {
		return 58.0f;
	}

	if (strcmp(label, "Shift") == 0) {
		return 64.0f;
	}

	if (strcmp(label, "Alt") == 0) {
		return 52.0f;
	}

	if (strcmp(label, "Space") == 0) {
		return 68.0f;
	}

	size_t length = strlen(label);

	if (length <= 1)
		return 48.0f;

	if (length <= 3)
		return 54.0f;

	if (length <= 5)
		return 64.0f;

	if (length <= 7)
		return 76.0f;

	return 88.0f;
}

static void clear_combo(struct cursor_source *context)
{
	context->combo_count = 0;
	context->key_row_width = 0.0f;

	memset(context->combo_labels, 0, sizeof(context->combo_labels));

	memset(context->combo_box_x, 0, sizeof(context->combo_box_x));

	memset(context->combo_box_width, 0, sizeof(context->combo_box_width));

	memset(context->key_text_base_pixels, 0, sizeof(context->key_text_base_pixels));

	memset(context->key_text_border_alpha, 0, sizeof(context->key_text_border_alpha));

	memset(context->key_text_border_work_alpha, 0, sizeof(context->key_text_border_work_alpha));
}

static void add_combo_box(struct cursor_source *context, const char *label)
{
	if (!context || !label)
		return;

	if (context->combo_count >= MAX_COMBO_BOXES) {
		return;
	}

	size_t index = context->combo_count;

	float width = get_label_box_width(label);

	float x = 0.0f;

	if (index > 0) {
		x = context->key_row_width + KEY_BOX_GAP;
	}

	context->combo_box_x[index] = x;

	context->combo_box_width[index] = width;

	snprintf(context->combo_labels[index], COMBO_LABEL_SIZE, "%s", label);

	context->combo_count++;

	context->key_row_width = x + width;
}

/* --------------------------------------------------------- */
/* Text rasterization                                        */
/* --------------------------------------------------------- */

static HFONT create_combo_font(const struct cursor_source *context, int size)
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

static void build_key_text_border_mask(struct cursor_source *context)
{
	if (!context)
		return;

	memset(context->key_text_border_alpha, 0,
	       sizeof(context->key_text_border_alpha));
	memset(context->key_text_border_work_alpha, 0,
	       sizeof(context->key_text_border_work_alpha));

	float logical_thickness = context->key_text_border_thickness;

	if (logical_thickness <= 0.0f || context->key_row_width <= 0.0f)
		return;

	int radius = (int)(logical_thickness * (float)KEY_TEXT_RENDER_SCALE + 0.5f);

	if (radius < 1)
		radius = 1;

	if (radius > 8)
		radius = 8;

	int used_width =
		(int)(context->key_row_width * (float)KEY_TEXT_RENDER_SCALE + 0.5f) +
		(radius * 2) + 4;

	if (used_width < 1)
		used_width = 1;

	if (used_width > KEY_TEXT_TEXTURE_WIDTH)
		used_width = KEY_TEXT_TEXTURE_WIDTH;

	for (int y = 0; y < KEY_TEXT_TEXTURE_HEIGHT; y++) {
		size_t row = (size_t)y * KEY_TEXT_TEXTURE_WIDTH;

		for (int x = 0; x < used_width; x++) {
			size_t index = row + (size_t)x;
			context->key_text_border_alpha[index] =
				context->key_text_base_pixels[(index * 4) + 3];
		}
	}

	uint8_t *source = context->key_text_border_alpha;
	uint8_t *target = context->key_text_border_work_alpha;

	for (int pass = 0; pass < radius; pass++) {
		for (int y = 0; y < KEY_TEXT_TEXTURE_HEIGHT; y++) {
			for (int x = 0; x < used_width; x++) {
				uint8_t maximum = 0;

				int min_y = y > 0 ? y - 1 : y;
				int max_y = y + 1 < KEY_TEXT_TEXTURE_HEIGHT ? y + 1 : y;
				int min_x = x > 0 ? x - 1 : x;
				int max_x = x + 1 < used_width ? x + 1 : x;

				for (int sample_y = min_y; sample_y <= max_y; sample_y++) {
					size_t row = (size_t)sample_y * KEY_TEXT_TEXTURE_WIDTH;

					for (int sample_x = min_x; sample_x <= max_x; sample_x++) {
						uint8_t value = source[row + (size_t)sample_x];

						if (value > maximum)
							maximum = value;
					}
				}

				target[(size_t)y * KEY_TEXT_TEXTURE_WIDTH + (size_t)x] = maximum;
			}
		}

		uint8_t *swap = source;
		source = target;
		target = swap;
	}

	if (source != context->key_text_border_alpha) {
		for (int y = 0; y < KEY_TEXT_TEXTURE_HEIGHT; y++) {
			size_t row = (size_t)y * KEY_TEXT_TEXTURE_WIDTH;
			memcpy(context->key_text_border_alpha + row,
			       source + row, (size_t)used_width);
		}
	}

	for (int y = 0; y < KEY_TEXT_TEXTURE_HEIGHT; y++) {
		size_t row = (size_t)y * KEY_TEXT_TEXTURE_WIDTH;

		for (int x = 0; x < used_width; x++) {
			size_t index = row + (size_t)x;
			uint8_t text_alpha =
				context->key_text_base_pixels[(index * 4) + 3];
			uint8_t expanded_alpha = context->key_text_border_alpha[index];

			context->key_text_border_alpha[index] =
				expanded_alpha > text_alpha ?
					(uint8_t)(expanded_alpha - text_alpha) : 0;
		}
	}
}

static void render_combo_text_pixels(struct cursor_source *context)
{
	if (!context)
		return;

	memset(context->key_text_base_pixels, 0, sizeof(context->key_text_base_pixels));
	memset(context->key_text_frame_pixels, 0, sizeof(context->key_text_frame_pixels));

	if (context->combo_count == 0)
		return;

	HDC hdc = CreateCompatibleDC(NULL);

	if (!hdc)
		return;

	BITMAPINFO bitmap_info;

	memset(&bitmap_info, 0, sizeof(bitmap_info));

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	bitmap_info.bmiHeader.biWidth = KEY_TEXT_TEXTURE_WIDTH;

	bitmap_info.bmiHeader.biHeight = -KEY_TEXT_TEXTURE_HEIGHT;

	bitmap_info.bmiHeader.biPlanes = 1;

	bitmap_info.bmiHeader.biBitCount = 32;

	bitmap_info.bmiHeader.biCompression = BI_RGB;

	void *bitmap_bits = NULL;

	HBITMAP bitmap = CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, &bitmap_bits, NULL, 0);

	if (!bitmap || !bitmap_bits) {

		if (bitmap)
			DeleteObject(bitmap);

		DeleteDC(hdc);

		return;
	}

	HGDIOBJ old_bitmap = SelectObject(hdc, bitmap);

	memset(bitmap_bits, 0, KEY_TEXT_PIXEL_COUNT);

	SetBkMode(hdc, TRANSPARENT);

	SetTextColor(hdc, RGB(255, 255, 255));

	int selected_font_size = context->key_font_size;

	if (selected_font_size < 1)
		selected_font_size = 17;

	float raster_scale = (float)KEY_TEXT_RENDER_SCALE;

	/*
	 * The OBS font property's Size value is now the single text-size
	 * control.  Size 17 reproduces the previous default appearance.
	 * Keep the existing long-label reduction ratio (14 / 17) so the
	 * current Cursor layout does not change when this setting migrates.
	 */
	int normal_font_size =
		(int)((float)selected_font_size * raster_scale + 0.5f);

	int small_font_size =
		(int)((float)selected_font_size * (14.0f / 17.0f) *
		      raster_scale + 0.5f);

	HFONT normal_font = create_combo_font(context, normal_font_size);

	HFONT small_font = create_combo_font(context, small_font_size);

	HGDIOBJ old_font = GetCurrentObject(hdc, OBJ_FONT);

	for (size_t i = 0; i < context->combo_count; i++) {

		const char *label = context->combo_labels[i];

		float box_x = context->combo_box_x[i];

		float box_width = context->combo_box_width[i];

		RECT rect;

		LONG text_padding = (LONG)(2.0f * raster_scale);

		rect.left = (LONG)(box_x * raster_scale) + text_padding;

		rect.top = text_padding;

		rect.right = (LONG)((box_x + box_width) * raster_scale) - text_padding;

		rect.bottom = KEY_TEXT_TEXTURE_HEIGHT - text_padding;

		HFONT font = (strlen(label) >= 5) ? small_font : normal_font;

		SelectObject(hdc, font);

		wchar_t wide_label[64];

		int wide_length = MultiByteToWideChar(CP_UTF8, 0, label, -1, wide_label,
						      (int)(sizeof(wide_label) / sizeof(wide_label[0])));

		if (wide_length > 0) {

			DrawTextW(hdc, wide_label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
	}

	SelectObject(hdc, old_font);

	GdiFlush();

	uint8_t *pixels = (uint8_t *)bitmap_bits;

	size_t pixel_count = (size_t)KEY_TEXT_TEXTURE_WIDTH * (size_t)KEY_TEXT_TEXTURE_HEIGHT;

	for (size_t i = 0; i < pixel_count; i++) {

		uint8_t *pixel = pixels + (i * 4);

		uint8_t blue = pixel[0];

		uint8_t green = pixel[1];

		uint8_t red = pixel[2];

		uint8_t alpha = red;

		if (green > alpha)
			alpha = green;

		if (blue > alpha)
			alpha = blue;

		pixel[0] = 255;
		pixel[1] = 255;
		pixel[2] = 255;
		pixel[3] = alpha;
	}

	memcpy(context->key_text_base_pixels, pixels, KEY_TEXT_PIXEL_COUNT);

	build_key_text_border_mask(context);

	if (normal_font)
		DeleteObject(normal_font);

	if (small_font)
		DeleteObject(small_font);

	SelectObject(hdc, old_bitmap);

	DeleteObject(bitmap);

	DeleteDC(hdc);
}

/* --------------------------------------------------------- */
/* Build current displayed combo                             */
/* --------------------------------------------------------- */

static void build_combo(struct cursor_source *context)
{
	if (!context)
		return;

	clear_combo(context);

	if (context->distinguish_left_right_modifiers) {
		if (context->left_ctrl &&
		    cursor_key_is_recognized(context, VK_LCONTROL))
			add_combo_box(context, "LCtrl");

		if (context->right_ctrl &&
		    cursor_key_is_recognized(context, VK_RCONTROL))
			add_combo_box(context, "RCtrl");

		if (context->left_shift &&
		    cursor_key_is_recognized(context, VK_LSHIFT))
			add_combo_box(context, "LShift");

		if (context->right_shift &&
		    cursor_key_is_recognized(context, VK_RSHIFT))
			add_combo_box(context, "RShift");

		if (context->left_alt &&
		    cursor_key_is_recognized(context, VK_LMENU))
			add_combo_box(context, "LAlt");

		if (context->right_alt &&
		    cursor_key_is_recognized(context, VK_RMENU))
			add_combo_box(context, "RAlt");
	} else {
		if ((context->left_ctrl &&
		     cursor_key_is_recognized(context, VK_LCONTROL)) ||
		    (context->right_ctrl &&
		     cursor_key_is_recognized(context, VK_RCONTROL)))
			add_combo_box(context, "Ctrl");

		if ((context->left_shift &&
		     cursor_key_is_recognized(context, VK_LSHIFT)) ||
		    (context->right_shift &&
		     cursor_key_is_recognized(context, VK_RSHIFT)))
			add_combo_box(context, "Shift");

		if ((context->left_alt &&
		     cursor_key_is_recognized(context, VK_LMENU)) ||
		    (context->right_alt &&
		     cursor_key_is_recognized(context, VK_RMENU)))
			add_combo_box(context, "Alt");
	}

	if (context->space &&
	    cursor_key_is_recognized(context, VK_SPACE))
		add_combo_box(context, "Space");

	for (size_t i = 0; i < context->simultaneous_primary_count; i++) {
		int key_code = context->simultaneous_primary_keys[i];

		if (!cursor_key_is_recognized(context, key_code))
			continue;

		char label[COMBO_LABEL_SIZE];
		get_key_label(key_code, label, sizeof(label));
		add_combo_box(context, label);
	}

	render_combo_text_pixels(context);
}

static void clear_simultaneous_primary_keys(struct cursor_source *context)
{
	if (!context)
		return;

	context->simultaneous_input_start_ms = 0;

	context->simultaneous_primary_count = 0;

	memset(context->simultaneous_primary_keys, 0, sizeof(context->simultaneous_primary_keys));
}

static bool simultaneous_primary_contains(const struct cursor_source *context, int key_code)
{
	if (!context)
		return false;

	for (size_t i = 0; i < context->simultaneous_primary_count; i++) {

		if (context->simultaneous_primary_keys[i] == key_code) {

			return true;
		}
	}

	return false;
}

static bool add_simultaneous_primary_key(struct cursor_source *context, int key_code)
{
	if (!context)
		return false;

	if (simultaneous_primary_contains(context, key_code)) {

		return false;
	}

	if (context->simultaneous_primary_count >= MAX_SIMULTANEOUS_PRIMARY_KEYS) {

		return false;
	}

	size_t index = context->simultaneous_primary_count;

	context->simultaneous_primary_keys[index] = key_code;

	context->simultaneous_primary_count++;

	return true;
}

static bool event_is_inside_simultaneous_window(const struct cursor_source *context, uint64_t timestamp_ms)
{
	if (!context)
		return false;

	if (context->simultaneous_input_window_ms == 0) {

		return false;
	}

	if (context->simultaneous_primary_count == 0) {

		return false;
	}

	if (context->simultaneous_input_start_ms == 0) {

		return false;
	}

	if (timestamp_ms < context->simultaneous_input_start_ms) {

		return false;
	}

	uint64_t elapsed = timestamp_ms - context->simultaneous_input_start_ms;

	return elapsed <= context->simultaneous_input_window_ms;
}

/* --------------------------------------------------------- */
/* Dynamic texture                                           */
/* --------------------------------------------------------- */

static void create_key_text_texture(struct cursor_source *context)
{
	if (!context)
		return;

	memset(context->key_text_frame_pixels, 0, sizeof(context->key_text_frame_pixels));

	const uint8_t *initial_data = context->key_text_frame_pixels;

	obs_enter_graphics();

	context->key_text_texture = gs_texture_create(KEY_TEXT_TEXTURE_WIDTH, KEY_TEXT_TEXTURE_HEIGHT, GS_BGRA, 1,
						      &initial_data, GS_DYNAMIC);

	obs_leave_graphics();
}

static void destroy_key_text_texture(struct cursor_source *context)
{
	if (!context || !context->key_text_texture) {
		return;
	}

	obs_enter_graphics();

	gs_texture_destroy(context->key_text_texture);

	obs_leave_graphics();

	context->key_text_texture = NULL;
}

/* --------------------------------------------------------- */
/* Source lifecycle                                          */
/* --------------------------------------------------------- */

static const char *cursor_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);

	return "PressHUD Cursor";
}

static void *cursor_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct cursor_source *context = bzalloc(sizeof(struct cursor_source));

	if (!context)
		return NULL;

	context->source = source;

	if (!input_service_start())
		obs_log(LOG_ERROR, "Could not start input service for cursor source");

	update_canvas_size(context);
	cursor_source_update(context, settings);

	context->last_sequence = input_service_get_latest_sequence();
	context->last_mouse_sequence = input_service_get_latest_mouse_sequence();

	context->last_key_down_ms = 0;
	context->last_key_flash_ms = 0;
	context->key_visibility = 0.0f;

	clear_simultaneous_primary_keys(context);

	context->left_ctrl = input_service_is_key_pressed(VK_LCONTROL);
	context->right_ctrl = input_service_is_key_pressed(VK_RCONTROL);

	context->left_shift = input_service_is_key_pressed(VK_LSHIFT);
	context->right_shift = input_service_is_key_pressed(VK_RSHIFT);

	context->left_alt = input_service_is_key_pressed(VK_LMENU);
	context->right_alt = input_service_is_key_pressed(VK_RMENU);

	context->space = input_service_is_key_pressed(VK_SPACE);

	context->left_mouse.pressed =
		input_service_is_mouse_pressed(INPUT_MOUSE_LEFT);

	context->left_mouse.intensity =
		context->left_mouse.pressed ? 1.0f : 0.0f;

	context->right_mouse.pressed =
		input_service_is_mouse_pressed(INPUT_MOUSE_RIGHT);

	context->right_mouse.intensity =
		context->right_mouse.pressed ? 1.0f : 0.0f;

	clear_combo(context);
	create_key_text_texture(context);

	obs_log(LOG_INFO, "Cursor source created. Canvas: %ux%u, Monitor: %d",
		context->width, context->height, context->target_monitor);

	return context;
}

static void cursor_source_destroy(void *data)
{
	struct cursor_source *context = data;

	if (!context)
		return;

	destroy_key_text_texture(context);

	obs_log(LOG_INFO, "Cursor source destroyed");

	bfree(context);
}

static uint32_t cursor_source_get_width(void *data)
{
	struct cursor_source *context = data;

	if (!context)
		return 1920;

	return context->width;
}

static uint32_t cursor_source_get_height(void *data)
{
	struct cursor_source *context = data;

	if (!context)
		return 1080;

	return context->height;
}

/* --------------------------------------------------------- */
/* Keyboard event processing                                 */
/* --------------------------------------------------------- */

static void update_keyboard_combo(struct cursor_source *context)
{
	struct input_key_event events[EVENT_READ_COUNT];

	while (true) {

		size_t count = input_service_read_events(&context->last_sequence, events, EVENT_READ_COUNT);

		if (count == 0)
			break;

		for (size_t i = 0; i < count; i++) {

			struct input_key_event *event = &events[i];

			apply_modifier_event(context, event->key_code, event->pressed);

			if (!event->pressed)
				continue;

			if (key_is_display_modifier(event->key_code)) {

				if (!modifier_is_enabled(context, event->key_code)) {

					continue;
				}

				/*
				 * A modifier pressed shortly
				 * after a primary key joins
				 * the currently displayed
				 * chord.
				 *
				 * Otherwise it becomes a
				 * modifier-only display.
				 */
				if (!event_is_inside_simultaneous_window(context, event->timestamp_ms)) {

					clear_simultaneous_primary_keys(context);
				}

				build_combo(context);

			} else {

				if (!primary_key_is_enabled(context, event->key_code)) {

					continue;
				}

				bool inside_window = event_is_inside_simultaneous_window(context, event->timestamp_ms);

				if (!inside_window) {

					clear_simultaneous_primary_keys(context);

					context->simultaneous_input_start_ms = event->timestamp_ms;

				} else if (simultaneous_primary_contains(context, event->key_code)) {

					/*
					 * Ignore duplicate Down
					 * events / OS repeat.
					 */
					continue;
				}

				if (!add_simultaneous_primary_key(context, event->key_code)) {

					continue;
				}

				build_combo(context);
			}

			context->last_key_down_ms = event->timestamp_ms;

			context->last_key_flash_ms = event->timestamp_ms;
		}

		if (count < EVENT_READ_COUNT) {

			break;
		}
	}

	if (context->last_key_down_ms == 0) {

		context->key_visibility = 0.0f;

		return;
	}

	uint64_t now = (uint64_t)GetTickCount64();

	uint64_t elapsed = now - context->last_key_down_ms;

	if (elapsed <= context->key_hold_ms) {

		context->key_visibility = 1.0f;

		return;
	}

	uint64_t fade_elapsed = elapsed - context->key_hold_ms;

	if (context->key_fade_ms == 0 || fade_elapsed >= context->key_fade_ms) {

		context->key_visibility = 0.0f;

		return;
	}

	context->key_visibility = 1.0f - ((float)fade_elapsed / (float)context->key_fade_ms);

	context->key_visibility = clamp_float(context->key_visibility, 0.0f, 1.0f);
}

/* --------------------------------------------------------- */
/* Mouse state                                               */
/* --------------------------------------------------------- */

static void apply_mouse_event(struct mouse_visual *visual, bool pressed, uint64_t timestamp_ms)
{
	if (!visual)
		return;

	visual->pressed = pressed;

	if (pressed) {

		visual->released_ms = 0;

		visual->intensity = 1.0f;

	} else {

		visual->released_ms = timestamp_ms;

		/*
		 * Start release fade from full
		 * brightness even if Down and Up
		 * both happened between OBS frames.
		 */
		visual->intensity = 1.0f;
	}
}

static void update_mouse_fade(struct mouse_visual *visual, uint64_t now, uint64_t fade_ms)
{
	if (!visual)
		return;

	if (visual->pressed) {

		visual->intensity = 1.0f;

		return;
	}

	if (visual->released_ms == 0) {

		visual->intensity = 0.0f;

		return;
	}

	uint64_t elapsed = now - visual->released_ms;

	if (fade_ms == 0 || elapsed >= fade_ms) {

		visual->intensity = 0.0f;

		return;
	}

	visual->intensity = 1.0f - ((float)elapsed / (float)fade_ms);

	visual->intensity = clamp_float(visual->intensity, 0.0f, 1.0f);
}

static void update_mouse_visuals(struct cursor_source *context)
{
	struct input_mouse_event events[EVENT_READ_COUNT];

	while (true) {

		size_t count = input_service_read_mouse_events(&context->last_mouse_sequence, events, EVENT_READ_COUNT);

		if (count == 0)
			break;

		for (size_t i = 0; i < count; i++) {

			struct mouse_visual *visual = NULL;

			if (events[i].mouse_button == INPUT_MOUSE_LEFT) {

				visual = &context->left_mouse;

			} else if (events[i].mouse_button == INPUT_MOUSE_RIGHT) {

				visual = &context->right_mouse;
			}

			if (!visual)
				continue;

			apply_mouse_event(visual, events[i].pressed, events[i].timestamp_ms);
		}

		if (count < EVENT_READ_COUNT) {
			break;
		}
	}

	uint64_t now = (uint64_t)GetTickCount64();

	/*
	 * Safety reconciliation.
	 *
	 * Normally the event buffer controls
	 * the state. This also recovers if a
	 * consumer ever falls too far behind.
	 */
	bool left_now = input_service_is_mouse_pressed(INPUT_MOUSE_LEFT);

	bool right_now = input_service_is_mouse_pressed(INPUT_MOUSE_RIGHT);

	if (left_now != context->left_mouse.pressed) {

		apply_mouse_event(&context->left_mouse, left_now, now);
	}

	if (right_now != context->right_mouse.pressed) {

		apply_mouse_event(&context->right_mouse, right_now, now);
	}

	/*
	 * Continue consuming events even while
	 * mouse rendering is disabled.
	 */
	if (context->show_left_mouse_button) {

		update_mouse_fade(&context->left_mouse, now, context->mouse_fade_ms);

	} else {

		context->left_mouse.intensity = 0.0f;
	}

	if (context->show_right_mouse_button) {

		update_mouse_fade(&context->right_mouse, now, context->mouse_fade_ms);

	} else {

		context->right_mouse.intensity = 0.0f;
	}
}

static float get_mouse_row_visibility(const struct cursor_source *context)
{
	if (!context)
		return 0.0f;

	if (context->show_mouse_outline_when_inactive &&
	    (context->show_left_mouse_button ||
	     context->show_right_mouse_button))
		return 1.0f;

	float visibility = 0.0f;

	if (context->show_left_mouse_button &&
	    context->left_mouse.intensity > visibility)
		visibility = context->left_mouse.intensity;

	if (context->show_right_mouse_button &&
	    context->right_mouse.intensity > visibility)
		visibility = context->right_mouse.intensity;

	return clamp_float(visibility, 0.0f, 1.0f);
}

/* --------------------------------------------------------- */
/* Current layout size                                       */
/* --------------------------------------------------------- */

static void get_overlay_size(const struct cursor_source *context, float *width, float *height)
{
	if (!context || !width || !height)
		return;

	float mouse_row_width =
		(context->mouse_box_width * 2.0f) + context->mouse_box_gap;

	float result_width = mouse_row_width;

	if (context->key_row_width > result_width)
		result_width = context->key_row_width;

	float scale = context->overlay_scale;

	if (scale <= 0.0f)
		scale = 1.0f;

	/*
	 * Keep the existing fixed two-row layout:
	 *
	 * [mouse][mouse]
	 * [keyboard row]
	 *
	 * Mouse dimensions and the gap between the two mouse buttons
	 * are now user-configurable.
	 */
	*width = result_width * scale;

	*height =
		(context->mouse_box_height + MOUSE_KEY_GAP + KEY_BOX_HEIGHT) *
		scale;
}

/* --------------------------------------------------------- */
/* Cursor position                                           */
/* --------------------------------------------------------- */

static void update_cursor_position(struct cursor_source *context)
{
	POINT cursor;

	if (!GetCursorPos(&cursor)) {

		context->cursor_on_target_monitor = false;

		return;
	}

	RECT monitor_rect;

	HMONITOR target_monitor = NULL;

	if (!get_monitor_rect(context->target_monitor, &monitor_rect, &target_monitor)) {

		context->cursor_on_target_monitor = false;

		return;
	}

	HMONITOR cursor_monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONULL);

	if (!cursor_monitor || cursor_monitor != target_monitor) {

		context->cursor_on_target_monitor = false;

		return;
	}

	context->cursor_on_target_monitor = true;

	LONG monitor_width = monitor_rect.right - monitor_rect.left;

	LONG monitor_height = monitor_rect.bottom - monitor_rect.top;

	if (monitor_width <= 0 || monitor_height <= 0) {

		context->cursor_on_target_monitor = false;

		return;
	}

	float relative_x = (float)(cursor.x - monitor_rect.left) / (float)monitor_width;

	float relative_y = (float)(cursor.y - monitor_rect.top) / (float)monitor_height;

	float cursor_canvas_x = relative_x * (float)context->width;

	float cursor_canvas_y = relative_y * (float)context->height;

	float overlay_width;
	float overlay_height;

	get_overlay_size(context, &overlay_width, &overlay_height);

	if (overlay_width <= 0.0f || overlay_height <= 0.0f) {
		return;
	}

	/*
	 * Default position:
	 *
	 * cursor [overlay]
	 *
	 * The complete visible overlay is
	 * vertically centered on the cursor.
	 */
	float overlay_x = cursor_canvas_x + CURSOR_OFFSET_X;

	float overlay_y = cursor_canvas_y - (overlay_height / 2.0f);

	/*
	 * Flip to the left only when needed.
	 */
	if (overlay_x + overlay_width > (float)context->width) {

		overlay_x = cursor_canvas_x - CURSOR_OFFSET_X - overlay_width;
	}

	overlay_x += context->cursor_offset_x;

	overlay_y += context->cursor_offset_y;

	float max_x = (float)context->width - overlay_width;

	float max_y = (float)context->height - overlay_height;

	if (max_x < 0.0f)
		max_x = 0.0f;

	if (max_y < 0.0f)
		max_y = 0.0f;

	context->overlay_x = clamp_float(overlay_x, 0.0f, max_x);

	context->overlay_y = clamp_float(overlay_y, 0.0f, max_y);
}

/* --------------------------------------------------------- */
/* Tick                                                      */
/* --------------------------------------------------------- */

static void cursor_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct cursor_source *context = data;

	if (!context)
		return;

	update_canvas_size(context);

	update_keyboard_combo(context);

	update_mouse_visuals(context);

	update_cursor_position(context);
}

/* --------------------------------------------------------- */
/* Rendering helpers                                         */
/* --------------------------------------------------------- */

static uint32_t color_with_opacity(uint32_t color, float opacity)
{
	opacity = clamp_float(opacity, 0.0f, 1.0f);

	uint32_t original_alpha = (color >> 24) & 0xFF;

	uint32_t alpha = (uint32_t)((float)original_alpha * opacity);

	return (color & 0x00FFFFFF) | (alpha << 24);
}

static uint32_t blend_color(uint32_t from, uint32_t to, float progress)
{
	progress = clamp_float(progress, 0.0f, 1.0f);

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

static uint32_t get_key_box_color(const struct cursor_source *context,
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

	return blend_color(context->key_flash_color,
			   context->key_background_color, progress);
}

static void draw_box(float x, float y, float width, float height, uint32_t color_value)
{
	gs_effect_t *solid_effect = obs_get_base_effect(OBS_EFFECT_SOLID);

	if (!solid_effect)
		return;

	gs_eparam_t *color = gs_effect_get_param_by_name(solid_effect, "color");

	if (!color)
		return;

	gs_effect_set_color(color, color_value);

	gs_matrix_push();

	gs_matrix_translate3f(x, y, 0.0f);

	while (gs_effect_loop(solid_effect, "Solid")) {

		gs_draw_sprite(NULL, 0, (uint32_t)width, (uint32_t)height);
	}

	gs_matrix_pop();
}

static void draw_outline_box(float x, float y, float width, float height, float thickness, uint32_t color_value)
{
	if (thickness <= 0.0f)
		return;

	/*
	 * Top
	 */
	draw_box(x, y, width, thickness, color_value);

	/*
	 * Bottom
	 */
	draw_box(x, y + height - thickness, width, thickness, color_value);

	/*
	 * Left
	 */
	draw_box(x, y + thickness, thickness, height - (thickness * 2.0f), color_value);

	/*
	 * Right
	 */
	draw_box(x + width - thickness, y + thickness, thickness, height - (thickness * 2.0f), color_value);
}

static void update_key_text_texture(struct cursor_source *context, float opacity)
{
	if (!context || !context->key_text_texture)
		return;

	opacity = clamp_float(opacity, 0.0f, 1.0f);

	int border_radius =
		(int)(context->key_text_border_thickness *
		      (float)KEY_TEXT_RENDER_SCALE + 0.5f);

	if (border_radius < 0)
		border_radius = 0;

	if (border_radius > 8)
		border_radius = 8;

	int used_width =
		(int)(context->key_row_width * (float)KEY_TEXT_RENDER_SCALE + 0.5f) +
		(border_radius * 2) + 4;

	if (used_width < 1)
		used_width = 1;

	if (used_width > KEY_TEXT_TEXTURE_WIDTH)
		used_width = KEY_TEXT_TEXTURE_WIDTH;

	uint8_t text_alpha = (uint8_t)((context->key_text_color >> 24) & 0xFF);
	uint8_t text_red = (uint8_t)((context->key_text_color >> 16) & 0xFF);
	uint8_t text_green = (uint8_t)((context->key_text_color >> 8) & 0xFF);
	uint8_t text_blue = (uint8_t)(context->key_text_color & 0xFF);

	uint8_t border_alpha =
		(uint8_t)((context->key_text_border_color >> 24) & 0xFF);
	uint8_t border_red =
		(uint8_t)((context->key_text_border_color >> 16) & 0xFF);
	uint8_t border_green =
		(uint8_t)((context->key_text_border_color >> 8) & 0xFF);
	uint8_t border_blue =
		(uint8_t)(context->key_text_border_color & 0xFF);

	float text_alpha_scale = ((float)text_alpha / 255.0f) * opacity;
	float border_alpha_scale = ((float)border_alpha / 255.0f) * opacity;

	for (int y = 0; y < KEY_TEXT_TEXTURE_HEIGHT; y++) {
		size_t row = (size_t)y * KEY_TEXT_TEXTURE_WIDTH;

		for (int x = 0; x < used_width; x++) {
			size_t pixel_index = row + (size_t)x;
			size_t offset = pixel_index * 4;

			uint8_t text_mask = context->key_text_base_pixels[offset + 3];
			uint8_t border_mask = context->key_text_border_alpha[pixel_index];

			if (text_mask == 0 && border_mask == 0) {
				context->key_text_frame_pixels[offset + 0] = 0;
				context->key_text_frame_pixels[offset + 1] = 0;
				context->key_text_frame_pixels[offset + 2] = 0;
				context->key_text_frame_pixels[offset + 3] = 0;
				continue;
			}

			if (border_mask == 0) {
				float final_alpha = (float)text_mask * text_alpha_scale;

				context->key_text_frame_pixels[offset + 0] = text_blue;
				context->key_text_frame_pixels[offset + 1] = text_green;
				context->key_text_frame_pixels[offset + 2] = text_red;
				context->key_text_frame_pixels[offset + 3] =
					(uint8_t)(final_alpha + 0.5f);
				continue;
			}

			if (text_mask == 0) {
				float final_alpha = (float)border_mask * border_alpha_scale;

				context->key_text_frame_pixels[offset + 0] = border_blue;
				context->key_text_frame_pixels[offset + 1] = border_green;
				context->key_text_frame_pixels[offset + 2] = border_red;
				context->key_text_frame_pixels[offset + 3] =
					(uint8_t)(final_alpha + 0.5f);
				continue;
			}

			float text_coverage =
				((float)text_mask / 255.0f) * text_alpha_scale;
			float border_coverage =
				((float)border_mask / 255.0f) * border_alpha_scale;

			float border_after_text = border_coverage * (1.0f - text_coverage);
			float final_alpha = text_coverage + border_after_text;

			if (final_alpha <= 0.0f) {
				context->key_text_frame_pixels[offset + 0] = 0;
				context->key_text_frame_pixels[offset + 1] = 0;
				context->key_text_frame_pixels[offset + 2] = 0;
				context->key_text_frame_pixels[offset + 3] = 0;
				continue;
			}

			float final_red =
				((float)text_red * text_coverage +
				 (float)border_red * border_after_text) / final_alpha;
			float final_green =
				((float)text_green * text_coverage +
				 (float)border_green * border_after_text) / final_alpha;
			float final_blue =
				((float)text_blue * text_coverage +
				 (float)border_blue * border_after_text) / final_alpha;

			context->key_text_frame_pixels[offset + 0] = (uint8_t)final_blue;
			context->key_text_frame_pixels[offset + 1] = (uint8_t)final_green;
			context->key_text_frame_pixels[offset + 2] = (uint8_t)final_red;
			context->key_text_frame_pixels[offset + 3] =
				(uint8_t)(final_alpha * 255.0f + 0.5f);
		}
	}

	gs_texture_set_image(context->key_text_texture,
			     context->key_text_frame_pixels,
			     KEY_TEXT_TEXTURE_WIDTH * 4, false);
}

static void draw_key_text(struct cursor_source * context, float x, float y)
{
	if (!context || !context->key_text_texture)
		return;

	update_key_text_texture(context,
				context->key_visibility * context->key_text_opacity);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (!effect)
		return;

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");

	if (!image)
		return;

	gs_effect_set_texture(image, context->key_text_texture);

	gs_matrix_push();

	gs_matrix_translate3f(x, y, 0.0f);

	while (gs_effect_loop(effect, "Draw")) {

		gs_draw_sprite(context->key_text_texture, 0, KEY_TEXT_LOGICAL_WIDTH, KEY_TEXT_LOGICAL_HEIGHT);
	}

	gs_matrix_pop();
}

/* --------------------------------------------------------- */
/* Render                                                    */
/* --------------------------------------------------------- */

static void cursor_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct cursor_source *context = data;

	if (!context)
		return;

	if (!context->cursor_on_target_monitor) {
		return;
	}

	float mouse_visibility = get_mouse_row_visibility(context);

	bool mouse_visible = mouse_visibility > 0.0f;

	bool key_visible = context->key_visibility > 0.0f && context->combo_count > 0;

	if (!mouse_visible && !key_visible) {
		return;
	}

	float scale = context->overlay_scale;

	if (scale <= 0.0f)
		scale = 1.0f;

	/*
	 * The source remains the full
	 * OBS canvas. Only the overlay UI
	 * is translated and scaled.
	 */
	gs_matrix_push();

	gs_matrix_translate3f(context->overlay_x, context->overlay_y, 0.0f);

	gs_matrix_scale3f(scale, scale, 1.0f);

	/*
	 * Fixed local positions.
	 *
	 * Mouse row:
	 * y = 0
	 *
	 * Keyboard row:
	 * below mouse row
	 */
	float mouse_y = 0.0f;

	float key_y = context->mouse_box_height + MOUSE_KEY_GAP;

	/*
	 * Mouse row.
	 */
	if (mouse_visible) {
		float left_x = 0.0f;
		float right_x = context->mouse_box_width + context->mouse_box_gap;

		float mouse_border_thickness = context->mouse_border_thickness;
		float max_mouse_border =
			(context->mouse_box_width < context->mouse_box_height
				 ? context->mouse_box_width
				 : context->mouse_box_height) /
			2.0f;

		if (mouse_border_thickness > max_mouse_border)
			mouse_border_thickness = max_mouse_border;

		uint32_t border_color =
			color_with_opacity(context->mouse_border_color, mouse_visibility);

		/*
		 * Left mouse button.
		 */
		if (context->show_left_mouse_button) {
			if (context->left_mouse.intensity > 0.0f) {
				draw_box(
					left_x + mouse_border_thickness,
					mouse_y + mouse_border_thickness,
					context->mouse_box_width -
						(mouse_border_thickness * 2.0f),
					context->mouse_box_height -
						(mouse_border_thickness * 2.0f),
					color_with_opacity(
						context->mouse_active_color,
						context->left_mouse.intensity));
			}

			draw_outline_box(
				left_x, mouse_y,
				context->mouse_box_width, context->mouse_box_height,
				mouse_border_thickness, border_color);
		}

		/*
		 * Right mouse button.
		 *
		 * Keep its right-side position even when the
		 * left button is hidden.
		 */
		if (context->show_right_mouse_button) {
			if (context->right_mouse.intensity > 0.0f) {
				draw_box(
					right_x + mouse_border_thickness,
					mouse_y + mouse_border_thickness,
					context->mouse_box_width -
						(mouse_border_thickness * 2.0f),
					context->mouse_box_height -
						(mouse_border_thickness * 2.0f),
					color_with_opacity(
						context->mouse_active_color,
						context->right_mouse.intensity));
			}

			draw_outline_box(
				right_x, mouse_y,
				context->mouse_box_width, context->mouse_box_height,
				mouse_border_thickness, border_color);
		}
	}

	/*
	 * Keyboard row.
	 */
	if (key_visible) {

		float row_x = 0.0f;

		float key_fill_opacity = context->key_background_opacity;
		uint32_t key_color = get_key_box_color(context, &key_fill_opacity);

		key_color = color_with_opacity(
			key_color,
			context->key_visibility * key_fill_opacity);

		for (size_t i = 0; i < context->combo_count; i++) {

			draw_box(row_x + context->combo_box_x[i], key_y, context->combo_box_width[i], KEY_BOX_HEIGHT,
				 key_color);

			if (context->key_border_thickness > 0.0f) {

				draw_outline_box(row_x + context->combo_box_x[i], key_y, context->combo_box_width[i],
						 KEY_BOX_HEIGHT, context->key_border_thickness,
						 color_with_opacity(
							 context->key_border_color,
							 context->key_visibility *
								 context->key_background_opacity));
			}
		}

		draw_key_text(context, row_x, key_y);
	}

	gs_matrix_pop();
}

/* --------------------------------------------------------- */
/* Source definition                                         */
/* --------------------------------------------------------- */

static struct obs_source_info cursor_source_info = {
	.id = CURSOR_SOURCE_ID,

	.type = OBS_SOURCE_TYPE_INPUT,

	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,

	.get_name = cursor_source_get_name,

	.create = cursor_source_create,

	.destroy = cursor_source_destroy,

	.update = cursor_source_update,

	.get_defaults = cursor_source_get_defaults,

	.get_properties = cursor_source_get_properties,

	.get_width = cursor_source_get_width,

	.get_height = cursor_source_get_height,

	.video_tick = cursor_source_video_tick,

	.video_render = cursor_source_video_render,
};

void cursor_source_register(void)
{
	obs_register_source(&cursor_source_info);
}