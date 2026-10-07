#include "input-service.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#define STANDARD_KEY_COUNT 256
#define NUMPAD_ENTER_INDEX 256
#define KEY_STATE_COUNT 257

#define MOUSE_BUTTON_COUNT 2

static volatile LONG key_states[KEY_STATE_COUNT];

static volatile LONG mouse_states[MOUSE_BUTTON_COUNT];

/* --------------------------------------------------------- */
/* Keyboard event history                                    */
/* --------------------------------------------------------- */

static struct input_key_event event_buffer[INPUT_EVENT_BUFFER_SIZE];

static uint64_t latest_event_sequence = 0;

static SRWLOCK event_lock = SRWLOCK_INIT;

/* --------------------------------------------------------- */
/* Mouse event history                                       */
/* --------------------------------------------------------- */

static struct input_mouse_event mouse_event_buffer[INPUT_EVENT_BUFFER_SIZE];

static uint64_t latest_mouse_event_sequence = 0;

static SRWLOCK mouse_event_lock = SRWLOCK_INIT;

/* --------------------------------------------------------- */
/* Hook thread                                               */
/* --------------------------------------------------------- */

static HANDLE hook_thread = NULL;
static DWORD hook_thread_id = 0;

static HANDLE hook_ready_event = NULL;

static HHOOK keyboard_hook = NULL;
static HHOOK mouse_hook = NULL;

static volatile LONG hook_started = 0;

/* --------------------------------------------------------- */
/* Key state helpers                                         */
/* --------------------------------------------------------- */

static int key_code_to_index(int key_code)
{
	if (key_code >= 0 && key_code < STANDARD_KEY_COUNT) {
		return key_code;
	}

	if (key_code == KEY_INPUT_NUMPAD_ENTER) {
		return NUMPAD_ENTER_INDEX;
	}

	return -1;
}

static void clear_all_key_states(void)
{
	for (int i = 0; i < KEY_STATE_COUNT; i++) {

		InterlockedExchange(&key_states[i], 0);
	}
}

static void clear_all_mouse_states(void)
{
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++) {

		InterlockedExchange(&mouse_states[i], 0);
	}
}

/* --------------------------------------------------------- */
/* Keyboard event buffer                                     */
/* --------------------------------------------------------- */

static void clear_event_history(void)
{
	AcquireSRWLockExclusive(&event_lock);

	latest_event_sequence = 0;

	for (size_t i = 0; i < INPUT_EVENT_BUFFER_SIZE; i++) {

		event_buffer[i].key_code = 0;
		event_buffer[i].pressed = false;
		event_buffer[i].timestamp_ms = 0;
		event_buffer[i].sequence = 0;
	}

	ReleaseSRWLockExclusive(&event_lock);
}

static void record_key_event(int key_code, bool pressed, uint64_t timestamp_ms)
{
	AcquireSRWLockExclusive(&event_lock);

	latest_event_sequence++;

	uint64_t sequence = latest_event_sequence;

	size_t index = (size_t)((sequence - 1) % INPUT_EVENT_BUFFER_SIZE);

	event_buffer[index].key_code = key_code;

	event_buffer[index].pressed = pressed;

	event_buffer[index].timestamp_ms = timestamp_ms;

	event_buffer[index].sequence = sequence;

	ReleaseSRWLockExclusive(&event_lock);
}

/* --------------------------------------------------------- */
/* Mouse event buffer                                        */
/* --------------------------------------------------------- */

static void clear_mouse_event_history(void)
{
	AcquireSRWLockExclusive(&mouse_event_lock);

	latest_mouse_event_sequence = 0;

	for (size_t i = 0; i < INPUT_EVENT_BUFFER_SIZE; i++) {

		mouse_event_buffer[i].mouse_button = 0;
		mouse_event_buffer[i].pressed = false;
		mouse_event_buffer[i].timestamp_ms = 0;
		mouse_event_buffer[i].sequence = 0;
	}

	ReleaseSRWLockExclusive(&mouse_event_lock);
}

