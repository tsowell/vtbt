#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <zephyr/kernel.h>

#include "vtbt.h"

void keyboard_event(sys_dlist_t *, const struct event *);

#endif /* KEYBOARD_H */
