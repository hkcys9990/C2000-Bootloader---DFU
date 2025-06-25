#include "peripheral.h"
#include "bootloader.h"

__interrupt void epwm2_isr(void)
{
    HWREGH(EPWM2_BASE + EPWM_O_ETCLR) |= EPWM_ETCLR_INT;
    HWREGH(PIECTRL_BASE + PIE_O_ACK) = INTERRUPT_ACK_GROUP3;
    bootloader_downcounting((BootLoader_t *)&Bootloader);
}


void epwm2_Init(void)
{

   EALLOW;
   EPWM_setTimeBaseCounterMode(EPWM2_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);

   EPWM_setClockPrescaler(EPWM2_BASE,\
                           EPWM_CLOCK_DIVIDER_1,\
                           EPWM_HSCLOCK_DIVIDER_10);//12Mhz

   EPWM_setTimeBasePeriod(EPWM2_BASE, 12000-1);//1kHz;
   EPWM_setTimeBaseCounter(EPWM2_BASE, 0);

   EPWM_disablePhaseShiftLoad(EPWM2_BASE);
   EPWM_setPhaseShift(EPWM2_BASE, 0);
   EPWM_setSyncInPulseSource(EPWM2_BASE, EPWM_SYNC_IN_PULSE_SRC_DISABLE);
   EPWM_enableSyncOutPulseSource(EPWM2_BASE,EPWM_SYNC_OUT_PULSE_ON_CNTR_ZERO );
   EPWM_setCountModeAfterSync(EPWM2_BASE, EPWM_COUNT_MODE_UP_AFTER_SYNC);
   Interrupt_register(INT_EPWM2, &epwm2_isr);
   Interrupt_enable(INT_EPWM2);
   EPWM_setInterruptSource(EPWM2_BASE,EPWM_INT_TBCTR_ZERO);
   EPWM_setInterruptEventCount(EPWM2_BASE, 1);
   EPWM_clearEventTriggerInterruptFlag(EPWM2_BASE);
   EPWM_enableInterrupt(EPWM2_BASE);
   EPWM_setTimeBaseCounterMode(EPWM2_BASE, EPWM_COUNTER_MODE_UP);
   EDIS;
}


void flash_Init(void)
{
    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, 5);
    EALLOW;
    Fapi_StatusType oReturnCheck;
    oReturnCheck = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, 120);
    if(oReturnCheck != Fapi_Status_Success)
    {
    }
    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);
    if(oReturnCheck != Fapi_Status_Success)
    {
    }
    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank1);
    if(oReturnCheck != Fapi_Status_Success)
    {
    }
    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank2);
    if(oReturnCheck != Fapi_Status_Success)
    {
    }
    EDIS;

}

void Peripheral_Config(void)
{
    epwm2_Init();
    flash_Init();
}

