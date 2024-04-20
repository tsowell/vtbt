#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <zephyr/kernel.h>

#include "vtbt.h"

void keyboard_ctrl_keyclick_enable(void);
void keyboard_ctrl_keyclick_disable(void);
void keyboard_init_defaults(void);
void keyboard_event(sys_dlist_t *, const struct event *);

#endif /* KEYBOARD_H */
