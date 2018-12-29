#include <stm32f10x_rcc.h>
#include <stm32f10x_gpio.h>
#include "../drivers/dallas.h"
#include "../drivers/dht22.h"


uint16_t bits[40];

uint8_t  hMSB = 0;
uint8_t  hLSB = 0;
uint8_t  tMSB = 0;
uint8_t  tLSB = 0;
uint8_t  parity_rcv = 0;

static GPIO_InitTypeDef PORT;


void DHT22_Init(void) {
	RCC_APB2PeriphClockCmd(DHT22_GPIO_CLOCK,ENABLE);
	PORT.GPIO_Mode = GPIO_Mode_Out_PP;
	PORT.GPIO_Pin = DHT22_GPIO_PIN;
	PORT.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT22_GPIO_PORT,&PORT);
}

uint32_t DHT22_GetReadings(void) {
	uint32_t wait;
	uint8_t i;

	DallasConfigTimer();

	// Generate start impulse for sensor
	DHT22_GPIO_PORT->BRR = DHT22_GPIO_PIN; // Pull down SDA (Bit_SET)
	delay_5us = 175;		// delay 875us
	while (delay_5us != 0);
	DHT22_GPIO_PORT->BSRR = DHT22_GPIO_PIN; // Release SDA (Bit_RESET)

	// Switch pin to input with Pull-Up
	PORT.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(DHT22_GPIO_PORT,&PORT);

	// Wait for AM2302 to start communicate
	delay_5us = 80;
	while ((DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
	if (delay_5us < 10) return DHT22_RCV_NO_RESPONSE;

	// Check ACK strobe from sensor
	delay_5us = 20;
	while (!(DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
	if ((delay_5us > 1) || (delay_5us < 3)) return DHT22_RCV_BAD_ACK1;

	delay_5us = 20;
	while ((DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
	if ((wait > 1) || (wait < 3)) return DHT22_RCV_BAD_ACK2;

	// ACK strobe received --> receive 40 bits
	i = 0;
	while (i < 40) {
		// Measure bit start impulse (T_low = 50us)
		delay_5us = 4;
		while (!(DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
		if (delay_5us <= 3) {
			// invalid bit start impulse length
			bits[i] = 0xffff;
			delay_5us = 4;
			while ((DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
		} else {
			// Measure bit impulse length (T_h0 = 25us, T_h1 = 70us)
			delay_5us = 4;
			while ((DHT22_GPIO_PORT->IDR & DHT22_GPIO_PIN) && (delay_5us > 0));
			bits[i] = (delay_5us <= 3) ? delay_5us * 5 : 0xffff;
		}

		i++;
	}

	DallasDeConfigTimer();

	for (i = 0; i < 40; i++) if (bits[i] == 0xffff) return DHT22_RCV_RCV_TIMEOUT;

	return DHT22_RCV_OK;
}

uint16_t DHT22_DecodeReadings(void) {
	uint8_t parity;
	uint8_t  i = 0;

	hMSB = 0;
	for (; i < 8; i++) {
		hMSB <<= 1;
		if (bits[i] > 7) hMSB |= 1;
	}
	hLSB = 0;
	for (; i < 16; i++) {
		hLSB <<= 1;
		if (bits[i] > 7) hLSB |= 1;
	}
	tMSB = 0;
	for (; i < 24; i++) {
		tMSB <<= 1;
		if (bits[i] > 7) tMSB |= 1;
	}
	tLSB = 0;
	for (; i < 32; i++) {
		tLSB <<= 1;
		if (bits[i] > 7) tLSB |= 1;
	}
	for (; i < 40; i++) {
		parity_rcv <<= 1;
		if (bits[i] > 7) parity_rcv |= 1;
	}

	parity  = hMSB + hLSB + tMSB + tLSB;

	return (parity_rcv << 8) | parity;
}

uint16_t DHT22_GetHumidity(void) {
	return (hMSB << 8) + hLSB;
}

uint16_t DHT22_GetTemperature(void) {
	return (tMSB << 8) + tLSB;
}
