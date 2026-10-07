#include <obs-module.h>
#include <plugin-support.h>
#include "input-service.h"
#include "cursor-source.h"
#include "history-source.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define KEYBOARD_SOURCE_ID "presshud_keyboard"

#define KEY_UNIT 48.0f
#define DEFAULT_KEY_GAP 3.0f
#define MIN_KEY_GAP 0.0f
#define MAX_KEY_GAP 6.0f

#define KEY_TEXT_HORIZONTAL_PADDING 3.0f
#define KEY_TEXT_VERTICAL_PADDING 2.0f

#define KEYBOARD_FALLBACK_CANVAS_WIDTH 1920
#define KEYBOARD_FALLBACK_CANVAS_HEIGHT 1080

struct key_visual {
	int vk;

	const char *label;

	float x;
	float y;

	float width;
	float height;
};

static const struct key_visual keyboard_layout[] = {
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

#define KEY_COUNT \
	(sizeof(keyboard_layout) / sizeof(keyboard_layout[0]))

struct keyboard_source {
	obs_source_t *source;

	bool pressed[KEY_COUNT];
	bool visible[KEY_COUNT];

	float keyboard_scale;
	float key_gap;
	float key_background_opacity;
	float key_pressed_opacity;
	float key_text_opacity;

	bool distinguish_left_right_modifiers;

	wchar_t key_font_face[LF_FACESIZE];
	int key_font_size;
	int key_font_weight;
	bool key_font_italic;
	bool key_font_underline;
	bool key_font_strikeout;

	uint32_t key_background_color;
	uint32_t key_pressed_color;
	uint32_t key_text_color;
	uint32_t key_border_color;

	float key_border_thickness;

	float render_scale;
	float bounds_min_x;
	float bounds_min_y;
	float active_base_width;
	float active_base_height;

	uint32_t canvas_width;
	uint32_t source_width;
	uint32_t source_height;

	gs_texture_t *label_texture;
};

static void keyboard_visibility_setting_name(size_t index, char *buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0 || index >= KEY_COUNT) {
		return;
	}

	snprintf(buffer, buffer_size, "key_visible_%04X", (unsigned int)keyboard_layout[index].vk);
}

static void keyboard_load_visibility(struct keyboard_source *context, obs_data_t *settings)
{
	if (!context || !settings) {
		return;
	}

	for (size_t i = 0; i < KEY_COUNT; i++) {

		char setting_name[64];

		keyboard_visibility_setting_name(i, setting_name, sizeof(setting_name));

		context->visible[i] = obs_data_get_bool(settings, setting_name);
	}
}

static uint32_t keyboard_obs_rgba_to_argb(uint32_t color)
{
	uint32_t alpha = color & 0xFF000000;

	uint32_t red = (color & 0x000000FF) << 16;

	uint32_t green = color & 0x0000FF00;

	uint32_t blue = (color & 0x00FF0000) >> 16;

	return alpha | red | green | blue;
}


static float keyboard_clamp_float(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;

	if (value > maximum)
		return maximum;

	return value;
}

static uint32_t keyboard_color_with_opacity(uint32_t color, float opacity)
{
	opacity = keyboard_clamp_float(opacity, 0.0f, 1.0f);

	uint32_t original_alpha = (color >> 24) & 0xFF;
	uint32_t alpha = (uint32_t)((float)original_alpha * opacity + 0.5f);

	return (color & 0x00FFFFFF) | (alpha << 24);
}

static bool keyboard_ascii_contains_ignore_case(const char *text, const char *needle)
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

static int keyboard_font_weight_from_style(const char *style, uint32_t flags)
{
	int weight = FW_NORMAL;

	if (style && *style) {
		if (keyboard_ascii_contains_ignore_case(style, "black") ||
		    keyboard_ascii_contains_ignore_case(style, "heavy")) {
			weight = FW_BLACK;
		} else if (keyboard_ascii_contains_ignore_case(style, "extra bold") ||
			   keyboard_ascii_contains_ignore_case(style, "extrabold") ||
			   keyboard_ascii_contains_ignore_case(style, "ultra bold") ||
			   keyboard_ascii_contains_ignore_case(style, "ultrabold")) {
			weight = FW_EXTRABOLD;
		} else if (keyboard_ascii_contains_ignore_case(style, "semi bold") ||
			   keyboard_ascii_contains_ignore_case(style, "semibold") ||
			   keyboard_ascii_contains_ignore_case(style, "demi bold") ||
			   keyboard_ascii_contains_ignore_case(style, "demibold")) {
			weight = FW_SEMIBOLD;
		} else if (keyboard_ascii_contains_ignore_case(style, "bold")) {
			weight = FW_BOLD;
		} else if (keyboard_ascii_contains_ignore_case(style, "medium")) {
			weight = FW_MEDIUM;
		} else if (keyboard_ascii_contains_ignore_case(style, "extra light") ||
			   keyboard_ascii_contains_ignore_case(style, "extralight") ||
			   keyboard_ascii_contains_ignore_case(style, "ultra light") ||
			   keyboard_ascii_contains_ignore_case(style, "ultralight")) {
			weight = FW_EXTRALIGHT;
		} else if (keyboard_ascii_contains_ignore_case(style, "light")) {
			weight = FW_LIGHT;
		} else if (keyboard_ascii_contains_ignore_case(style, "thin")) {
			weight = FW_THIN;
		}
	}

	if ((flags & OBS_FONT_BOLD) && weight < FW_BOLD)
		weight = FW_BOLD;

	return weight;
}

static void keyboard_load_font_settings(struct keyboard_source *context, obs_data_t *settings)
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

	obs_data_t *font = obs_data_get_obj(settings, "keyboard_key_font");

	if (!font)
		return;

	const char *face = obs_data_get_string(font, "face");
	const char *style = obs_data_get_string(font, "style");
	int64_t font_size = obs_data_get_int(font, "size");
	uint32_t flags = (uint32_t)obs_data_get_int(font, "flags");

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

	if (font_size > 0) {
		if (font_size > 512)
			font_size = 512;

		context->key_font_size = (int)font_size;
	}

	context->key_font_weight =
		keyboard_font_weight_from_style(style, flags);
	context->key_font_italic =
		(flags & OBS_FONT_ITALIC) != 0 ||
		keyboard_ascii_contains_ignore_case(style, "italic") ||
		keyboard_ascii_contains_ignore_case(style, "oblique");
	context->key_font_underline = (flags & OBS_FONT_UNDERLINE) != 0;
	context->key_font_strikeout = (flags & OBS_FONT_STRIKEOUT) != 0;

	obs_data_release(font);
}

