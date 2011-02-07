/*****************************************************************************
 *   i2c.h:  Header file for NXP LPC17xx Family Microprocessors
 *
 *   Copyright(C) 2009, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2009.05.26  ver 1.00    Prelimnary version, first Release
 *
 ******************************************************************************/
#ifndef __I2C_H 
#define __I2C_H
#include "lpc_types.h"

#define MAX_TIMEOUT		0x00FFFFFF

#define I2CMASTER		0x01
#define I2CSLAVE		0x02

/* For more info, read Philips's LM95 datasheet */
#define LM75_ADDR		0x90
#define LM75_TEMP		0x00
#define LM75_CONFIG		0x01
#define LM75_THYST		0x02
#define LM75_TOS		0x03

#define RD_BIT			0x01

typedef struct
{
	uint8_t address; // i2c slave device address
	uint8_t slaveRegister; //i2c slave device (start) register
	Bool readData; // false if writing data, else true
	uint8_t * buffer;
	uint32_t bufLength;
} I2C_DATA;

#define I2CONSET_I2EN		0x00000040  /* I2C Control Set Register */
#define I2CONSET_AA			0x00000004
#define I2CONSET_SI			0x00000008
#define I2CONSET_STO		0x00000010
#define I2CONSET_STA		0x00000020

#define I2CONCLR_AAC		0x00000004  /* I2C Control clear Register */
#define I2CONCLR_SIC		0x00000008
#define I2CONCLR_STAC		0x00000020
#define I2CONCLR_I2ENC		0x00000040

#define I2DAT_I2C			0x00000000  /* I2C Data Reg */
#define I2ADR_I2C			0x00000000  /* I2C Slave Address Reg */
#define I2SCLH_SCLH			0x00000080  /* I2C SCL Duty Cycle High Reg */
#define I2SCLL_SCLL			0x00000080  /* I2C SCL Duty Cycle Low Reg */

extern void I2C0_IRQHandler(void);
extern uint32_t I2CInit(void);
extern uint32_t I2CStart(void);
extern uint32_t I2CStop(void);
extern uint32_t I2CEngine(I2C_DATA *p);
#endif /* end __I2C_H */
/****************************************************************************
 **                            End Of File
 *****************************************************************************/
