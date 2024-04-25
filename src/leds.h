#ifndef LEDS_H
#define LEDS_H

#define NUM_LEDS 4

#define LED_HOLD_SCREEN  3
#define LED_LOCK         2
#define LED_COMPOSE      1
#define LED_WAIT         0

int leds_init(void);
void leds_on(int which);
void leds_off(int which);

#endif /* LEDS_H */
