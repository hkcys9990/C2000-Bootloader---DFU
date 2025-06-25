#include <uart_link.h>
#include "driverlib.h"
#include "device.h"
#include "board.h"

#include "global_variable.h"
#include "bootloader.h"
#include "peripheral.h"

void main(void)
{
    Device_init();
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Interrupt_enableMaster();
    Peripheral_Config();
    Uart_Init();
    DebugLink_Config(&g_DebugLink);
    bootloader_init((BootLoader_t *)&Bootloader);
    for(;;)
    {
        bootloader_handler((BootLoader_t *)&Bootloader);
        DebugLink_MainLoop(&g_DebugLink);
    }
}
