#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

#define STA_WIFI_SSID "IoT"
#define STA_WIFI_PASS "imggd12!@#"


EventGroupHandle_t event_group;

const int WIFI_CONNECTED_BIT = BIT0;
/**
 * 
 * 1. Initialize nvs
 * - Initialize default nvs partition.
 * - If the nvs storage contains no empty page (which is 'ESP_ERR_NVS_NO_FREE_PAGE')
 *   or no partition with label "nvs" is found in the partition table (which is 'ESP_ERR_NOT_FOUND')
 *   then erase the default nvs partition
 *   and reinitialize nvs.
 * 
 * 2. Initialize wifi mode
 * - Create EventGroupHandle.
 * - Initialize TCP/IP adapter.
 * - Initialize Event Loop with Event Handler (user defined)
 * - Initialize WiFi. (Allocate resource for WiFi driver, such as WiFi control structure, RX/TX buffer,
 *                     WiFi NVS structure etc)
 * - Set WiFi stack configuration with Default. (which is 'wifi_init_config_t' using 'WIFI_INIT_CONFIG_DEFAULT')
 * - Initialize configuration data for ESP32 AP or STA. (which is 'wifi_config_t')
 * - Set wifi mode.
 * - Set wifi configuration. (AP or STA, if wifi mode is APSTA then set AP and STA together)
 * - Start WiFi.
 * 
 * 3. Event handler
 * - Define Event handler (according to wifi mode)
 * 
*/

/**
 * 
 * xTaskCreate() creates a new task and add it to the list of tasks that are ready to run.
 * xTaskCreate(task_function name, task_name, stack_depth(for use), parameter, priority, task_handle)
 * 
 * xEventGroupWaitBits() read bits within an RTOS event group, optionally entering the Blocked state
 *                       (with a timeout) to wait for a bit or group of bits to become set. 
 * xEventGroupWaitBits(event_group, event_bits, clear_on_exit, wait_for_all_bits, ticks_to_wait)
 * 
 * 
*/

static esp_err_t event_handler(void* ctx, system_event_t* event)
{
    switch(event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
    }

    return ESP_OK;
}

void wifi_init()
{
    event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = STA_WIFI_SSID,
            .password = STA_WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_sta_init finished.");

}

void app_main()
{
    esp_err_t nvs_cfg = nvs_flash_init();
    if (nvs_cfg == ESP_ERR_NVS_NO_FREE_PAGES || nvs_cfg == ESP_ERR_NOT_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_cfg = nvs_flash_init();
    }
}