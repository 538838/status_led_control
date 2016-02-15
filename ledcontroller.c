#include <stdio.h>
#include <stdlib.h>
#include <xc.h>

//CONFIG BITS
// CONFIG1
#pragma config FOSC = INTOSC    //  (INTOSC oscillator; I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF      // PLL Enable (4x PLL disabled)
#pragma config STVREN = OFF      // Stack Overflow/Underflow Reset Disable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LPBOREN = OFF    // Low Power Brown-out Reset enable bit (LPBOR is disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)


void init(void);
void change(volatile unsigned char*, unsigned char*,unsigned char*,
        volatile unsigned char*, volatile unsigned char*, unsigned char);

#define MAXLEVEL 255
#define INPUTDELAY 5
#define MAXINTERRUPT 10
#define WDTSTART 60
#define WDTRUN 30
#define STARTLENGTH 4
#define ERR_WDT 1
#define ERR_TOO_SLOW 2

#define WAIT 0
#define CHANGE 1
#define BLINK 2

volatile signed char pwm_int = 0;
volatile unsigned char error = 0;
volatile unsigned char lock = 0;
volatile unsigned char red = 0;
volatile unsigned char green = 0;
volatile unsigned char blue = 0;
volatile unsigned char modered = WAIT;
volatile unsigned char modegreen = WAIT;
volatile unsigned char modeblue = BLINK;
volatile signed char input = -1;
volatile unsigned char startup = STARTLENGTH;
volatile unsigned char ticks = 0;
volatile unsigned int seconds = 0;

int main() {    
    init();
    
    while(1){
        if(lock){
            
        } else if(error){
            if(error == ERR_WDT){
                green = 0;
                blue = 0;
                modered = BLINK;
                modegreen = CHANGE;
                modeblue = CHANGE;
                PWM2CON |= (1<<6);
                lock = 1;
            } else if(error == ERR_TOO_SLOW){
                red = 0;
                blue = 0;
                modered = CHANGE;
                modegreen = BLINK;
                modeblue = CHANGE;
                PWM1CON |= (1<<6);
                lock = 1;
            }
        } else if(input == 0){
            input = -1;
            
            if(PORTA&(1<<5)){
                red = MAXLEVEL;
                PWM2CON |= (1<<6);
                modered = CHANGE;
            } else if(red != 0) {
                red = 0;
                PWM2CON |= (1<<6);
                modered = CHANGE;
            }
            if(PORTA&(1<<4)){
                green = MAXLEVEL;
                PWM1CON |= (1<<6);
                modegreen = CHANGE;
            } else if(green != 0) {
                green = 0;
                PWM1CON |= (1<<6);
                modegreen = CHANGE;
            }
            if(PORTA&(1<<3)){
                blue = MAXLEVEL;
                PWM3CON |= (1<<6);
                modeblue = CHANGE;
            } else if(blue != 0) {
                blue = 0;
                PWM3CON |= (1<<6);
                modeblue = CHANGE;
            }
        }
        
        change(&PWM2DCL,&red,&modered,&PWM2LDCON,&PWM2CON,0);
        change(&PWM1DCL,&green,&modegreen,&PWM1LDCON,&PWM1CON,1);
        change(&PWM3DCL,&blue,&modeblue,&PWM3LDCON,&PWM3CON,2);
        
        pwm_int-= 4;
        //Busy wait
        while(pwm_int <= 0){
            
        }
    }
        
    return (EXIT_SUCCESS);
}

//Runs at 62500/250 = 250 Hz)


void change(volatile unsigned char *dc, unsigned char *g,unsigned char* m,
        volatile unsigned char* ld, volatile unsigned char* con, unsigned char pin){
    if((*dc == *g) && *m){
        if(*m == BLINK){
            if(*g == 0){
                *g = MAXLEVEL;
            } else {
                *g = 0;
            }
        } else if(*m == CHANGE){
            if(*g == 0){
                *con &= ~(1<<6);
                //LATA &= ~(1<<pin);
            } else if(*g == MAXLEVEL){
                //LATA |= (1<<pin);
            }
            *m = WAIT;
        }
        
    } else {
        if(*dc > *g){
            *dc -= (*dc-*g)/20 + 1;
        } else{
            unsigned char tmp = *dc;
            tmp += tmp/20 + 1;
            if(tmp < *dc){
                *dc = *g;
            } else{
                *dc = tmp; 
            }
        }
        *ld |= 1<<7;
    }
}