static const char *keyboard_get_display_label(const struct keyboard_source *context,
					       const struct key_visual *key)
{
	if (!key)
		return "";

	if (!context || !context->distinguish_left_right_modifiers)
		return key->label;

	switch (key->vk) {
	case VK_LSHIFT:
		return "LShift";
	case VK_RSHIFT:
		return "RShift";
	case VK_LCONTROL:
		return "LCtrl";
	case VK_RCONTROL:
		return "RCtrl";
	case VK_LMENU:
		return "LAlt";
	case VK_RMENU:
		return "RAlt";
	default:
		return key->label;
	}
}

static float keyboard_get_scale_from_settings(obs_data_t *settings)
{
	if (!settings)
		return 1.0f;

	double percent = obs_data_get_double(settings, "keyboard_scale_percent");

	if (percent <= 0.0)
		percent = 100.0;

	float scale = (float)(percent / 100.0);

	if (scale < 0.5f)
		scale = 0.5f;

	if (scale > 2.0f)
		scale = 2.0f;

	return scale;
}

static void keyboard_load_visual_settings(struct keyboard_source *context, obs_data_t *settings)
{
	if (!context || !settings) {
		return;
	}

	context->keyboard_scale = keyboard_get_scale_from_settings(settings);

	context->key_gap = keyboard_clamp_float(
		(float)obs_data_get_double(settings, "keyboard_key_gap"),
		MIN_KEY_GAP, MAX_KEY_GAP);

	context->key_background_opacity = keyboard_clamp_float(
		(float)obs_data_get_int(settings, "keyboard_key_background_opacity_percent") / 100.0f,
		0.0f, 1.0f);

	context->key_pressed_opacity = keyboard_clamp_float(
		(float)obs_data_get_int(settings, "keyboard_key_pressed_opacity_percent") / 100.0f,
		0.0f, 1.0f);

	context->key_text_opacity = keyboard_clamp_float(
		(float)obs_data_get_int(settings, "keyboard_key_text_opacity_percent") / 100.0f,
		0.0f, 1.0f);


	context->distinguish_left_right_modifiers =
		obs_data_get_bool(settings, "keyboard_distinguish_left_right_modifiers");

	keyboard_load_font_settings(context, settings);

	context->key_background_color =
		keyboard_obs_rgba_to_argb((uint32_t)obs_data_get_int(settings, "keyboard_key_background_color"));

	context->key_pressed_color =
		keyboard_obs_rgba_to_argb((uint32_t)obs_data_get_int(settings, "keyboard_key_pressed_color"));

	context->key_text_color =
		keyboard_obs_rgba_to_argb((uint32_t)obs_data_get_int(settings, "keyboard_key_text_color"));

	context->key_border_color =
		keyboard_obs_rgba_to_argb((uint32_t)obs_data_get_int(settings, "keyboard_key_border_color"));

	context->key_border_thickness = (float)obs_data_get_double(settings, "keyboard_key_border_thickness");

	if (context->key_border_thickness < 0.0f) {

		context->key_border_thickness = 0.0f;
	}
}

static float keyboard_key_width_for_gap(float units, float gap)
{
	return (units * KEY_UNIT) + ((units - 1.0f) * gap);
}

static float keyboard_key_height_for_gap(float units, float gap)
{
	return (units * KEY_UNIT) + ((units - 1.0f) * gap);
}

/*
 * The layout table uses continuous unit coordinates.
 * Gap changes alter only the layout spacing calculation.  The native
 * Keyboard Size scale remains anchored to DEFAULT_KEY_GAP so changing
 * Key Gap does not also resize the keys.
 */
static float keyboard_base_key_x_for_gap(const struct key_visual *key, float gap)
{
	if (!key)
		return 0.0f;

	return key->x * (KEY_UNIT + gap);
}

static float keyboard_base_key_y_for_gap(const struct key_visual *key, float gap)
{
	if (!key)
		return 0.0f;

	return key->y * (KEY_UNIT + gap);
}

static bool keyboard_get_layout_bounds(
	const bool *visible,
	bool visible_only,
	float gap,
	float *min_x,
	float *min_y,
	float *max_x,
	float *max_y)
{
	if (!min_x || !min_y || !max_x || !max_y)
		return false;

	gap = keyboard_clamp_float(gap, MIN_KEY_GAP, MAX_KEY_GAP);

	bool found = false;

	float result_min_x = 0.0f;
	float result_min_y = 0.0f;
	float result_max_x = 0.0f;
	float result_max_y = 0.0f;

	for (size_t i = 0; i < KEY_COUNT; i++) {
		if (visible_only && visible && !visible[i])
			continue;

		const struct key_visual *key = &keyboard_layout[i];

		float x = keyboard_base_key_x_for_gap(key, gap);
		float y = keyboard_base_key_y_for_gap(key, gap);

		float right =
			x +
			keyboard_key_width_for_gap(key->width, gap);

		float bottom =
			y +
			keyboard_key_height_for_gap(key->height, gap);

		if (!found) {
			result_min_x = x;
			result_min_y = y;
			result_max_x = right;
			result_max_y = bottom;
			found = true;
			continue;
		}

		if (x < result_min_x)
			result_min_x = x;

		if (y < result_min_y)
			result_min_y = y;

		if (right > result_max_x)
			result_max_x = right;

		if (bottom > result_max_y)
			result_max_y = bottom;
	}

	if (!found)
		return false;

	/*
	 * No outer margin: crop exactly to the visible key bounds.
	 */
	*min_x = result_min_x;
	*min_y = result_min_y;
	*max_x = result_max_x;
	*max_y = result_max_y;

	return true;
}

static uint32_t keyboard_get_canvas_width(void)
{
	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info) &&
	    video_info.base_width > 0) {
		return video_info.base_width;
	}

	return KEYBOARD_FALLBACK_CANVAS_WIDTH;
}

/*
 * Snap a non-negative source-space coordinate to the nearest pixel.
 * Key rectangles use snapped left/right/top/bottom EDGES rather than
 * independently truncating x/y/width/height.  This guarantees that two
 * logical edges that are equal (for example when Key Gap = 0) resolve to
 * exactly the same rendered pixel boundary.
 */
static float keyboard_snap_source_pixel(float value)
{
	if (value <= 0.0f)
		return 0.0f;

	return (float)((uint32_t)(value + 0.5f));
}

static void keyboard_update_layout_metrics(
	struct keyboard_source *context)
{
	if (!context)
		return;

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	bool have_full_bounds =
		keyboard_get_layout_bounds(
			NULL,
			false,
			DEFAULT_KEY_GAP,
			&full_min_x,
			&full_min_y,
			&full_max_x,
			&full_max_y);

	if (!have_full_bounds)
		return;

	float full_width =
		full_max_x -
		full_min_x;

	if (full_width <= 0.0f)
		full_width = 1.0f;

	context->canvas_width =
		keyboard_get_canvas_width();

	float base_native_scale =
		(float)context->canvas_width /
		full_width;

