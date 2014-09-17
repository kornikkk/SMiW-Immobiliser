#include "main.h"

//TODO: better delay
void time_waste() {
	for (uint32_t i=0; i<100000; ++i);
}

void blink_LED(GPIO_TypeDef* gpio, uint16_t led, uint32_t times) {
	for (uint32_t i=0; i<times; ++i) {
		GPIO_WriteBit(gpio, led, Bit_SET);
		for (uint32_t j=0; j<150000; ++j);
		GPIO_WriteBit(gpio, led, Bit_RESET);
		for (uint32_t j=0; j<150000; ++j);
	}
}

void init_SPI_RFIDReader() {
	GPIO_InitTypeDef GPIO_InitStruct;
	SPI_InitTypeDef SPI_InitStruct;

	// enable clock for used IO pins
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* configure pins used by SPI1
	 * PA5 = SCK
	 * PA6 = MISO
	 * PA7 = MOSI
	 */
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	// connect SPI1 pins to SPI alternate function
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);

	// enable clock for used IO pins
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	/* Configure the chip select pin (PE7) and output to relay (PE0)*/
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStruct);

	GPIOE->BSRRL |= GPIO_Pin_7; // set PE7 high

	// enable peripheral clock
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* configure SPI1 in Mode 0
	 * CPOL = 0 --> clock is low when idle
	 * CPHA = 0 --> data is sampled at the first edge
	 */
	SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // set to full duplex mode, seperate MOSI and MISO lines
	SPI_InitStruct.SPI_Mode = SPI_Mode_Master;     // transmit in master mode, NSS pin has to be always high
	SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b; // one packet of data is 8 bits wide
	SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;        // clock is low when idle
	SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;      // data sampled at first edge
	SPI_InitStruct.SPI_NSS = SPI_NSS_Soft | SPI_NSSInternalSoft_Set; // set the NSS management to internal and pull internal NSS high
	SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; // SPI frequency is APB2 frequency / 4
	SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;// data is transmitted MSB first
	SPI_Init(SPI1, &SPI_InitStruct);

	SPI_Cmd(SPI1, ENABLE); // enable SPI1
}

void init_RFID_LEDS() {
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	GPIO_InitTypeDef gpioStructure;
	gpioStructure.GPIO_Pin = RFID_LED_READ | RFID_LED_OK | RFID_LED_ADD | RFID_LED_ERROR;
	gpioStructure.GPIO_Mode = GPIO_Mode_OUT;
	gpioStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &gpioStructure);

	GPIO_WriteBit(GPIOD, RFID_LED_READ | RFID_LED_OK | RFID_LED_ADD | RFID_LED_ERROR, Bit_RESET);
}

void init_add_button() {
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef gpioStructure;
	gpioStructure.GPIO_Pin = RFID_BUTTON;
	gpioStructure.GPIO_Mode = GPIO_Mode_IN;
	gpioStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioStructure);
}

void init_relay_output() {
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	GPIO_InitTypeDef gpioStructure;
	gpioStructure.GPIO_Pin = RELAY_OUT;
	gpioStructure.GPIO_Mode = GPIO_Mode_OUT;
	gpioStructure.GPIO_OType = GPIO_OType_PP;
	gpioStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &gpioStructure);

	GPIO_WriteBit(GPIOE, RELAY_OUT, Bit_RESET);
}

int main() {
     init();

    immo_wait(); //waiting for immobiliser OK

    do {
        loop();
    } while (1);
}


void init() {
	init_RFID_LEDS();
	init_SPI_RFIDReader();
	init_relay_output();
    immo = new Immobiliser(SPI1, GPIOE, GPIO_Pin_7);
}

void immo_wait() {
	//immo->addCard();
	uint32_t i;
	if (immo->getCardsNumber() > 0) {
		GPIO_WriteBit(GPIOD, RFID_LED_READ, Bit_SET); //LED signal - scanning
		while (immo->scanCard() != Immobiliser::CheckCorrect); //lock until card is correct
		GPIO_WriteBit(GPIOD, RFID_LED_READ, Bit_RESET); //LED signal OFF - scanning
	}

	//checking if user wants to add new card
	if (immo->getCardsNumber() == 0 || GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) ) {
		blink_LED(GPIOD, RFID_LED_OK, 3);
		GPIO_WriteBit(GPIOD, RFID_LED_ADD, Bit_SET); //LED signal - adding
		for (i=0; i<10; ++i)
			time_waste(); //TODO: better delay

		//waiting for card
		GPIO_WriteBit(GPIOD, RFID_LED_OK, Bit_SET); //LED signal - waiting for button
		while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) != (uint8_t)Bit_SET); //waiting for button
		GPIO_WriteBit(GPIOD, RFID_LED_OK, Bit_RESET); //LED signal OFF - waiting for button

		uint8_t cardStatus = immo->addCard();
		if (cardStatus == Immobiliser::AddSuccess)
			blink_LED(GPIOD, RFID_LED_OK, 3);
		else if (cardStatus == Immobiliser::AddFailed)
			blink_LED(GPIOD, RFID_LED_ERROR, 3);
		else
			blink_LED(GPIOD, RFID_LED_READ, 3);

		GPIO_WriteBit(GPIOD, RFID_LED_ADD, Bit_RESET); //LED signal OFF - adding end
		GPIO_WriteBit(GPIOE, RELAY_OUT, Bit_SET);
	}
	else {
		blink_LED(GPIOD, RFID_LED_OK, 3);
		GPIO_WriteBit(GPIOE, RELAY_OUT, Bit_SET);
	}
}

void loop() {

}
