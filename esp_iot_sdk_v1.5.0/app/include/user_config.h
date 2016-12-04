#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define ESP_PLATFORM        1

#define USE_OPTIMIZE_PRINTF

#if ESP_PLATFORM
#define PLUG_DEVICE             1

//#define SERVER_SSL_ENABLE
//#define CLIENT_SSL_ENABLE
//#define UPGRADE_SSL_ENABLE

#define USE_DNS

#ifdef USE_DNS
#define ESP_DOMAIN      "iot.espressif.cn"
#endif

#define BEACON_TIME     5000			// 心跳周期 50s 测试时5000=5s

#define AP_CACHE           0

#if AP_CACHE
#define AP_CACHE_NUMBER    5
#endif

#endif

#endif

