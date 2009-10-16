/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * One wire search code taken from here: http://www.maxim-ic.com/appnotes.cfm/appnote_number/187
 *
 * We claim no copyright on our code, but there may be different licenses for some of the code in this file.
  *
  * To the extent possible under law, the Bus Pirate project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

//abstraction for m_i2c_1.c
#include "m_1wire_213.h"
#include "1wire.h"
#include "base.h"

extern struct _modeConfig modeConfig;
extern struct _command bpCommand;

//the roster stores the first OW_DEV_ROSTER_SLOTS 1-wire addresses found during a ROM SEARCH command
//these addresses are available as MACROs for quick address entry
#define OW_DEV_ROSTER_SLOTS 10 //how many devices max to store addresses as MACROs
struct _OWID{
//	unsigned char familyID; //to lazy to do it right, for now...
	unsigned char id[8];
//	unsigned char crc;
};

struct _OWIDREG{
	unsigned char num;
	struct _OWID dev[OW_DEV_ROSTER_SLOTS];
} ;

struct _OWIDREG OWroster;

void DS1wireID(unsigned char famID);

unsigned char OWFirst(void);
unsigned char OWNext(void);
unsigned char OWSearch(void);
unsigned char OWVerify(void);
unsigned char docrc8(unsigned char value);

//because 1wire uses bit times, setting the data line high or low with (_-) has no effect
//we have to save the desired bus state, and then clock in the proper value during a clock(^)
static unsigned char DS1wireDataState=0;//data bits are low by default.

// global search state,
//these lovely globals are provided courtesy of MAXIM's code
//need to be put in a struct....
unsigned char ROM_NO[8];
unsigned char SearchChar=0xf0; //toggle between ROM and ALARM search types
unsigned char LastDiscrepancy;
unsigned char LastFamilyDiscrepancy;
unsigned char LastDeviceFlag;
unsigned char crc8;

void DS1wireReset(void);

