/*-------------------------------------------------------------------------
* FILENAME:  SMC1.c
*
* DESCRIPTION:
*
*   The code in this module provides echo capability on SMC1. It's
*   intent is to provide the beginnings of a debug port that is
*   mostly compiler independent. If an ASCII terminal is connected
*   at 9600,N,8,1 on Port 2 (PB3) on the ADS board with no hardware
*   control, characters typed on the keyboard will be received by
*   SMC1 and echoed back out the RS232 port to the ASCII terminal.
*   This function was designed and tested on an ADS860
*   development board. Note that if a different baud rate is
*   required, there is a header file on the netcomm website under
*   the General Software category that is labelled "Asynchronous
*   Baud Rate Tables".
*
* REFERENCES:
*
*   1) MPC860 Users Manual
*   2) PowerPC Microprocessor Family: The Programming Environments for
*      32-Bit Microprocessors
*
* HISTORY:
*
* 27 APR 98  jay    initial release
*
*-------------------------------------------------------------------------*/

#include "netcomm.h"
#include "mpc860.h"
#include "masks860.h"
#include "SMC1.h"

int stack[100] = { 0 };
int stackTop;
char mystack[1000000];
int mystacktop,frontpoint,tailpoint,op,iii;

/***********************/
/* Global Declarations */
/***********************/

EPPC  *IMMR;      /* IMMR base pointer */
BDRINGS *RxTxBD;  /* buffer descriptors base pointer */
LB *SMC1Buffers;  /* SMC1 base pointers */

				  /*---------------------*/
				  /* Function Prototypes */
				  /*---------------------*/
void  SMC1Init(void);
void  SMC1PutChar(UBYTE);
UBYTE SMC1GetChar(void);
UBYTE SMC1INPoll(void);
UBYTE SMC1OUTPoll(void);
void  InitBDs(void);
void  EchoChar(void);
int GetIMMR();
/*-----------------------------------------------------------------------------
*
* FUNCTION NAME:  main
*
* DESCRIPTION:
*
*  This is the main function for the SMC1 code.
*
* EXTERNAL EFFECT:
*
* PARAMETERS:  None
*
* RETURNS: None
*
*---------------------------------------------------------------------------*/
int Tp, Rp;
void main(void)
{
	/*------------------------*/
	/* Establish IMMR pointer */
	/*------------------------*/
	int timr = 0;
	IMMR = (EPPC *)(GetIMMR() & 0xFFFF0000);  /* MPC8xx internal register
											  map  */

											  /*--------------------------------------------------------------------*/
											  /* We add 64 bytes to the start of the buffer descriptors because     */
											  /* this code was also tested on the monitor version of SDS debugger.  */
											  /* The monitor program on our target uses most of the first 64 bytes  */
											  /* for buffer descriptors. If you are not using the SDS monitor  with */
											  /* Motorola's ADS development board, you can delete 64 below and      */
											  /* start at the beginning of this particular block of Dual Port RAM.  */
											  /*--------------------------------------------------------------------*/

											  /*----------------------------------*/
											  /* Get pointer to BD area on DP RAM */
											  /*----------------------------------*/

	RxTxBD = (BDRINGS *)(IMMR->qcp_or_ud.ud.udata_bd + 64);
	/*-------------------------------------------------------------------*/
	/* Establish the buffer pool in Dual Port RAM. We do this because the*/
	/* pool size is only 2 bytes (1 for Rx and 1 for Tx) and to avoid    */
	/* disabling data cache for the memory region where BufferPool would */
	/* reside. The CPM does not recognize data in buffer pools once it   */
	/* been cached. It's acesses are direct through DMA to external      */
	/* memory.                                                           */
	/*-------------------------------------------------------------------*/
	SMC1Buffers = (LB *)(IMMR->qcp_or_ud.ud.udata_bd + 192);
	/*----------------------------------------*/
	/* Initialize SMC1 and buffer descriptors */
	/*----------------------------------------*/

	SMC1Init();
	op=1;
	frontpoint=0;
	tailpoint=0;
	while (1)
	{
		op=(op+1)%1000;
		/*--------------------------------------------------*/
		/* if there is a receive character echo it back out */
		/*--------------------------------------------------*/
		if (op!=0) {
			if (SMC1INPoll())   /* Check BD status for Rx characters */
			{
				mystack[tailpoint]=SMC1GetChar();
				tailpoint=(tailpoint+1)%200000;
			/*	for(iii=1;iii<=500;iii++);*/
			}
		}
		else {
			if (SMC1OUTPoll()) {
				if (frontpoint!=tailpoint) {
					if (mystack[frontpoint] <= 'z' && mystack[frontpoint] >= 'a') {
						SMC1PutChar(mystack[frontpoint] - 'a' + 'A');  
					}
					else if (mystack[frontpoint] <= 'Z' && mystack[frontpoint] >= 'A')
					{
						SMC1PutChar(mystack[frontpoint] - 'A' + 'a');
					}
					else
					{
						SMC1PutChar('\'');
						SMC1PutChar(mystack[frontpoint]);
						SMC1PutChar('\'');
					}	
					frontpoint=(frontpoint+1)%200000;
				}
			}
		}
	}
		

} /* END main */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  SMC1Init
  *
  * DESCRIPTION:
  *
  *  Initialize SMC1 as a UART at 9600 Baud
  *  with 24MHZ CPU clock.
  *
  * EXTERNAL EFFECT:
  *
  * PARAMETERS:  None
  *
  * RETURNS: None
  *
  *---------------------------------------------------------------------------*/

