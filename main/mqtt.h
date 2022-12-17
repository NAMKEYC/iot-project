#ifndef __MQTT_HANDLER_H_
#define __MQTT_HANDLER_H_

#include <esp_err.h>
void wifi_init(void);

static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data);

static void mqtt_app_start(void);


esp_err_t mqtt_handler_publish_values(float, float);

static void mqtt_app_start(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
#endif /* __MQTT_HANDLER_H_ */
