
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_MACH_SP6820A
#include <mach/ldo.h>
#endif
#include "Qn8035_fm_ctrl.h"

#define I2C_BOARD_INFO_REGISTER_STATIC

#define I2C_RETRY_DELAY     5

#define I2C_RETRIES  3 

#define SLEEP_TO_WAKE_DELAY_TIME   5

#define	Qn8035_DEV_NAME	"KT0812G_FM"
#define Qn8035_I2C_NAME    Qn8035_DEV_NAME
#define Qn8035_I2C_ADDR    0x10

#define	KT0812G_FM_IOCTL_BASE     'R'
#define	KT0812G_FM_IOCTL_ENABLE		 _IOW(KT0812G_FM_IOCTL_BASE, 0, int)
#define KT0812G_FM_IOCTL_GET_ENABLE  _IOW(KT0812G_FM_IOCTL_BASE, 1, int)
#define KT0812g_FM_IOCTL_SET_TUNE    _IOW(KT0812G_FM_IOCTL_BASE, 2, int)
#define KT0812g_FM_IOCTL_GET_FREQ    _IOW(KT0812G_FM_IOCTL_BASE, 3, int)
#define KT0812G_FM_IOCTL_SEARCH      _IOW(KT0812G_FM_IOCTL_BASE, 4, int[4])
#define KT0812G_FM_IOCTL_STOP_SEARCH _IOW(KT0812G_FM_IOCTL_BASE, 5, int)
#define KT0812G_FM_IOCTL_MUTE        _IOW(KT0812G_FM_IOCTL_BASE, 6, int)
#define KT0812G_FM_IOCTL_SET_VOLUME  _IOW(KT0812G_FM_IOCTL_BASE, 7, int)
#define KT0812G_FM_IOCTL_GET_VOLUME  _IOW(KT0812G_FM_IOCTL_BASE, 8, int)




#define	Qn8035_FM_IOCTL_ENABLE		KT0812G_FM_IOCTL_ENABLE// _IOW(Qn8035_FM_IOCTL_BASE, 0, int)
#define Qn8035_FM_IOCTL_GET_ENABLE KT0812G_FM_IOCTL_GET_ENABLE// _IOW(Qn8035_FM_IOCTL_BASE, 1, int)
#define Qn8035_FM_IOCTL_SET_TUNE   KT0812g_FM_IOCTL_SET_TUNE// _IOW(Qn8035_FM_IOCTL_BASE, 2, int)
#define Qn8035_FM_IOCTL_GET_FREQ   KT0812g_FM_IOCTL_GET_FREQ //_IOW(Qn8035_FM_IOCTL_BASE, 3, int)
#define Qn8035_FM_IOCTL_SEARCH     KT0812G_FM_IOCTL_SEARCH// _IOW(Qn8035_FM_IOCTL_BASE, 4, int[4])
#define Qn8035_FM_IOCTL_STOP_SEARCH  KT0812G_FM_IOCTL_STOP_SEARCH//_IOW(Qn8035_FM_IOCTL_BASE, 5, int)
#define Qn8035_FM_IOCTL_MUTE        KT0812G_FM_IOCTL_MUTE//_IOW(Qn8035_FM_IOCTL_BASE, 6, int)
#define Qn8035_FM_IOCTL_SET_VOLUME  KT0812G_FM_IOCTL_SET_VOLUME//_IOW(Qn8035_FM_IOCTL_BASE, 7, int)
#define Qn8035_FM_IOCTL_GET_VOLUME  KT0812G_FM_IOCTL_GET_VOLUME//_IOW(Qn8035_FM_IOCTL_BASE, 8, int)





#ifdef I2C_BOARD_INFO_REGISTER_STATIC
#define I2C_STATIC_BUS_NUM        (2)

static struct i2c_board_info Qn8035_i2c_boardinfo = {
    I2C_BOARD_INFO(Qn8035_I2C_NAME, Qn8035_I2C_ADDR),
};
#endif

struct Qn8035_drv_data {
    struct i2c_client *client;
    struct class      fm_class;
    int               opened_before_suspend;
    int               bg_play_enable; /* enable/disable background play. */
    struct mutex      mutex;
    atomic_t          fm_opened;
    atomic_t          fm_searching;
    int               current_freq;
    int               current_volume;
    u8                muteOn;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend; 
#endif
};

//typedef void  (*Qn8035_SeekCallBack)(u8 ch, u8 bandtype);

struct Qn8035_drv_data *Qn8035_dev_data = NULL;
u8 qnd_chipID;
u8 qnd_IsQN8035B;
u8 qnd_CH_STEP  = 1;
u8 qnd_StepTbl[3]={5,10,20};
u16  qnd_CH_START = 7600;
u16  qnd_CH_STOP  = 10800;

u8   qnd_AutoScanAll = 1;
u8  qnd_ChCount = 0;
u16 qnd_ChList[QN_CCA_MAX_CH];
u8  qnd_div1; 
u8  qnd_div2;
u8  qnd_nco;
u16 FREQ;
u8 qnd_PreNoiseFloor = 40;
//Qn8035_SeekCallBack Qn8035_CallBackFunc = 0;


/***
 * Common i2c read and write function based i2c_transfer()
 *  The read operation sequence:
 *  7 bit chip address and Write command ("0") -> 8 bit
 *  register address n -> 7 bit chip address and Read command("1")
 *  The write operation sequence:
 * 7 bit chip address and Write command ("0") -> 8 bit
 * register address n -> write data n [15:8] -> write data n [7:0] 
 ***/