static void record_mouse_event(int mouse_button, bool pressed, uint64_t timestamp_ms)
{
	AcquireSRWLockExclusive(&mouse_event_lock);

	latest_mouse_event_sequence++;

	uint64_t sequence = latest_mouse_event_sequence;

	size_t index = (size_t)((sequence - 1) % INPUT_EVENT_BUFFER_SIZE);

	mouse_event_buffer[index].mouse_button = mouse_button;

	mouse_event_buffer[index].pressed = pressed;

	mouse_event_buffer[index].timestamp_ms = timestamp_ms;

	mouse_event_buffer[index].sequence = sequence;

	ReleaseSRWLockExclusive(&mouse_event_lock);
}

/* --------------------------------------------------------- */
/* State changes                                             */
/* --------------------------------------------------------- */

static void set_key_state(int key_code, bool pressed, uint64_t timestamp_ms)
{
	int index = key_code_to_index(key_code);

	if (index < 0)
		return;

	LONG new_value = pressed ? 1 : 0;

	LONG old_value = InterlockedExchange(&key_states[index], new_value);

	bool old_pressed = old_value != 0;

	/*
	 * Ignore Windows key repeat.
	 */
	if (old_pressed == pressed)
		return;

	record_key_event(key_code, pressed, timestamp_ms);
}

static void set_mouse_state(int mouse_button, bool pressed, uint64_t timestamp_ms)
{
	if (mouse_button < 0 || mouse_button >= MOUSE_BUTTON_COUNT) {
		return;
	}

	LONG new_value = pressed ? 1 : 0;

	LONG old_value = InterlockedExchange(&mouse_states[mouse_button], new_value);

	bool old_pressed = old_value != 0;

	if (old_pressed == pressed)
		return;

	record_mouse_event(mouse_button, pressed, timestamp_ms);
}

/* --------------------------------------------------------- */
/* Keyboard normalization                                    */
/* --------------------------------------------------------- */