	float user_scale =
		context->keyboard_scale;

	if (user_scale <= 0.0f)
		user_scale = 1.0f;

	context->render_scale =
		base_native_scale *
		user_scale;

	float min_x = 0.0f;
	float min_y = 0.0f;
	float max_x = 0.0f;
	float max_y = 0.0f;

	bool have_visible_bounds =
		keyboard_get_layout_bounds(
			context->visible,
			true,
			context->key_gap,
			&min_x,
			&min_y,
			&max_x,
			&max_y);

	if (!have_visible_bounds) {
		context->bounds_min_x = 0.0f;
		context->bounds_min_y = 0.0f;
		context->active_base_width = 0.0f;
		context->active_base_height = 0.0f;
		context->source_width = 1;
		context->source_height = 1;
		return;
	}

	context->bounds_min_x = min_x;
	context->bounds_min_y = min_y;

	context->active_base_width =
		max_x -
		min_x;

	context->active_base_height =
		max_y -
		min_y;

	float source_width =
		context->active_base_width *
		context->render_scale;

	float source_height =
		context->active_base_height *
		context->render_scale;

	if (source_width < 1.0f)
		source_width = 1.0f;

	if (source_height < 1.0f)
		source_height = 1.0f;

	context->source_width =
		(uint32_t)keyboard_snap_source_pixel(source_width);

	context->source_height =
		(uint32_t)keyboard_snap_source_pixel(source_height);

	if (context->source_width < 1)
		context->source_width = 1;

	if (context->source_height < 1)
		context->source_height = 1;
}

static float keyboard_render_key_x(
	const struct keyboard_source *context,
	const struct key_visual *key)
{
	if (!context || !key)
		return 0.0f;

	float logical_left =
		keyboard_base_key_x_for_gap(key, context->key_gap);

	return keyboard_snap_source_pixel(
		(logical_left - context->bounds_min_x) *
		context->render_scale);
}

static float keyboard_render_key_y(
	const struct keyboard_source *context,
	const struct key_visual *key)
{
	if (!context || !key)
		return 0.0f;

	float logical_top =
		keyboard_base_key_y_for_gap(key, context->key_gap);

	return keyboard_snap_source_pixel(
		(logical_top - context->bounds_min_y) *
		context->render_scale);
}

static float keyboard_render_key_width(
	const struct keyboard_source *context,
	const struct key_visual *key)
{
	if (!context || !key)
		return 0.0f;

	float logical_left =
		keyboard_base_key_x_for_gap(key, context->key_gap);

	float logical_right =
		logical_left +
		keyboard_key_width_for_gap(key->width, context->key_gap);

	float left = keyboard_snap_source_pixel(
		(logical_left - context->bounds_min_x) *
		context->render_scale);

	float right = keyboard_snap_source_pixel(
		(logical_right - context->bounds_min_x) *
		context->render_scale);

	float width = right - left;

	return width > 0.0f ? width : 1.0f;
}

static float keyboard_render_key_height(
	const struct keyboard_source *context,
	const struct key_visual *key)
{
	if (!context || !key)
		return 0.0f;

	float logical_top =
		keyboard_base_key_y_for_gap(key, context->key_gap);

	float logical_bottom =
		logical_top +
		keyboard_key_height_for_gap(key->height, context->key_gap);

	float top = keyboard_snap_source_pixel(
		(logical_top - context->bounds_min_y) *
		context->render_scale);

	float bottom = keyboard_snap_source_pixel(
		(logical_bottom - context->bounds_min_y) *
		context->render_scale);

	float height = bottom - top;

	return height > 0.0f ? height : 1.0f;
}

#ifdef _WIN32

#define KEY_EDITOR_WINDOW_CLASS \
	L"PressHUDKeyboardEditor"

#define KEY_EDITOR_BUTTON_BASE 1000
#define KEY_EDITOR_PRESS_ALL_ID 899
#define KEY_EDITOR_RELEASE_ALL_ID 900

#define KEY_EDITOR_PADDING 12
#define KEY_EDITOR_TOP_AREA 48

struct keyboard_editor_state {
	struct keyboard_source *context;

	HWND window;

	HWND buttons[KEY_COUNT];
};

static void keyboard_editor_get_key_text(
	size_t index,
	wchar_t *buffer,
	size_t buffer_count)
{
	if (!buffer ||
	    buffer_count == 0 ||
	    index >= KEY_COUNT) {
		return;
	}

	buffer[0] = L'\0';

	const struct key_visual *key =
		&keyboard_layout[index];

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

	MultiByteToWideChar(
		CP_UTF8,
		0,
		key->label,
		-1,
		buffer,
		(int)buffer_count);
}

static void keyboard_save_visibility(
	struct keyboard_source *context)
{
	if (!context ||
	    !context->source) {
		return;
	}

	obs_data_t *settings =
		obs_source_get_settings(
			context->source);

	if (!settings)
		return;

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		char setting_name[64];

		keyboard_visibility_setting_name(
			i,
			setting_name,
			sizeof(setting_name));

		obs_data_set_bool(
			settings,
			setting_name,
			context->visible[i]);
	}

	obs_source_update(
		context->source,
		settings);

	obs_data_release(settings);
}

static void keyboard_editor_invalidate_all(
	struct keyboard_editor_state *state)
{
	if (!state)
		return;

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		if (state->buttons[i]) {
			InvalidateRect(
				state->buttons[i],
				NULL,
				TRUE);
		}
	}
}

static void keyboard_editor_press_all(
	struct keyboard_editor_state *state)
{
	if (!state ||
	    !state->context) {
		return;
	}

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		state->context->visible[i] =
			true;
	}

	keyboard_save_visibility(
		state->context);

	keyboard_editor_invalidate_all(
		state);
}

static void keyboard_editor_release_all(
	struct keyboard_editor_state *state)
{
	if (!state ||
	    !state->context) {
		return;
	}

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		state->context->visible[i] =
			false;

		state->context->pressed[i] =
			false;
	}

	keyboard_save_visibility(
		state->context);

	keyboard_editor_invalidate_all(
		state);
}

