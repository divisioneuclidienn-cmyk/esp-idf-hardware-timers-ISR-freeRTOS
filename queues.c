#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <time.h>

#define RAND_MIN 1
#define MY_RAND_MAX 100

#define MEASURE1_AVERAGE_SAMPLES 100
#define MEASURE2_AVERAGE_SAMPLES 1000
#define MEASURE3_AVERAGE_SAMPLES 300

#define TASK_DELAY_US 10

// utilisé dans la fonction de traitement (moyenne) pour les indices
// enum + tableau = propre
typedef enum {
  MEASURE1 = 0,
  MEASURE2 = 1,
  MEASURE3 = 2
}msg_type_t;

// structure des données de la file des mesures initiales
typedef struct {
  msg_type_t msg_type;
  float value;
  uint64_t time; // not used
}raw_data_queue_msg_t;

typedef struct {
  msg_type_t msg_type;
  float value;
  uint64_t time;
}sampled_data_queue_msg_t;

static const char * TAG = "freeRTOS Queues";

TaskHandle_t measure1Task = NULL;
TaskHandle_t measure2Task = NULL;
TaskHandle_t processMeasuresManagerTask = NULL;
TaskHandle_t useSampledDataTask = NULL;

QueueHandle_t rawDataQueue = NULL; // mesures -> traitement
QueueHandle_t sampledDataQueue = NULL; // traitement -> tâches utilisant la donnée


// Génère une nombre pseudo-aléatoire et l'envoie sur une file freeRTOS
void measure1(void * arg){
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 10;
  volatile BaseType_t xWasDelayed;

  xLastWakeTime = xTaskGetTickCount ();

  while(1){
    xWasDelayed = xTaskDelayUntil(&xLastWakeTime, xFrequency); // Réveilla tâche à un temps précis qui comprend donc le temps d'exécution de la tâche

    uint64_t time = esp_timer_get_time();
    float measurement1 = (float)(rand() % (MY_RAND_MAX - RAND_MIN + 1)) + RAND_MIN; // CONVERSION FLOAT PAS SUR. COMMENT FONCTIONNE UNE CONVERSION DE TYPE EN C?

    raw_data_queue_msg_t measure1_msg = {
      .msg_type = MEASURE1,
      .value = measurement1,
      .time = time
    };

    xQueueSend(rawDataQueue, &measure1_msg, portMAX_DELAY); // Vérifier qu'il n'y aura pas de blocage (portMAX_DELAY)
  }
}


void measure2(void * arg){
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 10;
  volatile BaseType_t xWasDelayed;

  xLastWakeTime = xTaskGetTickCount ();

  while(1){
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    uint64_t time = esp_timer_get_time();
    float measurement2 = (float)(rand() % (MY_RAND_MAX - RAND_MIN + 1)) + RAND_MIN;
    
    raw_data_queue_msg_t measure2_msg = {
      .msg_type = MEASURE2,
      .value = measurement2,
      .time = time
    };

    xQueueSend(rawDataQueue, &measure2_msg, portMAX_DELAY);
  }
}


// Effectue une moyenne des mesures reçus en fonction de MEASUREx_AVERAGE_SAMPLES et l'envoie sur une file sampledDataQueue
void dataProcessingMethod1(const raw_data_queue_msg_t * msg){
  static float current_sum[3] = {0, 0, 0};
  static uint16_t current_data_transmitted_nb[3] = {0, 0, 0};
  static const uint16_t average_samples[3] = {
    MEASURE1_AVERAGE_SAMPLES,
    MEASURE2_AVERAGE_SAMPLES,
    MEASURE3_AVERAGE_SAMPLES
  };

  uint8_t index_nb = msg->msg_type;

  current_sum[index_nb] += msg->value;
  current_data_transmitted_nb[index_nb] += 1;

  //ESP_LOGI(TAG, "current_average of %d : %f\n", index_nb, current_sum[index_nb]);
  //ESP_LOGI(TAG, "current_data_transmitted_nb of %d : %d\n", index_nb, current_data_transmitted_nb[index_nb]);

  //Processus d'envoie de la donnée
  if (current_data_transmitted_nb[index_nb] == average_samples[index_nb]){
    float average = current_sum[index_nb] / average_samples[index_nb];

    //on crée la structure de donnée à envoyer sur la file
    sampled_data_queue_msg_t sampled_data = {
      .msg_type = msg->msg_type,
      .value = average,
      .time = esp_timer_get_time()
    };

    //on l'envoie
    xQueueSend(sampledDataQueue, &sampled_data, portMAX_DELAY);

    //remet à 0 les variables
    current_sum[index_nb] = 0;
    current_data_transmitted_nb[index_nb] = 0;
  }
}


void processMeasuresManager(void * arg){
  while(1){

    // récupération de la donnée
    raw_data_queue_msg_t queue_msg;
    xQueueReceive(rawDataQueue, &queue_msg, portMAX_DELAY);
    
    switch(queue_msg.msg_type){
      case MEASURE1:
      case MEASURE2:
      case MEASURE3:
        dataProcessingMethod1(&queue_msg);
        break;
    }

  }

}

// Reçoit les données traitées de la file sampledDataQueue et les envoie à travers l'UART pour les afficher
void useSampledData(void * arg){
  while(1){
    sampled_data_queue_msg_t sampled_data_queue_msg;
    xQueueReceive(sampledDataQueue, &sampled_data_queue_msg, portMAX_DELAY);
    //ESP_LOGI(TAG, "Entering useSampledData function\n");
    float sampled_data = sampled_data_queue_msg.value;

    switch(sampled_data_queue_msg.msg_type){
      case MEASURE1 :
        ESP_LOGI(TAG, "[MEASURE1] Sampled data : %f\n", sampled_data);
        break;
      
      case MEASURE2 :
        ESP_LOGI(TAG, "[MEASURE2] Sampled data : %f\n", sampled_data);
        break;
      
      case MEASURE3 :
        ESP_LOGI(TAG, "[MEASURE3] Sampled data : %f\n", sampled_data);
        break;
    }
  }
}

void app_main(void){
  ESP_LOGI(TAG, "Program started\n");

  BaseType_t BaseType_ret;
  esp_err_t esp_err_ret;

  srand(time(NULL));

  rawDataQueue = xQueueCreate(10, sizeof(raw_data_queue_msg_t));
  sampledDataQueue = xQueueCreate(10, sizeof(sampled_data_queue_msg_t));

  BaseType_ret = xTaskCreate(measure1, "measure1", 4096, NULL, 3, &measure1Task);
  configASSERT(BaseType_ret == pdTRUE);

  BaseType_ret = xTaskCreate(measure2, "measure2", 4096, NULL, 3, &measure2Task);
  configASSERT(BaseType_ret == pdTRUE);

  BaseType_ret = xTaskCreate(processMeasuresManager, "processMeasuresManager", 4096, NULL, 2, &processMeasuresManagerTask);
  configASSERT(BaseType_ret == pdTRUE);

  BaseType_ret = xTaskCreate(useSampledData, "useSampledData", 4096, NULL, 1, &useSampledDataTask);
  configASSERT(BaseType_ret == pdTRUE);

}

//Simulateur de centrale d'acquisition :
// Structures de données de file avec infos importantes (temps, mesures)
// Logs ESP-IDF
// ESP_ERROR_CHECK
// Modularité du code
// documentation 

// le temps de traitement de la mesure doit être inférieur à la période de la mesure sinon blocage de file possible
// -> Δt traitement < T mesure

//Cohésion du code, fonctions/tâches avec responsabilité claire et définie
//Couplage : Les différentes parties du programme doivent dépendre le moins possible les unes des autres