//this function links the underlying library functions to generic commands that the bus pirate issues
//put most used functions first for best performance
void DS1wireProcess(void){
	static unsigned char c,j;
	static unsigned int i;
	static unsigned char devID[8];
	switch(bpCommand.cmd){
		case CMD_READ:
			if(bpCommand.repeat==1){
				bpWmessage(MSG_READ);
				c=OWReadByte();
				bpWbyte(c);
			}else{
				bpWmessage(MSG_READBULK);
				bpWbyte(bpCommand.repeat);
				bpWmessage(MSG_READBULK_BYTES);
				for(i=0;i<bpCommand.repeat;i++){	
					c=OWReadByte();
					bpWbyte(c);
					bpSP;
				}
			}
			bpWBR;
			break;
		case CMD_WRITE:
			//bpWmessage(MSG_WRITE);
			//bpWbyte(bpCommand.num);
			//OWWriteByte(bpCommand.num);
			//bpWBR;
			bpWmessage(MSG_WRITE);
			bpWbyte(bpCommand.num);
			if(bpCommand.repeat==1){
				OWWriteByte(bpCommand.num);//send byte
			}else{
				bpWstring(" , ");
				bpWbyte(bpCommand.repeat);
				bpWmessage(MSG_WRITEBULK);
				for(i=0;i<bpCommand.repeat;i++)OWWriteByte(bpCommand.num);//send byte
			}
			bpWBR;
			break;
		case CMD_STARTR:
		case CMD_START:
			//bus reset
			DS1wireReset();
			break;
		case CMD_BIT_READ:
			c=OWReadBit();
			bpWmessage(MSG_BIT_READ);
			bpEchoState(c);
			bpWmessage(MSG_BIT_NOWINPUT);
			break;
		case CMD_BIT_CLK: //we use clock as the 1wire write slot, use _- to set data state
			bpWbyte(bpCommand.repeat);
			bpWstring(OUMSG_1W_BIT_BULKWRITE);
			for(i=0;i<bpCommand.repeat;i++) OWWriteBit(DS1wireDataState);
			bpEchoState(DS1wireDataState);
			bpWline(")");
			break;
		case CMD_BIT_DATH://this also sets a bit direction variable that sets the bits that are clocked with ^
		case CMD_BIT_DATL:
			bpWstring(OUMSG_1W_BIT_WRITE);
			if(bpCommand.cmd==CMD_BIT_DATL){
				DS1wireDataState=0;
				bpEchoState(0);
			}else{
				DS1wireDataState=1;
				bpEchoState(1);
			}
			bpWline(OUMSG_1W_BIT_WRITE_NOTE);
			OWWriteBit(DS1wireDataState);
			break;
		case CMD_PRESETUP: //set the options avaiable here....
			modeConfig.allowlsb=0;
			modeConfig.allowpullup=1; 
			modeConfig.HiZ=1;//yes, always HiZ
			break;
		case CMD_SETUP:
			OWroster.num=0;//clear any old 1-wire bus enumeration rosters
			//-- Ensure pins are in high impedance mode --
			SDA_TRIS=1;
			//writes to the PORTs write to the LATCH
			SDA=0;			//B9 sda
			bpWline("1WIRE routines Copyright (C) 2000 Michael Pearce");
			bpWline("Released under GNU General Public License");
			bpWmessage(MSG_READY);
			break;
		case CMD_CLEANUP: //no cleanup needed...
			break;
		case CMD_MACRO:
			if(bpCommand.num>0 && bpCommand.num<51){
				bpCommand.num--;//adjust down one for roster array index
				if(bpCommand.num>=OWroster.num){//no device #X on the bus, try ROM SEARCH (0xF0)
					bpWline(OUMSG_1W_MACRO_ADDRESS_NODEVICE);
					return;
				}
				//write out the address of the device in the macro
				bpWstring(OUMSG_1W_MACRO_ADDRESS);//xxx WRITE BUS #X ID:
				bpWdec(bpCommand.num+1);
				bpWstring(": ");
				for(j=0;j<8;j++){
					bpWbyte(OWroster.dev[bpCommand.num].id[j]); 
					bpSP; 
					OWWriteByte(OWroster.dev[bpCommand.num].id[j]);
				} //write address
				bpWline(" ");
				return;
			}
			switch(bpCommand.num){
				case 0://menu
					bpWline(OUMSG_1W_MACRO_MENU);
					bpWline(OUMSG_1W_MACRO_SEARCH_ROM_HEADER);
					//write out roster of devices and macros, or SEARCH ROM NOT RUN, TRY (0xf0)
					if(OWroster.num==0){
						bpWline(OUMSG_1W_MACRO_ADDRESS_NODEVICE);
					}else{
						for(c=0;c<OWroster.num; c++){
							bpSP;//space
							bpWdec(c+1);
							bpWstring(".");
							for(j=0;j<8;j++){bpWbyte(OWroster.dev[c].id[j]); bpSP;}
							bpWstring("\x0D\x0A   *");
							DS1wireID(OWroster.dev[c].id[0]);	//print the device family identity (if known)
						}
					}
					bpWline(OUMSG_1W_MACRO_MENU_ROM);
					break;
				//1WIRE ROM COMMANDS
				case 0xec://ALARM SEARCH
				case 0xf0: //SEARCH ROM
					SearchChar=bpCommand.num;
					if(bpCommand.num==0xec){
						bpWline(OUMSG_1W_MACRO_ALARMSEARCH_ROM);						
					}else{//SEARCH ROM command...
						bpWline(OUMSG_1W_MACRO_SEARCH_ROM);
					}

					bpWline(OUMSG_1W_MACRO_SEARCH_ROM_HEADER);
					// find ALL devices
					j = 0;
					c = OWFirst();
					OWroster.num=0;
					while (c){
						//the roster number is the shortcut macro
						bpSP;
						bpWdec(j+1);
						bpWstring(".");
				
						// print address
						for (i = 0; i <8; i++){
							bpWbyte(ROM_NO[i]);
							bpSP;
						}
						bpWstring("\x0D\x0A   *");
						DS1wireID(ROM_NO[0]);	//print the device family identity (if known)
						
						//keep the first X number of one wire IDs in a roster
						//so we can refer to them by macro, rather than ID
						if(j<OW_DEV_ROSTER_SLOTS){//only as many as we have room for
							for(i=0;i<8;i++) OWroster.dev[OWroster.num].id[i]=ROM_NO[i];
							OWroster.num++;//increment the roster count
						}

						j++;	
				
						c = OWNext();
					}

					bpWline(OUMSG_1W_MACRO_SEARCH_ROM_NOTE);				
					break;
				case 0x33://READ ROM
					DS1wireReset();
					bpWstring(OUMSG_1W_MACRO_READ_ROM);
					OWWriteByte(0x33);
						for(i=0; i<8; i++){
						devID[i]=OWReadByte();
						bpWbyte(devID[i]);
						bpSP;	
					}
					bpWBR;	
					DS1wireID(devID[0]);
					break;
				case 0x55://MATCH ROM
					DS1wireReset();
					bpWline(OUMSG_1W_MACRO_MATCH_ROM);
					OWWriteByte(0x55);
					break;
				case 0xcc://SKIP ROM
					DS1wireReset();
					bpWline(OUMSG_1W_MACRO_SKIP_ROM);
					OWWriteByte(0xCC);
					break;
				default:
					bpWmessage(MSG_ERROR_MACRO);
			}
			break;
		case CMD_ENDOFSYNTAX: break;
		default:
			bpWmessage(MSG_ERROR_MODE);
	}

}

