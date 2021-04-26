/*
 * 串口或红外发送风速数据和是否倒计时
 * 发送格式：1个8位16进制数表示风速(00-FF), 2个BCD码, 例如: 80 30 00
 * 若发送的时间为00 00, 则不开启倒计时
 * 风速由00-FF表示, 80H为关闭,倒计时时间为BCD码 由零至59分59秒,时间为0时不进行倒计时
 * 数码管前两位为风速档位-3, -2, -1, 0, 01, 02, 03 后4位对应分钟和秒
 *                    (00H 20H 40H 80H C0H E0H FFH)
 * 当不进行倒计时或倒计时结束时,后四位显示'0'
 * 
 * 风速保存在30H中, 倒计时时间BCD码保存在31H, 32H中
 * 
 * CS0接数码管, CS1接DA转换, CS2接红外接收
 * 
 * 245_CS 接CS2, 245_I0 接红外接收
 * 
*/

#include <reg52.h>

typedef unsigned int u16;
typedef unsigned char u8;

u8 xdata *BIT_CHOOSE = 0x8002;  // 位选
u8 xdata *SEG_CHOOSE = 0x8004;  // 段码
u8 xdata *DA_PORT = 0x9000;     // DA转换

u8 data LEDBUF[6];              // 显存
u8 data ir_value[4];            // 红外接收缓冲
u8 data t0_cnt;                 // 定时器计数
u8 data power_status;           // 档位
u8 data time_min;               // minutes
u8 data time_sec;               // seconds

bit data isrunning = 0;         // 是否运行
bit data istimeing = 0;         // 是否倒计时
bit data status_changed = 0;    // 状态是否修改
sbit IRIN=P3^2;                 // 红外接收入口

u8 code TAB[17] = {
    // 0、1、2、3、4、5、6、7
    0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,
    // 8、9、A、b、C、d、E、F
    0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71
};

void flush_buff();
void delay(u16 i);

