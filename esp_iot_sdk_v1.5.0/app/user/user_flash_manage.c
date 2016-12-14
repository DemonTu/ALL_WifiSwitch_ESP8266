#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "user_flash_manage.h"

SYSTEM_PARAM_STR esp_system_param;

/******************************************************************************
 * FunctionName : user_systemParam_init
 * Description  : system parame init
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_systemParam_init(void)
{
	/* 读取系统参数 */
	system_param_load(ESP_PARAM_START_SEC, 0, &esp_system_param, sizeof(esp_system_param));
	if (esp_system_param.flag != 0xabcd)
	{
		/* 第一次上电 系统参数初始化 */
		os_memset(&esp_system_param, 0, sizeof(esp_system_param));
		esp_system_param.flag = 0xabcd;
		esp_system_param.activeflag = 0;
		esp_system_param.BID = 0;
		os_memcpy(esp_system_param.devkey, "12345678", 8);
		os_memcpy(esp_system_param.deviceID, "0123456789000000", 16);
		os_memcpy(esp_system_param.ssid, "FJXMYKD", 7);
		os_memcpy(esp_system_param.password, "FJXMYKD456123", 13);
		system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_system_param, sizeof(esp_system_param));
		
	}
	os_printf("BID=%d\r\n", esp_system_param.BID);
}