static void keyboard_editor_draw_key_button(
	struct keyboard_editor_state *state,
	DRAWITEMSTRUCT *draw_item)
{
	if (!state ||
	    !state->context ||
	    !draw_item) {
		return;
	}

	int control_id =
		(int)draw_item->CtlID;

	if (control_id < KEY_EDITOR_BUTTON_BASE ||
	    control_id >=
		KEY_EDITOR_BUTTON_BASE +
		(int)KEY_COUNT) {
		return;
	}

	size_t index =
		(size_t)(
			control_id -
			KEY_EDITOR_BUTTON_BASE);

	bool visible =
		state->context->visible[index];

	COLORREF fill_color =
		visible
			? GetSysColor(COLOR_BTNFACE)
			: RGB(48, 48, 48);

	HBRUSH fill_brush =
		CreateSolidBrush(fill_color);

	FillRect(
		draw_item->hDC,
		&draw_item->rcItem,
		fill_brush);

	DeleteObject(fill_brush);

	RECT edge_rect =
		draw_item->rcItem;

	DrawEdge(
		draw_item->hDC,
		&edge_rect,
		(draw_item->itemState & ODS_SELECTED)
			? EDGE_SUNKEN
			: EDGE_RAISED,
		BF_RECT);

	wchar_t label[32];

	keyboard_editor_get_key_text(
		index,
		label,
		sizeof(label) /
			sizeof(label[0]));

	SetBkMode(
		draw_item->hDC,
		TRANSPARENT);

	SetTextColor(
		draw_item->hDC,
		visible
			? GetSysColor(COLOR_BTNTEXT)
			: RGB(205, 205, 205));

	HFONT button_font =
		(HFONT)SendMessageW(
			draw_item->hwndItem,
			WM_GETFONT,
			0,
			0);

	HGDIOBJ old_font =
		NULL;

	if (button_font) {
		old_font =
			SelectObject(
				draw_item->hDC,
				button_font);
	}

	RECT text_rect =
		draw_item->rcItem;

	DrawTextW(
		draw_item->hDC,
		label,
		-1,
		&text_rect,
		DT_CENTER |
			DT_VCENTER |
			DT_SINGLELINE |
			DT_NOPREFIX |
			DT_END_ELLIPSIS);

	if (old_font) {
		SelectObject(
			draw_item->hDC,
			old_font);
	}

	if (draw_item->itemState & ODS_FOCUS) {
		RECT focus_rect =
			draw_item->rcItem;

		InflateRect(
			&focus_rect,
			-3,
			-3);

		DrawFocusRect(
			draw_item->hDC,
			&focus_rect);
	}
}

static LRESULT CALLBACK keyboard_editor_window_proc(
	HWND window,
	UINT message,
	WPARAM w_param,
	LPARAM l_param)
{
	struct keyboard_editor_state *state =
		(struct keyboard_editor_state *)
			GetWindowLongPtrW(
				window,
				GWLP_USERDATA);

	switch (message) {
	case WM_CREATE: {
		CREATESTRUCTW *create =
			(CREATESTRUCTW *)l_param;

		state =
			(struct keyboard_editor_state *)
				create->lpCreateParams;

		SetWindowLongPtrW(
			window,
			GWLP_USERDATA,
			(LONG_PTR)state);

		if (state)
			state->window = window;

		return 0;
	}

	case WM_DRAWITEM:
		if (state) {
			keyboard_editor_draw_key_button(
				state,
				(DRAWITEMSTRUCT *)l_param);

			return TRUE;
		}
		break;

	case WM_COMMAND: {
		if (!state ||
		    !state->context) {
			break;
		}

		int control_id =
			LOWORD(w_param);

		if (control_id ==
			KEY_EDITOR_PRESS_ALL_ID &&
		    HIWORD(w_param) ==
			BN_CLICKED) {
			keyboard_editor_press_all(
				state);

			return 0;
		}

		if (control_id ==
			KEY_EDITOR_RELEASE_ALL_ID &&
		    HIWORD(w_param) ==
			BN_CLICKED) {
			keyboard_editor_release_all(
				state);

			return 0;
		}

		if (control_id <
				KEY_EDITOR_BUTTON_BASE ||
		    control_id >=
				KEY_EDITOR_BUTTON_BASE +
				(int)KEY_COUNT) {
			break;
		}

		if (HIWORD(w_param) !=
		    BN_CLICKED) {
			break;
		}

		size_t index =
			(size_t)(
				control_id -
				KEY_EDITOR_BUTTON_BASE);

		state->context->visible[index] =
			!state->context->visible[index];

		if (!state->context->visible[index]) {
			state->context->pressed[index] =
				false;
		}

		keyboard_save_visibility(
			state->context);

		if (state->buttons[index]) {
			InvalidateRect(
				state->buttons[index],
				NULL,
				TRUE);
		}

		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(window);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		break;
	}

	return DefWindowProcW(
		window,
		message,
		w_param,
		l_param);
}

static bool keyboard_editor_register_class(void)
{
	static ATOM editor_class = 0;

	if (editor_class != 0)
		return true;

	WNDCLASSEXW window_class;

	memset(
		&window_class,
		0,
		sizeof(window_class));

	window_class.cbSize =
		sizeof(WNDCLASSEXW);

	window_class.lpfnWndProc =
		keyboard_editor_window_proc;

	window_class.hInstance =
		GetModuleHandleW(NULL);

	window_class.hCursor =
		LoadCursorW(
			NULL,
			IDC_ARROW);

	window_class.hbrBackground =
		(HBRUSH)(
			COLOR_WINDOW + 1);

	window_class.lpszClassName =
		KEY_EDITOR_WINDOW_CLASS;

	editor_class =
		RegisterClassExW(
			&window_class);

	if (editor_class != 0)
		return true;

	if (GetLastError() ==
	    ERROR_CLASS_ALREADY_EXISTS) {
		return true;
	}

	return false;
}

static void keyboard_editor_get_full_bounds(
	float *min_x,
	float *min_y,
	float *max_x,
	float *max_y)
{
	if (!keyboard_get_layout_bounds(
			NULL,
			false,
			DEFAULT_KEY_GAP,
			min_x,
			min_y,
			max_x,
			max_y)) {
		*min_x = 0.0f;
		*min_y = 0.0f;
		*max_x = 1.0f;
		*max_y = 1.0f;
	}
}

static void keyboard_editor_create_buttons(
	struct keyboard_editor_state *state)
{
	if (!state ||
	    !state->window ||
	    !state->context) {
		return;
	}

	HFONT gui_font =
		(HFONT)GetStockObject(
			DEFAULT_GUI_FONT);

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	keyboard_editor_get_full_bounds(
		&full_min_x,
		&full_min_y,
		&full_max_x,
		&full_max_y);

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		const struct key_visual *key =
			&keyboard_layout[i];

		int x =
			KEY_EDITOR_PADDING +
			(int)(
				keyboard_base_key_x_for_gap(key, DEFAULT_KEY_GAP) -
				full_min_x +
				0.5f);

		int y =
			KEY_EDITOR_TOP_AREA +
			(int)(
				keyboard_base_key_y_for_gap(key, DEFAULT_KEY_GAP) -
				full_min_y +
				0.5f);

		int width =
			(int)(
				keyboard_key_width_for_gap(
					key->width, DEFAULT_KEY_GAP) +
				0.5f);

		int height =
			(int)(
				keyboard_key_height_for_gap(
					key->height, DEFAULT_KEY_GAP) +
				0.5f);

		wchar_t label[32];

		keyboard_editor_get_key_text(
			i,
			label,
			sizeof(label) /
				sizeof(label[0]));

		HWND button =
			CreateWindowExW(
				0,
				L"BUTTON",
				label,
				WS_CHILD |
					WS_VISIBLE |
					WS_TABSTOP |
					BS_OWNERDRAW,
				x,
				y,
				width,
				height,
				state->window,
				(HMENU)(INT_PTR)(
					KEY_EDITOR_BUTTON_BASE +
					(int)i),
				GetModuleHandleW(NULL),
				NULL);

		state->buttons[i] =
			button;

		if (!button)
			continue;

		SendMessageW(
			button,
			WM_SETFONT,
			(WPARAM)gui_font,
			TRUE);
	}
}

