#include "uart.h"


volatile uint8_t uartRxBuf[UART_RX_BUFSIZE];
volatile uint8_t uartRxBufIn, uartRxBufOut;
volatile bool uartFlagOverflow, uartFlagError;

#if (UART_RX_BUFSIZE & (UART_RX_BUFSIZE - 1)) != 0
#error "UART_RX_BUFSIZE must be a power of 2!"
#endif

volatile uint8_t uartTxBuf[UART_TX_BUFSIZE];
volatile uint8_t uartTxBufIn, uartTxBufOut;

#if (UART_TX_BUFSIZE & (UART_TX_BUFSIZE - 1)) != 0
#error "UART_TX_BUFSIZE must be a power of 2!"
#endif

volatile uint8_t uartTLastRx;
