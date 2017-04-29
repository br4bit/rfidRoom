#include <Arduino.h>
#include <MoMatic.h>

SoftwareSerial* mySerial;

MoMatic::MoMatic() {
	mySerial = new SoftwareSerial(RX_PIN, TX_PIN);
	id = 0;
}
void MoMatic::momatic_setup(unsigned int *registri, unsigned int size) {
	Serial.begin(9600);
	pinMode(ID_PIN_1, INPUT);
	pinMode(ID_PIN_2, INPUT);
	pinMode(ID_PIN_3, INPUT);
	pinMode(ID_PIN_4, INPUT);
	pinMode(LED, OUTPUT);
	momatic_set_id();
	if (MoMatic::id == 0 || MoMatic::id > 31) {
		momatic_alarm();
	}
	modbus_configure(mySerial, BAUD_RATE, SERIAL_8N2, id, TX_ENABLE_PIN, size, registri);
	Serial.println("MoSat Pronta!");
}

int MoMatic::momatic_get_id() {
	return id;
}

void MoMatic::update() {
	modbus_update();
}

void MoMatic::momatic_set_id() {
	Serial.print("MoSat ID (");
	id += (int)digitalRead(ID_PIN_4);
	Serial.print((int)digitalRead(ID_PIN_4));
	id = id << 1;
	id += (int)digitalRead(ID_PIN_3);
	Serial.print((int)digitalRead(ID_PIN_3));
	id = id << 1;
	id += (int)digitalRead(ID_PIN_2);
	Serial.print((int)digitalRead(ID_PIN_2));
	id = id << 1;
	id += (int)digitalRead(ID_PIN_1);
	Serial.print((int)digitalRead(ID_PIN_1));

	Serial.print("):");
	Serial.print(id, DEC);
	Serial.println("");
}

void MoMatic::momatic_alarm() {
	while (1) {
		digitalWrite(LED, HIGH);
		delay(500);
		digitalWrite(LED, LOW);
		delay(500);
		Serial.println("MoSat in allarme!!");
	}
}