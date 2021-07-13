#include <stdio.h>
#include "rgb_leds.h"
#include "coap_endpoints.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

const static char *TAG = "RGB Leds";

void set_rgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity) {
    if (intensity > 100) return;
    rgb_data[0] = red;
    rgb_data[1] = green;
    rgb_data[2] = blue;    
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, intensity*red/255);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, intensity*green/255);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_2, MCPWM_OPR_A, intensity*blue/255);
    // Update NVM
    if (ctrl_mode == manual || ctrl_mode == audio_intensity)
        xEventGroupSetBits(endpoint_events,E_RGB_BIT);
}

void mcpwm_gpio_init_config(void)
{
    ESP_LOGI(TAG, "Initializing mcpwm......\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, GPIO_PWM1A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2A, GPIO_PWM2A_OUT);
    //initial mcpwm configuration
    ESP_LOGI(TAG, "Configuring Initial Parameters of mcpwm......\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 60;    //frequency = 60Hz
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config); 
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config);
    if (ctrl_mode == manual || ctrl_mode == audio_intensity) 
        set_rgb(rgb_data[0], rgb_data[1], rgb_data[2], 100);
}
