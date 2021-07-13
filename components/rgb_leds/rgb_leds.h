#ifndef RGB_LEDS_H
#define RGB_LEDS_H

#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

#define GPIO_PWM0A_OUT 27   /* Set GPIO 27 as PWM0A / Red */
#define GPIO_PWM1A_OUT 26   /* Set GPIO 26 as PWM1A / Green */
#define GPIO_PWM2A_OUT 25   /* Set GPIO 25 as PWM2A / Blue */

#define COLOR_R_IDX 0
#define COLOR_G_IDX 1
#define COLOR_B_IDX 2

/* Debug mic input values */
#define DEBUG_MIC_INPUT   (0)
/* I2S sample rate */
#define EXAMPLE_I2S_SAMPLE_RATE   (44100)
/* I2S read buffer length */
#define EXAMPLE_I2S_READ_LEN      (2048)
/* I2S data format */
#define EXAMPLE_I2S_FORMAT        (I2S_CHANNEL_FMT_RIGHT_LEFT)
/* I2S built-in ADC unit */
#define I2S_ADC_UNIT              ADC_UNIT_1
/* I2S built-in ADC channel (GPIO 36 is VP pin) */
#define I2S_ADC_CHANNEL           ADC1_CHANNEL_0
/* Amplitude min and max values that influence the intensity of the rgb leds */
#define SOUND_AMPLITUDE_MIN_TRESH (350)
#define SOUND_AMPLITUDE_MAX_TRESH (2000-SOUND_AMPLITUDE_MIN_TRESH)
/* Frequency ranges that influence the colors of the rgb leds */
#define BLUE_FREQ_START  (300)
#define BLUE_FREQ_END  (500)
#define GREEN_FREQ_START  BLUE_FREQ_END
#define GREEN_FREQ_END  (1100)
#define RED_FREQ_START  GREEN_FREQ_END
#define RED_FREQ_END  (3500)

typedef struct rgb_mode_storage
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t mode;
} rgb_mode_storage;

typedef union color_data {
   rgb_mode_storage color;
   int32_t all;
} color_data;

void set_rgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity);

void mcpwm_gpio_init_config(void);

#endif /* RGB_LEDS_H */