static int normalize_key(const KBDLLHOOKSTRUCT *keyboard)
{
	if (!keyboard)
		return -1;

	DWORD scan_code = keyboard->scanCode & 0xFF;

	bool extended = (keyboard->flags & LLKHF_EXTENDED) != 0;

	if (keyboard->vkCode == VK_PAUSE) {
		return VK_PAUSE;
	}

	if (keyboard->vkCode == VK_SNAPSHOT) {
		return VK_SNAPSHOT;
	}

	/*
	 * Prefer an explicit left/right modifier VK when the keyboard
	 * driver already provides one.  Some devices do this even when
	 * their scan-code flags differ from the traditional PC keyboard.
	 */
	if (keyboard->vkCode == VK_LSHIFT)
		return VK_LSHIFT;

	if (keyboard->vkCode == VK_RSHIFT)
		return VK_RSHIFT;

	if (keyboard->vkCode == VK_LCONTROL)
		return VK_LCONTROL;

	if (keyboard->vkCode == VK_RCONTROL)
		return VK_RCONTROL;

	if (keyboard->vkCode == VK_LMENU)
		return VK_LMENU;

	if (keyboard->vkCode == VK_RMENU)
		return VK_RMENU;

	/*
	 * VK_SHIFT is commonly reported for both physical Shift keys.
	 * MAPVK_VSC_TO_VK_EX resolves the scan code to VK_LSHIFT or
	 * VK_RSHIFT and is more tolerant of keyboard/driver variations.
	 */
	if (keyboard->vkCode == VK_SHIFT) {
		UINT mapped = MapVirtualKeyW(keyboard->scanCode, MAPVK_VSC_TO_VK_EX);

		if (mapped == VK_LSHIFT || mapped == VK_RSHIFT)
			return (int)mapped;

		if (scan_code == 0x2A)
			return VK_LSHIFT;

		if (scan_code == 0x36)
			return VK_RSHIFT;
	}

	/*
	 * Main Enter / Numpad Enter
	 */
	if (scan_code == 0x1C) {
		if (extended)
			return KEY_INPUT_NUMPAD_ENTER;

		return VK_RETURN;
	}

	/*
	 * Left / Right Ctrl
	 */
	if (scan_code == 0x1D) {
		if (extended)
			return VK_RCONTROL;

		return VK_LCONTROL;
	}

	/*
	 * Left Shift
	 */
	if (scan_code == 0x2A) {
		if (extended)
			return -1;

		return VK_LSHIFT;
	}

	/*
	 * Right Shift
	 */
	if (scan_code == 0x36) {
		if (extended)
			return -1;

		return VK_RSHIFT;
	}

	/*
	 * Left / Right Alt
	 */
	if (scan_code == 0x38) {
		if (extended)
			return VK_RMENU;

		return VK_LMENU;
	}

	if (scan_code == 0x35 && extended) {
		return VK_DIVIDE;
	}

	if (scan_code == 0x37 && !extended) {
		return VK_MULTIPLY;
	}

	/*
	 * Navigation / Numpad
	 */
	if (scan_code == 0x47) {
		return extended ? VK_HOME : VK_NUMPAD7;
	}

	if (scan_code == 0x48) {
		return extended ? VK_UP : VK_NUMPAD8;
	}

	if (scan_code == 0x49) {
		return extended ? VK_PRIOR : VK_NUMPAD9;
	}

	if (scan_code == 0x4A && !extended) {
		return VK_SUBTRACT;
	}

	if (scan_code == 0x4B) {
		return extended ? VK_LEFT : VK_NUMPAD4;
	}

	if (scan_code == 0x4C && !extended) {
		return VK_NUMPAD5;
	}

	if (scan_code == 0x4D) {
		return extended ? VK_RIGHT : VK_NUMPAD6;
	}

	if (scan_code == 0x4E && !extended) {
		return VK_ADD;
	}

	if (scan_code == 0x4F) {
		return extended ? VK_END : VK_NUMPAD1;
	}

	if (scan_code == 0x50) {
		return extended ? VK_DOWN : VK_NUMPAD2;
	}

	if (scan_code == 0x51) {
		return extended ? VK_NEXT : VK_NUMPAD3;
	}

	if (scan_code == 0x52) {
		return extended ? VK_INSERT : VK_NUMPAD0;
	}

	if (scan_code == 0x53) {
		return extended ? VK_DELETE : VK_DECIMAL;
	}

	/*
	 * Windows keys
	 */
	if (extended && scan_code == 0x5B) {
		return VK_LWIN;
	}

	if (extended && scan_code == 0x5C) {
		return VK_RWIN;
	}

	if (extended && scan_code == 0x5D) {
		return VK_APPS;
	}

	if (keyboard->vkCode < STANDARD_KEY_COUNT) {
		return (int)keyboard->vkCode;
	}

	return -1;
}

/* --------------------------------------------------------- */
/* Keyboard hook                                             */
/* --------------------------------------------------------- */

static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM w_param, LPARAM l_param)
{
	if (code >= 0) {

		const KBDLLHOOKSTRUCT *keyboard = (const KBDLLHOOKSTRUCT *)l_param;

		bool key_down = w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN;

		bool key_up = w_param == WM_KEYUP || w_param == WM_SYSKEYUP;

		if (key_down || key_up) {

			int key_code = normalize_key(keyboard);

			if (key_code >= 0) {

				uint64_t timestamp = (uint64_t)GetTickCount64();

				set_key_state(key_code, key_down, timestamp);
			}
		}
	}

	return CallNextHookEx(keyboard_hook, code, w_param, l_param);
}

/* --------------------------------------------------------- */
/* Mouse hook                                                */
/* --------------------------------------------------------- */