void SMC1Init(void)

{

	unsigned long *bcsr1;

	/*-----------------------*/
	/* Allow SMC1 TX, RX out */
	/*-----------------------*/

	IMMR->pip_pbpar |= (0x0C0);
	IMMR->pip_pbdir &= 0xFF3F;

	/*------------------------------------------------*/
	/* Set Baud Rate to 9600 for 24MHz System Clock.  *///38400 64MHz
														/* Enable BRG Count.									     */
														/*------------------------------------------------*/

	IMMR->brgc1 = (0x000CC | 0x10000);
	/*	IMMR->brgc2 = (0xC00C0 | 0x10000);*/

	IMMR->si_simode &= ~(0x0000F000);    /* SCM2:  NMSI mode */
										 /*	IMMR->si_simode |= 0x00000000;*/       /* SCM2:  Tx/Rx Clocks are BRG2 */


																				   /*----------------------------------------*/
																				   /* Set RXBD table start at Dual Port +800 */
																				   /*----------------------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.rbase =
		(UHWORD)&RxTxBD->RxBD[0];
	/*----------------------------------------*/
	/* Set TXBD table start at Dual Port +864 */
	/*----------------------------------------*/
	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.tbase =
		(UHWORD)&RxTxBD->TxBD[0];
	/*---------------------------------------*/
	/* Initialize Rx and Tx Params for SMC1: */
	/* Spin until cpcr flag is cleared		  */
	/*---------------------------------------*/

	for (IMMR->cp_cr = 0x0091; IMMR->cp_cr & 0x0001;);


	/*--------------------------------------*/
	/* Set RFCR,TFCR -- Rx,Tx Function Code */
	/* Normal Operation and Motorola byte   */
	/* ordering                             */
	/*--------------------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.rfcr = 0x10;


	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.tfcr = 0x10;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	/* Protocol Specific Parameters */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	/*---------------------------*/
	/* MRBLR = MAX buffer length */
	/*---------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.mrblr = 1;

	/*------------------------------------*/
	/* MAX_IDL = Disable MAX Idle Feature */
	/*------------------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.max_idl = 0;

	/*-------------------------------------*/
	/* BRKEC = No break condition occurred */
	/*-------------------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.brkec = 0;

	/*---------------------------------------*/
	/* BRKCR = 1 break char sent on top XMIT */
	/*---------------------------------------*/

	IMMR->PRAM[PAGE3].pg.other.smc_dsp1.psmc1.u1.brkcr = 1;


	/*--------------------*/
	/* Initialize the BDs */
	/*--------------------*/

	InitBDs();
	/*
	IMMR->smc_regs[SMC1_REG].smc_smce = 0xFF;  /* Clear any pending events */

	/*--------------------------------------------------*/
	/* SMC_SMCM = Mask all interrupts, use polling mode */
	/*--------------------------------------------------*/

	/*	IMMR->smc_regs[SMC1_REG].smc_smcm = 0;*/

	/*IMMR->cpmi_cicr = 0;            Disable all CPM interrups
	IMMR->cpmi_cipr |= ~(0x00000010);   Clear all pending interrupt events
	*/
	IMMR->cpmi_cimr &= ~0x00000010;           /* Mask all event interrupts */

											  /*-----------------------------------------------*/
											  /* Enable RS232 interface on ADS board via BCSR1 */
											  /* Get the base address of BCSR                  */
											  /*-----------------------------------------------*/

	bcsr1 = (UWORD *)((IMMR->memc_br1 & 0xFFFFFFFE) + 4);
	*bcsr1 &= 0xFFFB0000;  /* Assert the RS232EN* bit */

						   /*------------------------------------*/
						   /* 8-bit mode,  no parity, 1 stop-bit */
						   /* UART SMC Mode							  */
						   /* Normal operation (no loopback),	  */
						   /* SMC Transmitter/Receiver Enabled	  */
						   /*------------------------------------*/

	IMMR->smc_regs[SMC1_REG].smc_smcmr = 0x4823;

} /* END SMC1Init */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  InitBDs
  *
  * DESCRIPTION: This function initializes the Tx and Rx Buffer Descriptors.
  *
  * EXTERNAL EFFECT: RxTxBD
  *
  * PARAMETERS:  None
  *
  * RETURNS: None
  *
  *---------------------------------------------------------------------------*/

