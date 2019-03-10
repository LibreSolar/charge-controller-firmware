

#ifndef __LEDS_H__
#define __LEDS_H__

void leds_init();
void led_timer_start(int freq_Hz);
void charlieplexing();

void update_dcdc_led(bool enabled);
void update_rxtx_led();
void trigger_tx_led();
void trigger_rx_led();
void update_soc_led(int soc);
void toggle_led_blink();

#endif /* __LEDS_H__ */