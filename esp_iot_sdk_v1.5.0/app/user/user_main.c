/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"

#include "user_interface.h"

#include "user_devicefind.h"
#include "user_webserver.h"
#include "driver/uart.h"

#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

void user_rf_pre_init(void)
{
}

void ICACHE_FLASH_ATTR
user_set_station_config(void)
{

	struct station_config stationConf;
	os_memset(&stationConf, 0, sizeof(stationConf));
	stationConf.bssid_set = 0; //need not check MAC address of AP
	os_memcpy(&stationConf.ssid, "FJXMYKD", 7);
	os_memcpy(&stationConf.password, "FJXMYKD456123", 13);

	wifi_station_set_config_current(&stationConf);
}


/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
	struct rst_info *rtc_info = system_get_rst_info();
	
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	os_printf("reset reason: %x\n", rtc_info->reason);

	if (rtc_info->reason == REASON_WDT_RST ||
		rtc_info->reason == REASON_EXCEPTION_RST ||
		rtc_info->reason == REASON_SOFT_WDT_RST) 
	{
		if (rtc_info->reason == REASON_EXCEPTION_RST) 
		{
			os_printf("Fatal exception (%d):\n", rtc_info->exccause);
		}
	}
	
	os_printf("SDK version:%s\n", system_get_sdk_version());

	/* 设置WiFi为STA模式，连接指定的AP */
	wifi_set_opmode_current(STATION_MODE);
	user_set_station_config();
	
    os_printf("name=%s, %d\r\n", wifi_station_get_hostname());
	
#if ESP_PLATFORM
    /*Initialization of the peripheral drivers*/
    /*For light demo , it is user_light_init();*/
    /* Also check whether assigned ip addr by the router.If so, connect to ESP-server  */
    user_esp_platform_init();
#endif
    /*Establish a udp socket to receive local device detect info.*/
    /*Listen to the port 1025, as well as udp broadcast.
    /*If receive a string of device_find_request, it rely its IP address and MAC.*/
    user_devicefind_init();

    /*Establish a TCP server for http(with JSON) POST or GET command to communicate with the device.*/
    /*You can find the command in "2B-SDK-Espressif IoT Demo.pdf" to see the details.*/
    /*the JSON command for curl is like:*/
    /*3 Channel mode: curl -X POST -H "Content-Type:application/json" -d "{\"period\":1000,\"rgb\":{\"red\":16000,\"green\":16000,\"blue\":16000}}" http://192.168.4.1/config?command=light      */
    /*5 Channel mode: curl -X POST -H "Content-Type:application/json" -d "{\"period\":1000,\"rgb\":{\"red\":16000,\"green\":16000,\"blue\":16000,\"cwhite\":3000,\"wwhite\",3000}}" http://192.168.4.1/config?command=light      */
#ifdef SERVER_SSL_ENABLE
    user_webserver_init(SERVER_SSL_PORT);
#else
    user_webserver_init(SERVER_PORT);
#endif
}

