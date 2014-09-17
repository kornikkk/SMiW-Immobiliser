#ifndef IMMOBILISER_H
#define IMMOBILISER_H

#include "stm32f4xx_flash.h"
#include "mifare.hpp"

#define FLASH_ADDRESS (uint32_t)0x08040000 //needed to be defined for macro, 0x8040000 is Flash Sector 6
#define FLASH_AT(pos) *(__IO uint8_t *)(FLASH_ADDRESS + (pos)) //macro getting pointer to FLASH memory

class Immobiliser {
private:
	/* CONSTANTS */
	const static uint8_t maxCards = 10; //maximum cards saved to FLASH
	const static uint8_t bufferSize = 16; //cards data buffer
	const static uint32_t flashAddress = FLASH_ADDRESS;
	const static uint32_t flashSector = (uint32_t)FLASH_Sector_6; //sector in FLASH
	const static uint8_t blockAddr = 1; //card block address
	const static uint8_t blockAddrkey = 3; //key block address

	/* VARIABLES */
	Mifare *reader; //pointer to Mifare reader class
	uint8_t cardsNumber; //number of cards saved in FLASH
	uint8_t cards[maxCards][bufferSize]; //array of cards read from FLASH
	uint8_t scannedCard[bufferSize]; //scanned card ID
	uint8_t sectorKeyDefault[bufferSize]; //default key - filled with 0xFF
	uint8_t sectorKeyNew[bufferSize]; //new key - setting in constructor
	uint8_t data[bufferSize]; //card data - additional authorization

	/* METHODS */
	//reading saved cards from FLASH memory
	void readFromFLASH();

	//checking cardID with saved cards
	uint8_t checkCard(uint8_t cardID[]);

	//authorizing card and checking data
	uint8_t checkCardData(uint8_t *serNum);

	//authorizing card, saving new key and data to card
	uint8_t writeCardData(uint8_t *serNum);

public:
	enum CardStatus {
		CheckCorrect,
		CheckIncorrect,
		AddSuccess,
		AddFailed,
		AddExists
	};

	Immobiliser(SPI_TypeDef* readerSPI, GPIO_TypeDef* readerGPIO, uint16_t readerChipSelectPin);
	~Immobiliser();

	//Scanning card and returning CardStatus (Check)
	uint8_t scanCard();

	//adding card and returning CardStatus (Add)
	uint8_t addCard();

	//GETTERS
	uint8_t getCardsNumber();
};

#endif //IMMOBILISER_H

