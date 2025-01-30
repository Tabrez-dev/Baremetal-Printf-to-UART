#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#define BIT(x) (1UL << (x))
#define PIN(bank, num) ((((bank)-'A')<<8) | (num))
#define PINNO(pin) (pin & 255)
#define PINBANK(pin) (pin >> 8)
#define RCC_GPIO_CLK_ENABLE(bank)  (RCC->AHBENR |= BIT(17 + (bank)))
#define RCC_GPIO_CLK_DISABLE(bank) (RCC->AHBENR &= ~BIT(17 + (bank)))
#define UART1 ((struct uart *)0x40013800)
#define FREQ 8000000  // HSI clock frequency is 8 MHz by default

static inline void spin(volatile uint32_t count){

while(count--) (void) 0;

}

struct rcc {
    volatile uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR, AHBRSTR, CFGR2, CFGR3, CR2;
};

#define RCC ((struct rcc *) 0x40021000)

struct gpio {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFRL, AFRH, BRR;
};

#define GPIO(bank) ((struct gpio *) (0x48000000 + 0x400 * (bank)))

enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_AF, GPIO_MODE_ANALOG };

static inline void gpio_set_mode(uint16_t pin, uint8_t mode) {
    // Get the GPIO bank (port) for the specified pin (A, B, C, etc.)
    struct gpio *gpio = GPIO(PINBANK(pin));
    // Extract the pin number (0-15) from the pin identifier (e.g., 'A5' -> 5)
    int n = PINNO(pin);

    RCC_GPIO_CLK_ENABLE(PINBANK(pin));//verify if this works
    // Clear the existing 2-bit mode configuration for the specific pin
    // The 2 bits for each pin are at positions 2*n and 2*n+1 in MODER.
    gpio->MODER &= ~(3U << (n * 2));
    // Set the new mode by OR'ing the correct mode value at the proper position
    // (mode & 3) ensures we only take the lower 2 bits of the mode
    // and then shift them to the correct position for the pin.
    gpio->MODER |= (mode & 3U) << (n * 2);
}

static inline void gpioSetAF(uint16_t pin, uint8_t afNum){

    struct gpio *gpio = GPIO(PINBANK(pin));
    int n = PINNO(pin);

    if(n<8){
        gpio->AFRL &= ~(0xFUL << (n * 4)); //clear AF bit sin AFRL
        gpio->AFRL |= ((uint32_t)afNum << (n *4));
    } else {
        gpio->AFRH &= ~(0xFUL << ((n-8) * 4));
        gpio->AFRH |= ((uint32_t)afNum << ((n-8) * 4));
    }
}

static inline void gpio_write(uint16_t pin, bool val) {
    // Access the GPIO structure corresponding to the pin's bank (group of pins)
    struct gpio *gpio = GPIO(PINBANK(pin));
    // Write the value to the pin's bit set/reset register (BSRR).
    // The value is shifted left based on whether 'val' is true or false.
    // If 'val' is true (high), it sets the corresponding pin high.
    // If 'val' is false (low), it resets the pin (sets it low).
    gpio->BSRR = (1U << PINNO(pin)) << (val ? 0 : 16);
}

struct uart {
    volatile uint32_t CR1, CR2, CR3, BRR, GTPR, RTOR, RQR, ISR, ICR, RDR, TDR;
}; 

//https://www.st.com/resource/en/datasheet/stm32f072rb.pdf
//Only enables USART1
static inline void uartInit(struct uart *uart , unsigned long baud){
    uint8_t af=1;		//AF1 for USART1 from AF memory mapping
    uint16_t rx=0,tx=0;	

    //Enable Clock for the selected UART
    if(uart == UART1) RCC->APB2ENR |= BIT(14);

    //configure the tx and rx pins
    if(uart == UART1){
        tx = PIN('A',9);
        rx = PIN('A',10);
    }
    //Configure GPIO for UART pins in AF mode
    gpio_set_mode(tx, GPIO_MODE_AF);
    gpioSetAF(tx,af);
    gpio_set_mode(rx, GPIO_MODE_AF);
    gpioSetAF(rx,af);

    //Configure the UART
    uart->CR1 = 0; 			//Disable the UART before configuring
    uart->BRR = FREQ / baud; 		//Set Baud rate (FREQ is the UART clock frequency) 
    uart->CR1 |= BIT(0) | BIT(2) | BIT(3);//Enable UART, RX, TX
}

//Check if UART RX data is ready
static inline int uartReadReady(struct uart *uart){
    return uart->ISR & BIT(5); //Check RXNE(Receiver Not Empty) (bit5 in ISR register)

}

//Read a single Byte from UART
static inline uint8_t uartReadByte(struct uart *uart){
    return (uint8_t)(uart->RDR & 0xFF); //Read 8 bit data from RDR reg
}

//write a single byte to UART 
static inline void uartWriteByte(struct uart *uart, uint8_t byte){
    while((uart->ISR & BIT(7)) == 0); //Wait untill TXE (bit 7 in ISR register) is set
    uart->TDR = byte; //write byte to transmit data register
}

//write a buffer to UART
static inline void uartWriteBuf(struct uart *uart, char *buf, size_t len){
    while(len-- > 0) {
        uartWriteByte(uart, *(uint8_t *) buf++);
    }

}




