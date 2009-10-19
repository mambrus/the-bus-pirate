/*
 * This file is part of the Bus Pirate project (buspirate.com).
 *
 * Originally written by hackaday.com <legal@hackaday.com>
 *
 * To the extent possible under law, hackaday.com <legal@hackaday.com> has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include "base.h"
//#include "uart2io.h"

//define the open drain control register for the TX pin
#define UARTRX_PIN 7
#define UARTTX_PIN RPOR4bits.RP9R
#define UARTTX_ODC BP_MOSI_ODC

void UART2Setup(unsigned int brg, unsigned char normalOutput, unsigned char rxp, unsigned char dbp, unsigned char sb ){
	//use open drain control register to 
	//enable Hi-Z mode on hardware module outputs
	//inputs are already HiZ
	if(normalOutput==0){
		UARTTX_ODC=1;
	}

	//assign UART2 with PPS
	RPINR19bits.U2RXR=UARTRX_PIN; //Inputs U2RX
	UARTTX_PIN=U2TX_IO;	// Outputs U2TX

    U2BRG = brg; 			//assign baud rate
    U2MODE = 0;
    U2MODEbits.BRGH = 1;
	U2MODEbits.RXINV=rxp; //rx polarity
	U2MODEbits.PDSEL=dbp; //databits and parity
	U2MODEbits.STSEL=sb; //stop bits
    U2STA = 0;
}

void UART2Enable(void){
    U2MODEbits.UARTEN = 1;
    U2STAbits.UTXEN = 1;
    IFS1bits.U2RXIF = 0;
}

void UART2Disable(void){
    U2MODEbits.UARTEN = 0;
    U2STAbits.UTXEN = 0;
	RPINR19bits.U2RXR=0b11111;	//B7 U2RX
	RPOR3bits.RP6R=0; 			//B6 U2TX
	UARTTX_ODC=0;
}

void UART2TX(unsigned char c){
	while(U2STAbits.UTXBF == 1); //if buffer is full, wait
    U2TXREG = c;
}

unsigned char UART2RX(void){
    while(U2STAbits.URXDA == 0);
	return U2RXREG;
}

unsigned char UART2RXRdy(void){
    return U2STAbits.URXDA;
}