static void keyboard_show_layout_editor(
	struct keyboard_source *context)
{
	if (!context)
		return;

	if (!keyboard_editor_register_class()) {
		MessageBoxW(
			NULL,
			L"Could not create keyboard editor window.",
			L"PressHUD",
			MB_OK |
				MB_ICONERROR);

		return;
	}

	struct keyboard_editor_state state;

	memset(
		&state,
		0,
		sizeof(state));

	state.context =
		context;

	float full_min_x = 0.0f;
	float full_min_y = 0.0f;
	float full_max_x = 0.0f;
	float full_max_y = 0.0f;

	keyboard_editor_get_full_bounds(
		&full_min_x,
		&full_min_y,
		&full_max_x,
		&full_max_y);

	int client_width =
		(int)(
			full_max_x -
			full_min_x +
			0.5f) +
		(KEY_EDITOR_PADDING * 2);

	int client_height =
		(int)(
			full_max_y -
			full_min_y +
			0.5f) +
		KEY_EDITOR_TOP_AREA +
		KEY_EDITOR_PADDING;

	RECT window_rect = {
		0,
		0,
		client_width,
		client_height,
	};

	DWORD window_style =
		WS_OVERLAPPED |
		WS_CAPTION |
		WS_SYSMENU |
		WS_MINIMIZEBOX;

	AdjustWindowRect(
		&window_rect,
		window_style,
		FALSE);

	HWND window =
		CreateWindowExW(
			WS_EX_CONTROLPARENT,
			KEY_EDITOR_WINDOW_CLASS,
			L"PressHUD Keyboard Layout Editor",
			window_style,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			window_rect.right -
				window_rect.left,
			window_rect.bottom -
				window_rect.top,
			NULL,
			NULL,
			GetModuleHandleW(NULL),
			&state);

	if (!window)
		return;

	HFONT gui_font =
		(HFONT)GetStockObject(
			DEFAULT_GUI_FONT);

	HWND description =
		CreateWindowExW(
			0,
			L"STATIC",
			L"Light = Visible    Dark = Hidden",
			WS_CHILD |
				WS_VISIBLE,
			KEY_EDITOR_PADDING,
			14,
			360,
			22,
			window,
			NULL,
			GetModuleHandleW(NULL),
			NULL);

	if (description) {
		SendMessageW(
			description,
			WM_SETFONT,
			(WPARAM)gui_font,
			TRUE);
	}

	int action_button_width = 120;
	int action_button_gap = 8;

	int release_x =
		client_width -
		KEY_EDITOR_PADDING -
		action_button_width;

	int press_x =
		release_x -
		action_button_gap -
		action_button_width;

	HWND press_button =
		CreateWindowExW(
			0,
			L"BUTTON",
			L"Press All Keys",
			WS_CHILD |
				WS_VISIBLE |
				WS_TABSTOP |
				BS_PUSHBUTTON,
			press_x,
			9,
			action_button_width,
			28,
			window,
			(HMENU)(INT_PTR)
				KEY_EDITOR_PRESS_ALL_ID,
			GetModuleHandleW(NULL),
			NULL);

	if (press_button) {
		SendMessageW(
			press_button,
			WM_SETFONT,
			(WPARAM)gui_font,
			TRUE);
	}

	HWND release_button =
		CreateWindowExW(
			0,
			L"BUTTON",
			L"Release All Keys",
			WS_CHILD |
				WS_VISIBLE |
				WS_TABSTOP |
				BS_PUSHBUTTON,
			release_x,
			9,
			action_button_width,
			28,
			window,
			(HMENU)(INT_PTR)
				KEY_EDITOR_RELEASE_ALL_ID,
			GetModuleHandleW(NULL),
			NULL);

	if (release_button) {
		SendMessageW(
			release_button,
			WM_SETFONT,
			(WPARAM)gui_font,
			TRUE);
	}

	keyboard_editor_create_buttons(
		&state);

	ShowWindow(
		window,
		SW_SHOW);

	UpdateWindow(window);

	MSG message;

	while (GetMessageW(
			&message,
			NULL,
			0,
			0) > 0) {
		if (!IsDialogMessageW(
				window,
				&message)) {
			TranslateMessage(
				&message);

			DispatchMessageW(
				&message);
		}
	}
}

static bool keyboard_layout_editor_clicked(
	obs_properties_t *properties,
	obs_property_t *property,
	void *data)
{
	UNUSED_PARAMETER(properties);
	UNUSED_PARAMETER(property);

	struct keyboard_source *context =
		data;

	if (!context)
		return false;

	keyboard_show_layout_editor(
		context);

	return false;
}

static HFONT create_keyboard_font(const struct keyboard_source *context, int size)
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

	return CreateFontW(
		-size,
		0,
		0,
		0,
		weight,
		italic,
		underline,
		strikeout,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		face);
}

static bool keyboard_font_fits_text(
	HDC hdc,
	HFONT font,
	const wchar_t *text,
	int available_width,
	int available_height)
{
	if (!hdc || !font || !text || !*text ||
	    available_width <= 0 || available_height <= 0) {
		return false;
	}

	HGDIOBJ old_font =
		SelectObject(
			hdc,
			font);

	SIZE text_size = {0, 0};
	int text_length = (int)wcslen(text);

	BOOL measured =
		GetTextExtentPoint32W(
			hdc,
			text,
			text_length,
			&text_size);

	if (old_font)
		SelectObject(hdc, old_font);

	if (!measured)
		return false;

	return text_size.cx <= available_width &&
	       text_size.cy <= available_height;
}