void InitBDs(void)

{
	/*-----------------------------------*/
	/* Setup Receiver Buffer Descriptors */
	/*-----------------------------------*/
	int i;
	for (i = 0; i < 8; i++)
	{
		if (i != 7)
		{
			RxTxBD->RxBD[i].bd_cstatus = 0x8000;//0x8000            /* Enable, Last BD */
			RxTxBD->TxBD[i].bd_cstatus = 0x0000;//0x0000      /* Buffer not yet ready; Last BD */
		}
		else
		{
			RxTxBD->RxBD[i].bd_cstatus = 0xa000;//0x8000            /* Enable, Last BD */
			RxTxBD->TxBD[i].bd_cstatus = 0x2000;//0x000      /* Buffer not yet ready; Last BD */
		}

		RxTxBD->RxBD[i].bd_length = 1;

		RxTxBD->RxBD[i].bd_addr = &(SMC1Buffers->RxBuffer[i]);//

		RxTxBD->TxBD[i].bd_length = 1;
		RxTxBD->TxBD[i].bd_addr = &(SMC1Buffers->TxBuffer[i]);
	}


	/*--------------------------------------*/
	/* Setup Transmitter Buffer Descriptors */
	/*--------------------------------------*/



} /* END InitBDs */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  EchoChar
  *
  * DESCRIPTION: This function facilitates the echoing of a received character.
  *
  * EXTERNAL EFFECT: RxTxBD
  *
  * PARAMETERS:  None
  *
  * RETURNS: None
  *
  *---------------------------------------------------------------------------*/

void  EchoChar(void)

{

	UBYTE mych;
	
	mych = SMC1GetChar();
	
	mystack[mystacktop++]=mych;

} /* end EchoChar */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  SMC1PutChar
  *
  * DESCRIPTION: Output a character to SMC1
  *
  * EXTERNAL EFFECT: RxTxBD
  *
  * PARAMETERS:  ch - input character
  *
  * RETURNS: None
  *
  *---------------------------------------------------------------------------*/

void SMC1PutChar(UBYTE ch)
{
	*(RxTxBD->TxBD[Tp].bd_addr) = ch;
	RxTxBD->TxBD[Tp].bd_length = 1;
	RxTxBD->TxBD[Tp].bd_cstatus |= 0x8000;  
	Tp = (Tp + 1) % 8;

} /* END SMC1PutChar */
UBYTE SMC1GetChar(void)
{
	UBYTE ch;   /* output character from SMC1 */

				/*--------------------*/
				/* Loop if RxBD empty */
				/*--------------------*/

/*	while (RxTxBD->RxBD[Rp].bd_cstatus & 0x8000);*/  /************!!!!!!!!!!!****************/
												   /*for (index = 0; index < 8;index = (index + 1) % 8)*/									   /*--------------*/										   /* Receive data */
												   /*--------------*/
	ch = *(RxTxBD->RxBD[Rp].bd_addr);
	/*----------------------*/
	/* Set Buffer Empty bit */
	/*----------------------*/
	RxTxBD->RxBD[Rp].bd_cstatus |= 0x8000;
	Rp = (Rp + 1) % 8;

	return ch;

} /* END SMC1GetChar */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  SMC1Poll
  *
  * DESCRIPTION: Poll SMC1 RxBD and check to see if a character was received
  *
  * EXTERNAL EFFECT: NONE
  *
  * PARAMETERS:  NONE
  *
  * RETURNS: A one if there is a character available in the receive buffer,
  *          else zero.
  *
  *---------------------------------------------------------------------------*/

UBYTE SMC1INPoll(void)

{
	if (RxTxBD->RxBD[Rp].bd_cstatus & 0x8000)
	{

		return 0;  /*  character NOT available */
	}

	else

	{
		return 1;  /*  character IS available */
	}


} /* END SMC1Poll */

UBYTE SMC1OUTPoll(void)

{
	if (RxTxBD->TxBD[Tp].bd_cstatus & 0x8000)
	{

		return 0;  /*  character NOT available */
	}

	else
	{
		return 1;  /*  character IS available */
	}


} /* END SMC1OUTPoll */



  /*-----------------------------------------------------------------------------
  *
  * FUNCTION NAME:  GetIMMR
  *
  * DESCRIPTION: Returns current value in the IMMR register.
  *
  * EXTERNAL EFFECT: NONE
  *
  * PARAMETERS:  NONE
  *
  * RETURNS: The IMMR value in r3. The compiler uses r3 as the register
  *          containing the return value.
  *
  *---------------------------------------------------------------------------*/

int GetIMMR()
{
	__asm__(" mfspr 3,638");
}


