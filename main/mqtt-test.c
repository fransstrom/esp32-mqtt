#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "secrets.h"
#include "soc/gpio_num.h"
#include <esp_wifi.h>
#include <stdio.h>
#define TAG "simple_connect_example"
#include "protocol_examples_common.h"

static const char *MQTT_TAG = "MQTT_TEST";
static int counter = 0;
static esp_mqtt_client_handle_t mqtt_client;

#define BUTTON_GPIO GPIO_NUM_10

static void publish_counter(void) {
  char payload[32];
  snprintf(payload, sizeof(payload), "%d", counter);
  int msg_id =
      esp_mqtt_client_publish(mqtt_client, "counter", payload, 0, 1, 0);
  ESP_LOGI(TAG, "Published counter=%d, msg_id=%d", counter, msg_id);
  counter++;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
    esp_mqtt_client_subscribe(mqtt_client, "date", 0);
    ESP_LOGI(MQTT_TAG, "Subscribed to date");
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
    printf("Topic: %.*s\n", event->topic_len, event->topic);
    printf("Data: %.*s\n", event->data_len, event->data);
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
    break;

  default:
    break;
  }
}

void periodic_timer_callback(void *arg) {
  if (mqtt_client != NULL) {
    publish_counter();
  }
}

static void publish_boom(void) {
  char payload[255];
  snprintf(payload, sizeof(payload), "%s",
           "panic button pressed from my esp32c3 SUPERMINI");
  int msg_id = esp_mqtt_client_publish(mqtt_client, "boom", payload, 0, 1, 0);
  ESP_LOGI(TAG, "Published counter=%d, msg_id=%d", counter, msg_id);
  counter++;
}
static void on_button_press(void *args) {
  uint8_t last_level = 1;

  while (1) {
    uint8_t level = gpio_get_level(BUTTON_GPIO);

    if (level == 0 && last_level == 1) {
      ESP_LOGI("BUTTON", "Pressed!");
      if (mqtt_client != NULL) {
        publish_boom();
      }
    }

    last_level = level;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void init_button() {
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

  xTaskCreate(on_button_press, "button_task", 2048, NULL, 5, NULL);
}

void app_main(void) {

  // System initialization
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());

  const esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.hostname = MQTT_HOSTNAME,
      .broker.address.port = 1883,
      .credentials.username = MQTT_USERNAME,
      .credentials.authentication.password = MQTT_PASSWORD,
      .broker.address.transport = MQTT_TRANSPORT_OVER_TCP};

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
  //
  // const esp_timer_create_args_t periodic_timer_args = {
  //     .callback = &periodic_timer_callback, .name = "periodic"};
  //
  // esp_timer_handle_t periodic_timer;
  // ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  // ESP_ERROR_CHECK(
  //     esp_timer_start_periodic(periodic_timer, 5000000)); // 5 seconds
  //
  init_button();
}
