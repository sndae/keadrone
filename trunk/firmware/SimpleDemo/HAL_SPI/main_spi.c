#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "lpc17xx_ssp.h"
#include "bma180.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define SSP_CHANNEL LPC_SSP0
#define PORT_CS	LPC_GPIO0
#define PIN_MASK_CS (1<<16)

#define delay_1ms			( 1 / portTICK_RATE_MS )
static void delay_ms(uint16_t value)
{
	portTickType xNextWakeTime;
	vTaskDelayUntil(&xNextWakeTime, (delay_1ms * value));

}

static char bmaRead(uint8_t address)
{
	//returns the contents of any 1 byte register from any address
	//sets the MSB for every address byte (READ mode)

	char rxBuff;
	SSP_DATA_SETUP_Type sspDataConfig;

	PORT_CS->FIOCLR |= PIN_MASK_CS; //CS low

	address |= 0x80;
	sspDataConfig.length = 1;
	sspDataConfig.tx_data = &address;
	sspDataConfig.rx_data = &rxBuff;

	SSP_ReadWrite(SSP_CHANNEL, &sspDataConfig, SSP_TRANSFER_POLLING);
	address = 0x55;//dummy write
	SSP_ReadWrite(SSP_CHANNEL, &sspDataConfig, SSP_TRANSFER_POLLING);

	PORT_CS->FIOSET |= PIN_MASK_CS; //CS High

	return rxBuff;
}

static void bmaReadMultiple(uint8_t address, uint8_t *data, uint8_t len)
{
	//returns the contents of any 1 byte register from any address
	//sets the MSB for every address byte (READ mode)

	static char rxBuff[9];
	static char txBuff[9];

	SSP_DATA_SETUP_Type sspDataConfig;

	PORT_CS->FIOCLR |= PIN_MASK_CS; //CS low
	address |= 0x80;

	txBuff[0] = address;

	sspDataConfig.length = 8;
	sspDataConfig.tx_data = &address;
	sspDataConfig.rx_data = &rxBuff;

	SSP_ReadWrite(SSP_CHANNEL, &sspDataConfig, SSP_TRANSFER_POLLING);
	static uint8_t counter;
	for (counter = 0; counter < len; counter++)
	{
		*(data + counter) = rxBuff[counter + 1];
	}

	PORT_CS->FIOSET |= PIN_MASK_CS; //CS High
}

static void bmaWrite(uint8_t address, char data)
{
	//write any data byte to any single address
	//adds a 0 to the MSB of the address byte (WRITE mode)

	address &= 0x7F;
	char rxBuff;
	SSP_DATA_SETUP_Type sspDataConfig;

	PORT_CS->FIOCLR |= PIN_MASK_CS; //CS low

	delay_ms(1);

	sspDataConfig.tx_data = &address;
	sspDataConfig.rx_data = &rxBuff;
	SSP_ReadWrite(SSP_CHANNEL, &sspDataConfig, SSP_TRANSFER_POLLING);

	delay_ms(1);

	sspDataConfig.tx_data = &data;
	sspDataConfig.rx_data = &rxBuff;
	SSP_ReadWrite(SSP_CHANNEL, &sspDataConfig, SSP_TRANSFER_POLLING);

	delay_ms(1);

	PORT_CS->FIOSET |= PIN_MASK_CS; //CS High
}

