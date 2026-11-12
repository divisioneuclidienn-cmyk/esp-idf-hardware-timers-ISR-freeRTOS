#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

const gpio_num_t input = GPIO_NUM_36; //ADC1_0 "SP" pin
const char * TAG = "ADC";

void app_main(void){

  // setup de l'ADCx
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1, // ADC1
    .ulp_mode = ADC_ULP_MODE_DISABLE // UltraLowPower mode
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  // pourquoi ?
  //adc_oneshot_del_unit(adc1_handle);

  
  // Setup de l'ADCx_y (channel de l'ADC)
  adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_12 // divise la tension par 3,5V pour lire de 0 à 3.3V car l'ADC ne fonctionne qu'en 0-1.1V
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config));

  int variableToStoreOutput;
  while(1){
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &variableToStoreOutput));
    ESP_LOGI(TAG, "GPIO_36 ANALOG INPUT : %d\n", variableToStoreOutput);
    esp_rom_delay_us(500000);
  }

}

// On se base sur le pinout de l'esp pour trouver les broches intégrants des ADC
// On remarque 2 groupes : ADC1_x et ADC2_x
// On remarque que l'ADC2 ne peut pas être uitilisé quand le WiFi est activé (Il l'utilise pour fonctionner)
// fonctionnement ADC intéressant (google)
// SAR = Successive Approximation Register = décrit le fonctionnement de l'ADC
// lecture par ADC bloquante car charge des condensateurs d'échantillonnages + N cycles d'horloges en fonctions de la résolution de l'ADC
// mode DMA pour du non bloquant