static int keyboard_find_largest_fitting_font_size(
	const struct keyboard_source *context,
	HDC hdc,
	const wchar_t *text,
	int requested_logical_size,
	float raster_scale,
	int available_width,
	int available_height)
{
	if (!context || !hdc || !text || !*text)
		return requested_logical_size;

	if (requested_logical_size < 1)
		requested_logical_size = 1;

	int low = 1;
	int high = requested_logical_size;
	int best = 1;

	while (low <= high) {
		int candidate = low + ((high - low) / 2);

		int raster_size =
			(int)(
				(float)candidate *
				raster_scale +
				0.5f);

		if (raster_size < 1)
			raster_size = 1;

		HFONT candidate_font =
			create_keyboard_font(
				context,
				raster_size);

		if (!candidate_font) {
			high = candidate - 1;
			continue;
		}

		bool fits =
			keyboard_font_fits_text(
				hdc,
				candidate_font,
				text,
				available_width,
				available_height);

		DeleteObject(candidate_font);

		if (fits) {
			best = candidate;
			low = candidate + 1;
		} else {
			high = candidate - 1;
		}
	}

	return best;
}

static void keyboard_get_overlay_key_text(
	const struct keyboard_source *context,
	size_t index,
	wchar_t *buffer,
	size_t buffer_count)
{
	if (!buffer ||
	    buffer_count == 0 ||
	    index >= KEY_COUNT) {
		return;
	}

	buffer[0] = L'\0';

	const struct key_visual *key =
		&keyboard_layout[index];

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

	const char *label =
		keyboard_get_display_label(context, key);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		label,
		-1,
		buffer,
		(int)buffer_count);
}

static gs_texture_t *create_label_texture(
	const struct keyboard_source *context)
{
	if (!context)
		return NULL;

	uint32_t texture_width =
		context->source_width;

	uint32_t texture_height =
		context->source_height;

	if (texture_width == 0)
		texture_width = 1;

	if (texture_height == 0)
		texture_height = 1;

	HDC hdc =
		CreateCompatibleDC(NULL);

	if (!hdc) {
		obs_log(
			LOG_ERROR,
			"Could not create GDI device context");

		return NULL;
	}

	BITMAPINFO bitmap_info;

	memset(
		&bitmap_info,
		0,
		sizeof(bitmap_info));

	bitmap_info.bmiHeader.biSize =
		sizeof(BITMAPINFOHEADER);

	bitmap_info.bmiHeader.biWidth =
		(LONG)texture_width;

	bitmap_info.bmiHeader.biHeight =
		-(LONG)texture_height;

	bitmap_info.bmiHeader.biPlanes =
		1;

	bitmap_info.bmiHeader.biBitCount =
		32;

	bitmap_info.bmiHeader.biCompression =
		BI_RGB;

	void *bitmap_bits =
		NULL;

	HBITMAP bitmap =
		CreateDIBSection(
			hdc,
			&bitmap_info,
			DIB_RGB_COLORS,
			&bitmap_bits,
			NULL,
			0);

	if (!bitmap ||
	    !bitmap_bits) {
		obs_log(
			LOG_ERROR,
			"Could not create keyboard label bitmap");

		if (bitmap)
			DeleteObject(bitmap);

		DeleteDC(hdc);

		return NULL;
	}

	HGDIOBJ old_bitmap =
		SelectObject(
			hdc,
			bitmap);

	size_t pixel_bytes =
		(size_t)texture_width *
		(size_t)texture_height *
		4;

	memset(
		bitmap_bits,
		0,
		pixel_bytes);

	SetBkMode(
		hdc,
		TRANSPARENT);

	SetTextColor(
		hdc,
		RGB(
			255,
			255,
			255));

	float raster_scale =
		context->render_scale;

	if (raster_scale <= 0.0f)
		raster_scale = 1.0f;

	int logical_font_size = context->key_font_size;

	if (logical_font_size < 1)
		logical_font_size = 17;

	/*
	 * Every key starts from exactly the size selected in
	 * Key Font & Text Size. Individual keys are reduced only when
	 * their actual rendered text would exceed that key's safe area.
	 */
	int requested_raster_size =
		(int)(
			(float)logical_font_size *
			raster_scale +
			0.5f);

	if (requested_raster_size < 1)
		requested_raster_size = 1;

	HFONT requested_font =
		create_keyboard_font(
			context,
			requested_raster_size);

	HGDIOBJ original_font =
		GetCurrentObject(
			hdc,
			OBJ_FONT);

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		if (!context->visible[i])
			continue;

		const struct key_visual *key =
			&keyboard_layout[i];

		float x =
			keyboard_render_key_x(
				context,
				key);

		float y =
			keyboard_render_key_y(
				context,
				key);

		float width =
			keyboard_render_key_width(
				context,
				key);

		float height =
			keyboard_render_key_height(
				context,
				key);

		float horizontal_inset =
			KEY_TEXT_HORIZONTAL_PADDING *
			context->render_scale;

		float vertical_inset =
			KEY_TEXT_VERTICAL_PADDING *
			context->render_scale;

		if (horizontal_inset < 2.0f)
			horizontal_inset = 2.0f;

		if (vertical_inset < 2.0f)
			vertical_inset = 2.0f;

		RECT rect;

		rect.left =
			(LONG)(
				x +
				horizontal_inset +
				0.5f);

		rect.top =
			(LONG)(
				y +
				vertical_inset +
				0.5f);

		rect.right =
			(LONG)(
				x +
				width -
				horizontal_inset +
				0.5f);

		rect.bottom =
			(LONG)(
				y +
				height -
				vertical_inset +
				0.5f);

		wchar_t wide_label[32];

		keyboard_get_overlay_key_text(
			context,
			i,
			wide_label,
			sizeof(wide_label) /
				sizeof(wide_label[0]));

		int available_width =
			(int)(rect.right - rect.left);

		int available_height =
			(int)(rect.bottom - rect.top);

		HFONT selected_font = requested_font;
		HFONT fitted_font = NULL;

		if (requested_font &&
		    !keyboard_font_fits_text(
			hdc,
			requested_font,
			wide_label,
			available_width,
			available_height)) {
			int fitted_logical_size =
				keyboard_find_largest_fitting_font_size(
					context,
					hdc,
					wide_label,
					logical_font_size,
					raster_scale,
					available_width,
					available_height);

			int fitted_raster_size =
				(int)(
					(float)fitted_logical_size *
					raster_scale +
					0.5f);

			if (fitted_raster_size < 1)
				fitted_raster_size = 1;

			fitted_font =
				create_keyboard_font(
					context,
					fitted_raster_size);

			if (fitted_font)
				selected_font = fitted_font;
		}

		if (selected_font) {
			SelectObject(
				hdc,
				selected_font);
		}

		DrawTextW(
			hdc,
			wide_label,
			-1,
			&rect,
			DT_CENTER |
				DT_VCENTER |
				DT_SINGLELINE |
				DT_NOPREFIX);

		if (fitted_font) {
			if (requested_font)
				SelectObject(hdc, requested_font);
			else if (original_font)
				SelectObject(hdc, original_font);

			DeleteObject(fitted_font);
		}
	}

	SelectObject(
		hdc,
		original_font);

	GdiFlush();

	uint8_t *pixels =
		(uint8_t *)bitmap_bits;

	size_t pixel_count =
		(size_t)texture_width *
		(size_t)texture_height;

	uint32_t text_color =
		context->key_text_color;

	uint8_t text_alpha =
		(uint8_t)(
			(float)((text_color >> 24) & 0xFF) *
			context->key_text_opacity +
			0.5f);

	uint8_t text_red =
		(uint8_t)(
			(text_color >> 16) &
			0xFF);

	uint8_t text_green =
		(uint8_t)(
			(text_color >> 8) &
			0xFF);

	uint8_t text_blue =
		(uint8_t)(
			text_color &
			0xFF);

	for (size_t i = 0;
	     i < pixel_count;
	     i++) {
		uint8_t *pixel =
			pixels +
			(i * 4);

		uint8_t blue =
			pixel[0];

		uint8_t green =
			pixel[1];

		uint8_t red =
			pixel[2];

		uint8_t alpha =
			red;

		if (green > alpha)
			alpha = green;

		if (blue > alpha)
			alpha = blue;

		pixel[0] =
			text_blue;

		pixel[1] =
			text_green;

		pixel[2] =
			text_red;

		pixel[3] =
			(uint8_t)(
				((uint32_t)alpha *
				 (uint32_t)text_alpha) /
				255);
	}

	const uint8_t *texture_data =
		(const uint8_t *)pixels;

	gs_texture_t *texture =
		NULL;

	obs_enter_graphics();

	texture =
		gs_texture_create(
			texture_width,
			texture_height,
			GS_BGRA,
			1,
			&texture_data,
			0);

	obs_leave_graphics();

	if (requested_font)
		DeleteObject(requested_font);

	SelectObject(
		hdc,
		old_bitmap);

	DeleteObject(bitmap);
	DeleteDC(hdc);

	if (!texture) {
		obs_log(
			LOG_ERROR,
			"Could not create keyboard label texture");
	}

	return texture;
}