static int Qn8035_i2c_read(struct Qn8035_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;
    struct i2c_msg msgs[] = {
        {
            .addr = cxt->client->addr,
            .flags = cxt->client->flags & I2C_M_TEN,
            .len = 1,
            .buf = buf,
        },
        {
            .addr = cxt->client->addr,
            .flags = (cxt->client->flags & I2C_M_TEN) | I2C_M_RD,
            .len = len,
            .buf = buf,
        },
    };

    do {
        err = i2c_transfer(cxt->client->adapter, msgs, 2);

        if (err != 2)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 2) && (++tries < I2C_RETRIES));

    if (err != 2) {
        dev_err(&cxt->client->dev, "Read transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}


static int Qn8035_i2c_write(struct Qn8035_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;
    struct i2c_msg msgs[] = { 
        { 
            .addr = cxt->client->addr,
            .flags = cxt->client->flags & I2C_M_TEN,
            .len = len, 
            .buf = buf, 
        },
    };

    do {
        err = i2c_transfer(cxt->client->adapter, msgs, 1);
        if (err != 1)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 1) && (++tries < I2C_RETRIES));

    if (err != 1) {
        dev_err(&cxt->client->dev, "write transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}

/**
 * Notes: all register are 16bit wide, so the register R&W function uses 
 *   2-byte arguments. */
static int Qn8035_register_read(struct Qn8035_drv_data *cxt, 
        u8 reg_address, 
        u16 *value) 
{
    int  ret = -EINVAL;
    u8   buf[8] = {0};

    buf[0] = reg_address;

    ret = Qn8035_i2c_read(cxt, buf, 2);
    if (ret >= 0) {	
	*value = buf[0];
	}

    return ret;
}

static int Qn8035_register_write(struct Qn8035_drv_data *cxt, 
        u8 reg_address,
        u16 value)
{
    int  ret = -EINVAL;
    u8   buf[8] = {0};

    buf[0] = reg_address;

    buf[1] = value;

    ret = Qn8035_i2c_write(cxt, buf, 2);

    return ret;    
}


#if 1
void Qn8035_SetRegBit(struct Qn8035_drv_data *cxt,UINT8 reg,UINT8 bitMask,UINT8 data_val)
{
	u16 temp;
	u16 reg_value = 0;
	
	Qn8035_register_read(cxt,reg,&reg_value);

	temp = reg_value;
	temp &= (UINT8)(~bitMask);
	temp |= data_val & bitMask;
	Qn8035_register_write(cxt,reg,temp);
}

void Qn8035_Delay(u16 ms)
{
	 u16 i;

	while(ms--)
	{
		for(i = 0;i < 80000;i++)
		{
		}
	}
}
#endif


static int Qn8035_check_chip_id(struct Qn8035_drv_data *cxt, u16 *chip_id) {
    int ret = 0;
    int i;
    /* read chip id of Qn8035 */
	
    ret = Qn8035_register_read(cxt, 0x06, chip_id);
    if (ret < 0) {
        printk(&cxt->client->dev, "Read chip id failed.\n");
    }
    else {
        printk(&cxt->client->dev, "Qn8035 chip id:0x%04x\n", *chip_id);
    }

    return ret;
}

extern  int Qn8035_fm_power_on(struct Qn8035_drv_data * cxt);
static int Qn8035_fm_open(struct Qn8035_drv_data *cxt)
{
    int ret = -EINVAL;

    if (atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, 
            "FM open: already opened, ignore this operation\n");
        return ret;
    }
	Qn8035_fm_power_on(cxt);

     atomic_cmpxchg(&cxt->fm_opened, 0, 1);


}


extern  void Qn8035_Set_mode(struct Qn8035_drv_data * cxt, u16 mode);

static int Qn8035_fm_close(struct Qn8035_drv_data *cxt)
{
        dev_err(&cxt->client->dev, 
            "FM close: already close, ignore this operation\n");
	Qn8035_Set_mode(cxt, QND_MODE_SLEEP);
   if (atomic_read(&cxt->fm_opened))
    {
        atomic_cmpxchg(&cxt->fm_opened, 1, 0);
    }
}

#if 1


void Qn8035_Mute(struct Qn8035_drv_data *cxt,u8 On)
{
	if(On)
		{
			if(qnd_chipID == CHIPSUBID_QN8035A0)
				{
					Qn8035_SetRegBit(cxt,REG_DAC,0x0b,0x0b);
				}
			else
				{
					Qn8035_SetRegBit(cxt,0x4a,0x20,0x20);
				}
		
		}
	else
		{
		      if(qnd_chipID == CHIPSUBID_QN8035A0)
				{
					Qn8035_SetRegBit(cxt,REG_DAC,0x0b,0x00);
				}
			else
				{
					Qn8035_SetRegBit(cxt,0x4a,0x20,0x00);
				}

		}
}

void Qn8035_RXInit(struct Qn8035_drv_data *cxt)
{
	Qn8035_SetRegBit(cxt,0x1b,0x08,0x00);
	Qn8035_SetRegBit(cxt,0x2c,0x3f,0x12);
	Qn8035_SetRegBit(cxt,0x1d,0x40,0x00);
	Qn8035_SetRegBit(cxt,0x41,0x0f,0x0a);
	Qn8035_register_write(cxt,0x45,0x50);
	Qn8035_SetRegBit(cxt,0x3e,0x80,0x80);
	Qn8035_SetRegBit(cxt,0x41,0xe0,0xc0);

	if(qnd_chipID == CHIPSUBID_QN8035A0)
		{
			Qn8035_SetRegBit(cxt,0x42,0x10,0x10);
		}
}

void Qn8035_ConfigScan(struct Qn8035_drv_data *cxt,u16 start,u16 stop,u16 step)
{
	    u8 tStep = 0;
	    u8 tS;
	    u16 fStart;
	    u16 fStop;

  	    fStart = FREQ2CHREG(start);
   	    fStop = FREQ2CHREG(stop);


	   tS = (u8) fStart;
    	   Qn8035_register_write(cxt, CH_START,tS);
	   tStep |= ((u8) (fStart >> 6) & CH_CH_START);

	    tS = (u8) fStop;
    	   Qn8035_register_write(cxt,CH_STOP, tS);
	   tStep |= ((u8) (fStop >> 4) & CH_CH_STOP);

	    tStep |= step << 6;
    	    Qn8035_register_write(cxt,CH_STEP, tStep);
		   
}

static int Qn8035_SetCh(struct Qn8035_drv_data *cxt,u16 freq)
{
	    u8 tStep;
	    u8 tCh;
	    u16 f; 
	    u16 pll_dlt;
		u16 value;

    if(freq == 8550)
    	{
    		Qn8035_register_write(cxt,XTAL_DIV1, QND_XTAL_DIV1_855);
		Qn8035_register_write(cxt,XTAL_DIV2, QND_XTAL_DIV2_855);
		Qn8035_register_write(cxt,NCO_COMP_VAL, 0x69);
		freq = 8570;
    	}
	else
	{
		Qn8035_register_write(cxt,XTAL_DIV1, qnd_div1);
		Qn8035_register_write(cxt,XTAL_DIV2, qnd_div2);
    		Qn8035_register_write(cxt,NCO_COMP_VAL, qnd_nco);
	}

	Qn8035_SetRegBit(cxt,SYSTEM1, CCA_CH_DIS, CCA_CH_DIS); 
	f = FREQ2CHREG(freq); 
	tCh = (u8) f;
	Qn8035_register_write(cxt,CH, tCh);
	Qn8035_register_read(cxt,CH_STEP,&value);
	tStep = value;
	tStep &= ~CH_CH;
	tStep |= ((u8) (f >> 8) & CH_CH);
	Qn8035_register_write(cxt,CH_STEP, tStep);   
	cxt->current_freq = freq;
	return 0;

}

u8 Qn8035_TuneToCH(struct Qn8035_drv_data *cxt,u16 ch)
{
	u8 reg;
	u16 value;
    	u8 imrFlag = 0;
	 int  ret = -EPERM;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Set tune: FM not open\n");
        return ret;
    }


	Qn8035_SetRegBit(cxt,REG_REF,ICPREF,0x0a);
	Qn8035_RXInit(cxt);
	Qn8035_Mute(cxt, 1);
	if(qnd_chipID == 0x13)
		{
			if((ch == 7630) ||(ch == 8580)||(ch == 9340) ||(ch == 9390) ||(ch == 9530) ||(ch == 9980) ||(ch == 10480))
				{
					imrFlag = 1;
				}
		}
	else if((qnd_chipID == CHIPSUBID_QN8035A0)||(qnd_chipID == CHIPSUBID_QN8035A1))
		{
			if((ch == 6910) ||(ch == 7290) ||(ch == 8430))
				{
					imrFlag = 1;
				}
			else if(qnd_chipID == CHIPSUBID_QN8035A1)
				{
					if((ch == 7860) ||(ch == 10710))
						{
							imrFlag = 1;
						}
				}
		}

	if(imrFlag)
		{
			Qn8035_SetRegBit(cxt,CCA,IMR,IMR);
		}
	else
		{
			Qn8035_SetRegBit(cxt,CCA,IMR,0x00);
		}

	 Qn8035_ConfigScan(cxt,ch, ch, qnd_CH_STEP); 
    	 Qn8035_SetCh(cxt,ch);

    	if((qnd_chipID== CHIPSUBID_QN8035A0)||(qnd_chipID== CHIPSUBID_QN8035A1))
    	{
    		Qn8035_SetRegBit(cxt,SYSTEM1, CHSC, CHSC); 
        }
	else
		{
			Qn8035_SetRegBit(cxt,0x55, 0x80, 0x80); 
			Qn8035_SetRegBit(cxt,0x55, 0x80, 0x00); 
		}

	 	Qn8035_register_write(cxt,0x4f, 0x80);
		 Qn8035_register_read(cxt,0x4f,&value);
		value = value >> 1;
		 
		Qn8035_register_write(cxt,0x4f,value);

     if(qnd_chipID == CHIPSUBID_QN8035A0)
    	{
    	}
	 else
	 {
	 }

	 Qn8035_SetRegBit(cxt,REG_REF,ICPREF,0x00);

	  msleep(300);
	 Qn8035_Mute(cxt, 0);
	 
	return 0;
	
}


u16 Qn8035_GetCh(struct Qn8035_drv_data *cxt)
{

	u8 tCh;
	u8  tStep; 
	u16 ch = 0;
	u16 value;
	
	 Qn8035_register_read(cxt,0x0a,&value);
	 tStep = value;
	 tStep &= 0x03;
        ch  =  tStep ;
	 Qn8035_register_read(cxt,0x07,&value); 
	 tCh = value;
	 ch = (ch<<8)+tCh;

//	dev_info(&cxt->client->dev, "Qn8035_GetCh:%d\n", CHREG2FREQ(ch));


	 return CHREG2FREQ(ch)/10;

}

void Qn8035_RXSetTH(struct Qn8035_drv_data *cxt,u8 th)
{
	u8 rssiTH;
	u8 snrTH;                                  
	u16 rssi_snr_TH;

	u16 rssi_snr_TH_tbl [10] = { CCA_SENSITIVITY_LEVEL_0,CCA_SENSITIVITY_LEVEL_1,
		CCA_SENSITIVITY_LEVEL_2,CCA_SENSITIVITY_LEVEL_3,
		CCA_SENSITIVITY_LEVEL_4,CCA_SENSITIVITY_LEVEL_5,
		CCA_SENSITIVITY_LEVEL_6,CCA_SENSITIVITY_LEVEL_7,
		 CCA_SENSITIVITY_LEVEL_8,CCA_SENSITIVITY_LEVEL_9 };
	
	rssi_snr_TH = rssi_snr_TH_tbl[th];
	rssiTH = qnd_PreNoiseFloor - 28 + (u8) (rssi_snr_TH >> 8);

	snrTH = (u8) (rssi_snr_TH & 0xff);

//	Qn8035_register_write(cxt,0x4f, 0x00);//enable auto tunning in CCA mode
	Qn8035_SetRegBit(cxt,REG_REF,ICPREF,0x0a);

	Qn8035_SetRegBit(cxt,GAIN_SEL,0x08,0x08);
	Qn8035_SetRegBit(cxt,CCA1,0x30,0x30);
	Qn8035_SetRegBit(cxt,SYSTEM_CTL2,0x40,0x00);
	Qn8035_register_write(cxt,CCA_CNT1,0x00);

	Qn8035_SetRegBit(cxt,CCA_CNT2,0x3f,0x01);
	Qn8035_SetRegBit(cxt,CCA_SNR_TH_1 , 0xc0, 0x00);  
	Qn8035_SetRegBit(cxt,CCA_SNR_TH_2, 0xc0, 0x40);  

	
	Qn8035_SetRegBit(cxt,CCA, 0x3f, rssiTH);
//	dev_info(&cxt->client->dev, "rssiTH:0x%04x\n", rssiTH);

	
	Qn8035_SetRegBit(cxt,CCA_SNR_TH_1 , 0x3f, snrTH);
//	dev_info(&cxt->client->dev, "snrTH:0x%04x\n", snrTH);

	 
}

u8 Qn8035_RXValidCH(struct Qn8035_drv_data *cxt,u16 freq,u8 step)
{
	u8 regValue;
	u16 value;
	u8 timeOut = 40; //time out is 200ms
	u8 isValidChannelFlag = 0; 

//dev_info(&cxt->client->dev, "freq:0x%04x\n", freq);

	Qn8035_ConfigScan(cxt,freq, freq, step);
    	Qn8035_SetCh(cxt,freq);

	Qn8035_register_write(cxt,0x4f, 0x80);
	 Qn8035_register_read(cxt,0x4f,&value);
	 regValue = value;
   	 regValue = (regValue >> 1);
	Qn8035_register_write(cxt,0x4f, regValue);

	  Qn8035_SetRegBit(cxt,SYSTEM1,RXCCA_MASK,RX_CCA);
//	Qn8035_Delay(5);
msleep_interruptible(50);
do{
	Qn8035_register_read(cxt,SYSTEM1,&value);
	regValue = value;
//	Qn8035_Delay(1);
msleep_interruptible(5);
	timeOut--;
	}
while((regValue & CHSC) && timeOut);


//dev_info(&cxt->client->dev, "regValue:0x%04x\n", regValue);
//dev_info(&cxt->client->dev, "timeOut:0x%04x\n", timeOut);


Qn8035_register_read(cxt,STATUS1,&value);

isValidChannelFlag = (value & RXCCA_FAIL ? 0:1) && timeOut;

//dev_info(&cxt->client->dev, "isValidChannelFlag:0x%04x\n", isValidChannelFlag);


	    if(isValidChannelFlag)    
    		{			
			return 1;
	     }
	else 
		{
			return 0;
		}
	


	
}

u8 Qn8035_RxSeekCH(struct Qn8035_drv_data *cxt,u16 start, u16 stop, u8 step, u8 db, u8 up)
{
	u16 value;
	u16 freq;
	u8 validCH;
	u8 stepValue;
	u16 pStart = start;
	u16 pStop = stop;
	u8 regValue;
	u16 timeOut;
	u8 isValidChannelFlag = 0;

    	up=(start <= stop) ? 1 : 0;    
    	stepValue = qnd_StepTbl[step];
	 if(qnd_chipID == CHIPSUBID_QN8035A0)
	 	{
	 		 Qn8035_Mute(cxt,1);
       		 Qn8035_RXSetTH(cxt,db);
	 	

	        do
        	{       
            		validCH = Qn8035_RXValidCH(cxt,freq, step);
			 if (validCH == 0)
           		 {		
           		 	  if ((!up && (freq <= stop)) || (up && (freq >= stop)))
           		 	  	{
           		 	  		break;
           		 	  	}
				else
					{
						freq = freq + (up ? stepValue : -stepValue);
					}
			 }
			  else if(validCH == -1)
			  	{
			  	     return -1;
			  	}
	        }while(validCH == 0);
	        Qn8035_TuneToCH(cxt,freq);
	 	}

	 else
	 	{
	 		 if(qnd_AutoScanAll == 0)
			{
				Qn8035_Mute(cxt,1);           
				Qn8035_RXSetTH(cxt,db);
			}
			 do{
			 	Qn8035_ConfigScan(cxt,pStart, pStop, step);
				Qn8035_SetRegBit(cxt,SYSTEM1,RXCCA_MASK,RX_CCA);
				timeOut = 400; 
				while(1)
					{
						
						Qn8035_register_read(cxt,SYSTEM1,&value);  
						regValue = value;
						 if((regValue & CHSC) == 0) break;
						   if((timeOut--) == 0) return -1;
					}
				Qn8035_register_read(cxt,STATUS1,&value);
				  isValidChannelFlag = ( value & RXCCA_FAIL ? 0:1);
				   freq = Qn8035_GetCh(cxt);
				   if(isValidChannelFlag == 0)
					{	
				 	 	pStart = freq + (up ? stepValue : -stepValue);        
				   	}
				   
			 	}
			 while ((isValidChannelFlag == 0) && (up ? (pStart<=pStop):(pStart>=pStop)));   
		 if(isValidChannelFlag)
        	{
//        		  if(Qn8035_CallBackFunc)
  //              		Qn8035_CallBackFunc(freq, BAND_FM);
        	}
		 else
		 	{
		 		freq = 0;
		 	}
		  if(qnd_AutoScanAll == 0)
        	{
        		 Qn8035_TuneToCH(cxt,(freq ? freq: stop)); 
        	}
			 
	 	}
	 return 0;
}

 u8 Qn8035_RXSeekCHAll(struct Qn8035_drv_data *cxt,u16 start, u16 stop, u8 step, u8 db, u8 up) 
{
	u16 freq;
	u16 temp;
	u8  stepValue;

	stop = stop > qnd_CH_STOP ? qnd_CH_STOP : stop;
	Qn8035_Mute(cxt,1); 
	
	qnd_AutoScanAll = 1;
	qnd_ChCount = 0;
	up=(start<stop) ? 1 : 0;
	stepValue = qnd_StepTbl[step];
    	Qn8035_RXSetTH(cxt,db);
	freq=start;

    do
    {
        if(qnd_chipID == CHIPSUBID_QN8035A0)
        {
                temp = Qn8035_RXValidCH(cxt,freq, step);
            		if(temp == -1)
			{
				break;
			}
			else if(temp == 1) 
			{
				qnd_ChList[qnd_ChCount++] = freq;
			}
			 freq += (up ? stepValue : -stepValue);
        }
	else
	{
		 temp = Qn8035_RxSeekCH(cxt,freq, stop, step, db, up);
			if(temp == -1)
				{
					break;
				}
			else if(temp)
				{
					qnd_ChList[qnd_ChCount++] = temp;
				}
			else
				{
					 temp = stop;
				}
			 freq = temp + (up ? stepValue : -stepValue);
	}
      }

	while((up ? (freq<=stop):(freq>=stop)) && (qnd_ChCount < QN_CCA_MAX_CH));
	Qn8035_TuneToCH(cxt,((qnd_ChCount >= 1)? qnd_ChList[0] : stop)); 

	 qnd_AutoScanAll = 0;
    	return qnd_ChCount;
	
}



static u16 Qn8035_fm_get_frequency(struct Qn8035_drv_data *cxt)
{
	u8  ret;
	u16 frep;
	    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Get frequency: FM not open\n");
        return ret;
    }
	frep = Qn8035_GetCh(cxt);
	
	return frep;
}


static u16 Qn8035_fm_do_seek(struct Qn8035_drv_data *cxt, 
        u16 frequency, 
        u8  seek_dir)
{
    u16 reg_value = 0x0;
    int  ret = -EPERM;
	u16 value;
	//u16 temp = cxt->current_freq;
	u8 flag = 0;
 
    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Do seek: FM not open\n");
        return ret;
    }

    mutex_lock(&cxt->mutex);    

 //   Qn8035_TuneToCH(cxt, frequency);

//Qn8035_RXSetTH(cxt, 9);
//dev_info(&cxt->client->dev, "**********frequency=**********:0x%04x\n", frequency);

#if 1
if(seek_dir)
{
		if(cxt->current_freq > 1080)
			{
				cxt->current_freq = 875;
			}
		cxt->current_freq = frequency + 1;
	//	flag = Qn8035_RXValidCH(cxt, cxt->current_freq, 1);
}

else
{
	if(frequency < 8750)
		{
			frequency = 10800;
		}
	//	frequency = frequency - 10;
		flag = Qn8035_RXValidCH(cxt, frequency, 1);
//		Qn8035_ConfigScan(cxt, frequency, 0x222e, 10);

}

//dev_info(&cxt->client->dev, "**********1111111111frequency=**********:0x%04x\n", frequency);
//Qn8035_TuneToCH(cxt, frequency);

#endif

#if 0
if(seek_dir)
{
		if(cxt->current_freq> 10800)
			{
				cxt->current_freq = 8750;
			}
		cxt->current_freq = frequency + 10;
		flag = Qn8035_RXValidCH(cxt, cxt->current_freq, 1);
}

else
{
	if(cxt->current_freq < 8750)
		{
			cxt->current_freq = 10800;
		}
		cxt->current_freq = frequency - 10;
		flag = Qn8035_RXValidCH(cxt, cxt->current_freq, 1);
}
//Qn8035_TuneToCH(cxt, frequency);
#endif



//dev_info(&cxt->client->dev, "**********flag**********:0x%04x\n", flag);



   mutex_unlock(&cxt->mutex);

    return flag;
}



