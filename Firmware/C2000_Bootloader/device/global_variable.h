
#ifndef DEVICE_GLOBAL_VARIABLE_H_
#define DEVICE_GLOBAL_VARIABLE_H_

#include "device.h"

#define STX 0x02
#define ETX 0x03

#define BOOTLOADER_STATE_IDLE                   0x00
#define BOOTLOADER_STATE_ERASING                0x01
#define BOOTLOADER_STATE_WRITING                0x02
#define BOOTLOADER_STATE_ENTRY                  0x03
#define BOOTLOADER_STATE_DELAY                  0xff

#define ERASE_ERROR_BIT 0x00
#define WRITING_ERROR_BIT 0x01

#define TASK_GET_ALL_PARAMETER 0x00
#define TASK_WRITING_FLASHWRITING 0x01


#endif /* DEVICE_GLOBAL_VARIABLE_H_ */
