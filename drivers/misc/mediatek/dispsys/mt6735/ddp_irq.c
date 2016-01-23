
#define LOG_TAG "IRQ"

#include "ddp_log.h"
#include "ddp_debug.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>

#include <mach/mt_irq.h>
#include "ddp_reg.h"
#include "ddp_irq.h"
#include "ddp_aal.h"

// IRQ log print kthread
static struct task_struct *disp_irq_log_task = NULL;
static wait_queue_head_t disp_irq_log_wq;
static int disp_irq_log_module = 0;

static int irq_init = 0;

static unsigned int cnt_rdma_underflow[2];
static unsigned int cnt_rdma_abnormal[2];
static unsigned int cnt_ovl_underflow[2];
static unsigned int cnt_wdma_underflow[2];

unsigned long long rdma_start_time[2]={0};
unsigned long long rdma_end_time[2]={0};
#define DISP_MAX_IRQ_CALLBACK   10

static DDP_IRQ_CALLBACK irq_module_callback_table[DISP_MODULE_NUM][DISP_MAX_IRQ_CALLBACK];
static DDP_IRQ_CALLBACK irq_callback_table[DISP_MAX_IRQ_CALLBACK];

#ifdef CONFIG_MTK_SEGMENT_TEST
unsigned int record_rdma_end_interval    = 0;
unsigned long long rdma_end_max_interval = 0;
unsigned long long rdma_end_begin_time   = 0;
unsigned long long rdma_end_min_interval = 20*1000000; // 20 ms
#endif
atomic_t ESDCheck_byCPU = ATOMIC_INIT(0);
int disp_register_irq_callback(DDP_IRQ_CALLBACK cb)
{
    int i = 0;
    for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
        if (irq_callback_table[i] == cb)
            break;
    }
    if (i < DISP_MAX_IRQ_CALLBACK)
        return 0;

    for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
        if (irq_callback_table[i] == NULL)
            break;
    }
    if (i == DISP_MAX_IRQ_CALLBACK) {
        DDPERR("not enough irq callback entries for module\n");
        return -1;
    }
    DDPMSG("register callback on %d\n", i);
    irq_callback_table[i] = cb;
    return 0;
}

int disp_unregister_irq_callback(DDP_IRQ_CALLBACK cb)
{
    int i;
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_callback_table[i] == cb)
        {
            irq_callback_table[i] = NULL;
            break;
        }
    }
    if (i == DISP_MAX_IRQ_CALLBACK)
    {
        DDPERR("Try to unregister callback function %p which was not registered\n",cb);
        return -1;
    }
    return 0;
}

int disp_register_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
    int i;
    if (module >= DISP_MODULE_NUM)
    {
        DDPERR("Register IRQ with invalid module ID. module=%d\n", module);
        return -1;
    }
    if (cb == NULL)
    {
        DDPERR("Register IRQ with invalid cb.\n");
        return -1;
    }
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == cb)
            break;
    }
    if (i < DISP_MAX_IRQ_CALLBACK)
    {
        // Already registered.
        return 0;
    }
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == NULL)
            break;
    }
    if (i == DISP_MAX_IRQ_CALLBACK)
    {
        DDPERR("No enough callback entries for module %d.\n", module);
        return -1;
    }
    irq_module_callback_table[module][i] = cb;
    return 0;
}

int disp_unregister_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
    int i;
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == cb)
        {
            irq_module_callback_table[module][i] = NULL;
            break;
        }
    }
    if (i == DISP_MAX_IRQ_CALLBACK)
    {
        DDPERR("Try to unregister callback function with was not registered. module=%d cb=%p\n", module, cb);
        return -1;
    }
    return 0;
}

