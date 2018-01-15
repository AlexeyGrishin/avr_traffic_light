/*
 (Reset) Y1 PB5 [     ] VCC
	     G1 PB3 [     ] PB2 G2
	     R1 PB4 [     ] PB1 Button (Int0) 
	        GND [     ] PB0 R2 

*/
#ifdef DEBUG
//I do not use yellow pin in debug 'cos it is used as RESET
#define YELLOW(x) 0
#else
#define YELLOW(x) x
#endif 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define RED1 PORTB4
#define GREEN1 PORTB3
#define RED2 PORTB0
#define GREEN2 PORTB2
#define BUTTON PORTB1
#define YELLOW1 PORTB5

#define MODE_OLD 255
#define MODE_OFF 0
#define MODE_1 1
#define MODE_2 2


#define FLASH_DELAY_MS 500
#define FLASH_COUNT 3
#define INTERMODE_DELAY_MS 5000

#ifdef DEBUG
#define TURN_OFF_AFTER_MS 10L*1000L
#else
#define TURN_OFF_AFTER_MS 3L*60L*1000L
#endif

#define BUTTON_DELAY_MS 500

volatile unsigned char mode = MODE_OFF;
volatile unsigned char newMode = MODE_OLD;
volatile long msSinceStart = 0;

#define TIMER_SCALE 256L
#define F_CPU 4800000L / 8L
#define TIMER_MAX_VALUE 256L
// something around 9. we do not need very good precision here
#define TIMER_OVERFLOWS_PER_S (F_CPU / (TIMER_MAX_VALUE * TIMER_SCALE))
// something around 100
#define TIMER_MS_IN_1_OVERFLOW (1000 / TIMER_OVERFLOWS_PER_S)


ISR(INT0_vect) {
	newMode = mode == MODE_1 ? MODE_2 : MODE_1;
	GIMSK &= (~(1 << INT0)); //disable interrupts by button
}

ISR(TIM0_OVF_vect)
{
	msSinceStart += TIMER_MS_IN_1_OVERFLOW; 
}

void restartTimer() {
	cli();
	msSinceStart = 0;
	TCNT0 = 0;
	sei();
}

long millis() {
	long temp;
	cli();
	temp = msSinceStart;
	sei();
	return temp;
}

#define SHOW(r1,y1,g1,r2,g2) {PORTB = (1 << BUTTON) | (r1 << RED1) | (g1 << GREEN1) | (r2 << RED2) | (g2 << GREEN2) | YELLOW((y1 << YELLOW1));}

void delay(int ms) {
	long now = millis();
	while (millis() - now < ms);
}

void modeOff() {
	SHOW(0,0,0,0,0);
}

void mode1() {
	SHOW(1,0,0,0,1);
}

void mode2() {
	SHOW(0,0,1,1,0);
}

void switchToMode2() {
	for (int i = 0; i < FLASH_COUNT*2; i++) {
		SHOW(1,1,0,0,i%2);
		delay(FLASH_DELAY_MS);
	}
	mode2();
}

void switchToMode1() {
	for (int i = 0; i < FLASH_COUNT*2; i++) {
		SHOW(0,0,i%2,1,0);
		delay(FLASH_DELAY_MS);
	}
	mode1();
}


void sleep() {
	modeOff();
	mode = MODE_OFF;
	newMode = MODE_OFF;
	GIMSK |= (1 << INT0);
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
	sleep_disable();
}

int main(void)
{
	//1. setup pins (5 outputs, 1 input) - DDRB
	DDRB = (1 << RED1) | (1 << GREEN1) | (1 << RED2) | (1 << GREEN2) | YELLOW((1 << YELLOW1));
	PORTB = (1 << BUTTON); //pull-up
	
	//1.5 setup button interrupts
	GIMSK |= (1 << INT0);
	
	//1.6 setup timer
	#if TIMER_SCALE == 256
	TCCR0B |= (1 << CS02); //scale 256
	#else
		#error "Not defined!"
	#endif
	TIMSK0 |= (1<< TOIE0); //interrupt by overflow
	restartTimer();	//calls 'sei' inside

    while (1) 
    {
		long now = millis();
		if (mode == MODE_OFF) {
			restartTimer();
		} else if (now > TURN_OFF_AFTER_MS) {
			sleep();
			continue;
		}
		//3. check button cooldown, check mode changed and update output
		if (newMode != MODE_OLD) {
			//someone pressed button
			switch (newMode) {
				case MODE_1:
					if (mode == MODE_OFF) {
						mode1();
						delay(BUTTON_DELAY_MS);
					} else {
						switchToMode1();
					}
					break;
				case MODE_2:
					switchToMode2();
					break;	
			}
			mode = newMode;
			newMode = MODE_OLD;
			restartTimer();
		}
		//if button "unpressed" - restore interrupts
		if ((PINB & (1 << BUTTON))) {
			GIMSK |= (1 << INT0); //restore interrupts
		}
		
    }
}



