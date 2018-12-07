/* Active click feedback Example */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <esp_spi_flash.h>
#include <driver/adc.h>
#include <driver/mcpwm.h>
#include <math.h>
const char* TAG = "main";

void hapticTask(void* arg){
    const char* TAG = "haptic";
    //  counter for print adc value.
    int count = 0;
    //  time from tapping in second
    double time = -1;
    //  Vibration feedback variable and coefficinets
    const double A = 2;
    const double damp[] = {-10, -20, -30};
    const int nDamp = sizeof(damp) / sizeof(damp[0]);
    const double freq[] = {100, 200, 300, 500};
    const int nFreq = sizeof(freq)/sizeof(freq[0]);
    int i=0;
    double omega = 0;
    double B=0;
    while(1){
        int ad = adc1_get_raw(ADC1_CHANNEL_6);
        if (ad < 2100 && time > 0.3){
            time = -1;
            printf("\r\n");
        }
        if (ad > 2400 && time == -1){
            time = 0;
            omega = freq[i % nFreq] * M_PI * 2;
            B = damp[i/nFreq];
            printf("Wave: %3.1fHz, A=%2.2f, B=%3.1f ", omega/(M_PI*2), A, B);
            i++;
            if (i >= nFreq * nDamp) i = 0;
        }
        double pwm = 0;
        if (time >= 0){
            pwm = A * cos (omega * time) * exp(B*time);
            time += 0.001;
        }else{
            pwm = 0;
        }
        //  Rotating direction
        if (pwm > 0){
            gpio_set_level(GPIO_NUM_5, 0);
            gpio_set_level(GPIO_NUM_17, 1);
            if (time >= 0) printf("+");
        }else{
            gpio_set_level(GPIO_NUM_5, 1);
            gpio_set_level(GPIO_NUM_17, 0);
            pwm = -pwm;
            if (time >= 0) printf("-");
        }
        if (pwm > 1) pwm = 1;
        
        //  set duty rate of pwm
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pwm* 100);
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
        count ++;
        if (count >= 1000 ){
            ESP_LOGI(TAG, "ADC:%d", ad);
            count = 0;
        }
        vTaskDelay(1);
    }
}

extern "C" void app_main()
{        
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
        chip_info.cores,
        (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    printf("silicon revision %d, ", chip_info.revision);
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    //----------------------------------
    printf("!!! Active Haptic Feedback Start !!!\n");
    
    ESP_LOGI("main", "Initialize ADC");
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    ESP_LOGI("main", "Initialize PWM");
    //1. mcpwm gpio initialization  
    const int GPIO_PWM0A_OUT = 16;
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm...\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 1000;    //frequency = 500Hz,
    pwm_config.cmpr_a = 0;          //duty cycle of PWMxA = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A with above settings
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, 20000);
    gpio_config_t conf;
    conf.pin_bit_mask = (1 << (17)) | (1 << (5));
    conf.mode = GPIO_MODE_OUTPUT;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.pull_up_en = GPIO_PULLUP_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&conf);

    TaskHandle_t taskHandle = NULL;
    xTaskCreate(hapticTask, "Haptic", 1024 * 10, NULL, 6, &taskHandle);
}