#endif


static int Qn8035_fm_full_search(struct Qn8035_drv_data *cxt, 
        u16  frequency, 
        u8   seek_dir,
        u32  time_out,
        u16 *freq_found)
{

    int      ret               = -EPERM;
    u16      reg_value         = 0x0;
    ulong    jiffies_comp      = 0;
    u8       is_timeout = 20;
    u8       is_search_end;
	u8 value;
	u8 flag;
	u8 isValidChannelFlag;
	u16 pStart,pStop,freq;
	u16 timeOut;
	u8 regValue;
	u8 stepValue;


 //	seek_dir ^= 0x1;
// atomic_cmpxchg(&cxt->fm_searching, 1, 0);
//dev_info(&cxt->client->dev, "cxt->current_freq = %d\n", cxt->current_freq);
//dev_info(&cxt->client->dev, "frequency = %d\n", frequency);
//dev_info(&cxt->client->dev, "seek_dir = %d\n", seek_dir);
if(frequency == cxt->current_freq)
	frequency += seek_dir?1:-1;
//dev_info(&cxt->client->dev, "frequency +=1; = %d\n", frequency);
	


#if 1
	
	if (!atomic_read(&cxt->fm_opened)) 
	{
	    dev_err(&cxt->client->dev, "Full search: FM not open\n");
	    return ret;
	}
        dev_err(&cxt->client->dev, "FM open successfully!\n");

	if (!atomic_read(&cxt->fm_searching))
	{
		atomic_cmpxchg(&cxt->fm_searching, 0, 1);
//		dev_err(&cxt->client->dev, "ccccccccccc\n");
		if (frequency == 0)
		{
//			dev_err(&cxt->client->dev, "ddddddddd\n");
			pStart = cxt->current_freq *10;					
		}
		else
		{
//		        dev_err(&cxt->client->dev, "bbbbbbbbbbbb\n");
		        
			pStart = frequency*10;
		}
	}
	else
	{
		dev_info(&cxt->client->dev, "%s, busy searching!", __func__);

		return -EBUSY;
	}


   


    	pStop =  seek_dir?10800: 8750;
        jiffies_comp = jiffies;
	Qn8035_RXSetTH(cxt, 2);
#if 0	
do{
		Qn8035_ConfigScan(cxt,pStart, pStop, 1);
		Qn8035_SetRegBit(cxt,SYSTEM1,RXCCA_MASK,RX_CCA);
		timeOut = 400; 
		stepValue = 10;
		while(1)
		{
			/* search is stopped manually */
			if (atomic_read(&cxt->fm_searching) == 0)
			break;
			if (msleep_interruptible(50) ||
			signal_pending(current)) 
			break;

			Qn8035_register_read(cxt,SYSTEM1,&value);  
			is_timeout = time_after(jiffies, 
			jiffies_comp + msecs_to_jiffies(time_out));
			regValue = value;
			if((regValue & CHSC) == 0) break;
			if(is_timeout) return -1;
		}
		Qn8035_register_read(cxt,STATUS1,&value);
		isValidChannelFlag = ( value & RXCCA_FAIL ? 0:1);
		freq = Qn8035_GetCh(cxt);
		if(isValidChannelFlag == 0)
		{	
			 	pStart = freq + (seek_dir ? stepValue : -stepValue);        
		}

	}
	while ((isValidChannelFlag == 0) && (seek_dir? (pStart<=pStop):(pStart>=pStop)));   
#endif
	Qn8035_ConfigScan(cxt,pStart, pStop, 1);
	cxt->current_freq = pStart;
//	Qn8035_SetCh(cxt, pStart);
	Qn8035_SetRegBit(cxt,SYSTEM1,RXCCA_MASK,RX_CCA);
	timeOut = 400; 
	stepValue = 10;
	while(1)
	{
		/* search is stopped manually */
		if (atomic_read(&cxt->fm_searching) == 0)
		break;
		if (msleep_interruptible(10) ||
		signal_pending(current)) 
		break;

		Qn8035_register_read(cxt,SYSTEM1,&value);  
		is_timeout = time_after(jiffies, 
		jiffies_comp + msecs_to_jiffies(time_out));
		regValue = value;
		if((regValue & CHSC) == 0) break;
		if(is_timeout) break;
	}
	Qn8035_register_read(cxt,STATUS1,&value);
	isValidChannelFlag = ( value & RXCCA_FAIL ? 0:1);
	freq = Qn8035_GetCh(cxt);
    	atomic_cmpxchg(&cxt->fm_searching, 1, 0);


	if(isValidChannelFlag)
	{
//		dev_info(&cxt->client->dev, "seek valid  freq:%d\n", freq);
		ret = 0;
	}
	else
	{
		 ret = -EAGAIN;
	}
	
//	dev_info(&cxt->client->dev, "%s, isValidChannelFlag=%d   is_timeout ", __func__,  isValidChannelFlag  , is_timeout);


	*freq_found = freq;
	cxt->current_freq = freq;


//	dev_info(&cxt->client->dev, "ret=:%d   freq:%d\n",ret, freq);












	



#endif

return ret;


 

}

