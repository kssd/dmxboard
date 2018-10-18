#pragma once

#include <stdint.h>

extern volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
enum { RXCIE0 = 7, TXCIE0 = 6, UDRIE0 = 5, RXEN0 = 4, TXEN0 = 3,
       FE0 = 4, USBS0 = 3, UCSZ00 = 1, UPM00 = 4 };

#define USART_RX_vect host_usart_rx
#define USART_TX_vect host_usart_tx
#define USART_UDRE_vect host_usart_udre
#define ISR(vector) void vector()
