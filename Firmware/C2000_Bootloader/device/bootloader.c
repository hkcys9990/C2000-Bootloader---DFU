#include "bootloader.h"

#pragma DATA_SECTION(Bootloader, "Bootloader");
BootLoader_t Bootloader;
uint32_t findSector(uint32_t start_addr, uint32_t end_addr, uint32_t* sector_count, uint32_t* sector_addr_buffer);

void bootloader_init(BootLoader_t* blr)
{
    uint32_t i;
    //for(i = 0; i<sizeof(blr->write_buffer); i++)
    for(i = 0; i<512; i++)
    {
        blr->write_buffer[i] = 0xFFFFFFFF;
    }
    blr->end_dfu_counter = 1000 * 10;
    blr->is_dfu_mode = 0;
}
void bootloader_downcounting(BootLoader_t* blr)
{
    if(blr->end_dfu_counter != 0x00)
    {
        blr->end_dfu_counter --;
    }
}
void task_idle(BootLoader_t* blr)
{
    if(blr->erase_trigger)
    {
        blr->erase_trigger = 0x00;
        blr->state = BOOTLOADER_STATE_ERASING;
    }
    else if (blr->write_trigger)
    {
        blr->write_trigger = 0x00;
        blr->task_state = 0x00;
        blr->state = BOOTLOADER_STATE_WRITING;
    }
    else if (blr->entry_trigger)
    {
        blr->entry_trigger = 0x00;
        blr->state = BOOTLOADER_STATE_ENTRY;
    }else
    {
        if(blr->is_dfu_mode == 0)
        {
            if(blr->end_dfu_counter == 0)
            {
                ((void (*)(void))(0x083000))();
            }
        }
    }
}
void task_erasing(BootLoader_t* blr)
{
    uint32_t i;
    for (i = 0; i<blr->erase_buffer_length; i++)
    {
        Fapi_StatusType oReturnCheck;
        Fapi_FlashStatusWordType oFlashStatusWord;
        Fapi_FlashStatusType oFlashStatus;

        uint32_t start_addr = blr->erase_start_address_buffer[i];
        uint32_t end_addr = start_addr + (blr->erase_data_length_buffer[i] - 1);
        static uint32_t sector_count;
        static uint32_t sector_address[16 * 3];
        findSector(start_addr, end_addr, (uint32_t*)&sector_count, (uint32_t *)sector_address);
        uint32_t j;
        for(j = 0; j<sector_count; j++)
        {
            oReturnCheck = Fapi_issueAsyncCommand(Fapi_ClearMore);
            while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady){}
            if(oReturnCheck != Fapi_Status_Success)
            {
                blr->error |= 1<<ERASE_ERROR_BIT;
                blr->state = BOOTLOADER_STATE_IDLE;
                return;
            }
            oReturnCheck = Fapi_issueAsyncCommandWithAddress( Fapi_EraseSector, (uint32_t *) sector_address[j]);
            while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady){}
            oFlashStatus = Fapi_getFsmStatus();
            oReturnCheck = Fapi_doBlankCheck(
                    (uint32_t *) sector_address[j], Sector_u32length,
                    &oFlashStatusWord);
            if (oReturnCheck != Fapi_Status_Success || oFlashStatus != 0)
            {
                blr->error |= 1<<ERASE_ERROR_BIT;
                blr->state = BOOTLOADER_STATE_IDLE;
                return;
            }
        }
    }
    blr->error &= ~(1<<ERASE_ERROR_BIT);
    blr->state = BOOTLOADER_STATE_IDLE;
}
void set_next_state_with_delay(BootLoader_t* blr,  uint32_t delay_cycle, uint32_t nextstate)
{
    blr->task_state_delay_counter = delay_cycle;
    blr->task_state = BOOTLOADER_STATE_DELAY;
    blr->task_nextstate = nextstate;
}
void set_next_state(BootLoader_t* blr,uint32_t nextstate)
{
    blr->task_state = nextstate;
}