static int Qn8035_fm_set_volume(struct Qn8035_drv_data *cxt, u8 volume_real)
{

	u8 regVal;
	u8 volume;
	
	if(volume_real > 15)
	{
		volume = QND_VOLUME_LEVEL7;
	}
	if(volume_real == 0)
	{
		Qn8035_SetRegBit(cxt,VOL_CTL, 0x80, 0x80);
	}
	else
	{
		Qn8035_SetRegBit(cxt,VOL_CTL, 0x80, 0x00);
	}



	if(volume_real == 15) 
	{
		volume = QND_VOLUME_LEVEL15;
	}

        else if (volume_real == 14)
	{
		volume = QND_VOLUME_LEVEL14;
	}
        else if (volume_real == 13)
	{
		volume = QND_VOLUME_LEVEL13;
	}
        else if (volume_real == 12)
	{
		volume = QND_VOLUME_LEVEL12;
	}
        else if (volume_real == 11)
	{
		volume = QND_VOLUME_LEVEL11;
	}
        else if (volume_real == 10)
	{
		volume = QND_VOLUME_LEVEL10;
	}
        else if (volume_real == 9)
	{
		volume = QND_VOLUME_LEVEL9;
	}
        else if (volume_real == 8)
	{
		volume = QND_VOLUME_LEVEL8;
	}
        else if (volume_real == 7)
	{
		volume = QND_VOLUME_LEVEL7;
	}
        else if (volume_real == 6)
	{
		volume = QND_VOLUME_LEVEL6;
	}
        else if (volume_real == 5)
	{
		volume = QND_VOLUME_LEVEL5;
	}
        else if (volume_real == 4)
	{
		volume = QND_VOLUME_LEVEL4;
	}
        else if (volume_real == 3)
	{
		volume = QND_VOLUME_LEVEL3;
	}
        else if (volume_real == 2)
	{
		volume = QND_VOLUME_LEVEL2;
	}
        else if (volume_real == 1)
	{
		volume = QND_VOLUME_LEVEL1;
	}
	else if(volume_real == 0)
	{
		volume = QND_VOLUME_LEVEL0;
	}

		regVal = (u8)(volume/6);  
		Qn8035_SetRegBit(cxt,VOL_CTL, 0x07, regVal);   //set analog gain
		regVal = (u8)(volume%6);
		Qn8035_SetRegBit(cxt,VOL_CTL, 0x38, (u8)((5-regVal)<<3));

        cxt->current_volume = volume_real;


}

