/***************************************************************************//**
  @file     App.c
  @brief    TP3: Protocolo Hart
  @author   Grupo 5
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "UART/uart.h"
#include "FTM/FTM.h"
#include "CMP/CMP.h"
#include "timer/timer.h"
#include "uart2char/uart2char.h"
#include "char2uart/char2uart.h"
#include "fskModulator/fskModulator.h"
#include "MCAL/gpio.h"
#include <stddef.h>
#include <stdio.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define VERSION 2


#define UART_ID 0
#define UART_BAUDRATE	1200
#define UART_PARITY		ODD_PARITY

#define FTM_MOD FTM_1
#define FTM_CH  0

#define PWM_MOD FTM_0
#define PWM_CH  2

#define PER1  833 //μseg
#define PER0  (PER1/2) //μseg
#define TOL   (PER0/2)  //μseg
#define UMBRAL   625  //μseg

#define START_BIT 0
#define STOP_BIT  1


#define TEST_PIN	PORTNUM2PIN(PC, 17)

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {IDLE_STATE, DATA_STATE} DECODER_STATE;

/*******************************************************************************
 * FUNCTION PROTOTYPES FOR PRIVATE FUNCTIONS WITH FILE LEVEL SCOPE
 ******************************************************************************/

bool byteDecoder(uint8_t* byte);

bool isNewBit(bool* bit);

/*******************************************************************************
 * ROM CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/


/*******************************************************************************
 *******************************************************************************
                        GLOBAL VARIABLES
 *******************************************************************************
 ******************************************************************************/

static uart2charParser uartParser;
static char2uartParser uartParserRx;
static uint16_t** fskPtr;

//static tim_id_t timer;
/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/* Función que se llama 1 vez, al comienzo del programa */
void App_Init (void)
{

	uart_cfg_t config = {.baudrate=UART_BAUDRATE, .MSBF=false, .parity=UART_PARITY};
	uartInit(UART_ID, config);
	initUart2charParser(&uartParser);
	initChar2UartParser(&uartParserRx);

	CMP_Init(CMP0_t, level_3, no_inv);
	
	ICInit(FTM_MOD, FTM_CH, CAPTURE_RISING, NULL);

	fskPtr = fskModulatorInit(VERSION-1);
	
	PWMInit(PWM_MOD, PWM_CH, DACFREQ);
	PWMFromPtr(PWM_MOD, PWM_CH, fskPtr);

	gpioMode(TEST_PIN, OUTPUT);


}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run (void)
{
	uint8_t byte;

	if (uartIsRxMsg(UART_ID)) {   // Recibo por UART

		gpioWrite(TEST_PIN, HIGH);

		uint8_t data;
		uartReadMsg(UART_ID, (char*)&data, 1);
			Push8Bit(&uartParserRx, data);
			if(IsNewByte(&uartParserRx)){
				uint16_t dataaux;
				dataaux=GetByte(&uartParserRx);
				fskSetMsg(dataaux);
			}

			gpioWrite(TEST_PIN, LOW);
	}

	if (byteDecoder(&byte)) {
		gpioWrite(TEST_PIN, HIGH);

		uartWriteMsg(UART_ID, (char*)&byte, 1);

		gpioWrite(TEST_PIN, LOW);
	}

}


/*******************************************************************************
 *******************************************************************************
                        LOCAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

bool byteDecoder(uint8_t* byte) {

	gpioWrite(TEST_PIN, HIGH);

  static DECODER_STATE state;

  bool bit;

  switch (state) {

    case IDLE_STATE:
      if (isNewBit(&bit) && bit == START_BIT)  {    // START detected
    	  state = DATA_STATE;
      }
      break;

    case DATA_STATE:
      if (isNewBit(&bit)) {
    	  if (isNewByte(&uartParser)) {
          state = IDLE_STATE;
          if (bit == STOP_BIT) {    // STOP
            ByteStruct newByte = getByte(&uartParser);
            if (!newByte.error) {
              *byte = newByte.byte;
          	gpioWrite(TEST_PIN, LOW);
              return true;
            }
          }
        }
        else {
          pushBit(&uartParser, bit);
        }
      }
      break;

  }

	gpioWrite(TEST_PIN, LOW);
  return false;

}




bool isNewBit(bool* bit) {

	static uint8_t counter=0;

	if (ICisEdge(FTM_MOD, FTM_CH)) {
		uint16_t period = FTM_TICK2US(ICGetCont(FTM_MOD, FTM_CH));

		if (period < UMBRAL) {    // 0
			counter++;
			if (counter==1) {
			  *bit=0;
			  return true;
			}
			else {
			counter = 0;
		  }
		}
		else {        // 1
			counter=0;
			*bit=1;
			return true;
		}
	}

	return false;

}
/*******************************************************************************
 ******************************************************************************/
