#define HSE_VALUE ((uint32_t)8000000)

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include <stdio.h>

#include "immobiliser.hpp"
#include "uartcommunicator.hpp"

//built in LEDs for RFID reader signals
#define RFID_LED_READ GPIO_Pin_13
#define RFID_LED_OK GPIO_Pin_12
#define RFID_LED_ERROR GPIO_Pin_14
#define RFID_LED_ADD GPIO_Pin_15

//built in button for RFID reader card saving
#define RFID_BUTTON GPIO_Pin_0

//relay output
#define RELAY_OUT GPIO_Pin_0

Immobiliser *immo; //pointer to immobiliser object
UARTCommunicator *uartCommunicator;

//basic functionality functions
void time_waste(); //simple delay (uC lock intended)
void blink_LED(GPIO_TypeDef* gpio, uint16_t led, uint32_t times); //simple LED blinker

//main functions
void init(); //main initialize function
void loop(); //main loop

//initializers
void init_SPI_RFIDReader(); //SPI for MFRC522 RFID reader initializer
void init_RFID_LEDS(); //RFID signalLEDs initializer
void init_add_button(); //button for new cards saving initializer

//other functions
void immo_wait(); //waiting for immobiliser to let program go