void ReadIr() interrupt 0
{
    // 引导码：9ms 0, 4.5ms 1
    // 数据0: 0.56ms 0, 0.56ms 1
    // 数据1: 0.56ms 0, 1.7ms 1
    // 接收格式为：1个起始码，4位用户码，4位用户反码，4位数据码，4位数据反码

	u8 j, k, htime;   // j,k 用来循环计数 htime计算接收数据时高电平的时长
	u16 err;            // 防止低电平或高电平超时
	htime = 0;	    // 高电平时长初始化				 
	delay(700);	        // 收到红外信号后等待7ms
	if (IRIN != 0)		// 若还为0则代表起始码, 若不是起始码则结束
        return;
		
    err = 1000;			//1000 * 10us = 10ms
    while ((IRIN == 0) && (err > 0))	//等起始码的9ms低电平结束	
    {			
        delay(1);       // 10us
        err--;
    }
    
    if (IRIN != 1)		// 如果超时后依旧没低电平，退出
        return;

    err = 500;            // 500 * 10us = 5ms
    while ((IRIN == 1) && (err > 0)) //等待4.5ms的高电平
    {
        delay(1);
        err--;
    }

    for (k = 0; k < 4; k++)		// 接收4组4位数据
    {
        for (j = 0; j < 8; j++)  // 一组4位的数据
        {
            htime=0;          // 计算高电平时长
            err = 60;		    // 60 * 10us = 0.6ms
            while ((IRIN == 0) && (err > 0)) // 等待560us低电平
            {
                delay(1);
                err--;
            }
            while (IRIN == 1)	 // 计算高电平时常
            {
                delay(10);	 // 0.1ms
                htime++;
                err--;
                if (htime > 30)    // 0.1ms * 30 = 3ms, 超时
                    return;
            }
            ir_value[k] >>= 1;
            if(htime >= 8) //如果高电平出现大于800us，是1
            {
                ir_value[k] |= 0x80;    // 从左往右移动，最高位为1
            }
        }
    }

    if(ir_value[2] != ~ir_value[3])     // 校验数据是否正确
        return;
    
    status_changed = 1;                 // 收到红外信号，状态被更改
    if (isrunning == 0 && ir_value[2] == 0x45)  // 如果关机状态按了电源键
    {
        isrunning = 1;              // 开机
        power_status = 0xC0;        // 默认01档
        istimeing = 0;              // 开机时不进行倒计时
        return;
    } 
    else if (isrunning == 1 && ir_value[2] == 0x45)   // 如果开机状态按了电源键
    {
        isrunning = 0;                  // 关机
        power_status = 0x80;            // 输出电压设为0
        time_min = time_sec = 0x00;     // 清空倒计时
        istimeing = 0;                  // 不进行倒计时
        return;
    }

    switch(ir_value[2])
    {
        case (0x46):    // mode 送风模式（正反转）
        {
            switch (power_status)
            {
                case (0x00): power_status = 0xff; break;
                case (0x20): power_status = 0xE0; break;
                case (0x40): power_status = 0xC0; break;
                case (0xC0): power_status = 0x40; break;
                case (0xE0): power_status = 0x20; break;
                case (0xff): power_status = 0x00; break;
                default: break;
            }
        }
        break;
        case (0x47):    // mute
            break;
        case (0x44):    // pau
            break;
        case (0x15):    // VO-
        case (0x40):    // left (调小档位)
        {
            if (isrunning == 0) break;
            switch (power_status)
            {
                case (0x00): power_status = 0x20; break;
                case (0x20): power_status = 0x40; break;
                case (0x40): power_status = 0x80; break;
                // case (0x80): power_status = 0x40; break;
                case (0xC0): power_status = 0x80; break;
                case (0xE0): power_status = 0xC0; break;
                case (0xff): power_status = 0xe0; break;
                default: break;
            }
        }
        break;
        case (0x09):    // VO+ 
        case (0x43):    // right 调大档位
        {
            if (isrunning == 0)     // 如果此时处于关机状态，将其开机并初始化为1档
            {
                isrunning = 1;
                power_status = 0xC0;
                istimeing = 0;
            }
            switch (power_status)
            {
                case (0x20): power_status = 0x00; break;
                case (0x40): power_status = 0x20; break;
                case (0x80): power_status = 0xC0; break;
                case (0xC0): power_status = 0xe0; break;
                case (0xE0): power_status = 0xff; break;
                default: break;
            }
        }
        break;
        case (0x16):    // 0
            time_sec = time_min = 0;
            istimeing = 0;
            break;
        case (0x0c):    // 1
            if (time_min <= 80)
            {
                time_min += 10;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x18):    // 2
            if (time_min <= 70)
            {
                time_min += 20;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x5e):    // 3
            if (time_min <= 60)
            {
                time_min += 30;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x08):    // 4 
            if (time_min <= 50)
            {
                time_min += 40;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x1c):    // 5 
            if (time_min <= 40)
            {
                time_min += 50;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x5a):    // 6
            if (time_min <= 30)
            {
                time_min += 60;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x42):    // 7
            if (time_min <= 20)
            {
                time_min += 70;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x52):    // 8 
            if (time_min <= 10)
            {
                time_min += 80;
                time_sec = 0;
            }
            isrunning = 1;
            istimeing = 1;
            break;
        case (0x4a):    // 9
            time_min = 90;
            time_sec = 0;
            isrunning = 1;
            istimeing = 1;
            break;
        default:
            break;
    }
}

void Clock0() interrupt 1
{
    // 秒判断是否为0
    // 秒不为0则减一
    // 秒为0 判断分钟是否为0, 分钟若为0则关掉电风扇, 分钟若不为0则秒设为59,分钟减一
    TH0 = 0x3c;
    TL0 = 0xb0;
    if (--t0_cnt != 0)   // DJNZ R2, RETURN
        return;
    t0_cnt = 0x14;

    if (istimeing == 0) // 不进行倒计时
        return;

    status_changed = 1; // 时间被更新

    if (isrunning == 0) // 如果风扇关闭，则清零倒计时
    {
        time_min = time_sec = 0;
        istimeing = 0;
        return;
    }

    if (time_sec != 0) {
        time_sec--;
    } else if (time_min == 0) {
        isrunning = 0;
    } else {
        time_sec = 59;
        time_min--;
    }
}