void task_writing(BootLoader_t* blr)
{
    static uint32_t i;
    static uint32_t flash_writing_cycle;
    static uint32_t flash_writing_address;
    static uint32_t flash_writing_cycle_counter;
    switch(blr->task_state)
    {
        case TASK_GET_ALL_PARAMETER:
            if(blr->write_buffer_length>512)
            {
                blr->error |= 1<<WRITING_ERROR_BIT;
                blr->state = BOOTLOADER_STATE_IDLE;
                return;
            }
            for(i = blr->write_buffer_length; i<512; i++)
            {
                blr->write_buffer[i] = 0xFFFFFFFF;
            }
            flash_writing_cycle_counter = 0;
            flash_writing_cycle = (blr->write_buffer_length + (4-1))/4;
            flash_writing_address = blr->write_address_pointer;
            set_next_state(blr,TASK_WRITING_FLASHWRITING);
            break;
        case TASK_WRITING_FLASHWRITING:
            if(flash_writing_cycle_counter<flash_writing_cycle)
            {
                Fapi_StatusType oReturnCheck;
                Fapi_FlashStatusType oFlashStatus;
                uint16_t flash_program_buffer[4];
                flash_program_buffer[0] = blr->write_buffer[4*flash_writing_cycle_counter];
                flash_program_buffer[1] = blr->write_buffer[1 + 4*flash_writing_cycle_counter];
                flash_program_buffer[2] = blr->write_buffer[2 + 4*flash_writing_cycle_counter];
                flash_program_buffer[3] = blr->write_buffer[3 + 4*flash_writing_cycle_counter];
                oReturnCheck = Fapi_issueProgrammingCommand(
                        (uint32_t *) flash_writing_address, flash_program_buffer,
                        sizeof(flash_program_buffer), 0, 0, Fapi_AutoEccGeneration);
                while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy);
                oFlashStatus = Fapi_getFsmStatus();
                if (oReturnCheck != Fapi_Status_Success || oFlashStatus != 0)
                {
                    blr->error |= 1<<WRITING_ERROR_BIT;
                    blr->state = BOOTLOADER_STATE_IDLE;
                    return;
                }

                for(i = 0; i<4; i+=2)
                {
                    Fapi_FlashStatusWordType oFlashStatusWord;
                    uint32_t data;
                    data = flash_program_buffer[i + 1];
                    data = (data<<16) | flash_program_buffer[i];
                    oReturnCheck = Fapi_doVerify(
                            (uint32_t *) (flash_writing_address+i), 1,\
                            (uint32_t *) (&data), &oFlashStatusWord);

                    if (oReturnCheck != Fapi_Status_Success)
                    {
                        blr->error |= 1<<WRITING_ERROR_BIT;
                        blr->state = BOOTLOADER_STATE_IDLE;
                        return;
                    }
                }
                flash_writing_address+=4;
                flash_writing_cycle_counter++;
            }else
            {
                set_next_state(blr,TASK_GET_ALL_PARAMETER);
                blr->error &= ~(1<<WRITING_ERROR_BIT);
                blr->state = BOOTLOADER_STATE_IDLE;
            }
            break;
    }
}

