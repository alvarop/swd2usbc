#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#define LED1 (2)
#define LED2 (3)

float read_adc_v(uint8_t adc_ch) {
  const float conversion_factor = 3.3f / (1 << 12);
  adc_select_input(adc_ch);
  return (float)adc_read() * conversion_factor;
}

// Defines for voltage at each CCx charge level in normal mode
// Debug Accessory Mode uses same voltage levels but they mean different things
// USB-C Spec R2.0 - Tables 4-24 and 4-36
#define USB_CC_500MA_V 0.2
#define USB_CC_1500MA_V 0.66
#define USB_CC_3000MA_V 1.23

#define CC1_ADC_CH (2)
#define CC2_ADC_CH (0)

static uint32_t get_charge_current(bool *in_debug_mode) {
  uint32_t charge_current_ma = 0;

  float cc1 = read_adc_v(CC1_ADC_CH);
  float cc2 = read_adc_v(CC2_ADC_CH);

  // Make sure this works in both orientations
  float USB_cc_max = MAX(cc1, cc2);
  float USB_cc_min = MIN(cc1, cc2);

  // Both cc1 AND cc2 are high, use Debug Accessory Mode decoding
  // USB-C spec R2.0 Table B-2 and Table 4-36
  if ((USB_cc_max > USB_CC_500MA_V) && (USB_cc_min > USB_CC_500MA_V)) {
    if ((USB_cc_max >= USB_CC_3000MA_V) && (USB_cc_min <= USB_CC_3000MA_V) &&
        (USB_cc_min >= USB_CC_1500MA_V)) {
      charge_current_ma = 500;
    } else if ((USB_cc_max <= USB_CC_3000MA_V) &&
               (USB_cc_max >= USB_CC_1500MA_V) &&
               (USB_cc_min <= USB_CC_1500MA_V)) {
      charge_current_ma = 1500;
    } else if ((USB_cc_max >= USB_CC_3000MA_V) &&
               (USB_cc_min <= USB_CC_1500MA_V)) {
      charge_current_ma = 3000;
    } else {
      charge_current_ma = 0;
    }
    if (in_debug_mode) {
      *in_debug_mode = true;
    }
  } else {
    // Normal charge current detection
    float USB_cc_max = MAX(cc1, cc2);

    // USB-C spec R2.0 Table 4-36
    if (USB_cc_max <= USB_CC_500MA_V) {
      charge_current_ma = 0;
    } else if (USB_cc_max <= USB_CC_1500MA_V) {
      charge_current_ma = 500;
    } else if (USB_cc_max <= USB_CC_3000MA_V) {
      charge_current_ma = 1500;
    } else {
      charge_current_ma = 3000;
    }

    if (in_debug_mode) {
      *in_debug_mode = false;
    }
  }

  if (charge_current_ma == 0) {
    printf("Unknown charge current CC1: %0.2fV CC2: %0.2fV\n", cc1, cc2);
  }

  return charge_current_ma;
}

int main() {
  stdio_init_all();

  // Initialize LEDs
  gpio_init(LED1);
  gpio_init(LED2);
  gpio_set_dir(LED1, GPIO_OUT);
  gpio_set_dir(LED2, GPIO_OUT);

  // Initialize ADC and ADC pins
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(28);

  uint8_t count = 0;
  while (true) {
    bool in_debug_mode;
    uint32_t charge_current_ma = get_charge_current(&in_debug_mode);

    printf("Charge current: %lumA (Debug Accessory Mode = %d)\n",
           charge_current_ma, in_debug_mode);

    // Change blink pattern according to charge current
    switch (charge_current_ma) {
      case 500: {
        // 500mA blink LED1
        gpio_put(LED1, (count & 1));
        gpio_put(LED2, 0);
        break;
      }

      case 1500: {
        // 1500mA blink LED2
        gpio_put(LED1, 0);
        gpio_put(LED2, (count & 1));
        break;
      }

      case 3000: {
        // 3000mA blink both LEDs
        gpio_put(LED1, (count & 1));
        gpio_put(LED2, (count & 1));
        break;
      }

      default: {
        // Error/unknown - alternating blinks
        gpio_put(LED1, (count & 1));
        gpio_put(LED2, !(count & 1));
      }
    }

    sleep_ms(500);
    count++;
  }
}