// 串行口中断
void SRecv() interrupt 4
{
    u8 min, sec;
    RI = 0; // 清标记
    EA = 0; // 关中断
    
    status_changed = 1; // 状态被更新

    power_status = SBUF;
    if (power_status == 0x80) {
        isrunning = 0;
    } else {
        isrunning = 1;
    }

    while (RI != 1) continue;   // 等待下一个数据

    RI = 0;
    min = SBUF;
    if (min <= 99) { time_min = min; }  // 分钟小于99，秒小于59

    while(RI != 1) continue;

    RI = 0;
    sec = SBUF;
    if (sec <= 59) { time_sec = sec; }

    if (istimeing == 0 && sec != 0 || min != 0)
        istimeing = 1;  // 确保开启倒计时

    EA = 1; // 恢复中断
}

// 延时程序
void delay(u16 i)
{
    while(i--);  
}

// 初始化风扇状态，设置定时器，中断，串口等
void Initialize()
{
    // 初始化风扇状态和倒计时
    power_status = 0x80;
    time_min = 0;
    time_sec = 0;
    isrunning = 0;
    istimeing = 0;

    // 初始化定时器
    TMOD = 0x21;
    TH0 = 0x3C;      // t0_cnt 1s (50ms)
    TL0 = 0xB0;
    t0_cnt = 0x14;
    TH1 = 0xF3;      // bitrate 2400
    TL1 = 0xF3;

    // 1001 0011
    IE = 0x93;       // 串口中断，定时器0中断，外部0中断
    SCON = 0x50;     // 方式1, 允许接收

    TR1 = 1;
    TR0 = 1;
    IT0 = 1;         // 下降沿
    
    status_changed = 1;
}

void flush_buff()
{
    LEDBUF[2] = TAB[time_min / 10];
    LEDBUF[3] = TAB[time_min % 10];
    LEDBUF[4] = TAB[time_sec / 10];
    LEDBUF[5] = TAB[time_sec % 10];
    if (isrunning == 0)
    {
        LEDBUF[0] = LEDBUF[1] = TAB[0];
        return;
    }
    switch(power_status)
    {
        case(0x00):
            LEDBUF[0] = 0x40;
            LEDBUF[1] = TAB[3];
            break;
        case(0x20):
            LEDBUF[0] = 0x40;
            LEDBUF[1] = TAB[2]; // -2
            break;
        case(0x40):
            LEDBUF[0] = 0x40;
            LEDBUF[1] = TAB[1]; // -1
            break;
        case(0x80):
            LEDBUF[0] = LEDBUF[1] = TAB[0];
            break;
        case(0xC0):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[1]; // 01
            break;
        case(0xE0):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[2]; // 02
            break;
        case(0xff):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[3];
            break;
        default:
            LEDBUF[0] = LEDBUF[1] = TAB[0xe]; // EE
            break;
    }

    // 更新完显存数据后，恢复状态
    status_changed = 0;
}

// 将显存显示在屏幕上
void flush_display()
{
    u8 i, pos;
    pos = 0x20;
    for (i = 0; i < 6; i++)
    {
        *BIT_CHOOSE = pos;
        *SEG_CHOOSE = LEDBUF[i];
        delay(1000);    // 仿真时设置为10ms防止数码管闪烁，实际上需要改为1ms
        pos >>= 1;
        *SEG_CHOOSE = 0x00;
    }
}

// 刷新风扇状态
void refresh_status()
{
    // 如果没有状态更新
    if (status_changed == 0)
        return;

    if (isrunning == 0) // 关机
    {
        power_status = 0x80;
        time_min = time_sec = 0x00;
    }
    // 将电压值发送至DAC0832
    *DA_PORT = power_status;
    P1 = power_status;
    // 更新显存数据
    flush_buff();
}

void main()
{
    Initialize();
    while (1)
    {
        flush_display();
        refresh_status();
    }
}