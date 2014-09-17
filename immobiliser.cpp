#include "immobiliser.hpp"

Immobiliser::Immobiliser(SPI_TypeDef* readerSPI, GPIO_TypeDef* readerGPIO, uint16_t readerChipSelectPin) {
	/****************************************
	 |   6 BYTES   | 4 BYTES |   6 BYTES   |
	 |    Key A    | AC Bits |    Key B    |
	****************************************/
	/* AC Bits (0xAA 0x52 0xD5 0x00)
	 * key A needed for Block 1 and Sector Trailer (read/write/increment/decrement) */
	const char key[bufferSize] = {0x53, 0x6B, 0x6F, 0x64, 0x61, 0x00,
						  	  	  0xAA, 0x52, 0xD5, 0x00,
						  	  	  0x53, 0x6B, 0x6F, 0x64, 0x61, 0x00};

	//additional data to compare with saved on card
	const char checkData[bufferSize] = "!Turbo Felicia!";

	//setting variables
	for (uint8_t i=0; i<bufferSize; ++i) {
		sectorKeyDefault[i] = 0xFF; //empty key for new cards
		sectorKeyNew[i] = key[i]; //key for programmed cards
		data[i] = checkData[i]; //additional control data
	}

	reader = new Mifare(readerSPI, readerGPIO, readerChipSelectPin); //creating instance of MFRC522 reader class
	cardsNumber = 0;

	//clearing array
	for(uint8_t i=0; i<maxCards; i++)
		for(uint8_t j=0; j<bufferSize; j++)
			cards[i][j] = 0;

	reader->MFRC522_Init(); //initializing reader
	readFromFLASH(); //reading saved cards from FLASH memory
}

Immobiliser::~Immobiliser() {
	delete reader;
}

void Immobiliser::readFromFLASH() {
 	cardsNumber = FLASH_AT(0); //reading from FLASH
	if (cardsNumber > maxCards)
		cardsNumber = 0; //in case of not empty FLASH
	for(uint8_t i=0; i<cardsNumber; i++)
		for(uint8_t j=0; j<bufferSize; j++)
			cards[i][j] = FLASH_AT(1 + i * bufferSize + j);
}



uint8_t Immobiliser::checkCard(uint8_t cardID[]) {
	for(uint8_t i=0; i<cardsNumber; i++) {
			for(uint8_t j=0; j<bufferSize; j++) {
				if (cardID[j] != cards[i][j])
					break;
				else if (j == bufferSize - 1)
					return CheckCorrect;
			}
		}
		return CheckIncorrect;
}

uint8_t Immobiliser::checkCardData(uint8_t *serNum) {
	uint8_t status; //RFID reader status
	uint8_t buffer[bufferSize]; //buffer to read data

	reader->MFRC522_SelectTag(serNum); //selecting RFID tag
	status = reader->MFRC522_Auth(PICC_AUTHENT1A, blockAddr, sectorKeyNew, serNum); //authorize with new Key A
	if (status == MI_OK) {
		status = reader->MFRC522_Read(blockAddr, buffer); //reading card data
		reader->MFRC522_Halt();
		reader->MFRC522_Init(); //for some reason it's necessary

		//if read correctly - checking card data
		if (status == MI_OK) {
			for(uint8_t i=0; i<bufferSize; i++) {
				if (buffer[i] != data[i])
					return CheckIncorrect;
			}
			return CheckCorrect;
		}
	}
	return CheckIncorrect;
}

