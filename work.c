#include "MPC860.h"
#include "vxWorks.h"
#include "semLib.h"
#include "taskLib.h"
#include "iv.h"
#include "intLib.h"
#include "drv/intrCtl/ppc860Intr.h"
#define MY_INT_VEC IV_TIMER1

struct eppc *pimm; /* POINTER TO INTNL MEM MAP */
				   /* 信号量*/
SEM_ID	semSync;

extern int 	taskSpawn(char *name, int priority, int options, int stackSize,
	FUNCPTR entryPt, int arg1, int arg2, int arg3,
	int arg4, int arg5, int arg6, int arg7,
	int arg8, int arg9, int arg10);
extern SEM_ID 	semBCreate(int options, SEM_B_STATE initialState);
extern STATUS 	semTake(SEM_ID semId, int timeout);
extern STATUS 	semGive(SEM_ID semId);
extern STATUS 	intConnect(VOIDFUNCPTR *vector, VOIDFUNCPTR routine,
	int parameter);

#define Signal semGive
#define TIMRR 1

int n, a[N];

void Up(int i)
{
	while(i/2>=1) {
		if(a[i] < a[i/2]) swap(a[i], a[i/2]);
		else break;  i = i/2;
	}
}

void Down(int i)
{
	while(i*2<=n) {
		int tp = i*2;
		if(tp+1<=n && a[tp+1]<a[tp]) tp++;
		if(a[i]<a[tp]) swap(a[i], a[tp]), i = tp;
		else break;
	}
}

void Push(int x)
{
	a[++n] = x; up(n);
}

int Pop()
{
	swap(a[1], a[n]); n--;
	if(n) down(1); return a[n+1];
}

void Wait(SEM_ID &tmp)	//等待信号量（需要再次测试） 
{
	semTake(tmp, WAIT_FOREVER);
}

unsigned int getimmr()
{
	__asm__(" mfspr 3,638");
}

void sw_light(int x)	//灯，正数为开，负数为关，零为全关 
{
	if (x == 0) pimm->pio_pddat |= 0x1C00;
	else if (x == 1) pimm->pio_pddat &= ~0x0400;
	else if (x == 2) pimm->pio_pddat &= ~0x0800;
	else if (x == 3) pimm->pio_pddat &= ~0x1000;
	else if (x == -1) pimm->pio_pddat |= 0x0400;
	else if (x == -2) pimm->pio_pddat |= 0x0800;
	else if (x == -3) pimm->pio_pddat |= 0x1000;
}

void initPIMM()	//初始化PIMM 
{
	pimm = (struct eppc*) (getimmr() & 0xFFFF0000);
	pimm->pio_pddir |= 0x1C00; /* MAKE PORT PD3-5 OUTPUT*/
	pimm->pio_pddat &= ~0x1C00; /* CLEAR PORT PD3/PD4/PD5 */
	sw_light(0);
}

void myTask()	//中断相关的线程 
{
	static int i = 0;
	while (1)
	{
		/* wait for Interrupt */
		Wait(semSync);
		sw_light(0);
		sw_light(i + 1);
		i = (i + 1) % 3;
	}
}

void Clear_Interrupt()	//清除中断（中断结束） 
{
	if (TIMRR == 1)
	{
		pimm->timer_ter1 |= 0xFFFF;
		pimm->cpmi_cisr |= 0x02000000;
	}
	else
	{
		pimm->timer_ter2 |= 0xFFFF;
        pimm->cpmi_cisr |= 0x00040000;
	}
}

void myIsr()	//（中断执行程序） 
{
	Signal(semSync);
	Clear_Interrupt();
}

void init_tim(double tim)	//计时器初始化，间隔tim秒一个中断 
{
	if (TIMRR == 1)
	{
		pimm->timer_trr1 = (short)(20000 * tim);
		pimm->timer_tmr1 = 0xC81C;

		pimm->timer_tgcr = 0x0000;
		pimm->timer_tcn1 = 0x0000;
		pimm->timer_ter1 = 0xFFFF;
		pimm->cpmi_cimr = 0x02000000;
		pimm->timer_tgcr = 0x0001;
	}
	else
	{
		pimm->timer_trr2 = (short)(20000 * tim);
		pimm->timer_tmr2 = 0xC81C;
		
		pimm->timer_tgcr = 0x0000;
		pimm->timer_tcn2 = 0x0000;
		pimm->timer_ter2 |= 0xFFFF;
		pimm->cpmi_cimr |= 0x00040000;
		pimm->timer_tgcr = 0x0010;
	}
}

extern SEM_ID Create(int z)	//建立一个初值为z的信号量 
{
	if (z == 0) return semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	else semBCreate(SEM_Q_FIFO, SEM_FULL);
}

int main()
{
	/* create a binary semaphore that is initially empty */
	semSync = Create(0);

	/* Initialize software/hardware for device */
	if (TIMRR == 1) intConnect(57, myIsr, 0);
	else if (TIMMRR == 2) intConnect(50, myIsr, 0);
	initPIMM();
	init_tim(2);
	/* ... */
	taskSpawn("myTask", 100, 0, 20000, (FUNCPTR)	//名字，优先级，x，栈空间，对应函数 
		myTask, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	return(0);
}

