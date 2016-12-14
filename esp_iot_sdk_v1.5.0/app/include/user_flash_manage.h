#ifndef __USER_FLASH_MANAGE_H
    #define __USER_FLASH_MANAGE_H

/* NOTICE---this is for 4MB spi flash.
 * you can change to other sector if you use other size spi flash. */
#define ESP_PARAM_START_SEC		0x100	// 第256个扇区

typedef struct  {
	uint16 flag;			// flash标志位，用于出厂设置
	uint16 BID; 			// 设备激活标志
    uint8  devkey[8];
    uint8  deviceID[16];
    uint8  ssid[32];        // 不要为32，要留一位放'\0'
    uint8  password[64];    // 不要为64，要留一位放'\0'

    uint8  activeflag;		// 是否在sta模式 1:sta  0:ap_sta
    uint8  pad[3];
}SYSTEM_PARAM_STR;

extern SYSTEM_PARAM_STR esp_system_param;

extern void  user_systemParam_init(void);
#endif