// init_BMA180
// Input: range is a 3-bit value between 0x00 and 0x06 will set the range as described in the BMA180 datasheet (pg. 27)
// bw is a 4-bit value between 0x00 and 0x09.  Again described on pg. 27
// Output: -1 on error, 0 on success
static Bool bmaInit(uint8_t range, uint8_t bw)
{
	char temp, temp1;

	// if connected correctly, ID register should be 3
	if (bmaRead(ID) != 3)
		return FALSE;

	//-------------------------------------------------------------------------------------
	// Set ee_w bit
	temp = bmaRead(CTRLREG0);
	temp |= 0x10;
	bmaWrite(CTRLREG0, temp); // Have to set ee_w to write any other registers
	//-------------------------------------------------------------------------------------
	// Set BW
	temp = bmaRead(BWTCS);
	temp1 = bw;
	temp1 = temp1 << 4;
	temp &= (~BWMASK);
	temp |= temp1;
	bmaWrite(BWTCS, temp); // Keep tcs<3:0> in BWTCS, but write new BW
	//-------------------------------------------------------------------------------------
	// Set Range
	temp = bmaRead(OLSB1);
	temp1 = range;
	temp1 = (temp1 << RANGESHIFT);
	temp &= (~RANGEMASK);
	temp |= temp1;
	bmaWrite(OLSB1, temp); //Write new range data, keep other bits the same
	//-------------------------------------------------------------------------------------

	// Set interrupt
	temp = bmaRead(CTRLREG3);
	temp |= (1 << NEW_DATA_INT_SHIFT);
	bmaWrite(CTRLREG3, temp); //Enable interrupt on new data only
	//-------------------------------------------------------------------------------------

	return TRUE;
}

void gpioIntInit(void)
{
	//Enable rising edge interrupt for P0.8
	LPC_GPIOINT->IO0IntEnR |= 1 << 8;//| 1<<9;
	NVIC_EnableIRQ(EINT3_IRQn);
}

void spiInit(void)
{
	SSP_CFG_Type sspChannelConfig;
	SSP_DATA_SETUP_Type sspDataConfig;

	LPC_PINCON->PINSEL0 |= 0x2 << 30; //SCK0 p0.15
	LPC_PINCON->PINSEL1 |= 0x2 << 4; //MOSI0 p0.18
	LPC_PINCON->PINSEL1 |= 0x2 << 2; //MISO0 p0.17

	LPC_PINCON->PINMODE1 |= 0x3 << 4;// pull down mosi pin.
	LPC_PINCON->PINMODE1 |= 0x2 << 2;// pull down mosi pin.

	PORT_CS->FIODIR |= 1 << 16; //P0.16 as CSn (acc. meter)

	// init BMA interrupt pin
	// POR VALUE IS ALREADY INPUT

	SSP_ConfigStructInit(&sspChannelConfig);
	SSP_Init(SSP_CHANNEL, &sspChannelConfig);
	SSP_Cmd(SSP_CHANNEL, ENABLE);
	while (bmaInit(0x02, 6) == FALSE)
	{
		printf("BMA180: Error communication\n");
		delay_ms(5);
	}
	printf("BMA180: Connected\n");

	gpioIntInit();
}
struct
{
	INT_16 X;
	INT_16 Y;
	INT_16 Z;
	INT_16 temp;
} acc;

int spiPoll(void)
{

	static uint8_t regData[7];

	// get 7 bytes starting from reg. ACCXLSB
	bmaReadMultiple(ACCXLSB, &regData[0], 7);

	acc.X = regData[0];
	acc.X |= (uint16_t) regData[1] << 8;
	acc.X = acc.X >> 2; // Get rid of two non-value bits in LSB

	acc.Y = regData[2];
	acc.Y |= (uint16_t) regData[3] << 8;
	acc.Y = acc.Y >> 2; // Get rid of two non-value bits in LSB

	acc.Z = regData[4];
	acc.Z |= (uint16_t) regData[5] << 8;
	acc.Z = acc.Z >> 2; // Get rid of two non-value bits in LSB

	acc.temp = regData[6];
	/*	static uint16_t counter = 0;
	 if (counter++ == 1000)
	 {
	 printf("%.4d,%.4d,%.4d\n", acc.X, acc.Y, acc.Z);
	 counter = 0;
	 }
	 */
	return 0;
}

void EINT3_IRQHandler(void)
{
	spiPoll();
	LPC_GPIOINT->IO0IntClr = 1 << 8;
}
