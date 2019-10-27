#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

uint8_t hours = 20;
uint8_t minutes = 0;
uint8_t seconds = 0;

uint16_t lbtn_time = 0;
uint16_t rbtn_time = 0;

/* 0 - time 
 * 1 - DCF-setting
 * 2 - brightness
 * 3 - set hours(time)
 * 4 - set minutes(time)
 * 5 - reset seconds(time)
 * 6 - alarm active
 * 7 - set hours(alarm)
 * 8 - set minutes
*/
uint8_t active_menu = 0;
bool dcf_active = 0;
uint8_t brightness = 0;
bool alarm_active = 0;
uint8_t alarm_running = 2;
uint8_t a_hours = 6;
uint8_t a_minutes = 30;

uint8_t dcf_eeprom EEMEM;
uint8_t brightness_eeprom EEMEM;
uint8_t alarm_eeprom EEMEM;
uint8_t a_hours_eeprom EEMEM;
uint8_t a_minutes_eeprom EEMEM;

bool rec_active = false;


ISR(TIMER0_COMPA_vect){
	static uint32_t div_counter = 0;
	static int8_t bit_num = -2;
	static uint16_t pulse_len = 0;
	static uint16_t pulse_on = 0;
	static bool rec_bits[59];
	cli();
	if(rec_active){
		if (bit_num < 0){
			//PORTB = PIND & (1 << PIND0);
			if (PIND & (1 << PIND0)){
				if (pulse_len > 172){
					bit_num++;
					PORTB = 33;
				}
				pulse_len = 0;
			}else{
				pulse_len++;
			}
		}else{
			if (PIND & (1 << PIND0)){
				pulse_on++;
				if (pulse_len > 96){
					rec_bits[bit_num] = pulse_on > 18;
					PORTB &= ~1;
					PORTB |= rec_bits[bit_num];
					bit_num++;
					if(bit_num == 59){
						rec_active = false;
						bit_num = -1;
						if((rec_bits[0] == 0) && (rec_bits[20] == 1)){
							uint8_t rec_valm = rec_bits[21];
							rec_valm += rec_bits[22] << 1;
							rec_valm += rec_bits[23] << 2;
							rec_valm += rec_bits[24] << 3;
							rec_valm += rec_bits[25] * 10;
							rec_valm += (rec_bits[26] << 1) * 10;
							rec_valm += (rec_bits[27] << 2) * 10;
							
							uint8_t par = rec_bits[21];
							par += rec_bits[22];
							par += rec_bits[23];
							par += rec_bits[24];
							par += rec_bits[25];
							par += rec_bits[26];
							par += rec_bits[27];
							if( (par & 1) == rec_bits[28]){
								uint8_t rec_valh = rec_bits[29];
								rec_valh += rec_bits[30] << 1;
								rec_valh += rec_bits[31] << 2;
								rec_valh += rec_bits[32] << 3;
								rec_valh += rec_bits[33] * 10;
								rec_valh += (rec_bits[34] << 1) * 10;

								par = rec_bits[29];
								par += rec_bits[30];
								par += rec_bits[31];
								par += rec_bits[32];
								par += rec_bits[33];
								par += rec_bits[34];
								if((par & 1) == rec_bits[35]){
									minutes = rec_valm;
									hours = rec_valh;
									seconds = 0;
									div_counter = 0;
									PORTB = 0b101010;
								}else{
									rec_active = true;
								}
							}else{
								rec_active = true;
							}
						}else{
							rec_active = true;
						}
					}
					pulse_len = 0;
					pulse_on = 0;
				}
			}
			
			pulse_len++;
		}
		sei();
	}

	if(div_counter == 117){
		if(alarm_running){
			alarm_running--;
			if(!alarm_running){
				PORTD &= ~0b1000000;
			}
		}
		seconds++;
		if (seconds == 60){
			seconds = 0;
			minutes++;
			if (minutes == 60){
				if(alarm_active && (minutes == a_minutes) && (hours == a_hours)){
					alarm_running = 16;
				}
				minutes = 0;
				hours++;
				if (hours == 24){
					hours = 0;
					rec_active = dcf_active;
				}
			}
			if(minutes == 59 && minutes == 29){
				div_counter = -40;
			}else{
				div_counter = -5;	
			}
		}else{
			div_counter = 0;
		}
	}
	div_counter++;
}

void left_pressed(){
	if(rec_active){
		rec_active = false;
	}else if(alarm_running){
		alarm_running = 0;
	}else{
		switch (active_menu){
			case 0:
				rec_active = true;
				break;
			case 1:
				dcf_active = !dcf_active;
				break;
			case 2:
				brightness++;
				if(brightness >= 64){
					brightness = 0;
				}
				break;
			case 3:
				hours++;
				if(hours >= 24){
					hours = 0;
				}
				break;
			case 4:
				minutes++;
				if(minutes >= 60){
					minutes = 0;
				}
				break;
			case 5:
				seconds = 0;
				break;
			case 6:
				alarm_active = !alarm_active;
				break;
			case 7:
				a_hours++;
				if(a_hours >= 24){
					a_hours = 0;
				}
				break;
			case 8:
				a_minutes++;
				if(a_minutes >= 60){
					a_minutes = 0;
				}
				break;
			default:
				break;
		}
	}
}

