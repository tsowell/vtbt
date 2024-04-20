#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define HID_REPORT_SIZE 8
#define HID_REPORT_FIRST_KEY 2

struct key_down {
	sys_dnode_t node;
	int keycode;
	int64_t time;
	bool repeating;
	bool sent;
	bool inhibit_auto_repeat;
};

enum event_source { EVT_HOST, EVT_KEYBOARD, EVT_METRONOME };

struct event {
	enum event_source source;
	uint8_t size;
	uint8_t buf[HID_REPORT_SIZE];
};

#endif /* CONFIG_H */
