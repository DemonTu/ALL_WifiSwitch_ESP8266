#ifndef __USER_SWITCH_VERSION_H__
#define __USER_SWITCH_VERSION_H__

#include "user_config.h"

#define IOT_VERSION_MAJOR		1U  // 主版本号
#define IOT_VERSION_MINOR		0U  // 次要的
#define IOT_VERSION_REVISION	0U  // 修正版本

#define VERSION_NUM   (IOT_VERSION_MAJOR * 1000 + IOT_VERSION_MINOR * 100 + IOT_VERSION_REVISION)

#define VERSION_TYPE   	  "v"

#define VERSION_DATA      161213 // 日期

#define ONLINE_UPGRADE    0
#define LOCAL_UPGRADE     0
#define ALL_UPGRADE       1
#define NONE_UPGRADE      0

#if	ONLINE_UPGRADE
	#define UPGRADE_FALG	"O"
#elif  LOCAL_UPGRADE
	#define UPGRADE_FALG	"l"
#elif  ALL_UPGRADE
	#define UPGRADE_FALG	"a"
#elif NONE_UPGRADE
	#define UPGRADE_FALG	"n"
#endif

#define IOT_VERSION


#endif