uint8_t Immobiliser::writeCardData(uint8_t *serNum) {
	uint8_t status; //RFID reader data

	reader->MFRC522_SelectTag(serNum); //selecting RFID tag
	status = reader->MFRC522_Auth(PICC_AUTHENT1A, 3, sectorKeyDefault, serNum); //trying authorize for empty key

	//if wrong key trying again for our key
	if (status != MI_OK) {
		//card must be halted and read again
		reader->MFRC522_Halt();
		status = reader->MFRC522_Request(PICC_REQIDL, serNum);
		if (status == MI_OK) {
			status = reader->MFRC522_Anticoll(serNum);
			if (status == MI_OK) {
				reader->MFRC522_SelectTag(serNum); //selecting RFID tag
				status = reader->MFRC522_Auth(PICC_AUTHENT1A, 3, sectorKeyNew, serNum);
			}
		}
	}

	// if authorized correctly
	if (status == MI_OK) {
		// Writing new Key to Card
		status = reader->MFRC522_Write(3, sectorKeyNew); //key write
		if (status == MI_OK)
			status = reader->MFRC522_Auth(PICC_AUTHENT1A, 1, sectorKeyNew, serNum); //authorizing again with new key
		else
			return AddFailed;

		// Writing additional check data
		status = reader->MFRC522_Write(1, data);
		if (status == MI_OK) {
			reader->MFRC522_Halt();
			reader->MFRC522_Init(); //for some reason it's necessary
			return AddSuccess;
		}
	}
	return AddFailed;
}

uint8_t Immobiliser::scanCard() {
	uint8_t status; //RFID reader status

	//clearing scannedCard field
	for(uint8_t i=0; i<bufferSize; i++)
		scannedCard[i] = 0;

	//scanning until card is found
	while (1) {
		status = reader->MFRC522_Request(PICC_REQIDL, scannedCard);
		if (status == MI_OK) {
			status = reader->MFRC522_Anticoll(scannedCard);
			if (status == MI_OK) {
				if (checkCard(scannedCard) == MI_OK) {
					return checkCardData(scannedCard);
				}
			}
		}
	}
}

uint8_t Immobiliser::addCard() {
	uint8_t rfidStatus; //RFID reader status
	uint8_t cardStatus = AddSuccess; //Card operations status
	FLASH_Status flashStatus; //FLASH operations status
	uint8_t buffer[bufferSize]={0}; //card ID buffer

	rfidStatus = reader->MFRC522_Request(PICC_REQIDL, buffer);
	if (rfidStatus == MI_OK) {
		rfidStatus = reader->MFRC522_Anticoll(buffer);
		if (rfidStatus == MI_OK && cardsNumber != maxCards)
			cardStatus = writeCardData(buffer); //writing key and data
		else
			return AddFailed;

		if (cardStatus == AddSuccess) {
			flashStatus = FLASH_COMPLETE;

			FLASH_Unlock(); //unlocking FLASH for writing

			//erasing data from FLASH
			flashStatus = FLASH_EraseSector(flashSector, VoltageRange_3);
			flashStatus = FLASH_WaitForLastOperation();
			if (flashStatus != FLASH_COMPLETE) {
				FLASH_Lock();
				return AddFailed;
			}

			//FLASH configuration
			FLASH->CR &= CR_PSIZE_MASK;
			FLASH->CR |= FLASH_PSIZE_BYTE; //setting data size to bytes
			FLASH->CR |= FLASH_CR_PG;

			//checking if card already exists in FLASH
			if (checkCard(buffer) != CheckCorrect) {
				//writing new card to cards array
				for (uint8_t i=0; i<bufferSize; ++i)
					cards[cardsNumber][i] = buffer[i];
				cardsNumber++;
			}
			else
				cardStatus = AddExists;

			//writing cards number to FLASH
			FLASH_AT(0) = cardsNumber;
			flashStatus = FLASH_WaitForLastOperation();
			if (flashStatus != FLASH_COMPLETE)
				return AddFailed;

			//writing all cards to FLASH (also empty rows in array)
			for (uint8_t i=0; i<maxCards; ++i) {
				for (uint8_t j=0; j<bufferSize; ++j) {
					FLASH_AT(1 + i * bufferSize + j) = cards[i][j];
					flashStatus = FLASH_WaitForLastOperation();
					if (flashStatus != FLASH_COMPLETE)
						return AddFailed;
				}
			}

			FLASH->CR &= (~FLASH_CR_PG);
			FLASH_Lock(); //locking FLASH for writing

			return cardStatus;
		}
	}
	return AddFailed;
}

uint8_t Immobiliser::getCardsNumber() {
	return cardsNumber;
}