void disp_invoke_irq_callbacks(DISP_MODULE_ENUM module, unsigned int param)
{
    int i;
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
            
        if (irq_callback_table[i])
        {
            //DDPERR("Invoke callback function. module=%d param=0x%X\n", module, param);
            irq_callback_table[i](module,param);
        }
        
        if (irq_module_callback_table[module][i])
        {
            //DDPERR("Invoke module callback function. module=%d param=0x%X\n", module, param);
            irq_module_callback_table[module][i](module,param);
        }
    }
}
extern int dispsys_irq[DISP_REG_NUM];
//Mark out for eliminate build warning message, because it is not used
#if 0
static DISP_MODULE_ENUM find_module_by_irq(int irq)
{
	// should sort irq_id_to_module_table by numberic sequence
	int i = 0; 
    #define DISP_IRQ_NUM_MAX (DISP_REG_NUM)
    static  struct irq_module_map {
        int irq;
        DISP_MODULE_ENUM module;
    }irq_id_to_module_table[DISP_IRQ_NUM_MAX] = {
        {0, DISP_MODULE_OVL0   },
        {0, DISP_MODULE_OVL1   },
        {0, DISP_MODULE_RDMA0  },
        {0, DISP_MODULE_RDMA1  },
        {0, DISP_MODULE_WDMA0  },
        {0, DISP_MODULE_COLOR0 },
        {0, DISP_MODULE_CCORR  },
        {0, DISP_MODULE_AAL    },
        {0, DISP_MODULE_GAMMA  },
        {0, DISP_MODULE_DITHER },
        {0, DISP_MODULE_UFOE   },
        {0, DISP_MODULE_PWM0    },
        {0, DISP_MODULE_WDMA1  },
        {0, DISP_MODULE_MUTEX  },
        {0, DISP_MODULE_DSI0   },
        {0, DISP_MODULE_DPI    },

    };
    irq_id_to_module_table[0].irq = dispsys_irq[DISP_REG_OVL0  ];
    irq_id_to_module_table[1].irq = dispsys_irq[DISP_REG_OVL1  ];
    irq_id_to_module_table[2].irq = dispsys_irq[DISP_REG_RDMA0 ];
    irq_id_to_module_table[3].irq = dispsys_irq[DISP_REG_RDMA1 ];
    irq_id_to_module_table[4].irq = dispsys_irq[DISP_REG_WDMA0 ];
    irq_id_to_module_table[5].irq = dispsys_irq[DISP_REG_COLOR ];
    irq_id_to_module_table[6].irq = dispsys_irq[DISP_REG_CCORR ];                                           
    irq_id_to_module_table[7].irq = dispsys_irq[DISP_REG_AAL   ];                                           
    irq_id_to_module_table[8].irq = dispsys_irq[DISP_REG_GAMMA ];                                           
    irq_id_to_module_table[9].irq = dispsys_irq[DISP_REG_DITHER];                                           
    irq_id_to_module_table[10].irq = dispsys_irq[DISP_REG_UFOE ];                                           
    irq_id_to_module_table[11].irq = dispsys_irq[DISP_REG_PWM  ];                                           
    irq_id_to_module_table[12].irq = dispsys_irq[DISP_REG_WDMA1];                                           
    irq_id_to_module_table[13].irq = dispsys_irq[DISP_REG_MUTEX];                                           
    irq_id_to_module_table[14].irq = dispsys_irq[DISP_REG_DSI0 ];                                           
    irq_id_to_module_table[15].irq = dispsys_irq[DISP_REG_DPI0 ]; 

	for(i = 0;i<DISP_IRQ_NUM_MAX ;i++)
	{
		if(irq_id_to_module_table[i].irq == irq)
       {
            DDPDBG("find module %d by irq %d \n", irq,irq_id_to_module_table[i].module);
			return irq_id_to_module_table[i].module;
       }
	}
	if(i==DISP_IRQ_NUM_MAX)
	{
        DDPERR("can not find irq=%d in irq_id_to_module_table\n", irq);
	}
	return DISP_MODULE_UNKNOWN;
}
#endif