static int Qn8035_fm_get_volume(struct Qn8035_drv_data *cxt)
{

	return cxt->current_volume;
}

static int Qn8035_fm_stop_search(struct Qn8035_drv_data *cxt)
{
	atomic_cmpxchg(&cxt->fm_searching, 1, 0);
	
	return 0;
}

static int Qn8035_fm_misc_open(struct inode *inode, struct file *filep)
{
    int ret = -EINVAL;

    ret = nonseekable_open(inode, filep);
    if (ret < 0) {
        pr_err("Qn8035 open misc device failed.\n");
        return ret;
    }

    filep->private_data = Qn8035_dev_data;

    return 0;
}



static int Qn8035_fm_misc_ioctl(struct inode *inode, struct file *filep,
        unsigned int cmd, unsigned long arg)
{
    void  __user         *argp       = (void __user *)arg;
    int                       ret        = 0;
    int                       iarg       = 0;  
    int                       buf[4]     = {0};
    struct Qn8035_drv_data  *dev_data   = filep->private_data;




    switch (cmd) {
        case Qn8035_FM_IOCTL_ENABLE:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_ENABLE", __func__);

            if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1) {
                ret =  -EFAULT;
            }
            if (iarg ==1) {
                ret = Qn8035_fm_open(dev_data);
            }
            else {
                ret = Qn8035_fm_close(dev_data);
            }
 
            break;

        case Qn8035_FM_IOCTL_GET_ENABLE:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_GET_ENABLE", __func__);
            iarg = atomic_read(&dev_data->fm_opened);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;

        case Qn8035_FM_IOCTL_SET_TUNE:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_SET_TUNE", __func__);
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
			iarg = 10 * iarg;
			
//		dev_info(&dev_data->client->dev, "Qn8035_TuneToCH  iarg:%d\n", iarg);
		Qn8035_TuneToCH(dev_data, iarg);
          break;

        case Qn8035_FM_IOCTL_GET_FREQ:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_GET_FREQ", __func__);
	    iarg = Qn8035_GetCh(dev_data); 
           if (copy_to_user(argp, &iarg, sizeof(iarg))) {
			printk("copy_form_user is erro!\n");
                	ret = -EFAULT;
            }
		   	
            break;

        case Qn8035_FM_IOCTL_SEARCH:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_SEARCH", __func__);
            if (copy_from_user(buf, argp, sizeof(buf))) {
			printk("copy_form_user is erro!\n");
			ret = -EFAULT;
            }
				
			
//		dev_err(&dev_data->client->dev, "Search FM  \n");

            ret = Qn8035_fm_full_search(dev_data, 
                 buf[0], /* start frequency */
                   buf[1], /* seek direction*/
                   buf[2], /* time out */
                   (u16*)&buf[3]);/* frequency found will be stored to */


		

			
            break;

        case Qn8035_FM_IOCTL_STOP_SEARCH:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_STOP_SEARCH", __func__);
            ret = Qn8035_fm_stop_search(dev_data);
            break;

        case Qn8035_FM_IOCTL_MUTE:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_MUTE", __func__);
            break;

        case Qn8035_FM_IOCTL_SET_VOLUME:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_SET_VOLUME", __func__);
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
            ret = Qn8035_fm_set_volume(dev_data, (u8)iarg);
            break;            

        case Qn8035_FM_IOCTL_GET_VOLUME:
//		dev_info(&dev_data->client->dev, "%s, Qn8035_FM_IOCTL_GET_VOLUME", __func__);
            iarg = Qn8035_fm_get_volume(dev_data);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;            

        default:
            return -EINVAL;
    }

    return ret;

}


