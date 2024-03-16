#ifndef LEDS_H
#define LEDS_h

#define NUM_LEDS 4

int leds_init(void);
void leds_set(int which, int value);

#endif /* LEDS_H */
