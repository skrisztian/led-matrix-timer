/*
 * led_matrix.c
 *
 * Created: 2017-05-27 20:56:49
 * Author : Krisztian Stancz
 */ 

/* === HARDWARE SETUP ===
 *
 * LED matrix: GTM2088RGB - 8x8 common cathode (-)
 * Pin layout:
 * 1-8 BLUE
 * 9-16 RED
 * 28-21 GREEN
 * 17-20 ROW 1-4
 * 29-32 ROW 5-8
 * LED (1,1): upper left
 * bit order MSB-> rows-R-G-B <-LSB
 *
 * Shift register:  SN74HC595N
 * Pin layout:
 * IC 14 SER - PC2 - DATA_PIN - yellow cable
 * IC 12 RCLK - PC1 - LATCH_PIN - green cable
 * IC 11 SRCLK - PC0 - CLOCK_PIN - orange cable
 *
 * MCU: ATmega168PB
 * Timer: pulse every 3.75 sec
 * Button - PB7
 * LED - PB5
 */

#ifndef F_CPU
#define F_CPU 16000000
#endif // F_CPU

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define SHIFT_REG_PORT			PORTC
#define DDR_SHIFT_REG			DDRC
#define DATA_PIN				PORTC2
#define LATCH_PIN				PORTC1
#define CLOCK_PIN				PORTC0
#define BUTTON_PIN				PINB7
#define BUTTON_PIN_REG 			PINB
#define BUTTON_PIN_INT			PCINT7
#define BUTTON_PIN_INT_GROUP	PCIE0


volatile uint8_t state = 255;
volatile uint8_t enabled = 0;

// Interrupt function for button press (pin change)
ISR(PCINT0_vect)
{
	// Start/reset on button press (0->1 transition)
	if (BUTTON_PIN_REG & (1 << BUTTON_PIN)) {
		if (state == 255) {
			enabled = 1;	// start timer
			state = 0;		// initialize timer display
			TCNT1 = 0;		// initialize timer counter
		} else {
			enabled = 0;	// stop timer
			state = 255;	// set display to waiting timer
		}	
	}
}

// Interrupt function for timer counter (clear timer on compare)
ISR(TIMER1_COMPA_vect)
{
	if (enabled) {
	
		// Increase state at every timer compare
		// The timer display cycles through 64 states, each lasting 3.75 sec
		state++;
	
		// Turn off timer after 63
		if (state > 63)
			enabled = 0;
	}
}

// Timer settings
uint8_t timer_init(void)
{
	// Set timer clock prescaler to 1024
	TCCR1B |= (1<<CS10) | (1<<CS12);
	
	// Set CTC mode
	TCCR1B |= 1 << WGM12;
	
	// Initialize counter
	TCNT1 = 0;
	
	// Set compare value for 3.75 sec at 16 MHz / 1024
	OCR1A = 58593;
	
	// Enable compare interrupt
	TIMSK1 |= 1 << OCIE1A;

	return 0;
}


// Start/reset button settings
uint8_t button_init(void)
{
	// Enable interrupts on button pin
	PCMSK0 |= 1 << BUTTON_PIN_INT;

	// Enable interrupts on button pin group
	PCICR |=  1 << BUTTON_PIN_INT_GROUP;

	return 0;
}

// Initialize shift register port
uint8_t shift_port_init(void)
{
	// Set shift register port to output
	DDR_SHIFT_REG |= (1 << DATA_PIN);
	DDR_SHIFT_REG |= (1 << LATCH_PIN);
	DDR_SHIFT_REG |= (1 << CLOCK_PIN);
	
	// Set shift register pins to low
	SHIFT_REG_PORT &= ~(1 << DATA_PIN);
	SHIFT_REG_PORT &= ~(1 << LATCH_PIN);
	SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);
	
	return 0;
}