void interrupt isr(void){
    if(PWM1INTF&(1<<0)){
        PWM1INTF &= ~(1<<0);
        if(input > 0){
            input--;
        }
        
        ticks++;
        if(ticks == 250){
            ticks = 0;
            seconds++;
            if((!startup && seconds == WDTRUN) || (startup && seconds == WDTSTART)){
                    error = ERR_WDT;
            }       
        }
            
        //Check for too slow
        pwm_int++;
        if(pwm_int >= MAXINTERRUPT){
            pwm_int = 1;
            //error = ERR_TOO_SLOW;
        }
    }
    if(IOCAF){
        if(startup){
            startup--;
        } else {
            if(input == -1){
                input = INPUTDELAY;
            }
            seconds = 0;
        } 
        // Clear IOC-flag
        IOCAF = 0x00; 
    }
}
 
void init(void){
    // Set clock settings, 500kHz, and tune
    OSCCON = 0b00111010;
    OSCTUNE = 0b00000000;
    
    //Disable analog functions
    ANSELA = 0x00;
    //Set weak pull-up
    WPUA = 0b00111000;
    OPTION_REG &= ~(1<<7);
    //SET RA0-RA2 to outputs, & RA3-5 to inputs
    TRISA &= ~(1<<0);
    TRISA &= ~(1<<1);
    TRISA &= ~(1<<2);
    LATA = 0x00;
    
    //Enable PWM modules and outputs
    PWM1CON |= (1<<7);
    //PWM1CON |= (1<<6);
    PWM2CON |= (1<<7);
    //PWM2CON |= (1<<6);
    PWM3CON |= (1<<7);
    PWM3CON |= (1<<6);
    
    //Set clock for pwm
    PWM1CLKCON = 0b00110000;
    PWM2CLKCON = 0b00110000;
    PWM3CLKCON = 0b00110000;
    
    //Set PWM phase count,duty cycle, period, offset,timer counter
    PWM1PHH = 0x00;
    PWM1PHL = 0x00;
    PWM1DCH = 0x00;
    PWM1DCL = 0x00;
    PWM1PRH = 0x00;
    PWM1PRL = 0xfa;
    PWM1OFH = 0x00;
    PWM1OFL = 0x00;
    
    PWM2PHH = 0x00;
    PWM2PHL = 0x00;
    PWM2DCH = 0x00;
    PWM2DCL = 0x00;
    PWM2PRH = 0x00;
    PWM2PRL = 0xfa;
    PWM2OFH = 0x00;
    PWM2OFL = 0x00;
    
    PWM3PHH = 0x00;
    PWM3PHL = 0x00;
    PWM3DCH = 0x00;
    PWM3DCL = 0x00;
    PWM3PRH = 0x00;
    PWM3PRL = 0xfa;
    PWM3OFH = 0x00;
    PWM3OFL = 0x00;
    
    //load values
    PWM1LDCON |= 1<<7;
    PWM2LDCON |= 1<<7;
    PWM3LDCON |= 1<<7;
    
    //Enable PWM1 interrupt on Period match
    PIE3 |= (1<<4);
    PWM1INTE |= (1<<0);
    //Clear interrupt flag
    PWM1INTF &= ~(1<<0);
    
    // Enable interrupt on change (pos & neg) on RA3-5
    IOCAP = 0b00111000;
    IOCAN = 0b00111000;
    
    //Clear interrupt flag
    IOCAF = 0x00; 
    
    //Enable Interrupt on change and peripheral 
    INTCON |= (1<<3);
    INTCON |= (1<<6);
    //Global interrupt enable
    INTCON |= 1<<7;
}