static const struct file_operations Qn8035_fm_misc_fops = {
    .owner = THIS_MODULE,
    .open  = Qn8035_fm_misc_open,
    .ioctl = Qn8035_fm_misc_ioctl,
};

static struct miscdevice Qn8035_fm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = Qn8035_DEV_NAME,
    .fops  = &Qn8035_fm_misc_fops,
};


/*------------------------------------------------------------------------------
 * Qn8035 class attribute method
 ------------------------------------------------------------------------------*/
static ssize_t Qn8035_fm_attr_open(struct class *class , const char *buf, size_t size)
{
    u8 open;

    if (size) {
        open = simple_strtol(buf, NULL, 10);
        if (open)
            Qn8035_fm_open(Qn8035_dev_data);
        else 
            Qn8035_fm_close(Qn8035_dev_data);
    }

    return size;
}


static ssize_t Qn8035_fm_attr_get_open(struct class *class, char *buf)
{
    u8 opened;

    opened = atomic_read(&Qn8035_dev_data->fm_opened);
    if (opened) 
        return sprintf(buf, "Opened\n");
    else
        return sprintf(buf, "Closed\n");
}


static ssize_t Qn8035_fm_attr_set_tune(struct class *class, const char *buf, size_t size)
{
    u16 frequency;

    if (size) {
        frequency = simple_strtol(buf, NULL, 10);/* decimal string to int */


        Qn8035_SetCh(Qn8035_dev_data, frequency);
    }

    return size;
}

