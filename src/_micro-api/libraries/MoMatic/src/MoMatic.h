/*
* MoMatic.h - Library for Modbus RTU RS485.
* for Arduino ATmega 2560 use:
* RX_PIN=RO(receive output)=PIN 10,
* TX_PIN=DI(data input)=PIN 11,
* RE jumper with DE on TX_ENABLE_PIN
* more info in main sketch.
*/
#ifndef MoMatic_h
#define MoMatic_h
#define LED 13
#define BAUD_RATE 9600
#define RX_PIN 10
#define TX_PIN 11
#define TX_ENABLE_PIN 4
#define ID_PIN_1 5
#define ID_PIN_2 6
#define ID_PIN_3 7
#define ID_PIN_4 8

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
#include <SimpleModbusSlave.h>

class MoMatic
{
public:
	MoMatic();
	void momatic_set_id();
	int momatic_get_id();
	void momatic_setup(unsigned int *registri, unsigned int size);
	void update();
private:
	void momatic_alarm();
	int id;
};

#endif