int main() {
	DDRB = ~0; //all outputs
	DDRD = 0b1111000;
	PORTD |= 0b110;

	OCR0A  = 0x20;             // number to count up to (0x70 = 112)
	TCCR0A = 1 << WGM01;       // Clear Timer on Compare Match (CTC) mode
	TIFR |= 1 << OCF0A;        // clear interrupt flag
	TIMSK = 1 << OCIE0A;       // TC0 compare match A interrupt enable
	TCCR0B = 1 << CS02;        // clock source CLK/256, start timer

	sei();              // global interrupt enable

	PORTD |=  0b0011000;
	PORTD &= ~0b1100000;

	uint8_t temp_setting = 0;

	eeprom_busy_wait();
	dcf_active = eeprom_read_byte(&dcf_eeprom) > 0;
	eeprom_busy_wait();
	brightness = eeprom_read_byte(&brightness_eeprom);
	eeprom_busy_wait();
	alarm_active = eeprom_read_byte(&alarm_eeprom) > 0;
	eeprom_busy_wait();
	a_hours = eeprom_read_byte(&a_hours_eeprom);
	eeprom_busy_wait();
	a_minutes = eeprom_read_byte(&a_minutes_eeprom);
	
	if(eeprom_read_byte(&dcf_eeprom) > 1){
		dcf_active = true;
		brightness = 32;
		alarm_active = false;
		a_hours = 10;
		a_minutes = 0;
		eeprom_update_byte(&dcf_eeprom, +dcf_active);
		eeprom_update_byte(&brightness_eeprom, brightness);
		eeprom_update_byte(&alarm_eeprom, +alarm_active);
		eeprom_update_byte(&a_hours_eeprom, a_hours);
		eeprom_update_byte(&a_minutes_eeprom, a_minutes);
	}
	
	
	while (1){
		while(alarm_running && (seconds & 1)){
			PORTD ^= 0b1000000;
			_delay_loop_2(4096);
		}

		if (!(PIND & (1 << PIND1))){
			lbtn_time++;
			if (lbtn_time > 512 && !(lbtn_time & 0b11111)){
				left_pressed();
			}
		}else{
			if (lbtn_time > 64){
				left_pressed();
			}
			lbtn_time = 0;
		}
		
		if (!(PIND & (1 << PIND2))){
			rbtn_time++;
		}else{
			if (rbtn_time > 64){
				eeprom_busy_wait();
				switch (active_menu){
				case 1:
					if((eeprom_read_byte(&dcf_eeprom) != 0) != dcf_active){
						eeprom_busy_wait();
						eeprom_update_byte(&dcf_eeprom, +dcf_active);
					}
					break;
				case 2:
					if(eeprom_read_byte(&brightness_eeprom) != brightness){
						eeprom_busy_wait();
						eeprom_update_byte(&brightness_eeprom, brightness);
					}
					break;
				case 6:
					if((eeprom_read_byte(&alarm_eeprom) != 0) != alarm_active){
						eeprom_busy_wait();
						eeprom_update_byte(&alarm_eeprom, +alarm_active);
					}
					break;
				case 7:
					if(eeprom_read_byte(&a_hours_eeprom) != a_hours){
						eeprom_busy_wait();
						eeprom_update_byte(&a_hours_eeprom, a_hours);
					}
					break;
				case 8:
					if(eeprom_read_byte(&a_minutes_eeprom) != a_minutes){
						eeprom_busy_wait();
						eeprom_update_byte(&a_minutes_eeprom, a_minutes);
					}
					break;
					break;
				default:
					break;
				}
				active_menu++;
			}
			rbtn_time = 0;
		}
		if(!rec_active){
			if (!active_menu){
				PORTD |= 0b011000;
				PORTB = hours;
				_delay_loop_2(64 + 6 * brightness);
				PORTD &= ~0b100000;
				_delay_loop_2(512 - 6 * brightness);
				
				PORTD |= 0b101000;
				PORTB = minutes;
				_delay_loop_2(64 + 6 * brightness);
				PORTD &= ~0b010000;
				_delay_loop_2(512 - 6 * brightness);
				
				PORTD |= 0b110000;
				PORTB = seconds;
				_delay_loop_2(64 + 6 * brightness);
				PORTD &= ~0b001000;
				_delay_loop_2(512 - 6 * brightness);
			}else{
				switch (active_menu){
					case 1:
						temp_setting = dcf_active;
						break;
					case 2:
						temp_setting = brightness;
						break;
					case 3:
						temp_setting = hours;
						break;
					case 4:
						temp_setting = minutes;
						break;
					case 5:
						temp_setting = seconds;
						break;
					case 6:
						temp_setting = alarm_active;
						break;
					case 7:
						temp_setting = a_hours;
						break;
					case 8:
						temp_setting = a_minutes;
						break;
					default:
						active_menu = 0;
						break;
				}
				PORTD |= 0b011000;
				PORTB = temp_setting;
				PORTD &= ~0b100000;
				_delay_loop_2(1536);
			}
		}else{
			_delay_loop_2(1536);
			PORTB &= ~32;
		}
		
	}
	return 0;
}