static ssize_t Qn8035_fm_attr_get_frequency(struct class *class, char *buf)
{
    u16 frequency;

    frequency = Qn8035_fm_get_frequency(Qn8035_dev_data);

    return sprintf(buf, "Frequency %d\n", frequency);
}


static ssize_t Qn8035_fm_attr_search(struct class *class, const char *buf, size_t size)
{
    u32 timeout;
    u16 frequency;
    u8  seek_dir;
    u16 freq_found;
    char *p = (char*)buf;
    char *pi = NULL;

    if (size) {
        while (*p == ' ') p++;
        frequency = simple_strtol(p, &pi, 10); /* decimal string to int */
        if (pi == p) goto out;

        p = pi;
        while (*p == ' ') p++;
        seek_dir = simple_strtol(p, &pi, 10);
        if (pi == p) goto out;

        p = pi;
        while (*p == ' ') p++;
        timeout = simple_strtol(p, &pi, 10);
        if (pi == p) goto out;

       // Qn8035_fm_full_search(Qn8035_dev_data, frequency, seek_dir, timeout, &freq_found);
    } 

out:
    return size;
}


static ssize_t Qn8035_fm_attr_set_volume(struct class *class, const char *buf, size_t size)
{
    u8 volume;

    if (size) {
        volume = simple_strtol(buf, NULL, 10);/* decimal string to int */

        Qn8035_fm_set_volume(Qn8035_dev_data, volume);
    }

    return size;
}

static ssize_t Qn8035_fm_attr_get_volume(struct class *class, char *buf)
{
    u8 volume;

    volume = Qn8035_fm_get_volume(Qn8035_dev_data);

    return sprintf(buf, "Volume %d\n", volume);
}

static struct class_attribute Qn8035_fm_attrs[] = {
    __ATTR(fm_open,   S_IRUGO|S_IWUGO, Qn8035_fm_attr_get_open,      Qn8035_fm_attr_open),
    __ATTR(fm_tune,   S_IRUGO|S_IWUGO, Qn8035_fm_attr_get_frequency, Qn8035_fm_attr_set_tune),
    __ATTR(fm_seek,   S_IWUGO,         NULL,                          Qn8035_fm_attr_search),
    __ATTR(fm_volume, S_IRUGO|S_IWUGO, Qn8035_fm_attr_get_volume,    Qn8035_fm_attr_set_volume),
    {},     
};

void Qn8035_fm_sysfs_init(struct class *class)
{
    class->class_attrs = Qn8035_fm_attrs;
}



























static void Qn8035_fm_Init(struct Qn8035_drv_data *cxt)
{
		u16 value;
		u8 i;
		Qn8035_register_write(cxt,0x00, 0x81);
		
	//	Qn8035_Delay(10);
		Qn8035_register_read(cxt,CID1,&value);
		qnd_chipID = ((value) & 0x03);
		Qn8035_register_read(cxt,0x58,&value);
		
		qnd_IsQN8035B = ((value) & 0x1f);
		
		if(qnd_IsQN8035B == 0x13)
			{
				Qn8035_register_write(cxt,0x58,0x13);
				Qn8035_SetRegBit(cxt,0x58,0x80,QND_INPUT_CLOCK);
			}
		
		Qn8035_SetRegBit(cxt,0x01,0x80,QND_DIGITAL_CLOCK);
		Qn8035_register_write(cxt,XTAL_DIV0,QND_XTAL_DIV0);
		Qn8035_register_write(cxt,XTAL_DIV1,QND_XTAL_DIV1);
		Qn8035_register_write(cxt,XTAL_DIV2,QND_XTAL_DIV2);

		
		//Qn8035_Delay(10);
		Qn8035_register_write(cxt,0x54,0x47);
		Qn8035_register_write(cxt,SMP_HLD_THRD,0xc4);
		Qn8035_SetRegBit(cxt,0x40,0x70,0x70);
		Qn8035_register_write(cxt,0x33,0x9c);
		Qn8035_register_write(cxt,0x2d,0xd6);
		Qn8035_register_write(cxt,0x43,0x10);
		Qn8035_SetRegBit(cxt,SMSTART,0x7f,SMSTART_VAL);
		Qn8035_SetRegBit(cxt,SNCSTART,0x7f,SNCSTART_VAL);
		Qn8035_SetRegBit(cxt,HCCSTART,0x7f,HCCSTART_VAL);
		if(qnd_chipID == CHIPSUBID_QN8035A1)
			{
				Qn8035_SetRegBit(cxt,0x47,0x0c,0x08);
			}


		Qn8035_register_read(cxt,XTAL_DIV1,&value);
		qnd_div1 = value;
		Qn8035_register_read(cxt,XTAL_DIV2,&value);
		qnd_div2 = value;
		Qn8035_register_read(cxt,NCO_COMP_VAL,&value);
		qnd_nco = value;
		cxt->current_volume = 15;
}



 void Qn8035_Set_mode(struct Qn8035_drv_data *cxt,u16 mode)
{
	u8 val,temp;
	switch(mode){
		case QND_MODE_SLEEP:
			Qn8035_SetRegBit(cxt,REG_DAC,0x08,0x00);
			Qn8035_SetRegBit(cxt,SYSTEM1,STNBY_RX_MASK,STNBY_MODE);
			break;
		case QND_MODE_WAKEUP:
			Qn8035_SetRegBit(cxt,REG_DAC,0x08,0x00);
			Qn8035_SetRegBit(cxt,SYSTEM1,STNBY_RX_MASK,STNBY_MODE);

		//	Qn8035_Delay(SLEEP_TO_WAKEUP_DELAY_TIME);

			break;
		default:
			val = (u8)(mode >>8);
			if(val)
				{
					val = val >> 3;
					Qn8035_register_read(cxt,SYSTEM1,&temp);
					if(val & 0x10)
						{
							if((temp & STNBY_RX_MASK) != val)
								{
								
									Qn8035_SetRegBit(cxt,SYSTEM1,STNBY_RX_MASK,val);
								}
						}
					}
			break;
		}
}


 int Qn8035_fm_power_on(struct Qn8035_drv_data *cxt)
{

	Qn8035_fm_Init(cxt);
	Qn8035_Set_mode(cxt, 0x8000);

    	return 0;

}