static void keyboard_rebuild_label_texture(
	struct keyboard_source *context)
{
	if (!context)
		return;

	gs_texture_t *new_texture =
		create_label_texture(
			context);

	if (!new_texture)
		return;

	gs_texture_t *old_texture =
		context->label_texture;

	context->label_texture =
		new_texture;

	if (old_texture) {
		obs_enter_graphics();

		gs_texture_destroy(
			old_texture);

		obs_leave_graphics();
	}
}

#endif

static void keyboard_source_get_defaults(obs_data_t *settings)
{
	if (!settings)
		return;

	obs_data_set_default_double(settings, "keyboard_scale_percent", 100.0);

	obs_data_set_default_double(settings, "keyboard_key_gap", DEFAULT_KEY_GAP);

	obs_data_set_default_int(settings, "keyboard_key_background_opacity_percent", 100);

	obs_data_set_default_int(settings, "keyboard_key_pressed_opacity_percent", 100);

	obs_data_set_default_int(settings, "keyboard_key_text_opacity_percent", 100);


	obs_data_set_default_bool(settings, "keyboard_distinguish_left_right_modifiers", false);

	obs_data_t *key_font = obs_data_create();
	obs_data_set_string(key_font, "face", "Segoe UI");
	obs_data_set_string(key_font, "style", "Semibold");
	obs_data_set_int(key_font, "size", 17);
	obs_data_set_int(key_font, "flags", 0);
	obs_data_set_default_obj(settings, "keyboard_key_font", key_font);
	obs_data_release(key_font);

	obs_data_set_default_int(settings, "keyboard_key_background_color", 0xFF242424);

	obs_data_set_default_int(settings, "keyboard_key_pressed_color", 0xFF787878);

	obs_data_set_default_int(settings, "keyboard_key_text_color", 0xFFFFFFFF);

	obs_data_set_default_int(settings, "keyboard_key_border_color", 0xFF808080);

	obs_data_set_default_double(settings, "keyboard_key_border_thickness", 0.0);

	for (size_t i = 0; i < KEY_COUNT; i++) {

		char setting_name[64];

		keyboard_visibility_setting_name(i, setting_name, sizeof(setting_name));

		obs_data_set_default_bool(settings, setting_name, true);
	}
}

static const char *keyboard_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);

	return "PressHUD Keyboard";
}

static obs_properties_t *keyboard_source_get_properties(void *data)
{
	obs_properties_t *properties = obs_properties_create();

#ifdef _WIN32
	obs_properties_add_button2(properties, "keyboard_layout_editor", "Keyboard Layout Editor...",
				   keyboard_layout_editor_clicked, data);


	obs_properties_add_float_slider(properties, "keyboard_scale_percent", "Keyboard Size (%)", 50.0, 200.0, 5.0);

	obs_properties_add_float_slider(properties, "keyboard_key_gap", "Key Gap (px)",
					MIN_KEY_GAP, MAX_KEY_GAP, 0.5);

	obs_properties_add_int_slider(properties, "keyboard_key_background_opacity_percent",
				      "Key Background Opacity (%)", 0, 100, 1);

	obs_properties_add_int_slider(properties, "keyboard_key_pressed_opacity_percent",
				      "Key Pressed Opacity (%)", 0, 100, 1);

	obs_properties_add_int_slider(properties, "keyboard_key_text_opacity_percent",
				      "Key Text Opacity (%)", 0, 100, 1);

	obs_property_t *key_font =
		obs_properties_add_font(properties, "keyboard_key_font", "Key Font & Text Size");

	if (key_font) {
		obs_property_set_long_description(
			key_font,
			"Select the key text font and set its size in the font dialog. Size 17 matches the previous default. All keys use the selected size unless a label would exceed its key, in which case only that key is reduced to the largest size that fits.");
	}

	obs_properties_add_color_alpha(properties, "keyboard_key_background_color", "Key Background Color");

	obs_properties_add_color_alpha(properties, "keyboard_key_pressed_color", "Key Pressed Color");

	obs_properties_add_color_alpha(properties, "keyboard_key_text_color", "Key Text Color");

	obs_properties_add_color_alpha(properties, "keyboard_key_border_color", "Key Border Color");

	obs_properties_add_float_slider(properties, "keyboard_key_border_thickness", "Key Border Thickness", 0.0, 6.0,
					0.5);

	obs_properties_add_bool(properties, "keyboard_distinguish_left_right_modifiers",
				"Distinguish Left / Right Ctrl, Shift, Alt");
#else
	UNUSED_PARAMETER(data);
#endif

	return properties;
}

static void keyboard_source_update(void *data, obs_data_t *settings)
{
	struct keyboard_source *context = data;

	if (!context || !settings) {
		return;
	}

	keyboard_load_visibility(context, settings);

	keyboard_load_visual_settings(context, settings);

	keyboard_update_layout_metrics(context);

#ifdef _WIN32
	keyboard_rebuild_label_texture(context);
#endif
}

