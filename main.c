//#include <avr.h>
#include <avr/io.h>
#include <avr/interrupt.h>

void digitalWrite(unsigned char data);
void setGreenCar(unsigned char data);
void setYellowCar(unsigned char data);
void setRedCar(unsigned char data);
void setWaitingButton(unsigned char data);
void setRedPed(unsigned char data);
void setGreenPad(unsigned char data);

/* global initialization routine */
void init(void);

unsigned char state = 0;

/* const waiting times (tc + tp = 20) */
unsigned char tc = 13;
unsigned char tp = 7+1;
/* current time counters */
unsigned char wait_tc = 0;					/* until tc */
unsigned char wait_tp = 0;					/* until tp */
unsigned char wait_green = 0;				/* until 3s */
unsigned char nobody_passes = 0;			/* until 2s */
unsigned char blinking_pedestrian = 0;		/* until 3s */
unsigned char blinking_wait_ped_but = 0;	/* until 2s */
unsigned char onesec = 0; 					/* measure of 1 second */

unsigned char button_on = 1;				/* avoid double interrupt execution */

/* wait pedestrian button state */
unsigned char pbut_on = 0;


int main (void) {
	/* initialization routine */
	init();
	
	/* enable interrupts */
	sei();
	
	while(1);
	
}

/* button C interrupt (waiting) */
ISR(PCINT0_vect){
	if(button_on ^= 1){
		if(pbut_on){ /* blinking pedestrian button */
			pbut_on = 2;
		} else if(wait_tc >= tc){
			setWaitingButton(1);
			setGreenCar(0);
			setYellowCar(1);
			pbut_on = 1;
			state = 1; /* timer changes according to states */
		} else if(wait_tc < tc) {
			setWaitingButton(1);
			pbut_on = 1;
			state = 2;
		}
	}
}
/* button A interrupt (reset) */
ISR(PCINT2_vect){
	
}
/* timer interrupt */
ISR(TIMER1_COMPA_vect){
	if(!((onesec++) % 10)){
		if(state == 0){ /* waiting for pedestrian */
			wait_tc++;
		} else if(state == 1){ /* pedestrian pushed the waiting button and wait_tc >= tc */
			if(!((++wait_green) % 4)){
				setYellowCar(0);
				setRedCar(1);
				setGreenCar(0);
				wait_green = 0;
				state = 3;
			}
			wait_tc++;
		} else if(state == 2){ /* pedestrian pushed the waiting button and wait_tc < tc */
			if(wait_tc >= tc){
				setYellowCar(0);
				setRedCar(1);
				state = 3;
			} else if(wait_tc + 3 >= tc){
				setYellowCar(1);
				setGreenCar(0);
			}
			wait_tc++;
		} else if(state == 3){ /* nobody can pass */
			if(!((++nobody_passes) % 3)){
				setWaitingButton(0);
				setRedPed(0);
				setGreenPad(1);
				wait_tc = 0;
				pbut_on = 0;
				nobody_passes = 0;
				blinking_wait_ped_but = 0;
				state = 4;
			}
		} else if(state == 4){ /* pedestrians pass */
			if(!((++wait_tp) % tp)){
				if (!(onesec % 2))
					setGreenPad(2); /* blink */
				wait_tp = 0;
				state = 5;
			}
		} else if(state == 5){ /* blinking green */
			if(!((++wait_green) % 4)){
				setGreenPad(0);
				setRedPed(1);
				wait_green = 0;
				state = 6;
			}
		} else if(state == 6){
			if(!((++nobody_passes) % 3)){
				setRedCar(0);
				setGreenCar(1);
				nobody_passes = 0;
				state = 0;
			}
		}
		onesec = 0;
	} 
	/* blinking goes in terms of external timer of 1ms */
	if(state == 1 && pbut_on > 1){
		if(blinking_wait_ped_but++ < 2){
			if(!(onesec % 2))
				setWaitingButton(2); /* blink */
		}
	} else if(state == 2 && pbut_on > 1){
		if(blinking_wait_ped_but++ < 2){
			if(!(onesec % 2))
				setWaitingButton(2); /* blink */
		}
	} if(state == 3 && pbut_on > 1){
		if(blinking_wait_ped_but++ < 2){
			if(!(onesec % 2))
				setWaitingButton(2); /* blink */
		}
	} else if(state == 5){ /* blinking green */
		if(!(onesec % 2))
			setGreenPad(2); /* blink */
	}
}
/* ADC interrupts */
ISR(ADC_vect){
	tp = 20*ADCH/255;
	tc = 20 - tp;
	tp++; /* sum one for balancing. The real waiting is still 20s */
}

void init(void){
	/* set up leds as output */
	DDRB = 0x07;
	DDRC = 0x18;
	DDRD = 0xB0;
	
	/* set up external interrupts pg74 data sheet*/
	PCMSK0 |= (1 << PCINT4);  // button C
	PCMSK2 |= (1 << PCINT18); // button A
	/* enable interrupts assigned to the muxs 0 and 2 */
	PCICR |= (1 << PCIE0) | (1 << PCIE2);
	
	/* set up timer interrupts */
	/*   en timer interrupts | match A interrupt enable */
	TIMSK1 |= 3;//(1 << TOIE1) | (1 << OCIE0A);
	OCR1A = 15626; /* should happen 10 times to produce one second with /1024 prescalar */
	TCCR1B |= 0b00001101;
	
	/*   ref voltage src | left alignment | ADC5 */
	ADMUX = (1 << REFS0) | (1 << ADLAR) | 5;
	/*	     enable ADC | 16 div factor | auto trigger | enable ADC interrupts | start first conversion */
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADATE) | (1 << ADIE) | (1 << ADSC);
	
	/* initial leds' state */
	//digitalWrite(0b001010);
	setRedPed(1);
	setGreenCar(1);
}

void digitalWrite(unsigned char data){
	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
	
	PORTB |= ((data & 0x01) << 2);  // LED 0  =>  B2
	PORTB |= data & 0x02;			// LED 1  =>  B1
	PORTB |= ((data & 0x04) >> 2);	// LED 2  =>  B0
	PORTD |= ((data & 0x08) << 4);	// LED 3  =>  D7
	PORTD |= ((data & 0x10) << 1);	// LED 4  =>  D5
	PORTD |= ((data & 0x20) >> 1);	// LED 5  =>  D4
	PORTC |= ((data & 0x40) >> 2);	// LED 6  =>  C4
	PORTC |= ((data & 0x80) >> 4);	// LED 7  =>  C3
}

void setGreenPad(unsigned char data){
	if(data == 1)
		PORTB |= (1 << 2);
	else if(data == 2)
		PORTD ^= (1 << 2);
	else
		PORTB &= ~(1 << 2);
}
void setRedPed(unsigned char data){
	if(data)
		PORTB |= (1 << 1);
	else
		PORTB &= ~(1 << 1);
}
void setWaitingButton(unsigned char data){
	if(data == 1)
		PORTB |= 0b00000001;
	else if(data == 2)
		PORTB ^= 0b00000001;
	else
		PORTB &= 0xFE; /* can cause problems */
}
void setGreenCar(unsigned char data){
	if(data == 1)
		PORTD |= (1 << 7);
	else
		PORTD &= ~(1 << 7);
}
void setYellowCar(unsigned char data){
	if(data)
		PORTD |= (1 << 5);
	else
		PORTD &= ~(1 << 5);
}
void setRedCar(unsigned char data){
	if(data)
		PORTD |= (1 << 4);
	else
		PORTD &= ~(1 << 4);
}