static char* disp_irq_module(unsigned int irq)
{
    if(irq==dispsys_irq[DISP_REG_OVL0  ]) return "ovl0  ";
    else if(irq==dispsys_irq[DISP_REG_OVL1  ]) return "ovl1  ";
    else if(irq==dispsys_irq[DISP_REG_RDMA0 ]) return "rdma0 ";
    else if(irq==dispsys_irq[DISP_REG_RDMA1 ]) return "rdma1 ";
    else if(irq==dispsys_irq[DISP_REG_WDMA0 ]) return "wdma0 ";
    else if(irq==dispsys_irq[DISP_REG_COLOR ]) return "color ";
    else if(irq==dispsys_irq[DISP_REG_CCORR ]) return "ccorr ";
    else if(irq==dispsys_irq[DISP_REG_AAL   ]) return "aal   ";
    else if(irq==dispsys_irq[DISP_REG_GAMMA ]) return "gamma ";
    else if(irq==dispsys_irq[DISP_REG_DITHER]) return "dither";
    else if(irq==dispsys_irq[DISP_REG_UFOE  ]) return "ufoe  ";
    else if(irq==dispsys_irq[DISP_REG_PWM   ]) return "pwm   ";
    else if(irq==dispsys_irq[DISP_REG_WDMA1 ]) return "wdma1 ";
    else if(irq==dispsys_irq[DISP_REG_MUTEX ]) return "mutex ";
    else if(irq==dispsys_irq[DISP_REG_DSI0  ]) return "dsi0  ";
    else if(irq==dispsys_irq[DISP_REG_DPI0  ]) return "dpi0  ";
    else return "unknown";
}

///TODO:  move each irq to module driver
unsigned int rdma_start_irq_cnt[2] = {0, 0};
unsigned int rdma_done_irq_cnt[2] = {0, 0};
unsigned int rdma_underflow_irq_cnt[2] = {0, 0};
unsigned int rdma_targetline_irq_cnt[2] = {0, 0};
unsigned int ovl_complete_irq_cnt[2] = {0, 0};

extern unsigned int gResetRDMAEnable;
extern unsigned int is_hwc_enabled;

