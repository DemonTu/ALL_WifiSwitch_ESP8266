#ifndef __USER_DEVICE_H__
#define __USER_DEVICE_H__

/* NOTICE---this is for 4MB spi flash.
 * you can change to other sector if you use other size spi flash. */
#define ESP_PARAM_START_SEC		0x100	// 第256个扇区

#define packet_size   (2 * 1024)

struct esp_platform_saved_param {
	uint16 flag;			// flash标志位，用于出厂设置
	uint16 BID; 			// 设备激活标志
    uint8  devkey[8];
    uint8  deviceID[16];
    uint8  ssid[32];
    uint8  password[64];

    uint8  activeflag;		// 是否在sta模式 1:sta  0:ap_sta
    uint8  pad[3];
};

enum {
    DEVICE_CONNECTING = 40,
    DEVICE_ACTIVE_DONE,
    DEVICE_ACTIVE_FAIL,
    DEVICE_CONNECT_SERVER_FAIL
};

struct dhcp_client_info {
	ip_addr_t ip_addr;
	ip_addr_t netmask;
	ip_addr_t gw;
	uint8 flag;
	uint8 pad[3];
};
#endif
