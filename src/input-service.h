#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KEY_INPUT_NUMPAD_ENTER 0x1000

#define INPUT_EVENT_BUFFER_SIZE 512

#define INPUT_MOUSE_LEFT 0
#define INPUT_MOUSE_RIGHT 1

struct input_key_event {
	int key_code;
	bool pressed;
	uint64_t timestamp_ms;
	uint64_t sequence;
};

struct input_mouse_event {
	int mouse_button;
	bool pressed;
	uint64_t timestamp_ms;
	uint64_t sequence;
};

bool input_service_start(void);
void input_service_stop(void);

bool input_service_is_key_pressed(int key_code);

bool input_service_is_mouse_pressed(int mouse_button);

uint64_t input_service_get_latest_sequence(void);

size_t input_service_read_events(uint64_t *last_sequence, struct input_key_event *events, size_t capacity);

uint64_t input_service_get_latest_mouse_sequence(void);

size_t input_service_read_mouse_events(uint64_t *last_sequence, struct input_mouse_event *events, size_t capacity);