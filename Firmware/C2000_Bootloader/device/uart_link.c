#include <uart_link.h>

stDebugLink g_DebugLink;

#define STX 0x02
#define ETX 0x03
uint16_t hextodec(uint32_t* data,uint32_t* dec);
void DebugLink_Config(stDebugLink* d)
{
    d->command_length =28;
    d->uart_buffer_size = BUFFER_SIZE;
    d->uart_rx_buffer_pointer = 0;
    d->uart_tx_in_pointer = 0;
    d->uart_tx_out_pointer = 0;
    d->ascii[0] = '0';
    d->ascii[1] = '1';
    d->ascii[2] = '2';
    d->ascii[3] = '3';
    d->ascii[4] = '4';
    d->ascii[5] = '5';
    d->ascii[6] = '6';
    d->ascii[7] = '7';
    d->ascii[8] = '8';
    d->ascii[9] = '9';
    d->ascii[10] = 'A';
    d->ascii[11] = 'B';
    d->ascii[12] = 'C';
    d->ascii[13] = 'D';
    d->ascii[14] = 'E';
    d->ascii[15] = 'F';

}

void DebugLink_MainLoop(stDebugLink* d)
{
    uint16_t start_index, i;
    uint32_t addr, data, command[28];
    while(SCI_getRxFIFOStatus(SCIA_BASE) != SCI_FIFO_RX0)
    {
        d->uart_rx_buffer[d->uart_rx_buffer_pointer++] = ((uint16_t)(HWREGH(SCIA_BASE + SCI_O_RXBUF) & SCI_RXBUF_SAR_M));

        d->uart_rx_buffer_pointer%= d->uart_buffer_size;
        start_index = (d->uart_rx_buffer_pointer + d->uart_buffer_size - 1 - (d->command_length - 1)) % (d->uart_buffer_size);
        for(i = 0; i<d->command_length; i++)
        {
            command[i] = d->uart_rx_buffer[start_index++];
            start_index %= d->uart_buffer_size;
        }
        if(command[0] == STX && command[27] == ETX && command [1]== '0' && command[2] == '0' && command [7]== '0' && command[8] == '0')
        {
            if(command[3] == '0'&& command[4] == '1' && command[5] == '0'&& command[6] == '1' )
            {
                hextodec((uint32_t*)&command[9], &addr);
                hextodec((uint32_t*)&command[17], &data);
                *(uint32_t*)addr = data;//general writing data
            }
            else if(command[3] == '0'&& command[4] == '2' && command[5] == '0'&& command[6] == '2' )
            {
                hextodec((uint32_t*)&command[9], &addr);
                data = *(uint32_t*)addr;//general read data
                command[17] = d->ascii[(data>> 28)&0xf];
                command[18] = d->ascii[(data>> 24)&0xf];
                command[19] = d->ascii[(data>> 20)&0xf];
                command[20] = d->ascii[(data>> 16)&0xf];
                command[21] = d->ascii[(data>> 12)&0xf];
                command[22] = d->ascii[(data>> 8)&0xf];
                command[23] = d->ascii[(data>> 4)&0xf];
                command[24] = d->ascii[(data)&0xf];
            }
            d->uart_rx_buffer_pointer = 0;
            command[4]++;// protocol define the header return value must +1
            for(i = 0; i<d->command_length; i++)
            {
                d->uart_tx_buffer[d->uart_tx_in_pointer++] = command[i];
                d->uart_tx_in_pointer %= d->uart_buffer_size;
            }
        }
    }
    while(d->uart_tx_in_pointer != d->uart_tx_out_pointer) // handling the TX fifo
    {
        if(((SCI_TxFIFOLevel)((HWREGH(SCIA_BASE + SCI_O_FFTX) & SCI_FFTX_TXFFST_M) >>
                SCI_FFTX_TXFFST_S)) == SCI_FIFO_TX16)// fifo full
        {
            break;
        }
        HWREGH(SCIA_BASE + SCI_O_TXBUF) = d->uart_tx_buffer[d->uart_tx_out_pointer++]; // filling the fifo
        d->uart_tx_out_pointer %= d->uart_buffer_size;
    }
}



void Uart_Init(void)
{
    EALLOW;
    GPIO_setPinConfig(GPIO_28_SCIA_RX);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(28, GPIO_QUAL_SYNC);

    GPIO_setPinConfig(GPIO_29_SCIA_TX);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

    SCI_performSoftwareReset(SCIA_BASE);
    SCI_clearInterruptStatus(SCIA_BASE, SCI_INT_RXFF | SCI_INT_TXFF | SCI_INT_FE | SCI_INT_OE | SCI_INT_PE | SCI_INT_RXERR | SCI_INT_RXRDY_BRKDT | SCI_INT_TXRDY);
    SCI_clearOverflowStatus(SCIA_BASE);
    SCI_resetTxFIFO(SCIA_BASE);
    SCI_resetRxFIFO(SCIA_BASE);
    SCI_resetChannels(SCIA_BASE);

    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, 115200 , (SCI_CONFIG_WLEN_8|SCI_CONFIG_STOP_ONE|SCI_CONFIG_PAR_NONE));
    SCI_disableLoopback(SCIA_BASE);
    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setFIFOInterruptLevel(SCIA_BASE, SCI_FIFO_TX16, SCI_FIFO_RX16);
    SCI_enableFIFO(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);

}
uint16_t hextodec(uint32_t* data,uint32_t* dec)
{
    *dec = 0;
    uint16_t i;
    for(i = 0; i<8; i++)
    {
        if(data[7-i]<58)
        {
            *dec+=((uint32_t)(data[7-i] -48)<<(i*4));
        }
        else if (data[7-i]>64 && data[7-i]<71)
        {
            *dec+=((uint32_t)(data[7-i] -55)<<(i*4));
        }
        else if (data[7-i]>96 && data[7-i]<103)
        {
            *dec+=((uint32_t)(data[7-i] -87)<<(i*4));
        }
        else
        {
            return 0;
        }
    }
    return 0xffff;
}