static void *keyboard_source_create(obs_data_t *settings, obs_source_t *source)
{	

	struct keyboard_source *context = bzalloc(sizeof(struct keyboard_source));

	if (!context)
		return NULL;

	context->source = source;

	keyboard_load_visibility(context, settings);

	keyboard_load_visual_settings(context, settings);

	keyboard_update_layout_metrics(context);

#ifdef _WIN32
	if (!input_service_start()) {
		obs_log(LOG_ERROR, "Could not start keyboard input service");
	}

	context->label_texture = create_label_texture(context);
#endif

	obs_log(LOG_INFO, "Keyboard source created. Key count: %zu", KEY_COUNT);

	return context;
}

static void keyboard_source_destroy(void *data)
{
	struct keyboard_source *context = data;

	if (!context)
		return;

	if (context->label_texture) {
		obs_enter_graphics();

		gs_texture_destroy(context->label_texture);

		obs_leave_graphics();

		context->label_texture = NULL;
	}

	obs_log(LOG_INFO, "Keyboard source destroyed");

	bfree(context);
}

static uint32_t keyboard_source_get_width(void *data)
{
	struct keyboard_source *context =
		data;

	if (!context ||
	    context->source_width == 0) {
		return
			KEYBOARD_FALLBACK_CANVAS_WIDTH;
	}

	return context->source_width;
}

static uint32_t keyboard_source_get_height(void *data)
{
	struct keyboard_source *context =
		data;

	if (!context ||
	    context->source_height == 0) {
		return
			KEYBOARD_FALLBACK_CANVAS_HEIGHT;
	}

	return context->source_height;
}

static void keyboard_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct keyboard_source *context = data;

	if (!context)
		return;

	for (size_t i = 0; i < KEY_COUNT; i++) {

		if (!context->visible[i]) {
			context->pressed[i] = false;

			continue;
		}

		context->pressed[i] = input_service_is_key_pressed(keyboard_layout[i].vk);
	}
}

static void draw_keyboard_rect(float x, float y, float width, float height, uint32_t color_value,
			       gs_effect_t *solid_effect, gs_eparam_t *color)
{
	if (!solid_effect || !color || width <= 0.0f || height <= 0.0f) {
		return;
	}

	gs_effect_set_color(color, color_value);

	gs_matrix_push();

	gs_matrix_translate3f(x, y, 0.0f);

	while (gs_effect_loop(solid_effect, "Solid")) {

		gs_draw_sprite(NULL, 0, (uint32_t)width, (uint32_t)height);
	}

	gs_matrix_pop();
}

static void draw_key_box(
	const struct keyboard_source *context,
	const struct key_visual *key,
	bool pressed,
	gs_effect_t *solid_effect,
	gs_eparam_t *color)
{
	if (!context ||
	    !key) {
		return;
	}

	float x =
		keyboard_render_key_x(
			context,
			key);

	float y =
		keyboard_render_key_y(
			context,
			key);

	float width =
		keyboard_render_key_width(
			context,
			key);

	float height =
		keyboard_render_key_height(
			context,
			key);

	uint32_t fill_color =
		pressed
			? keyboard_color_with_opacity(
				context->key_pressed_color,
				context->key_pressed_opacity)
			: keyboard_color_with_opacity(
				context->key_background_color,
				context->key_background_opacity);

	draw_keyboard_rect(
		x,
		y,
		width,
		height,
		fill_color,
		solid_effect,
		color);

	float thickness =
		context->key_border_thickness *
		context->render_scale;

	if (thickness <= 0.0f)
		return;

	if ((thickness * 2.0f) >
	    width) {
		thickness =
			width / 2.0f;
	}

	if ((thickness * 2.0f) >
	    height) {
		thickness =
			height / 2.0f;
	}

	uint32_t border_color =
		context->key_border_color;

	/* Top */
	draw_keyboard_rect(
		x,
		y,
		width,
		thickness,
		border_color,
		solid_effect,
		color);

	/* Bottom */
	draw_keyboard_rect(
		x,
		y +
			height -
			thickness,
		width,
		thickness,
		border_color,
		solid_effect,
		color);

	/* Left */
	draw_keyboard_rect(
		x,
		y +
			thickness,
		thickness,
		height -
			(thickness * 2.0f),
		border_color,
		solid_effect,
		color);

	/* Right */
	draw_keyboard_rect(
		x +
			width -
			thickness,
		y +
			thickness,
		thickness,
		height -
			(thickness * 2.0f),
		border_color,
		solid_effect,
		color);
}

static void draw_label_texture(
	const struct keyboard_source *context)
{
	if (!context ||
	    !context->label_texture) {
		return;
	}

	gs_effect_t *effect =
		obs_get_base_effect(
			OBS_EFFECT_DEFAULT);

	if (!effect)
		return;

	gs_eparam_t *image =
		gs_effect_get_param_by_name(
			effect,
			"image");

	if (!image)
		return;

	gs_effect_set_texture(
		image,
		context->label_texture);

	while (gs_effect_loop(
		       effect,
		       "Draw")) {
		gs_draw_sprite(
			context->label_texture,
			0,
			context->source_width,
			context->source_height);
	}
}

static void keyboard_source_video_render(
	void *data,
	gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct keyboard_source *context =
		data;

	if (!context)
		return;

	gs_effect_t *solid_effect =
		obs_get_base_effect(
			OBS_EFFECT_SOLID);

	if (!solid_effect)
		return;

	gs_eparam_t *color =
		gs_effect_get_param_by_name(
			solid_effect,
			"color");

	if (!color)
		return;

	for (size_t i = 0;
	     i < KEY_COUNT;
	     i++) {
		if (!context->visible[i])
			continue;

		draw_key_box(
			context,
			&keyboard_layout[i],
			context->pressed[i],
			solid_effect,
			color);
	}

	draw_label_texture(context);
}

static struct obs_source_info keyboard_source_info = {
	.id = KEYBOARD_SOURCE_ID,

	.type = OBS_SOURCE_TYPE_INPUT,

	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,

	.get_name = keyboard_source_get_name,

	.create = keyboard_source_create,

	.destroy = keyboard_source_destroy,

	.update = keyboard_source_update,

	.get_defaults = keyboard_source_get_defaults,

	.get_properties = keyboard_source_get_properties,

	.get_width = keyboard_source_get_width,

	.get_height = keyboard_source_get_height,

	.video_tick = keyboard_source_video_tick,

	.video_render = keyboard_source_video_render,
};

bool obs_module_load(void)
{
	obs_register_source(&keyboard_source_info);

	cursor_source_register();

	history_source_register();

	obs_log(LOG_INFO,
		"PressHUD loaded successfully "
		"(version %s)",
		PLUGIN_VERSION);

	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
	input_service_stop();
#endif

	obs_log(LOG_INFO, "PressHUD unloaded");
}