static LRESULT CALLBACK mouse_hook_proc(int code, WPARAM w_param, LPARAM l_param)
{
	(void)l_param;

	if (code >= 0) {

		uint64_t timestamp = (uint64_t)GetTickCount64();

		switch (w_param) {

		case WM_LBUTTONDOWN:
			set_mouse_state(INPUT_MOUSE_LEFT, true, timestamp);
			break;

		case WM_LBUTTONUP:
			set_mouse_state(INPUT_MOUSE_LEFT, false, timestamp);
			break;

		case WM_RBUTTONDOWN:
			set_mouse_state(INPUT_MOUSE_RIGHT, true, timestamp);
			break;

		case WM_RBUTTONUP:
			set_mouse_state(INPUT_MOUSE_RIGHT, false, timestamp);
			break;

		default:
			break;
		}
	}

	return CallNextHookEx(mouse_hook, code, w_param, l_param);
}

/* --------------------------------------------------------- */
/* Hook thread                                               */
/* --------------------------------------------------------- */

static HMODULE get_current_module_handle(void)
{
	HMODULE module = NULL;

	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,

			   (LPCWSTR)(const void *)&keyboard_hook_proc,

			   &module);

	return module;
}

static DWORD WINAPI hook_thread_proc(LPVOID parameter)
{
	(void)parameter;

	MSG message;

	/*
	 * Force creation of this thread's
	 * message queue before start() may
	 * post WM_QUIT to it.
	 */
	PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	HMODULE module = get_current_module_handle();

	keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook_proc, module, 0);

	if (!keyboard_hook) {

		InterlockedExchange(&hook_started, 0);

		if (hook_ready_event) {
			SetEvent(hook_ready_event);
		}

		return 0;
	}

	mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook_proc, module, 0);

	if (!mouse_hook) {

		UnhookWindowsHookEx(keyboard_hook);

		keyboard_hook = NULL;

		InterlockedExchange(&hook_started, 0);

		if (hook_ready_event) {
			SetEvent(hook_ready_event);
		}

		return 0;
	}

	InterlockedExchange(&hook_started, 1);

	if (hook_ready_event) {
		SetEvent(hook_ready_event);
	}

	while (GetMessageW(&message, NULL, 0, 0) > 0) {

		TranslateMessage(&message);

		DispatchMessageW(&message);
	}

	if (mouse_hook) {

		UnhookWindowsHookEx(mouse_hook);

		mouse_hook = NULL;
	}

	if (keyboard_hook) {

		UnhookWindowsHookEx(keyboard_hook);

		keyboard_hook = NULL;
	}

	InterlockedExchange(&hook_started, 0);

	clear_all_key_states();
	clear_all_mouse_states();

	return 0;
}

/* --------------------------------------------------------- */
/* Public service functions                                  */
/* --------------------------------------------------------- */

bool input_service_start(void)
{
	if (hook_thread)
		return true;

	clear_all_key_states();
	clear_all_mouse_states();

	clear_event_history();
	clear_mouse_event_history();

	InterlockedExchange(&hook_started, 0);

	hook_ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);

	if (!hook_ready_event)
		return false;

	hook_thread = CreateThread(NULL, 0, hook_thread_proc, NULL, 0, &hook_thread_id);

	if (!hook_thread) {

		CloseHandle(hook_ready_event);

		hook_ready_event = NULL;

		return false;
	}

	DWORD wait_result = WaitForSingleObject(hook_ready_event, 5000);

	CloseHandle(hook_ready_event);

	hook_ready_event = NULL;

	if (wait_result != WAIT_OBJECT_0 || InterlockedCompareExchange(&hook_started, 0, 0) == 0) {

		WaitForSingleObject(hook_thread, 1000);

		CloseHandle(hook_thread);

		hook_thread = NULL;
		hook_thread_id = 0;

		return false;
	}

	return true;
}

void input_service_stop(void)
{
	if (!hook_thread)
		return;

	if (hook_thread_id != 0) {

		PostThreadMessageW(hook_thread_id, WM_QUIT, 0, 0);
	}

	WaitForSingleObject(hook_thread, 3000);

	CloseHandle(hook_thread);

	hook_thread = NULL;
	hook_thread_id = 0;

	keyboard_hook = NULL;
	mouse_hook = NULL;

	InterlockedExchange(&hook_started, 0);

	clear_all_key_states();
	clear_all_mouse_states();

	clear_event_history();
	clear_mouse_event_history();
}