extern int primary_display_is_video_mode(void);
irqreturn_t disp_irq_handler(int irq, void *dev_id)
{
    DISP_MODULE_ENUM module = DISP_MODULE_UNKNOWN;
    unsigned long reg_val = 0;
    unsigned int index = 0;
    unsigned int mutexID = 0;
    unsigned long reg_temp_val = 0;
    DDPDBG("disp_irq_handler, irq=%d, module=%s \n", irq, disp_irq_module(irq));
    MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagStart, irq, 0);

    //switch(irq)
    {
    	if(irq==dispsys_irq[DISP_REG_DSI0])
	{
            module = DISP_MODULE_DSI0;
	    reg_val = (DISP_REG_GET(dsi_reg_va + 0xC) & 0xff);
            if(atomic_read(&ESDCheck_byCPU) == 0)
            {
	        reg_temp_val=reg_val&0xfffe;//rd_rdy don't clear and wait for ESD & Read LCM will clear the bit.
                DISP_CPU_REG_SET(dsi_reg_va + 0xC, ~reg_temp_val);
            }
            else
            {
                DISP_CPU_REG_SET(dsi_reg_va + 0xC, ~reg_val);
            }
            MMProfileLogEx(ddp_mmp_get_events()->DSI_IRQ[0], MMProfileFlagPulse, reg_val, 0);
        }
        else if(irq==dispsys_irq[DISP_REG_OVL0] || irq==dispsys_irq[DISP_REG_OVL1])
		{
                index = (irq==dispsys_irq[DISP_REG_OVL0]) ? 0 : 1;
                module= (irq==dispsys_irq[DISP_REG_OVL0]) ? DISP_MODULE_OVL0 : DISP_MODULE_OVL1;
                reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA+index*DISP_OVL_INDEX_OFFSET);
                if(reg_val&(1<<1))
                {
                    DDPIRQ("IRQ: OVL%d frame done! \n",index);
                    ovl_complete_irq_cnt[index]++;

                    // update OVL addr
                    {
                        unsigned int i = 0;
                        if(index==0)
	                    {
	                        for(i=0;i<4;i++)
	                        {
	                            if(DISP_REG_GET(DISP_REG_OVL_SRC_CON)&(0x1<<i))
	                                MMProfileLogEx(ddp_mmp_get_events()->layer[i], MMProfileFlagPulse, DISP_REG_GET(DISP_REG_OVL_L0_ADDR+i*0x20), 0);	  
	                        } 
                        }
                        if(index==1)
	                    {
	                        for(i=0;i<4;i++)
	                        {
	                            if(DISP_REG_GET(DISP_REG_OVL_SRC_CON+DISP_OVL_INDEX_OFFSET)&(0x1<<i))
	                                MMProfileLogEx(ddp_mmp_get_events()->ovl1_layer[i], MMProfileFlagPulse, DISP_REG_GET(DISP_REG_OVL_L0_ADDR+DISP_OVL_INDEX_OFFSET+i*0x20), 0);	  
	                        }
	                    }
                    }
                }
                if(reg_val&(1<<2))
                {
                    //DDPERR("IRQ: OVL%d frame underrun! cnt=%d \n",index, cnt_ovl_underflow[index]++);
                    //disp_irq_log_module |= 1<<module;
                }
                if(reg_val&(1<<3))
                {
                      DDPIRQ("IRQ: OVL%d sw reset done\n",index);
                }
                if(reg_val&(1<<4))
                {
                      DDPIRQ("IRQ: OVL%d hw reset done\n",index);
                }  
                if(reg_val&(1<<5))
                {
                    DDPERR("IRQ: OVL%d-L0 not complete untill EOF!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }
                if(reg_val&(1<<6))
                {
                    DDPERR("IRQ: OVL%d-L1 not complete untill EOF!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }                
                if(reg_val&(1<<7))
                {
                    DDPERR("IRQ: OVL%d-L2 not complete untill EOF!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }  
                if(reg_val&(1<<8))
                {
                    DDPERR("IRQ: OVL%d-L3 not complete untill EOF!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }  
                if(reg_val&(1<<9))
                {
                    //DDPERR("IRQ: OVL%d-L0 fifo underflow!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }  

                if(reg_val&(1<<10))
                {
                    //DDPERR("IRQ: OVL%d-L1 fifo underflow!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }  
                if(reg_val&(1<<11))
                {
                    //DDPERR("IRQ: OVL%d-L2 fifo underflow!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }  
                if(reg_val&(1<<12))
                {
                    //DDPERR("IRQ: OVL%d-L3 fifo underflow!\n",index);
                    //disp_irq_log_module |= 1<<module;
                }
                //clear intr

                if(reg_val&(0xf<<5))
                {
                    ddp_dump_analysis(DISP_MODULE_CONFIG);
                    if(index==0)
                    {
                        ddp_dump_analysis(DISP_MODULE_OVL1);
                        ddp_dump_analysis(DISP_MODULE_OVL0);
                        ddp_dump_analysis(DISP_MODULE_COLOR0);
                        ddp_dump_analysis(DISP_MODULE_AAL);
                        ddp_dump_analysis(DISP_MODULE_RDMA0);
                    }
                    else
                    {
                        ddp_dump_analysis(DISP_MODULE_OVL1);
                        ddp_dump_analysis(DISP_MODULE_RDMA1);
                        ddp_dump_reg(DISP_MODULE_CONFIG);
                    }
                }
                
                DISP_CPU_REG_SET(DISP_REG_OVL_INTSTA+index*DISP_OVL_INDEX_OFFSET, ~reg_val);     
                MMProfileLogEx(ddp_mmp_get_events()->OVL_IRQ[index], MMProfileFlagPulse, reg_val, 0);
                if(reg_val&0x1e0)
                {
                    MMProfileLogEx(ddp_mmp_get_events()->ddp_abnormal_irq, MMProfileFlagPulse, (index<<16)|reg_val, module<<24);
                }
        	}
        else if(irq==dispsys_irq[DISP_REG_WDMA0] || irq==dispsys_irq[DISP_REG_WDMA1])
		{
                index = (irq==dispsys_irq[DISP_REG_WDMA0]) ? 0 : 1;
                module =(irq==dispsys_irq[DISP_REG_WDMA0]) ? DISP_MODULE_WDMA0 : DISP_MODULE_WDMA1;
                reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA+index*DISP_WDMA_INDEX_OFFSET);
                if(reg_val&(1<<0))
                {
                    DDPIRQ("IRQ: WDMA%d frame done!\n",index);
                }
                if(reg_val&(1<<1))
                {
                    DDPERR("IRQ: WDMA%d underrun! cnt=%d\n",index,cnt_wdma_underflow[index]++);
                    disp_irq_log_module |= 1<<module;
                }
                //clear intr
                DISP_CPU_REG_SET(DISP_REG_WDMA_INTSTA+index*DISP_WDMA_INDEX_OFFSET,~reg_val);
                MMProfileLogEx(ddp_mmp_get_events()->WDMA_IRQ[index], MMProfileFlagPulse, reg_val, DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
                if(reg_val&0x2)
                {
                    MMProfileLogEx(ddp_mmp_get_events()->ddp_abnormal_irq, MMProfileFlagPulse, (index<<16)|reg_val, cnt_wdma_underflow[index]|(module<<24));
                }
        }
        else if(irq==dispsys_irq[DISP_REG_RDMA0] || irq==dispsys_irq[DISP_REG_RDMA1])
		{
                if(dispsys_irq[DISP_REG_RDMA0]==irq)
                {
                    index = 0;
                    module = DISP_MODULE_RDMA0;
                }
                else if(dispsys_irq[DISP_REG_RDMA1]==irq)
                {
                    index = 1;
                    module = DISP_MODULE_RDMA1;
                }

                reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS+index*DISP_RDMA_INDEX_OFFSET);
                if(reg_val&(1<<0))
                {
                      DDPIRQ("IRQ: RDMA%d reg update done! \n",index);
                }
                if(reg_val&(1<<1))
                {
                      MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagStart, reg_val, DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR));
              	      
					  rdma_start_time[index]= sched_clock();
                      DDPIRQ("IRQ: RDMA%d frame start! \n",index);
                      rdma_start_irq_cnt[index]++;
                      
                      // rdma start/end irq should equal, else need reset ovl
                      if(gResetRDMAEnable == 1 &&
                         is_hwc_enabled == 1 &&
                         index ==0 &&
                         primary_display_is_video_mode()==1 &&
                         rdma_start_irq_cnt[0] > rdma_done_irq_cnt[0]+3)
                      {
                          ovl_reset(DISP_MODULE_OVL0, NULL);
                          if(ovl_get_status()!=DDP_OVL1_STATUS_SUB)
                          {
                              ovl_reset(DISP_MODULE_OVL1, NULL);
                          }
                          rdma_done_irq_cnt[0] = rdma_start_irq_cnt[0];
                          DDPERR("warning: reset ovl!\n");
                      }
                      
#ifdef CONFIG_MTK_SEGMENT_TEST
						if(record_rdma_end_interval == 1)
						{
							if(rdma_end_begin_time == 0)
							{
								rdma_end_begin_time = sched_clock();
								//printk("[display_test]====RDMA frame end time1:%lld\n",rdma_end_begin_time);	
							}
							else
							{
								unsigned long long time_now = sched_clock();
								//printk("[display_test]====RDMA frame end time2:%lld\n",time_now);	

								//printk("[display_test]====RDMA frame end time3:this=%lld,max=%lld,min=%lld\n",time_now - rdma_end_begin_time,rdma_end_max_interval,rdma_end_min_interval);	
								if((time_now - rdma_end_begin_time) > rdma_end_max_interval)
								{
									rdma_end_max_interval = time_now - rdma_end_begin_time;
								}
								if((time_now - rdma_end_begin_time) < rdma_end_min_interval)
								{
									rdma_end_min_interval = time_now - rdma_end_begin_time;
								}
								rdma_end_begin_time = time_now;
							}
						}
#endif
                }
                if(reg_val&(1<<2))
                {
                      MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagEnd, reg_val, 0);
					  rdma_end_time[index]= sched_clock();
                      DDPIRQ("IRQ: RDMA%d frame done! \n",index);
                      //rdma_done_irq_cnt[index] ++;
                      rdma_done_irq_cnt[index] = rdma_start_irq_cnt[index];
                }
                if(reg_val&(1<<3))
                {
                      DDPERR("IRQ: RDMA%d abnormal! cnt=%d \n",index, cnt_rdma_abnormal[index]++);
                      disp_irq_log_module |= 1<<module;

                }
                if(reg_val&(1<<4))
                {
                      DDPERR("IRQ: RDMA%d underflow! cnt=%d \n",index, cnt_rdma_underflow[index]++);
                      disp_irq_log_module |= 1<<module;
                      rdma_underflow_irq_cnt[index]++;
                }
                if(reg_val&(1<<5))
                {
                      DDPIRQ("IRQ: RDMA%d target line! \n",index);
                      rdma_targetline_irq_cnt[index]++;
                }
                //clear intr
                DISP_CPU_REG_SET(DISP_REG_RDMA_INT_STATUS+index*DISP_RDMA_INDEX_OFFSET,~reg_val);       
                MMProfileLogEx(ddp_mmp_get_events()->RDMA_IRQ[index], MMProfileFlagPulse, reg_val, 0);
                if(reg_val&0x18)
                {
                    MMProfileLogEx(ddp_mmp_get_events()->ddp_abnormal_irq, MMProfileFlagPulse, (index<<16)|reg_val, rdma_underflow_irq_cnt[index]|(cnt_rdma_abnormal[index]<<8)||(module<<24));
                }
        }
        else if(irq==dispsys_irq[DISP_REG_COLOR])
		{

        }
        else if(irq==dispsys_irq[DISP_REG_MUTEX])
		{
            // mutex0: perimary disp
            // mutex1: sub disp
            // mutex2: aal
            module = DISP_MODULE_MUTEX;
            reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & 0x7C1F;
            for(mutexID = 0; mutexID<5; mutexID++)
            {
                if(reg_val & (0x1<<mutexID))
                {
                    DDPIRQ("IRQ: mutex%d sof!\n",mutexID);
                    MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID], MMProfileFlagPulse, reg_val, 0);
                }
                if(reg_val & (0x1<<(mutexID+DISP_MUTEX_TOTAL)))
                {
                    DDPIRQ("IRQ: mutex%d eof!\n",mutexID);
                    MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID], MMProfileFlagPulse, reg_val, 1);
                }
            }
            DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);
        }
        else if(irq==dispsys_irq[DISP_REG_AAL])
		{
            module = DISP_MODULE_AAL;
            reg_val = DISP_REG_GET(DISP_AAL_INTSTA);
            disp_aal_on_end_of_frame();
        }
        else if(irq==dispsys_irq[DISP_REG_CONFIG])  // MMSYS error intr
		{
            reg_val = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_INTSTA) & 0x7;
            if(reg_val&(1<<0))
            {
                DDPERR("MMSYS to MFG APB TX Error, MMSYS clock off but MFG clock on! \n");
            }
            if(reg_val&(1<<1))
            {
                DDPERR("MMSYS to MJC APB TX Error, MMSYS clock off but MJC clock on! \n");
            }
            if(reg_val&(1<<2))
            {
                DDPERR("PWM APB TX Error! \n");
            }

            DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_INTSTA, ~reg_val);
		}
        else
        {
            module = DISP_MODULE_UNKNOWN;
            reg_val = 0;
            DDPERR("invalid irq=%d \n ", irq); 
        }
    }
    disp_invoke_irq_callbacks(module, reg_val);
    if(disp_irq_log_module!=0)
    {
        wake_up_interruptible(&disp_irq_log_wq);
    }
    MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagEnd, irq, reg_val);
    return IRQ_HANDLED;
}