void bootloader_handler(BootLoader_t* blr)
{
    switch(blr->state)
    {
        case BOOTLOADER_STATE_IDLE:
            task_idle(blr);
            break;
        case BOOTLOADER_STATE_ERASING:
            task_erasing(blr);
            break;
        case BOOTLOADER_STATE_WRITING:
            task_writing(blr);
            break;
        case BOOTLOADER_STATE_ENTRY:
            ((void (*)(void))(blr->entry_address))();
            break;
    }
}
uint32_t findSector(uint32_t start_addr, uint32_t end_addr, uint32_t* sector_count, uint32_t* sector_addr_buffer)
{
    *sector_count = 0;
    if(((start_addr >= Bzero_Sector0_start) && (start_addr < Bzero_Sector1_start)) \
            || ((end_addr >= Bzero_Sector0_start) && (end_addr < Bzero_Sector1_start))\
            || ((start_addr < Bzero_Sector0_start) && (end_addr >= Bzero_Sector1_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector0_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector1_start) && (start_addr < Bzero_Sector2_start)) \
            || ((end_addr >= Bzero_Sector1_start) && (end_addr < Bzero_Sector2_start))\
            || ((start_addr < Bzero_Sector1_start) && (end_addr >= Bzero_Sector2_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector1_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector2_start) && (start_addr < Bzero_Sector3_start)) \
            || ((end_addr >= Bzero_Sector2_start) && (end_addr < Bzero_Sector3_start))\
            || ((start_addr < Bzero_Sector2_start) && (end_addr >= Bzero_Sector3_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector2_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector3_start) && (start_addr < Bzero_Sector4_start)) \
            || ((end_addr >= Bzero_Sector3_start) && (end_addr < Bzero_Sector4_start))\
            || ((start_addr < Bzero_Sector3_start) && (end_addr >= Bzero_Sector4_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector3_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector4_start) && (start_addr < Bzero_Sector5_start)) \
            || ((end_addr >= Bzero_Sector4_start) && (end_addr < Bzero_Sector5_start))\
            || ((start_addr < Bzero_Sector4_start) && (end_addr >= Bzero_Sector5_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector4_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector5_start) && (start_addr < Bzero_Sector6_start)) \
            || ((end_addr >= Bzero_Sector5_start) && (end_addr < Bzero_Sector6_start))\
            || ((start_addr < Bzero_Sector5_start) && (end_addr >= Bzero_Sector6_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector5_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector6_start) && (start_addr < Bzero_Sector7_start)) \
            || ((end_addr >= Bzero_Sector6_start) && (end_addr < Bzero_Sector7_start))\
            || ((start_addr < Bzero_Sector6_start) && (end_addr >= Bzero_Sector7_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector6_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector7_start) && (start_addr < Bzero_Sector8_start)) \
            || ((end_addr >= Bzero_Sector7_start) && (end_addr < Bzero_Sector8_start))\
            || ((start_addr < Bzero_Sector7_start) && (end_addr >= Bzero_Sector8_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector7_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector8_start) && (start_addr < Bzero_Sector9_start)) \
            || ((end_addr >= Bzero_Sector8_start) && (end_addr < Bzero_Sector9_start))\
            || ((start_addr < Bzero_Sector8_start) && (end_addr >= Bzero_Sector9_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector8_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector9_start) && (start_addr < Bzero_Sector10_start)) \
            || ((end_addr >= Bzero_Sector9_start) && (end_addr < Bzero_Sector10_start))\
            || ((start_addr < Bzero_Sector9_start) && (end_addr >= Bzero_Sector10_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector9_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector10_start) && (start_addr < Bzero_Sector11_start)) \
            || ((end_addr >= Bzero_Sector10_start) && (end_addr < Bzero_Sector11_start))\
            || ((start_addr < Bzero_Sector10_start) && (end_addr >= Bzero_Sector11_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector10_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector11_start) && (start_addr < Bzero_Sector12_start)) \
            || ((end_addr >= Bzero_Sector11_start) && (end_addr < Bzero_Sector12_start))\
            || ((start_addr < Bzero_Sector11_start) && (end_addr >= Bzero_Sector12_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector11_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector12_start) && (start_addr < Bzero_Sector13_start)) \
            || ((end_addr >= Bzero_Sector12_start) && (end_addr < Bzero_Sector13_start))\
            || ((start_addr < Bzero_Sector12_start) && (end_addr >= Bzero_Sector13_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector12_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector13_start) && (start_addr < Bzero_Sector14_start)) \
            || ((end_addr >= Bzero_Sector13_start) && (end_addr < Bzero_Sector14_start))\
            || ((start_addr < Bzero_Sector13_start) && (end_addr >= Bzero_Sector14_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector13_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector14_start) && (start_addr < Bzero_Sector15_start)) \
            || ((end_addr >= Bzero_Sector14_start) && (end_addr < Bzero_Sector15_start))\
            || ((start_addr < Bzero_Sector14_start) && (end_addr >= Bzero_Sector15_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector14_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bzero_Sector15_start) && (start_addr <= FlashBank0EndAddress)) \
            || ((end_addr >= Bzero_Sector15_start) && (end_addr <= FlashBank0EndAddress))\
            || ((start_addr < Bzero_Sector15_start) && (end_addr > FlashBank0EndAddress)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bzero_Sector15_start;
        *sector_count = *sector_count +1;
    }

    ///
    if(((start_addr >= Bone_Sector0_start) && (start_addr < Bone_Sector1_start)) \
            || ((end_addr >= Bone_Sector0_start) && (end_addr < Bone_Sector1_start))\
            || ((start_addr < Bone_Sector0_start) && (end_addr >= Bone_Sector1_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector0_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector1_start) && (start_addr < Bone_Sector2_start)) \
            || ((end_addr >= Bone_Sector1_start) && (end_addr < Bone_Sector2_start))\
            || ((start_addr < Bone_Sector1_start) && (end_addr >= Bone_Sector2_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector1_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector2_start) && (start_addr < Bone_Sector3_start)) \
            || ((end_addr >= Bone_Sector2_start) && (end_addr < Bone_Sector3_start))\
            || ((start_addr < Bone_Sector2_start) && (end_addr >= Bone_Sector3_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector2_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector3_start) && (start_addr < Bone_Sector4_start)) \
            || ((end_addr >= Bone_Sector3_start) && (end_addr < Bone_Sector4_start))\
            || ((start_addr < Bone_Sector3_start) && (end_addr >= Bone_Sector4_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector3_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector4_start) && (start_addr < Bone_Sector5_start)) \
            || ((end_addr >= Bone_Sector4_start) && (end_addr < Bone_Sector5_start))\
            || ((start_addr < Bone_Sector4_start) && (end_addr >= Bone_Sector5_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector4_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector5_start) && (start_addr < Bone_Sector6_start)) \
            || ((end_addr >= Bone_Sector5_start) && (end_addr < Bone_Sector6_start))\
            || ((start_addr < Bone_Sector5_start) && (end_addr >= Bone_Sector6_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector5_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector6_start) && (start_addr < Bone_Sector7_start)) \
            || ((end_addr >= Bone_Sector6_start) && (end_addr < Bone_Sector7_start))\
            || ((start_addr < Bone_Sector6_start) && (end_addr >= Bone_Sector7_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector6_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector7_start) && (start_addr < Bone_Sector8_start)) \
            || ((end_addr >= Bone_Sector7_start) && (end_addr < Bone_Sector8_start))\
            || ((start_addr < Bone_Sector7_start) && (end_addr >= Bone_Sector8_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector7_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector8_start) && (start_addr < Bone_Sector9_start)) \
            || ((end_addr >= Bone_Sector8_start) && (end_addr < Bone_Sector9_start))\
            || ((start_addr < Bone_Sector8_start) && (end_addr >= Bone_Sector9_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector8_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector9_start) && (start_addr < Bone_Sector10_start)) \
            || ((end_addr >= Bone_Sector9_start) && (end_addr < Bone_Sector10_start))\
            || ((start_addr < Bone_Sector9_start) && (end_addr >= Bone_Sector10_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector9_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector10_start) && (start_addr < Bone_Sector11_start)) \
            || ((end_addr >= Bone_Sector10_start) && (end_addr < Bone_Sector11_start))\
            || ((start_addr < Bone_Sector10_start) && (end_addr >= Bone_Sector11_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector10_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector11_start) && (start_addr < Bone_Sector12_start)) \
            || ((end_addr >= Bone_Sector11_start) && (end_addr < Bone_Sector12_start))\
            || ((start_addr < Bone_Sector11_start) && (end_addr >= Bone_Sector12_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector11_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector12_start) && (start_addr < Bone_Sector13_start)) \
            || ((end_addr >= Bone_Sector12_start) && (end_addr < Bone_Sector13_start))\
            || ((start_addr < Bone_Sector12_start) && (end_addr >= Bone_Sector13_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector12_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector13_start) && (start_addr < Bone_Sector14_start)) \
            || ((end_addr >= Bone_Sector13_start) && (end_addr < Bone_Sector14_start))\
            || ((start_addr < Bone_Sector13_start) && (end_addr >= Bone_Sector14_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector13_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector14_start) && (start_addr < Bone_Sector15_start)) \
            || ((end_addr >= Bone_Sector14_start) && (end_addr < Bone_Sector15_start))\
            || ((start_addr < Bone_Sector14_start) && (end_addr >= Bone_Sector15_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector14_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Bone_Sector15_start) && (start_addr <= FlashBank1EndAddress)) \
            || ((end_addr >= Bone_Sector15_start) && (end_addr <= FlashBank1EndAddress))\
            || ((start_addr < Bone_Sector15_start) && (end_addr > FlashBank1EndAddress)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Bone_Sector15_start;
        *sector_count = *sector_count +1;
    }
    ///


    if(((start_addr >= Btwo_Sector0_start) && (start_addr < Btwo_Sector1_start)) \
            || ((end_addr >= Btwo_Sector0_start) && (end_addr < Btwo_Sector1_start))\
            || ((start_addr < Btwo_Sector0_start) && (end_addr >= Btwo_Sector1_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector0_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector1_start) && (start_addr < Btwo_Sector2_start)) \
            || ((end_addr >= Btwo_Sector1_start) && (end_addr < Btwo_Sector2_start))\
            || ((start_addr < Btwo_Sector1_start) && (end_addr >= Btwo_Sector2_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector1_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector2_start) && (start_addr < Btwo_Sector3_start)) \
            || ((end_addr >= Btwo_Sector2_start) && (end_addr < Btwo_Sector3_start))\
            || ((start_addr < Btwo_Sector2_start) && (end_addr >= Btwo_Sector3_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector2_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector3_start) && (start_addr < Btwo_Sector4_start)) \
            || ((end_addr >= Btwo_Sector3_start) && (end_addr < Btwo_Sector4_start))\
            || ((start_addr < Btwo_Sector3_start) && (end_addr >= Btwo_Sector4_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector3_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector4_start) && (start_addr < Btwo_Sector5_start)) \
            || ((end_addr >= Btwo_Sector4_start) && (end_addr < Btwo_Sector5_start))\
            || ((start_addr < Btwo_Sector4_start) && (end_addr >= Btwo_Sector5_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector4_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector5_start) && (start_addr < Btwo_Sector6_start)) \
            || ((end_addr >= Btwo_Sector5_start) && (end_addr < Btwo_Sector6_start))\
            || ((start_addr < Btwo_Sector5_start) && (end_addr >= Btwo_Sector6_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector5_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector6_start) && (start_addr < Btwo_Sector7_start)) \
            || ((end_addr >= Btwo_Sector6_start) && (end_addr < Btwo_Sector7_start))\
            || ((start_addr < Btwo_Sector6_start) && (end_addr >= Btwo_Sector7_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector6_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector7_start) && (start_addr < Btwo_Sector8_start)) \
            || ((end_addr >= Btwo_Sector7_start) && (end_addr < Btwo_Sector8_start))\
            || ((start_addr < Btwo_Sector7_start) && (end_addr >= Btwo_Sector8_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector7_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector8_start) && (start_addr < Btwo_Sector9_start)) \
            || ((end_addr >= Btwo_Sector8_start) && (end_addr < Btwo_Sector9_start))\
            || ((start_addr < Btwo_Sector8_start) && (end_addr >= Btwo_Sector9_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector8_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector9_start) && (start_addr < Btwo_Sector10_start)) \
            || ((end_addr >= Btwo_Sector9_start) && (end_addr < Btwo_Sector10_start))\
            || ((start_addr < Btwo_Sector9_start) && (end_addr >= Btwo_Sector10_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector9_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector10_start) && (start_addr < Btwo_Sector11_start)) \
            || ((end_addr >= Btwo_Sector10_start) && (end_addr < Btwo_Sector11_start))\
            || ((start_addr < Btwo_Sector10_start) && (end_addr >= Btwo_Sector11_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector10_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector11_start) && (start_addr < Btwo_Sector12_start)) \
            || ((end_addr >= Btwo_Sector11_start) && (end_addr < Btwo_Sector12_start))\
            || ((start_addr < Btwo_Sector11_start) && (end_addr >= Btwo_Sector12_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector11_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector12_start) && (start_addr < Btwo_Sector13_start)) \
            || ((end_addr >= Btwo_Sector12_start) && (end_addr < Btwo_Sector13_start))\
            || ((start_addr < Btwo_Sector12_start) && (end_addr >= Btwo_Sector13_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector12_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector13_start) && (start_addr < Btwo_Sector14_start)) \
            || ((end_addr >= Btwo_Sector13_start) && (end_addr < Btwo_Sector14_start))\
            || ((start_addr < Btwo_Sector13_start) && (end_addr >= Btwo_Sector14_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector13_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector14_start) && (start_addr < Btwo_Sector15_start)) \
            || ((end_addr >= Btwo_Sector14_start) && (end_addr < Btwo_Sector15_start))\
            || ((start_addr < Btwo_Sector14_start) && (end_addr >= Btwo_Sector15_start)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector14_start;
        *sector_count = *sector_count +1;
    }

    if(((start_addr >= Btwo_Sector15_start) && (start_addr <= FlashBank2EndAddress)) \
            || ((end_addr >= Btwo_Sector15_start) && (end_addr <= FlashBank2EndAddress))\
            || ((start_addr < Btwo_Sector15_start) && (end_addr > FlashBank2EndAddress)))
    {
        sector_addr_buffer[*sector_count] = (uint32_t)Btwo_Sector15_start;
        *sector_count = *sector_count +1;
    }
}