/* --------------------------------------------------------- */
/* Current key / mouse state                                 */
/* --------------------------------------------------------- */

bool input_service_is_key_pressed(int key_code)
{
	int index = key_code_to_index(key_code);

	if (index < 0)
		return false;

	LONG value = InterlockedCompareExchange(&key_states[index], 0, 0);

	return value != 0;
}

bool input_service_is_mouse_pressed(int mouse_button)
{
	if (mouse_button < 0 || mouse_button >= MOUSE_BUTTON_COUNT) {
		return false;
	}

	LONG value = InterlockedCompareExchange(&mouse_states[mouse_button], 0, 0);

	return value != 0;
}

/* --------------------------------------------------------- */
/* Keyboard event reading                                    */
/* --------------------------------------------------------- */

uint64_t input_service_get_latest_sequence(void)
{
	uint64_t result;

	AcquireSRWLockShared(&event_lock);

	result = latest_event_sequence;

	ReleaseSRWLockShared(&event_lock);

	return result;
}

size_t input_service_read_events(uint64_t *last_sequence, struct input_key_event *events, size_t capacity)
{
	if (!last_sequence || !events || capacity == 0) {
		return 0;
	}

	AcquireSRWLockShared(&event_lock);

	uint64_t latest = latest_event_sequence;

	if (*last_sequence >= latest) {

		ReleaseSRWLockShared(&event_lock);

		return 0;
	}

	uint64_t oldest;

	if (latest >= INPUT_EVENT_BUFFER_SIZE) {

		oldest = latest - INPUT_EVENT_BUFFER_SIZE + 1;

	} else {

		oldest = 1;
	}

	uint64_t first = *last_sequence + 1;

	if (first < oldest)
		first = oldest;

	uint64_t available = latest - first + 1;

	size_t count = (size_t)available;

	if (count > capacity)
		count = capacity;

	for (size_t i = 0; i < count; i++) {

		uint64_t sequence = first + i;

		size_t index = (size_t)((sequence - 1) % INPUT_EVENT_BUFFER_SIZE);

		events[i] = event_buffer[index];
	}

	if (count > 0) {

		*last_sequence = events[count - 1].sequence;
	}

	ReleaseSRWLockShared(&event_lock);

	return count;
}

/* --------------------------------------------------------- */
/* Mouse event reading                                       */
/* --------------------------------------------------------- */

uint64_t input_service_get_latest_mouse_sequence(void)
{
	uint64_t result;

	AcquireSRWLockShared(&mouse_event_lock);

	result = latest_mouse_event_sequence;

	ReleaseSRWLockShared(&mouse_event_lock);

	return result;
}

size_t input_service_read_mouse_events(uint64_t *last_sequence, struct input_mouse_event *events, size_t capacity)
{
	if (!last_sequence || !events || capacity == 0) {
		return 0;
	}

	AcquireSRWLockShared(&mouse_event_lock);

	uint64_t latest = latest_mouse_event_sequence;

	if (*last_sequence >= latest) {

		ReleaseSRWLockShared(&mouse_event_lock);

		return 0;
	}

	uint64_t oldest;

	if (latest >= INPUT_EVENT_BUFFER_SIZE) {

		oldest = latest - INPUT_EVENT_BUFFER_SIZE + 1;

	} else {

		oldest = 1;
	}

	uint64_t first = *last_sequence + 1;

	if (first < oldest)
		first = oldest;

	uint64_t available = latest - first + 1;

	size_t count = (size_t)available;

	if (count > capacity)
		count = capacity;

	for (size_t i = 0; i < count; i++) {

		uint64_t sequence = first + i;

		size_t index = (size_t)((sequence - 1) % INPUT_EVENT_BUFFER_SIZE);

		events[i] = mouse_event_buffer[index];
	}

	if (count > 0) {

		*last_sequence = events[count - 1].sequence;
	}

	ReleaseSRWLockShared(&mouse_event_lock);

	return count;
}