#include "mifare.hpp"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_gpio.h"

Mifare::~Mifare() {

}

uint8_t Mifare::transfer(uint8_t val) {
	spi->DR = val; // write data to be transmitted to SPI data register
	while( !(spi->SR & SPI_I2S_FLAG_TXE) ); // wait until transmit is complete
	while( !(spi->SR & SPI_I2S_FLAG_RXNE) ); // wait until receive is complete
	while( spi->SR & SPI_I2S_FLAG_BSY ); // wait until SPI is not busy anymore
	return spi->DR; // get received data from SPI data register
}


void Mifare::Write_MFRC522(uint8_t addr, uint8_t val) {
	GPIO_ResetBits(gpio, gpioPin);

	transfer((addr<<1)&0x7E);
	transfer(val);

	GPIO_SetBits(gpio, gpioPin);
}


uint8_t Mifare::Read_MFRC522(uint8_t addr) {
	uint8_t val;

	GPIO_ResetBits(gpio, gpioPin);

	transfer(((addr<<1)&0x7E) | 0x80);
	val = transfer(0x00);

	GPIO_SetBits(gpio, gpioPin);

	return val;
}


void Mifare::SetBitMask(uint8_t reg, uint8_t mask) {
	uint8_t tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp | mask);  // set bit mask
}



void Mifare::ClearBitMask(uint8_t reg, uint8_t mask) {
	uint8_t tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp & (~mask));  // clear bit mask
}



void Mifare::AntennaOn(void) {
	uint8_t temp;

	temp = Read_MFRC522(TxControlReg);
	if (!(temp & 0x03)) {
		SetBitMask(TxControlReg, 0x03);
	}
}


void Mifare::AntennaOff(void) {
	ClearBitMask(TxControlReg, 0x03);
}


void Mifare::MFRC522_Reset(void) {
    Write_MFRC522(CommandReg, PCD_RESETPHASE);
}



void Mifare::MFRC522_Init(void) {
	MFRC522_Reset();

	//Timer: TPrescaler*TreloadVal/6.78MHz = 24ms
    Write_MFRC522(TModeReg, 0x8D);		//Tauto=1; f(Timer) = 6.78MHz/TPreScaler
    Write_MFRC522(TPrescalerReg, 0x3E);	//TModeReg[3..0] + TPrescalerReg
    Write_MFRC522(TReloadRegL, 30);
    Write_MFRC522(TReloadRegH, 0);

	Write_MFRC522(TxAutoReg, 0x40);		//100%ASK
	Write_MFRC522(ModeReg, 0x3D);		//CRC0x6363

	//ClearBitMask(Status2Reg, 0x08);		//MFCrypto1On=0
	//Write_MFRC522(RxSelReg, 0x86);		//RxWait = RxSelReg[5..0]
	//Write_MFRC522(RFCfgReg, 0x7F);   		//RxGain = 48dB

	AntennaOn();
}


uint8_t Mifare::MFRC522_Request(uint8_t reqMode, uint8_t *TagType) {
	uint8_t status;
	uint32_t backBits;

	Write_MFRC522(BitFramingReg, 0x07);		//TxLastBists = BitFramingReg[2..0]

	TagType[0] = reqMode;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

	if ((status != MI_OK) || (backBits != 0x10)) {
		status = MI_ERR;
	}

	return status;
}



uint8_t Mifare::MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint32_t *backLen) {
	uint8_t status = MI_ERR;
	uint8_t irqEn = 0x00;
	uint8_t waitIRq = 0x00;
	uint8_t lastBits;
	uint8_t n;
    uint32_t i;

    switch (command) {
        case PCD_AUTHENT: {
			irqEn = 0x12;
			waitIRq = 0x10;
			break;
		}
		case PCD_TRANSCEIVE: {
			irqEn = 0x77;
			waitIRq = 0x30;
			break;
		}
		default:
			break;
    }

    Write_MFRC522(CommIEnReg, irqEn|0x80);
    ClearBitMask(CommIrqReg, 0x80);
    SetBitMask(FIFOLevelReg, 0x80);

	Write_MFRC522(CommandReg, PCD_IDLE);


    for (i=0; i<sendLen; i++) {
		Write_MFRC522(FIFODataReg, sendData[i]);
	}


	Write_MFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
		SetBitMask(BitFramingReg, 0x80);		//StartSend=1,transmission of data starts
	}


	i = 2000;
    do {
		//CommIrqReg[7..0]
		//Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
        n = Read_MFRC522(CommIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x01) && !(n&waitIRq));

    ClearBitMask(BitFramingReg, 0x80);			//StartSend=0

    if (i != 0) {
        if(!(Read_MFRC522(ErrorReg) & 0x1B)) { //BufferOvfl Collerr CRCErr ProtecolErr
            status = MI_OK;
            if (n & irqEn & 0x01) {
				status = MI_NOTAGERR;
			}

            if (command == PCD_TRANSCEIVE) {
               	n = Read_MFRC522(FIFOLevelReg);
              	lastBits = Read_MFRC522(ControlReg) & 0x07;
                if (lastBits) {
					*backLen = (n-1)*8 + lastBits;
				}
                else {
					*backLen = n*8;
				}

                if (n == 0) {
					n = 1;
				}
                if (n > MAX_LEN) {
					n = MAX_LEN;
				}


                for (i=0; i<n; i++) {
					backData[i] = Read_MFRC522(FIFODataReg);
				}
            }
        }
        else {
			status = MI_ERR;
		}

    }

    //SetBitMask(ControlReg,0x80);           //timer stops
    //Write_MFRC522(CommandReg, PCD_IDLE);

    return status;
}



