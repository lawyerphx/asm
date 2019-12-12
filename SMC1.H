/*-----------------------------------------------------------------------------
*
* File:  860smc.h
*
* Description:
*
* 		Constants and Definitions for 8xx SMC UART Example.
*
* History:
*
* 09 APR 98   ecg     Initial version.
* 27 APR 98   jay     revised for initialization example
*---------------------------------------------------------------------------*/


#define SMC1_REG 1          /* SMC Regs Array Index for SMC1 */

/*-------------------------*/
/* Single buffer component */
/*-------------------------*/

typedef struct BufferPool

{
	UBYTE  RxBuffer[8];
	UBYTE  TxBuffer[8];

} LB;


/*--------------------------*/
/* Buffer Descriptor Format */
/*--------------------------*/

typedef struct BufferDescriptor

{
	UHWORD bd_cstatus;     /* control and status */
	UHWORD bd_length;      /* transfer length */
	UBYTE  *bd_addr;       /* buffer address */

} BD;


/*-------------------------------*/
/* Buffer Descriptor Ring format */
/*-------------------------------*/

typedef struct BufferDescRings

{
	BD RxBD[8];    /* Rx BD ring */
	BD TxBD[8];    /* Tx BD ring */

} BDRINGS;


/*-----------------------------------*/
/* Board Control and Status Register */
/*-----------------------------------*/

typedef struct bcsr

{
	UWORD  bcsr0;
	UWORD  bcsr1;
	UWORD  bcsr2;
	UWORD  bcsr3;

} BCSR;

