// Shift bytes into shif registers
uint8_t shift_out(uint8_t *bytes, uint8_t size_in_bytes)
{
	// Set latch low while serial shift goes in
	SHIFT_REG_PORT &= ~(1 << LATCH_PIN);
	
	// For start, set clock to low
	SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);

	// Send out one bit for each rising clock pulse
	for (uint8_t j = 0; j < size_in_bytes; j++) {

		// For every bit in a byte
		for (uint8_t i = 0; i <= 7; i++) {
			
			// Shift out HIGH bit
			if (bytes[j] & (1 << i)) {
				SHIFT_REG_PORT |= 1 << DATA_PIN;

			// Shift out LOW bit
			} else {
				SHIFT_REG_PORT &= ~(1 << DATA_PIN);
			}
		
			// Set clock pulse high
			SHIFT_REG_PORT |= 1 << CLOCK_PIN;
	
			// Set clock pulse low
			SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);
		}
	}
	
	// Set latch high to parallel write
	SHIFT_REG_PORT |= 1 << LATCH_PIN;
	
	return 0;
}

// Light up the reuired LEDS in a row
uint8_t show_leds(uint8_t *bytes)
{
	// Negate RGB bits for common cathode configuration
	for (uint8_t i = 1; i <= 3; i++) {
		bytes[i] = ~bytes[i];
	}
	
	// Shift out all bytes
	// Each LED row data is 4 bytes: row number, R, G, B
	shift_out(bytes, 4);
	
	return 0;
}


int main(void)
{

	// Initialize services
    shift_port_init();
	button_init();
	timer_init();

	// Enable global interrupts
	sei();

	// To hold the actual LED row parameters			
	uint8_t leds[4];	// {row byte, RED byte, GREEN byte, BLUE byte}
	uint8_t actual_row;
	uint8_t last_red;

	// Pattern to light up on the left and right side of the LED matrix
	static uint8_t red_left[] = {0x1, 0x3, 0x7, 0xf, 0x8, 0xc, 0xe, 0xf};
	static uint8_t red_right[] = {0x8, 0xc, 0xe, 0xf, 0x1, 0x3, 0x7, 0xf};
	
	while (1) {
		
		// After reset: all blue
		if (state == 255) {
			leds[0] = 0xff;
			leds[1] = 0x00;
			leds[2] = 0x00;
			leds[3] = 0xff;
			show_leds(leds);

		// After finish (during overtime): flashing red
		} else if (state > 63) {
			leds[0] = 0xff;
			leds[1] = 0xff;
			leds[2] = 0x00;
			leds[3] = 0x00;
			show_leds(leds);
			_delay_ms(500);
			leds[0] = 0x00;
			leds[1] = 0x00;
			leds[2] = 0x00;
			leds[3] = 0x00;
			show_leds(leds);
			_delay_ms(500);

		// Timer in operation: show green and red dots
		} else {

			// Calculate actual row
			actual_row = (int) state / 4;
			if (actual_row > 7) 
				actual_row -= 8;
			
			// Calculate last red led position in actual row
			last_red = state % 8;
			
			// Turn on leds row-by-row
			for (uint8_t j = 0; j < 8; j++) {

				// Row's position
				// Until state 31 go downwards then upwards
				if (state < 32)
					leds[0] = 1 << (7 - j);
				else
					leds[0] = 1 << j;
			
				// Reds in row
				// Until state 31 use the left side then the right
				if (state < 32)	{
					if (j == actual_row)
						leds[1] = red_left[last_red] << 4;
					else if (j < actual_row)
						leds[1] = red_left[3] << 4;
					else if (j > actual_row)
						leds[1] = 0x00;
				} else {
					if (j == actual_row)
						leds[1] = red_right[last_red] | 0xf0;
					else if (j < actual_row)
						leds[1] = red_right[3] | 0xf0;
					else if (j > actual_row)
						leds[1] = 0xf0;
				}
				
				// Greens in row
				leds[2] = ~leds[1];

				// No Blues
				leds[3] = 0x00;

				// Turn on leds
				show_leds(leds);		
			}
		}
	}
	return 0;
}
