#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED1 25

static const char * TAG = "Hardware timer interrupt lesson";

static volatile int counter = 0;
static volatile bool led_state = false;

TaskHandle_t print_counter1_handle;
TaskHandle_t print_counter2_handle;
TaskHandle_t update_led_handle;

static bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t * edata, void * user_ctx)
{
  counter++;
  led_state = !led_state;

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(print_counter1_handle, 0, eIncrement, &higherPriorityTaskWoken);
  xTaskNotifyFromISR(print_counter2_handle, 0, eIncrement, &higherPriorityTaskWoken);
  xTaskNotifyFromISR(update_led_handle, 0, eIncrement, &higherPriorityTaskWoken);

  return true;
}

void print_counter(void * arg){
  
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    char * tag = (char *)arg;
    printf("TAG [%s] Counter = %d\n", tag, counter);
  }
}

void update_led(void * arg){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    gpio_set_level(LED1, led_state);
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "app_main starting\n");

  esp_err_t ret;
  BaseType_t BaseType_ret;

  gpio_config_t led_config = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = (1ULL << LED1),
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  ret = gpio_config(&led_config);
  if(ret != ESP_OK){
    ESP_LOGE(TAG, "error during led config\n");
    abort();
  }


  gptimer_handle_t gptimer;
  gptimer_config_t config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000, // 1MHz -> 1tick = 1us
  };
  ret = gptimer_new_timer(&config, &gptimer);
  configASSERT(ret == ESP_OK);


  gptimer_event_callbacks_t callbacks = {
    .on_alarm = timer_callback,
  };
  ret = gptimer_register_event_callbacks(gptimer, &callbacks, NULL);
  configASSERT(ret == ESP_OK);
  

  gptimer_alarm_config_t alarm_config = {
    .alarm_count = 1000000,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true
  };
  ret = gptimer_set_alarm_action(gptimer, &alarm_config);
  configASSERT(ret == ESP_OK);
  
  ret = gptimer_enable(gptimer);
  configASSERT(ret == ESP_OK);

  ret = gptimer_start(gptimer);
  configASSERT(ret == ESP_OK);

  BaseType_ret = xTaskCreate(print_counter, "print_counter1", 2048, "func1", 1, &print_counter1_handle);
  configASSERT(BaseType_ret == pdPASS);

  BaseType_ret = xTaskCreate(print_counter, "print_counter2", 2048, "func2", 2, &print_counter2_handle);
  configASSERT(BaseType_ret == pdPASS);

  BaseType_ret = xTaskCreate(update_led, "update_led", 1024, NULL, 3, &update_led_handle);
  configASSERT(BaseType_ret == pdPASS);
}