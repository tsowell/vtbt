#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define HID_REPORT_SIZE 8
#define HID_REPORT_FIRST_KEY 2

/* A node for the list of keys currently down */
struct key_down {
	sys_dnode_t node;
	/* LK201 keycode. */
	int keycode;
	/* Timestamp from k_uptime_get() when the key was first pressed. */
	int64_t time;
	/* True if the key has been held long enough to trigger auto-repeat. */
	bool repeating;
	/* False if the key hasn't been sent yet.
	 * This should only happen when the keyboard was locked. */
	bool sent;
	/* True if the host has disabled auto-repeat for this keypress via
	 * a Temporary Auto-Repeat Inhibit command. */
	bool inhibit_auto_repeat;
};

enum event_source {
	EVT_HOST,      /* A message from the terminal. */
	EVT_KEYBOARD,  /* A HID report from the Bluetooth keyboard. */
	EVT_METRONOME, /* The 1 ms auto-repeat timer has triggered. */
};

/* An event for the main thread's event queue. */
struct event {
	enum event_source source;
	/* Number of used bytes in buf. Always 0 for EVT_METRONOME. */
	uint8_t size;
	/* Message from host (EVT_HOST) or HID report (EVT_KEYBOARD). */
	uint8_t buf[HID_REPORT_SIZE];
};

#endif /* CONFIG_H */
