#ifndef UARTCOMMUNICATOR_H
#define UARTCOMMUNICATOR_H
#include "stm32f4xx_usart.h"

class UARTCommunicator{
private:
	USART_TypeDef* uart;

public:
	//for card checking
	static const char* IMMO_CARD_WAIT;
	static const char* IMMO_CARD_OK;

	//for adding a new card
	static const char* IMMO_ADD_WAIT;
	static const char* IMMO_ADD_OK;
	static const char* IMMO_ADD_ERR;

	UARTCommunicator(USART_TypeDef* uart);
	void putChar(const char ch);
	void putString(const char* message);

	//TODO: reading - using interrupts!
};

#endif
