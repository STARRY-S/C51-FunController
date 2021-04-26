#include <reg52.h>

typedef unsigned int u16;
typedef unsigned char u8;

u8 xdata *BIT_CHOOSE = 0x8002;
u8 xdata *SEG_CHOOSE = 0x8004;
u8 xdata *DA_PORT = 0x9000;

u8 data LEDBUF[6];
u8 data ir_value[4];
u8 data t0_cnt;
u8 data power_status;
u8 data time_min;
u8 data time_sec;

bit data isrunning = 0;
bit data istimeing = 0;
bit data status_changed = 0;
sbit IRIN=P3^2;

u8 code TAB[17] = {
    0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,
    0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71
};

void flush_buff();
void delay(u16 i);

void ReadIr() interrupt 0
{
	u8 j, k, htime;
	u16 err;
	htime = 0;				 
	delay(700);
	if (IRIN != 0)
        return;
		
    err = 1000;
    while ((IRIN == 0) && (err > 0))	
    {			
        delay(1);
        err--;
    }
    
    if (IRIN != 1)
        return;

    err = 500;
    while ((IRIN == 1) && (err > 0))
    {
        delay(1);
        err--;
    }

    for (k = 0; k < 4; k++)
    {
        for (j = 0; j < 8; j++)
        {
            htime=0;
            err = 60;
            while ((IRIN == 0) && (err > 0))
            {
                delay(1);
                err--;
            }
            while (IRIN == 1)
            {
                delay(10);
                htime++;
                err--;
                if (htime > 30)
                    return;
            }
            ir_value[k] >>= 1;
            if(htime >= 8)
            {
                ir_value[k] |= 0x80;
            }
        }
    }

    if(ir_value[2] != ~ir_value[3])
        return;
    
    status_changed = 1;
    if (isrunning == 0 && ir_value[2] == 0x45)
    {
        isrunning = 1;
        power_status = 0xC0;
        istimeing = 0;
        return;
    } 
    else if (isrunning == 1 && ir_value[2] == 0x45)
    {
        isrunning = 0;
        power_status = 0x80;
        time_min = time_sec = 0x00;
        istimeing = 0;
        return;
    }

    switch(ir_value[2])
    {
        case (0x46):    // mode 
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
        case (0x40):    // left
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
        case (0x43):    // right
        {
            if (isrunning == 0)
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
    TH0 = 0x3c;
    TL0 = 0xb0;
    if (--t0_cnt != 0)
        return;
    t0_cnt = 0x14;

    if (istimeing == 0)
        return;

    status_changed = 1;

    if (isrunning == 0)
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

void SRecv() interrupt 4
{
    u8 min, sec;
    RI = 0;
    EA = 0;
    
    status_changed = 1;

    power_status = SBUF;
    if (power_status == 0x80) {
        isrunning = 0;
    } else {
        isrunning = 1;
    }

    while (RI != 1) continue;

    RI = 0;
    min = SBUF;
    if (min <= 99) { time_min = min; }

    while(RI != 1) continue;

    RI = 0;
    sec = SBUF;
    if (sec <= 59) { time_sec = sec; }

    if (istimeing == 0 && sec != 0 || min != 0)
        istimeing = 1;

    EA = 1;
}

void delay(u16 i)
{
    while(i--);  
}

void Initialize()
{
    power_status = 0x80;
    time_min = 0;
    time_sec = 0;
    isrunning = 0;
    istimeing = 0;

    TMOD = 0x21;
    TH0 = 0x3C;
    TL0 = 0xB0;
    t0_cnt = 0x14;
    TH1 = 0xF3;
    TL1 = 0xF3;

    IE = 0x93;
    SCON = 0x50;

    TR1 = 1;
    TR0 = 1;
    IT0 = 1;
    
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
            LEDBUF[1] = TAB[2];
            break;
        case(0x40):
            LEDBUF[0] = 0x40;
            LEDBUF[1] = TAB[1];
            break;
        case(0x80):
            LEDBUF[0] = LEDBUF[1] = TAB[0];
            break;
        case(0xC0):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[1];
            break;
        case(0xE0):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[2];
            break;
        case(0xff):
            LEDBUF[0] = TAB[0];
            LEDBUF[1] = TAB[3];
            break;
        default:
            LEDBUF[0] = LEDBUF[1] = TAB[0xe];
            break;
    }

    status_changed = 0;
}

void flush_display()
{
    u8 i, pos;
    pos = 0x20;
    for (i = 0; i < 6; i++)
    {
        *BIT_CHOOSE = pos;
        *SEG_CHOOSE = LEDBUF[i];
        delay(100);
        pos >>= 1;
        *SEG_CHOOSE = 0x00;
    }
}

void refresh_status()
{
    if (status_changed == 0)
        return;

    if (isrunning == 0)
    {
        power_status = 0x80;
        time_min = time_sec = 0x00;
    }
    *DA_PORT = power_status;
    P1 = power_status;
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