void DS1wireReset(void){
	unsigned char c;

	c=OWReset();
	bpWstring(OUMSG_1W_RESET);	
	if(c==0){
		bpWline(OUMSG_1W_RESET_OK);			
	}else{
		bpWstring(OUMSG_1W_RESET_ERROR);
		if(c&0b1)  bpWstring(OUMSG_1W_RESET_SHORT);	
		if(c&0b10) bpWstring(OUMSG_1W_RESET_NODEV);
		bpWstring("(");	
		bpWbyte(c);
		bpWline(")");
	}
}

void DS1wireID(unsigned char famID){
	//some devices, according to:
	//http://owfs.sourceforge.net/commands.html
	#define DS2404 0x04
	#define DS18S20 0x10
	#define DS1822 0x22
	#define DS18B20 0x28
	#define DS2431 0x2D
	switch(famID){//check for device type
		case DS18S20:
			bpWline("DS18S20 High Pres Dig Therm");
			break;
		case DS18B20:
			bpWline("DS18B20 Prog Res Dig Therm");
			break;
		case DS1822:
			bpWline("DS1822 Econ Dig Therm");
			break;
		case DS2404:
			bpWline("DS2404 Econram time Chip");
			break;
		case DS2431:
			bpWline("DS2431 1K EEPROM");
			break;
		default:
			bpWline("Unknown device");

	}
}


//the 1-wire search algo taken from:
//http://www.maxim-ic.com/appnotes.cfm/appnote_number/187
#define TRUE 1 //if !=0
#define FALSE 0

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
unsigned char OWFirst()
{
   // reset the search state
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;

   return OWSearch();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
unsigned char OWNext()
{
   // leave the search state alone
   return OWSearch();
}

//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
unsigned char OWSearch()
{
   unsigned char id_bit_number;
   unsigned char last_zero, rom_byte_number, search_result;
   unsigned char id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;
   crc8 = 0;

   // if the last call was not the last one
   if (!LastDeviceFlag)
   {
      // 1-Wire reset
      if (OWReset())
      {
         // reset the search
         LastDiscrepancy = 0;
         LastDeviceFlag = FALSE;
         LastFamilyDiscrepancy = 0;
         return FALSE;
      }

      // issue the search command 
      OWWriteByte(SearchChar);  //!!!!!!!!!!!!!!!

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit = OWReadBit();
         cmp_id_bit = OWReadBit();

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1))
            break;
         else
         {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit)
               search_direction = id_bit;  // bit write value for search
            else
            {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < LastDiscrepancy)
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
               else
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == LastDiscrepancy);

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     LastFamilyDiscrepancy = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
              ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;

            // serial number search direction write bit
            OWWriteBit(search_direction);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0)
            {
                docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!((id_bit_number < 65) || (crc8 != 0)))
      {
         // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
         LastDiscrepancy = last_zero;

         // check for last device
         if (LastDiscrepancy == 0)
            LastDeviceFlag = TRUE;
         
         search_result = TRUE;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[0])
   {
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      search_result = FALSE;
   }

   return search_result;
}

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
unsigned char OWVerify()
{
   unsigned char rom_backup[8];
   unsigned char i,rslt,ld_backup,ldf_backup,lfd_backup;

   // keep a backup copy of the current state
   for (i = 0; i < 8; i++)
      rom_backup[i] = ROM_NO[i];
   ld_backup = LastDiscrepancy;
   ldf_backup = LastDeviceFlag;
   lfd_backup = LastFamilyDiscrepancy;

   // set search to find the same device
   LastDiscrepancy = 64;
   LastDeviceFlag = FALSE;

   if (OWSearch())
   {
      // check if same device found
      rslt = TRUE;
      for (i = 0; i < 8; i++)
      {
         if (rom_backup[i] != ROM_NO[i])
         {
            rslt = FALSE;
            break;
         }
      }
   }
   else
     rslt = FALSE;

   // restore the search state 
   for (i = 0; i < 8; i++)
      ROM_NO[i] = rom_backup[i];
   LastDiscrepancy = ld_backup;
   LastDeviceFlag = ldf_backup;
   LastFamilyDiscrepancy = lfd_backup;

   // return the result of the verify
   return rslt;
}

// TEST BUILD
static unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current 
// global 'crc8' value. 
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
   // See Application Note 27
   // TEST BUILD
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}