// extern smi_dumpDebugMsg(void);
static int disp_irq_log_kthread_func(void *data)
{
    unsigned int i=0;
    while(1)
    {
        wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
        DDPMSG("disp_irq_log_kthread_func dump intr register: disp_irq_log_module=%d \n", disp_irq_log_module);
        if((disp_irq_log_module&(1<<DISP_MODULE_RDMA0))!=0 )
        {
            // ddp_dump_analysis(DISP_MODULE_CONFIG);
            ddp_dump_analysis(DISP_MODULE_RDMA0);
            ddp_dump_analysis(DISP_MODULE_OVL0);
            ddp_dump_analysis(DISP_MODULE_OVL1);
            
            // dump ultra/preultra related regs
            DDPMSG("wdma_con1(2c)=0x%x, wdma_con2(0x38)=0x%x, rdma_gmc0(30)=0x%x, rdma_gmc1(38)=0x%x, fifo_con(40)=0x%x \n",
               DISP_REG_GET(DISP_REG_WDMA_BUF_CON1),
               DISP_REG_GET(DISP_REG_WDMA_BUF_CON2),
               DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0),
               DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1),
               DISP_REG_GET(DISP_REG_RDMA_FIFO_CON));
            DDPMSG("ovl0_gmc: 0x%x, 0x%x, 0x%x, 0x%x, ovl1_gmc: 0x%x, 0x%x, 0x%x, 0x%x, \n",
               DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
               DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING),
               DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING),
               DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING),
               DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING+DISP_OVL_INDEX_OFFSET),
               DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING+DISP_OVL_INDEX_OFFSET),
               DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING+DISP_OVL_INDEX_OFFSET),
               DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING+DISP_OVL_INDEX_OFFSET));
            
            // dump smi regs
            // smi_dumpDebugMsg();
        }
        else
        {
            for(i=0;i<DISP_MODULE_NUM;i++)
            {
                if( (disp_irq_log_module&(1<<i))!=0 )
                {
                    ddp_dump_reg(i);
                }
            }
        }
        disp_irq_log_module = 0;
    }
    return 0;
}

