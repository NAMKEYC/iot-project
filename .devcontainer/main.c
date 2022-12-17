#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sht3x.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "mqtt_client.h"




#define EXAMPLE_ESP_WIFI_SSID          "qtfreet"
#define EXAMPLE_ESP_WIFI_PASS          "melodysoyani"
#define MAX_RETRY                      10
#define MQTT_PUB_TEMP_SHT30            "esp32/sht30/temperature"
#define MQTT_PUB_HUM_SHT30             "esp32/sht30/humidity"
#define MQTT_PUB_STATUS                "esp32/status"

#define RED_LED                         GPIO_NUM_32 
#define GREEN_LED                       GPIO_NUM_33
#define BLUE_LED                        GPIO_NUM_27
#define BUZZER                          GPIO_NUM_4

static const char *TAG = "MQTT_EXAMPLE";
static int retry_cnt = 0;
static void mqtt_app_start(void);
static sht3x_t dev;

uint32_t MQTT_CONNEECTED = 0;

static void set_direction(){
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);
}

static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    set_direction();
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "Trying to connect with Wi-Fi\n");
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Wi-Fi connected\n");
        break;

    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip: startibg MQTT Client\n");
        mqtt_app_start();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "disconnected: Retrying Wi-Fi\n");
        if (retry_cnt++ < MAX_RETRY)
        {
            esp_wifi_connect();

        }
        else{
            ESP_LOGI(TAG, "Max Retry Failed: Wi-Fi Connection\n");
            gpio_set_level(BUZZER,0);
            gpio_set_level(RED_LED,0);
            gpio_set_level(GREEN_LED, 0);
            gpio_set_level(BLUE_LED,0);
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

void wifi_init(void)
{
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_netif_init();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        MQTT_CONNEECTED=1;
        
        msg_id = esp_mqtt_client_subscribe(client, "/topic/test1", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/test2", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        MQTT_CONNEECTED=0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}



esp_mqtt_client_handle_t client = NULL;
static void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "STARTING MQTT");
    esp_mqtt_client_config_t mqttConfig = {
        .uri = "mqtt://broker.emqx.io"
    };

    client = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

}

#if defined(CONFIG_EXAMPLE_SHT3X_DEMO_HL)

/*
 * User task that triggers a measurement every 5 seconds. Due to power
 * efficiency reasons it uses *single shot* mode. In this example it uses the
 * high level function *sht3x_measure* to perform one measurement in each cycle.
 */
void Publisher_Task(void *params)
{
    char temp[12];
    char humi[12];
    char stab[12] = "STABLE";
    char warn[12] = "WARNING !!";
    char crit[12] = "CRITICAL !!";
    float temperature;
    float humidity;
    TickType_t last_wakeup = xTaskGetTickCount();

    set_direction();
    while (true)
    {
        // perform one measurement and do something with the results
        ESP_ERROR_CHECK(sht3x_measure(&dev, &temperature, &humidity));
        printf("SHT3x Sensor: %.2f °C, %.2f %%\n", temperature, humidity);

        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(5000));
        // convert float to char
		sprintf(temp, "%.2f", temperature);
        sprintf(humi, "%.2f", humidity);
        if(MQTT_CONNEECTED)
        {
            esp_mqtt_client_publish(client, MQTT_PUB_TEMP_SHT30 , temp, 0, 0, 0);
            esp_mqtt_client_publish(client, MQTT_PUB_HUM_SHT30 , humi, 0, 0, 0);
            if(temperature > 35){
                gpio_set_level(BUZZER,1);
                gpio_set_level(RED_LED,1);
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BLUE_LED,0);
                esp_mqtt_client_publish(client, MQTT_PUB_STATUS , crit, 0, 0, 0);

            }
            else if (temperature > 30){
                gpio_set_level(RED_LED,1);
                gpio_set_level(GREEN_LED, 1);
                gpio_set_level(BLUE_LED,0);
                gpio_set_level(BUZZER,0);
                esp_mqtt_client_publish(client, MQTT_PUB_STATUS , warn, 0, 0, 0);
            }
            else{
                gpio_set_level(GREEN_LED,1);
                gpio_set_level(RED_LED,0);
                gpio_set_level(BLUE_LED,0);
                gpio_set_level(BUZZER,0);
                esp_mqtt_client_publish(client, MQTT_PUB_STATUS , stab, 0, 0, 0);
            }
        }
        else
        {
            ESP_LOGE(TAG, "MQTT Not connected");
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

#elif defined(CONFIG_EXAMPLE_SHT3X_DEMO_LL)

// /*
//  * User task that triggers a measurement every 5 seconds. Due to power
//  * efficiency reasons it uses *single shot* mode. In this example it starts the
//  * measurement, waits for the results and fetches the results using separate
//  * functions
//  */
// void Publisher_Task(void *params)
// {
//     char temper[12];
//     char humidi[12];
//     float temperature;
//     float humidity;
//     gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
//     gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
//     gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);
//     TickType_t last_wakeup = xTaskGetTickCount();

//     // get the measurement duration for high repeatability;
//     uint8_t duration = sht3x_get_measurement_duration(SHT3X_HIGH);

//     while (1)
//     {
//         // Trigger one measurement in single shot mode with high repeatability.
//         ESP_ERROR_CHECK(sht3x_start_measurement(&dev, SHT3X_SINGLE_SHOT, SHT3X_HIGH));

//         // Wait until measurement is ready (constant time of at least 30 ms
//         // or the duration returned from *sht3x_get_measurement_duration*).
//         vTaskDelay(duration);

//         // retrieve the values and do something with them
//         ESP_ERROR_CHECK(sht3x_get_results(&dev, &temperature, &humidity));
//         printf("SHT3x Sensor: %.2f °C, %.2f %%\n", temperature, humidity);

//         // wait until 5 seconds are over
//         vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(5000));
//         // convert float to char
// 		sprintf(temper, "%.2f", temperature);
//         sprintf(humidi, "%.2f", humidity);
//         gpio_set_level(RED_LED, 1);
//         if(MQTT_CONNEECTED)
//         {
//             esp_mqtt_client_publish(client, MQTT_PUB_TEMP_SHT30 , temper, 0, 0, 0);
//             esp_mqtt_client_publish(client, MQTT_PUB_HUM_SHT30 , humidi, 0, 0, 0);
//         }
//         else
//         {
//             ESP_LOGE(TAG, "MQTT Not connected");
//         }
//         vTaskDelay(5000 / portTICK_PERIOD_MS);
//     }
// }

// #else // CONFIG_SHT3X_DEMO_PERIODIC
// /*
//  * User task that fetches latest measurement results of sensor every 2
//  * seconds. It starts the SHT3x in periodic mode with 1 measurements per
//  * second (*SHT3X_PERIODIC_1MPS*).
//  */
// void Publisher_Task(void *params)
// {
//     char temper[12];
//     char humidi[12];
//     float temperature;
//     float humidity;
//     esp_err_t res;
//     gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
//     gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
//     gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);  
//     // Start periodic measurements with 1 measurement per second.
//     ESP_ERROR_CHECK(sht3x_start_measurement(&dev, SHT3X_PERIODIC_1MPS, SHT3X_HIGH));

//     // Wait until first measurement is ready (constant time of at least 30 ms
//     // or the duration returned from *sht3x_get_measurement_duration*).
//     vTaskDelay(sht3x_get_measurement_duration(SHT3X_HIGH));

//     TickType_t last_wakeup = xTaskGetTickCount();

//     while (1)
//     {
//         // Get the values and do something with them.
//         if ((res = sht3x_get_results(&dev, &temperature, &humidity)) == ESP_OK)
//             printf("SHT3x Sensor: %.2f °C, %.2f %%\n", temperature, humidity);
//         else
//             printf("Could not get results: %d (%s)", res, esp_err_to_name(res));

//         // Wait until 2 seconds (cycle time) are over.
//         vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(2000));
//         // convert float to char
// 		sprintf(temper, "%.2f", temperature);
//         sprintf(humidi, "%.2f", humidity);
//         gpio_set_level(RED_LED, 1);
//         if(MQTT_CONNEECTED)
//         {
//             esp_mqtt_client_publish(client, MQTT_PUB_TEMP_SHT30 , temper, 0, 0, 0);
//             esp_mqtt_client_publish(client, MQTT_PUB_HUM_SHT30 , humidi, 0, 0, 0);
//         }
//         else
//         {
//             ESP_LOGE(TAG, "MQTT Not connected");
//         }
//         vTaskDelay(5000 / portTICK_PERIOD_MS)
//     }
// }

#endif

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init();
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&dev, 0, sizeof(sht3x_t));

    ESP_ERROR_CHECK(sht3x_init_desc(&dev, CONFIG_EXAMPLE_SHT3X_ADDR, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(sht3x_init(&dev));
    xTaskCreate(Publisher_Task, "Publisher_Task", 1024 * 5, NULL, 5, NULL);
    ESP_LOGI(TAG, "MQTT Publisher_Task is up and running\n");
}  