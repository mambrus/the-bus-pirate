/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#define TERMINAL_BUFFER 4096
//these settings persist between modes
// such as terminal side baud rate, display mode, bus mode
struct _bpConfig {
	unsigned char terminalInput[TERMINAL_BUFFER]; //hold user terminal input
	unsigned char termSpeed;
	enum {
	 	HEX=0,
		DEC,
		BIN,
		RAW,
	}displayMode;
	enum { //needs to be same order as mode variable in .c
		HIZ=0,
		#ifdef BP_USE_1WIRE
		   	DS1WIRE,
		#endif
		#ifdef BP_USE_HWUART
			HWUART,
		#endif
		#ifdef BP_USE_I2C
		   	I2C,
		#endif
		#ifdef BP_USE_HWSPI
		   	HWSPI,
		#endif
		#ifdef BP_USE_JTAG
			JTAG,
		#endif
		#ifdef BP_USE_RAW2WIRE
		    RAW2WIRE,
		#endif
		#ifdef BP_USE_RAW3WIRE
		    RAW3WIRE,
		#endif
		#ifdef BP_USE_PCATKB
			PCATKB,	
		#endif
		#ifdef BP_USE_MIDI
			MIDI,
		#endif
		#ifdef BP_USE_LIN
			LIN,
		#endif
		#ifdef BP_USE_CAN
			CAN,
		#endif
		#ifdef BP_USE_LCD
			LCD,
		#endif
 	}busMode;
	// Device IDs from the chip
	unsigned int dev_type;
	unsigned int dev_rev;
};

//send command to correct protocol library for processing
//switch based on bpConfig.busMode variable
void bpProcess(void);

//echo the name of the current bus mode to the user terminal
//used to show user prompt
void bpEchoCurrentBusMode(void);

//echo the name of the bus mode in the name array at position m to the user terminal
//used for listing available bus modes
void bpEchoBusMode(unsigned char m);

//the number of bus modes available
//used to navigate the protocol list array, 
//done here because mode array isn't availble to other functions
unsigned char bpNumBusModes(void);

//bridge UART input in UART mode
void busPirateAsyncUARTService(void);