static int Qn8035_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    u16    reg_value = 0x0;
    int    ret = -EINVAL;

    struct Qn8035_drv_data *cxt = NULL;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "Qn8035 driver: client is not i2c capable.\n");
        ret = -ENODEV;
        goto i2c_functioality_failed;
    }

    cxt = kzalloc(sizeof(struct Qn8035_drv_data), GFP_KERNEL);
    if (cxt == NULL) {
        dev_err(&client->dev, "Can't alloc memory for module data.\n");
        ret = -ENOMEM;
        goto alloc_data_failed;
    }

    mutex_init(&cxt->mutex);
    mutex_lock(&cxt->mutex);

    cxt->client = client;
    i2c_set_clientdata(client, cxt);

    atomic_set(&cxt->fm_searching, 0);
    atomic_set(&cxt->fm_opened, 0);
    ret = Qn8035_fm_power_on(cxt);

    if (ret < 0) {
        goto poweron_failed;
    }
	

	
    Qn8035_check_chip_id(cxt, &reg_value);


#if 1

    Qn8035_fm_close(cxt); //add gongzhen

    cxt->fm_class.owner = THIS_MODULE;
    cxt->fm_class.name = "fm_class";
    Qn8035_fm_sysfs_init(&cxt->fm_class);
    ret = class_register(&cxt->fm_class);
    if (ret < 0) {
        dev_err(&client->dev, "Qn8035 class init failed.\n");
        goto class_init_failed;
    }

    ret = misc_register(&Qn8035_fm_misc_device);
    if (ret < 0) {
        dev_err(&client->dev, "Qn8035 misc device register failed.\n");
        goto misc_register_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
//    cxt->early_suspend.suspend = kt0812g_early_suspend;
 //   cxt->early_suspend.resume  = kt0812g_early_resume;
 //   cxt->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
//    register_early_suspend(&cxt->early_suspend);
#endif
    cxt->opened_before_suspend = 0;
    cxt->bg_play_enable = 1;
    cxt->current_freq = 875; /* init current frequency, search may use it. */
    
    Qn8035_dev_data = cxt;

    mutex_unlock(&cxt->mutex);
#endif
    return ret; 
misc_register_failed:
    misc_deregister(&Qn8035_fm_misc_device);
class_init_failed:    
    class_unregister(&cxt->fm_class);
poweron_failed:
    mutex_unlock(&cxt->mutex);
    kfree(cxt);
alloc_data_failed:
i2c_functioality_failed:
    dev_err(&client->dev,"Qn8035 driver init failed.\n");
    return ret;
} 



static const struct i2c_device_id Qn8035_i2c_id[] = {
	{Qn8035_I2C_NAME,0},
	{},
};


MODULE_DEVICE_TABLE(i2c,Qn8035_i2c_id);

static struct i2c_driver Qn8035_i2c_driver = {
	.driver = {
	.name = Qn8035_I2C_NAME,	
	},
	.probe = Qn8035_probe,
	.id_table = Qn8035_i2c_id,
};




int i2c_static_add_device(struct i2c_board_info *info)
{
    struct i2c_adapter *adapter;
    struct i2c_client  *client;
    int    ret;
    adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);
    if (!adapter) {
        pr_err("%s: can't get i2c adapter\n", __func__);
        ret = -ENODEV;
        goto i2c_err;
    }

    client = i2c_new_device(adapter, info);
    if (!client) {
		
//	pr_err("%s: can't add i2c device at 0x%x\n",
//		__FUNCTION__,(unsigned int)info->addr);


	ret = -ENODEV;
        goto i2c_err;
    }
	
    i2c_put_adapter(adapter);

	return 0;

i2c_err:
	return ret;



}


static int __init Qn8035_driver_init(void)
{
    int  ret = 0;        
    printk("Qn8035 driver: init\n");
#ifdef I2C_BOARD_INFO_REGISTER_STATIC
    ret = i2c_static_add_device(&Qn8035_i2c_boardinfo);
    if (ret < 0) {
        pr_err("%s: add i2c device error %d\n", __func__, ret);
        goto init_err;
    }
#endif 

    return i2c_add_driver(&Qn8035_i2c_driver);

init_err:
    return ret;
}

static void __exit Qn8035_driver_exit(void)
{
    printk("Qn8035 driver exit\n");

#ifdef I2C_BOARD_INFO_REGISTER_STATIC
    i2c_unregister_device(Qn8035_dev_data->client);
#endif

    i2c_del_driver(&Qn8035_i2c_driver);
    return;
}




module_init(Qn8035_driver_init);
module_exit(Qn8035_driver_exit);

MODULE_DESCRIPTION("Qn8035 FM radio driver");
MODULE_AUTHOR("Spreadtrum Inc.");
MODULE_LICENSE("GPL");

