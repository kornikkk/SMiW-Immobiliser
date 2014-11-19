#include "uartcommunicator.hpp"

const char* UARTCommunicator::IMMO_CARD_WAIT = "IMMO_CARD_WAIT";
const char* UARTCommunicator::IMMO_CARD_OK = "IMMO_CARD_OK";
const char* UARTCommunicator::IMMO_ADD_WAIT = "IMMO_ADD_WAIT";
const char* UARTCommunicator::IMMO_ADD_OK = "IMMO_ADD_OK";
const char* UARTCommunicator::IMMO_ADD_ERR = "IMMO_ADD_ERR";

UARTCommunicator::UARTCommunicator(USART_TypeDef* uart) {
	this->uart = uart;
}

void UARTCommunicator::putChar(const char ch) {
	while (USART_GetFlagStatus(uart, USART_FLAG_TXE) == RESET);
	USART_SendData(uart, (uint8_t)ch);
}

void UARTCommunicator::putString(const char* message) {
	int i = 0;
	while (message[i] != '\0') {
		putChar(message[i]);
	}
}
