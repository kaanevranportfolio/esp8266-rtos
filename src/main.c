#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart.h"

#define SCAN_INTERVAL_MS (10000)

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reserves 4 sectors, used for rf init data and
 *                 parameters. Required by the linker for every project.
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

static const char *auth_mode_to_str(AUTH_MODE auth_mode)
{
    switch (auth_mode) {
        case AUTH_OPEN:
            return "OPEN";
        case AUTH_WEP:
            return "WEP";
        case AUTH_WPA_PSK:
            return "WPA";
        case AUTH_WPA2_PSK:
            return "WPA2";
        case AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";
        default:
            return "UNKNOWN";
    }
}

static void scan_done_cb(void *arg, STATUS status)
{
    if (status != OK) {
        printf("WiFi scan failed, status: %d\n", status);
        printf("--------------------------------------------\n");
        return;
    }

    struct bss_info *bss_link = (struct bss_info *)arg;

    if (bss_link == NULL) {
        printf("No networks found.\n");
    } else {
        for (struct bss_info *bss = bss_link; bss != NULL; bss = bss->next.stqe_next) {
            printf("SSID: %s | RSSI: %d dBm | Channel: %d | Auth: %s\n",
                   (const char *)bss->ssid,
                   bss->rssi,
                   bss->channel,
                   auth_mode_to_str(bss->authmode));
        }
    }

    printf("--------------------------------------------\n");
}

static void wifi_scan_task(void *pvParameters)
{
    while (1) {
        wifi_station_scan(NULL, scan_done_cb);
        vTaskDelay(SCAN_INTERVAL_MS / portTICK_RATE_MS);
    }
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    /* The SDK brings UART0 up at 74880 baud (uart_init_new()'s hardcoded
     * rate) before user_init() runs. Switch it to 115200 to match
     * platformio.ini's monitor_speed, or all app output prints as garbage
     * in a monitor opened at 115200. */
    UART_SetBaudrate(UART0, BIT_RATE_115200);

    printf("SDK version: %s\n", system_get_sdk_version());

    wifi_set_opmode(STATION_MODE);

    xTaskCreate(wifi_scan_task, "wifi_scan_task", 2048, NULL, 2, NULL);
}