uint8_t Mifare::MFRC522_Anticoll(uint8_t *serNum) {
	uint8_t status;
	uint8_t i;
	uint8_t serNumCheck=0;
    uint32_t unLen;


    //ClearBitMask(Status2Reg, 0x08);		//TempSensclear
    //ClearBitMask(CollReg,0x80);			//ValuesAfterColl
	Write_MFRC522(BitFramingReg, 0x00);		//TxLastBists = BitFramingReg[2..0]

    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK) {
		for (i=0; i<4; i++) {
		 	serNumCheck ^= serNum[i];
		}
		if (serNumCheck != serNum[i]) {
			status = MI_ERR;
		}
    }
    //SetBitMask(CollReg, 0x80);		//ValuesAfterColl=1

    return status;
}


void Mifare::CalulateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData) {
	uint8_t i, n;

    ClearBitMask(DivIrqReg, 0x04);			//CRCIrq = 0
    SetBitMask(FIFOLevelReg, 0x80);
    //Write_MFRC522(CommandReg, PCD_IDLE);


    for (i=0; i<len; i++) {
		Write_MFRC522(FIFODataReg, *(pIndata+i));
	}
    Write_MFRC522(CommandReg, PCD_CALCCRC);


    i = 0xFF;
    do {
        n = Read_MFRC522(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04));			//CRCIrq = 1


    pOutData[0] = Read_MFRC522(CRCResultRegL);
    pOutData[1] = Read_MFRC522(CRCResultRegM);
}



uint8_t Mifare::MFRC522_SelectTag(uint8_t *serNum) {
	uint8_t i;
	uint8_t status;
	uint8_t size;
    uint32_t recvBits;
    uint8_t buffer[9];

	//ClearBitMask(Status2Reg, 0x08);			//MFCrypto1On=0

    buffer[0] = PICC_SElECTTAG;
    buffer[1] = 0x70;
    for (i=0; i<5; i++) {
    	buffer[i+2] = *(serNum+i);
    }
	CalulateCRC(buffer, 7, &buffer[7]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);

    if ((status == MI_OK) && (recvBits == 0x18)) {
		size = buffer[0];
	}
    else {
		size = 0;
	}

    return size;
}



uint8_t Mifare::MFRC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t *Sectorkey, uint8_t *serNum) {
	uint8_t status;
    uint32_t recvBits;
    uint8_t i;
    uint8_t buff[12];


    buff[0] = authMode;
    buff[1] = BlockAddr;
    for (i=0; i<6; i++) {
		buff[i+2] = *(Sectorkey+i);
	}
    for (i=0; i<4; i++) {
		buff[i+8] = *(serNum+i);
	}
    status = MFRC522_ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);

    if ((status != MI_OK) || (!(Read_MFRC522(Status2Reg) & 0x08))) {
		status = MI_ERR;
	}

    return status;
}



uint8_t Mifare::MFRC522_Read(uint8_t blockAddr, uint8_t *recvData) {
	uint8_t status;
    uint32_t unLen;

    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;
    CalulateCRC(recvData,2, &recvData[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

    if ((status != MI_OK) || (unLen != 0x90)) {
        status = MI_ERR;
    }

    return status;
}



uint8_t Mifare::MFRC522_Write(uint8_t blockAddr, uint8_t *writeData) {
	uint8_t status;
    uint32_t recvBits;
    uint8_t i;
    uint8_t buff[18];

    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    CalulateCRC(buff, 2, &buff[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

    if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) {
		status = MI_ERR;
	}

    if (status == MI_OK) {
        for (i=0; i<16; i++) {
        	buff[i] = *(writeData+i);
        }
        CalulateCRC(buff, 16, &buff[16]);
        status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);

		if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) {
			status = MI_ERR;
		}
    }

    return status;
}



void Mifare::MFRC522_Halt(void) {
	//uint8_t status;
    uint32_t unLen;
    uint8_t buff[4];

    buff[0] = PICC_HALT;
    buff[1] = 0;
    CalulateCRC(buff, 2, &buff[2]);

    /*status = */MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff,&unLen);
}
