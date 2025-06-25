#ifndef DEVICE_UART_LINK_H_
#define DEVICE_UART_LINK_H_

#include "driverlib.h"
#include "device.h"
#include "board.h"

#define BUFFER_SIZE 30

typedef struct
{
    uint32_t command_length;
    uint32_t uart_buffer_size;
    uint32_t uart_rx_buffer[BUFFER_SIZE];
    uint32_t uart_rx_buffer_pointer;

    uint32_t uart_tx_buffer[BUFFER_SIZE];
    uint32_t uart_tx_in_pointer;
    uint32_t uart_tx_out_pointer;
    uint32_t ascii[16];
}stDebugLink;

void Uart_Init(void);
void DebugLink_Config(stDebugLink* d);
void DebugLink_MainLoop(stDebugLink* d);

extern stDebugLink g_DebugLink;


#endif /* DEVICE_UART_LINK_H_ */
