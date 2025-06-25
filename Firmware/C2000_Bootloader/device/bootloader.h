#ifndef DEVICE_BOOTLOADER_H_
#define DEVICE_BOOTLOADER_H_

#include "device.h"
#include "global_variable.h"
#include "flash.h"
#include "F021_F28003x_C28x.h"

typedef struct{
    uint32_t state;
    uint32_t error;
    uint32_t entry_trigger;
    uint32_t entry_address;
    uint32_t erase_trigger;
    uint32_t erase_buffer_length;
    uint32_t erase_start_address_buffer[32];
    uint32_t erase_data_length_buffer[32];
    uint32_t write_trigger;
    uint32_t write_address_pointer;
    uint32_t write_buffer_length;
    uint32_t write_buffer[512];

    uint32_t task_state;
    uint32_t task_nextstate;
    uint32_t task_state_delay_counter;
    uint32_t is_dfu_mode;
    uint32_t end_dfu_counter;

}BootLoader_t;

// Bank0 Sector start addresses
#define FlashBank0StartAddress      0x80000U
#define Bzero_Sector0_start         0x80000U
#define Bzero_Sector1_start         0x81000U
#define Bzero_Sector2_start         0x82000U
#define Bzero_Sector3_start         0x83000U
#define Bzero_Sector4_start         0x84000U
#define Bzero_Sector5_start         0x85000U
#define Bzero_Sector6_start         0x86000U
#define Bzero_Sector7_start         0x87000U
#define Bzero_Sector8_start         0x88000U
#define Bzero_Sector9_start         0x89000U
#define Bzero_Sector10_start        0x8A000U
#define Bzero_Sector11_start        0x8B000U
#define Bzero_Sector12_start        0x8C000U
#define Bzero_Sector13_start        0x8D000U
#define Bzero_Sector14_start        0x8E000U
#define Bzero_Sector15_start        0x8F000U
#define FlashBank0EndAddress        0x8FFFFU

#define FlashBank1StartAddress     0x90000U
#define Bone_Sector0_start         0x90000U
#define Bone_Sector1_start         0x91000U
#define Bone_Sector2_start         0x92000U
#define Bone_Sector3_start         0x93000U
#define Bone_Sector4_start         0x94000U
#define Bone_Sector5_start         0x95000U
#define Bone_Sector6_start         0x96000U
#define Bone_Sector7_start         0x97000U
#define Bone_Sector8_start         0x98000U
#define Bone_Sector9_start         0x99000U
#define Bone_Sector10_start        0x9A000U
#define Bone_Sector11_start        0x9B000U
#define Bone_Sector12_start        0x9C000U
#define Bone_Sector13_start        0x9D000U
#define Bone_Sector14_start        0x9E000U
#define Bone_Sector15_start        0x9F000U
#define FlashBank1EndAddress       0x9FFFFU

#define FlashBank2StartAddress     0xA0000U
#define Btwo_Sector0_start         0xA0000U
#define Btwo_Sector1_start         0xA1000U
#define Btwo_Sector2_start         0xA2000U
#define Btwo_Sector3_start         0xA3000U
#define Btwo_Sector4_start         0xA4000U
#define Btwo_Sector5_start         0xA5000U
#define Btwo_Sector6_start         0xA6000U
#define Btwo_Sector7_start         0xA7000U
#define Btwo_Sector8_start         0xA8000U
#define Btwo_Sector9_start         0xA9000U
#define Btwo_Sector10_start        0xAA000U
#define Btwo_Sector11_start        0xAB000U
#define Btwo_Sector12_start        0xAC000U
#define Btwo_Sector13_start        0xAD000U
#define Btwo_Sector14_start        0xAE000U
#define Btwo_Sector15_start        0xAF000U
#define FlashBank2EndAddress       0xAFFFFU

#define Sector_u32length           0x800

void bootloader_init(BootLoader_t* blr);
void bootloader_handler(BootLoader_t* blr);
void bootloader_downcounting(BootLoader_t* blr);



extern BootLoader_t Bootloader;

#endif /* DEVICE_BOOTLOADER_H_ */
