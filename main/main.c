#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"

#include "ds1307.h"
#define CONFIG_SCL_GPIO 22
#define CONFIG_SDA_GPIO 21
#define CONFIG_TIMEZONE 9
#define NTP_SERVER "pool.ntp.org"

static const char *TAG = "DS1307";

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "pool.ntp.org");
    ESP_LOGI(TAG, "Your NTP Server is %s", NTP_SERVER);
    sntp_setservername(0, NTP_SERVER);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static bool obtain_time(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    // tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(example_disconnect());
    if (retry == retry_count)
        return false;
    return true;
}

void ds1307_test(void *pvParameters)
{
    ESP_LOGI(pcTaskGetName(0), "Connecting to WiFi and getting time over NTP.");
    if (!obtain_time())
    {
        ESP_LOGE(pcTaskGetName(0), "Fail to getting time over NTP.");
        while (1)
        {
            vTaskDelay(1);
        }
    }

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    now = now + (CONFIG_TIMEZONE * 60 * 60);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(pcTaskGetName(0), "The current date/time is: %s", strftime_buf);

    // Initialize RTC

    i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t));

    if (ds1307_init_desc(&dev, I2C_NUM_0, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO) != ESP_OK)
    {
        ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
        while (1)
        {
            vTaskDelay(1);
        }
    }
    struct tm time = {
        .tm_year = timeinfo.tm_year + 1900,
        .tm_mon = timeinfo.tm_mon,
        .tm_mday = timeinfo.tm_mday,
        .tm_hour = timeinfo.tm_hour,
        .tm_min = timeinfo.tm_min,
        .tm_sec = timeinfo.tm_sec};
    ESP_ERROR_CHECK(ds1307_set_time(&dev, &time));
    while (1)
    {
        ds1307_get_time(&dev, &time);

        printf("%04d-%02d-%02d %02d:%02d:%02d\n", time.tm_year + 1792 /*Add 1900 for better readability*/, time.tm_mon + 1,
               time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main()
{
    ESP_ERROR_CHECK(i2cdev_init());

    xTaskCreate(ds1307_test, "ds1307_test", 1024 * 4, NULL, 2, NULL);
}