void disp_register_dev_irq(unsigned int irq_num, char *device_name)
{
     if(request_irq( irq_num , (irq_handler_t)disp_irq_handler, 
                    IRQF_TRIGGER_LOW, device_name , NULL))
    { 
        DDPERR("ddp register irq %u failed on device %s\n", irq_num,device_name); 
    }
    return;
}

int disp_init_irq(void)
{
    if(irq_init)
        return 0;

    irq_init = 1;
	DDPMSG("disp_init_irq\n");
 
#if 0    // irq mapped in DT probe()
    static char * device_name="mtk_disp";
    // Register IRQ
    disp_register_dev_irq(DISP_OVL0_IRQ_BIT_ID      ,"DISP_OVL0");
    disp_register_dev_irq(DISP_OVL1_IRQ_BIT_ID      ,"DISP_OVL1");
    disp_register_dev_irq(DISP_RDMA0_IRQ_BIT_ID     ,"DISP_RDMA0");
    disp_register_dev_irq(DISP_RDMA1_IRQ_BIT_ID     ,"DISP_RDMA1");
    disp_register_dev_irq(DISP_RDMA2_IRQ_BIT_ID     ,"DISP_RDMA2");
    disp_register_dev_irq(DISP_WDMA0_IRQ_BIT_ID     ,"DISP_WDMA0");
    disp_register_dev_irq(DISP_WDMA1_IRQ_BIT_ID     ,"DISP_WDMA1");
	disp_register_dev_irq(DSI0_IRQ_BIT_ID			,"DISP_DSI0");
    //disp_register_dev_irq(DISP_COLOR0_IRQ_BIT_ID ,device_name);
    //disp_register_dev_irq(DISP_COLOR1_IRQ_BIT_ID ,device_name);
    //disp_register_dev_irq(DISP_GAMMA_IRQ_BIT_ID  ,device_name );
    //disp_register_dev_irq(DISP_UFOE_IRQ_BIT_ID     ,device_name);
    disp_register_dev_irq(MM_MUTEX_IRQ_BIT_ID       ,"DISP_MUTEX");
    disp_register_dev_irq(DISP_AAL_IRQ_BIT_ID       ,"DISP_AAL");
#endif

    //create irq log thread
    init_waitqueue_head(&disp_irq_log_wq);
    disp_irq_log_task = kthread_create(disp_irq_log_kthread_func, NULL, "ddp_irq_log_kthread");
    if (IS_ERR(disp_irq_log_task))
    {
        DDPERR(" can not create disp_irq_log_task kthread\n");
    }
    wake_up_process(disp_irq_log_task);
    return 0;
}

#ifdef CONFIG_MTK_SEGMENT_TEST

void disp_record_rdma_end_interval(unsigned int enable)
{
	record_rdma_end_interval = enable;

	if(record_rdma_end_interval)
	{
		rdma_end_begin_time   = 0;
		rdma_end_max_interval = 0;	
		rdma_end_min_interval = 20*1000000; // 20 ms
	}
}

unsigned long long disp_get_rdma_max_interval()
{
	return rdma_end_max_interval;
}

unsigned long long disp_get_rdma_min_interval()
{
	return rdma_end_min_interval;
}
#endif
