/**
* Typical pin layout used:
* -----------------------------------------------------------------------------------------
*             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
*             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
* Signal      Pin          Pin           Pin       Pin        Pin              Pin
* -----------------------------------------------------------------------------------------
* RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
* SPI SS 0    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required ** (53)
* SPI SS 1    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required ** (31)
* SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
* SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
* SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*
* Limitations SoftwareSerial for Modbus
*
* The library has the following known limitations:
* If using multiple software serial ports, only one can receive data at a time.
* Not all pins on the Mega and Mega 2560 support change interrupts,
* so only the following can be used for RX: 10, 11, 12, 13, 14, 15,
* 50, 51, 52, 53, A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69).
* Not all pins on the Leonardo and Micro support change interrupts,
* so only the following can be used for RX: 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).
* On Arduino or Genuino 101 the current maximum RX speed is 57600bps
* On Arduino or Genuino 101 RX doesn't work on Pin 13
*/

#include <MoMatic.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS0_PIN 53
#define SS1_PIN 31
#define RST_PIN 9
#define RED_LED 37
#define YELLOW_LED 33
#define GREEN_LED 39

byte ssPins[] = { SS0_PIN, SS1_PIN };

struct Room
{
	byte id[4];
	unsigned int number;
};
// Empty struct used for reinitialize the struct
static struct Room EmptyStruct;
MFRC522 rfid[2];
MFRC522::MIFARE_Key key;
// Init array that will store new NUID
byte tempID[4];
byte tempID2[4];
unsigned int accessCounter[] = { 0,0,0,0,0 }; //counter array number of accesses for each keys
unsigned int addressLocation; //last address of value in memory
Room card[10];
unsigned int piccSize = 0;
bool static indoor = false;
MoMatic momatic; //initialize an instance of the class
enum
{
	INDOOR,
	ROOM,
	NUMBEROFKEYS,
	KEY0,
	KEY1,
	KEY2,
	KEY3,
	KEY4,
	HOLDING_REGS_SIZE
};
unsigned int registri[HOLDING_REGS_SIZE];

void setup()
{
	momatic.momatic_setup(registri, HOLDING_REGS_SIZE);
	pinMode(RED_LED, OUTPUT);
	pinMode(YELLOW_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
	SPI.begin(); // Init SPI bus
	rfid[0].PCD_Init(ssPins[0], RST_PIN);
	rfid[1].PCD_Init(ssPins[1], RST_PIN);
	/// Auth Key
	key.keyByte[0] = 0xFF;
	key.keyByte[1] = 0xFF;
	key.keyByte[2] = 0xFF;
	key.keyByte[3] = 0xFF;
	key.keyByte[4] = 0xFF;
	key.keyByte[5] = 0xFF;
	LoadStructFromEEPROM();
	Serial.println("Master Card:");
	Serial.print("Numero stanza: ");
	Serial.print(card[0].number);
	Serial.println();
	dump_byte_array(card[0].id, 4);
	Serial.println();
	Serial.print("Last value address: ");
	Serial.println(addressLocation);
	Serial.println("Cards Memorizzate:");
	StampUser();
	digitalWrite(RED_LED, HIGH);
	//load struct from eeprom and master to position 0
}

void loop()
{
	momatic.update();
	registri[INDOOR] = (unsigned int)indoor;
	registri[ROOM] = card[0].number;
	registri[NUMBEROFKEYS] = CountElements();
	registri[KEY0] = accessCounter[0];
	registri[KEY1] = accessCounter[1];
	registri[KEY2] = accessCounter[2];
	registri[KEY3] = accessCounter[3];
	registri[KEY4] = accessCounter[4];
	/*Check led and presence*/
	if (indoor && (digitalRead(GREEN_LED)) == HIGH) {
		digitalWrite(RED_LED, LOW);
		indoor = true;
	}
	else {
		if (digitalRead(RED_LED) == LOW)
			digitalWrite(RED_LED, HIGH);
		indoor = false;
	}

	if (digitalRead(YELLOW_LED == HIGH))
		digitalWrite(YELLOW_LED, LOW);

	CheckCard();
	CheckCardPresence();
	momatic.update();
}

void CheckCard() {
	// Look for new cards
	if (!rfid[0].PICC_IsNewCardPresent())
		return;

	// Select one of the cards
	if (!rfid[0].PICC_ReadCardSerial())
		return;

	for (byte i = 0; i < 4; i++) {
		tempID[i] = rfid[0].uid.uidByte[i];
	}

	Serial.print(F("Card UID:"));
	dump_byte_array(rfid[0].uid.uidByte, rfid[0].uid.size);
	Serial.println();
	Serial.print(F("PICC type: "));
	MFRC522::PICC_Type piccType = rfid[0].PICC_GetType(rfid[0].uid.sak);
	Serial.println(rfid[0].PICC_GetTypeName(piccType));
	// Check for compatibility
	if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
		&&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
		&&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
		Serial.println(F("This sample only works with MIFARE Classic cards."));
		return;
	}

	if (IsMaster()) {
		//BlinkYellow(50);
		FreeArray();
		digitalWrite(RED_LED, LOW);
		digitalWrite(GREEN_LED, LOW);
		//picc size for dumpinfo
		if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI)
			piccSize = 320;
		else if (piccType == MFRC522::PICC_TYPE_MIFARE_1K)
			piccSize = 1024;
		else if (piccType == MFRC522::PICC_TYPE_MIFARE_4K)
			piccSize = 4096;
		FreeStruct();
		DumpCard();
		ClearEEPROM();
		SaveStructToEEPROM();
		StampUser();
		LoadMaster();
	}
	else if (IsGuest()) {
		FreeArray();
		digitalWrite(GREEN_LED, HIGH);
		digitalWrite(RED_LED, LOW);
		Serial.println("Apri Porta");
		delay(800);
		digitalWrite(GREEN_LED, LOW);
		for (byte j = 0; j < 5; j++) {
			Serial.println();
			Serial.print("Card ");
			Serial.print(j + 1);
			Serial.print(": ");
			Serial.print(accessCounter[j]);
		}
	}
	rfid[0].PICC_HaltA();
	rfid[0].PCD_StopCrypto1();
	if (digitalRead(RED_LED == LOW))
		digitalWrite(RED_LED, HIGH);
	FreeArray();
}

void CheckCardPresence() {

	// Look for new cards
	if (!rfid[1].PICC_IsNewCardPresent()) {
		if (digitalRead(GREEN_LED == HIGH))
			digitalWrite(GREEN_LED, LOW);
		return;
	}

	// Select one of the cards
	if (!rfid[1].PICC_ReadCardSerial()) {
		if (digitalRead(GREEN_LED == HIGH))
			digitalWrite(GREEN_LED, LOW);
		return;
	}
	for (byte i = 0; i < 4; i++) {
		tempID2[i] = rfid[1].uid.uidByte[i];
	}
	if (IsGuestIndoor()) {

		do {
			digitalWrite(GREEN_LED, HIGH);
			indoor = true;
			digitalWrite(RED_LED, LOW);

		} while ((rfid[1].PICC_IsNewCardPresent() && rfid[1].PICC_ReadCardSerial()));
	}
	FreeArray2();
	rfid[1].PCD_StopCrypto1();
}


void BlinkYellow(unsigned int time) {
	if (digitalRead(YELLOW_LED) == LOW) {
		for (unsigned int i = 0; i < time; i++) {
			OnOffBlink(200, 100);
		}
	}
}

void LoadMaster() {
	EEPROM.get(EEPROM.length() - 6, card[0]);
}

void OnOffBlink(unsigned int tOn, unsigned int tOff) {
	if (digitalRead(RED_LED) == HIGH)
		digitalWrite(RED_LED, LOW);

	static unsigned int timer = tOn;
	static unsigned long previousMillis;
	if ((millis() - previousMillis) >= timer) {
		if (digitalRead(YELLOW_LED) == HIGH) {
			timer = tOff;
		}
		else {
			timer = tOn;
		}
		digitalWrite(YELLOW_LED, !digitalRead(YELLOW_LED));
		previousMillis = millis();
	}
}

void StampUser() {
	for (unsigned int i = 1; i <= CountElements(); i++) {
		Serial.println();
		Serial.println("Card: ");
		dump_byte_array(card[i].id, 4);
		Serial.println();
		Serial.print("Numero stanza: ");
		Serial.print(card[i].number);
	}
	Serial.println();
	Serial.print("Numero card: ");
	Serial.print(CountElements());
	Serial.println();
}

void LoadStructFromEEPROM() {
	unsigned int address = 1;
	unsigned int i = 1;
	/*Load Master to position 0*/
	LoadMaster();
	addressLocation = EEPROM.read(0);
	if (addressLocation != 0) {
		while (address < addressLocation) {
			EEPROM.get(address, card[i]);
			address = address + sizeof(card[i]);
			i++;
		}
	}
}

unsigned int CountElements() {
	unsigned int count = 0;
	for (unsigned int i = 1; i < sizeof(card) / 10; i++) {
		if (card[i].number != 0) {
			count++;
		}
	}
	return count;
}

void SaveStructToEEPROM() {
	unsigned int n = CountElements();
	unsigned int address = 1;
	unsigned int i = 1;
	if (addressLocation == 0) {
		while (i <= n) {
			EEPROM.put(address, card[i]);
			address = address + sizeof(card[i]);
			i++;
		}
	}
	else {
		for (; i <= n; i++) {
			EEPROM.put(address, card[i]);
			address = address + sizeof(card[i]);
		}
	}
	addressLocation = address;
	EEPROM.write(0, addressLocation);
}

void DumpCard() {
	byte byteHigh;
	byte byteLow;
	byte blockAddr = 8;
	byte trailerBlock = 11;
	MFRC522::StatusCode status;
	byte buffer[18];
	byte size = sizeof(buffer);
	unsigned int j = 1;
	unsigned int roomNumber;
	unsigned int totalBlocks = piccSize / 16;
	// Authenticate using key A
	status = (MFRC522::StatusCode) rfid[0].PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid[0].uid));
	if (status != MFRC522::STATUS_OK) {
		digitalWrite(RED_LED, HIGH);
		digitalWrite(GREEN_LED, HIGH);
		delay(1000);
		return;
	}
	while (blockAddr < totalBlocks - 1) {
		OnOffBlink(300, 100);
		if (blockAddr == trailerBlock) {
			trailerBlock = trailerBlock + 4;
			blockAddr++;
			// Authenticate using key A
			status = (MFRC522::StatusCode) rfid[0].PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid[0].uid));
			if (status != MFRC522::STATUS_OK) {
				digitalWrite(RED_LED, HIGH);
				digitalWrite(GREEN_LED, HIGH);
				delay(3000);
				return;
			}
		}
		// Read data from block
		status = (MFRC522::StatusCode) rfid[0].MIFARE_Read(blockAddr, buffer, &size);
		if (status != MFRC522::STATUS_OK) {
			digitalWrite(RED_LED, HIGH);
			digitalWrite(GREEN_LED, HIGH);
			delay(3000);
			break;
		}
		if (buffer[0] != 0 &&
			buffer[1] != 0 &&
			buffer[2] != 0 &&
			buffer[3] != 0 &&
			buffer[4] != 0 &&
			buffer[5] != 0) {
			byteHigh = buffer[4];
			byteLow = buffer[5];
			roomNumber = ByteToInt(byteHigh, byteLow);
			if (roomNumber == card[0].number) {
				//load id from buff to struct
				for (int i = 0; i < 4; i++)
					card[j].id[i] = buffer[i];
				//load number room to struct
				card[j].number = roomNumber;
				j++;
				Serial.println("Caricata card.");
			}
		}
		blockAddr++;
		FreeBuffer(buffer);
	}
	rfid[0].PICC_HaltA();
	rfid[0].PCD_StopCrypto1();
	Serial.println("Dump completato");
}

void ClearEEPROM() {
	for (unsigned int i = 0; i < addressLocation; i++) {
		OnOffBlink(300, 100);
		EEPROM.write(i, 0);
	}
	addressLocation = 0;
}

void FreeBuffer(byte *buff) {
	for (int i = 0; i < 18; i++)
		buff[i] = 0;
}

bool IsGuestIndoor() {
	unsigned int n = CountElements();
	for (unsigned int i = 1; i <= n; i++) {
		if (card[i].id[0] == tempID2[0] &&
			card[i].id[1] == tempID2[1] &&
			card[i].id[2] == tempID2[2] &&
			card[i].id[3] == tempID2[3] &&
			card[i].number == card[0].number) {
			digitalWrite(GREEN_LED, HIGH);
			return true;
		}
	}
	return false;
}

bool IsGuest() {
	unsigned int n = CountElements();
	for (unsigned int i = 1; i <= n; i++) {
		if (card[i].id[0] == tempID[0] &&
			card[i].id[1] == tempID[1] &&
			card[i].id[2] == tempID[2] &&
			card[i].id[3] == tempID[3] &&
			card[i].number == card[0].number) {
			accessCounter[i - 1]++;
			return true;
		}
	}
	return false;
}

bool IsMaster() {
	if (card[0].id[0] == tempID[0] &&
		card[0].id[1] == tempID[1] &&
		card[0].id[2] == tempID[2] &&
		card[0].id[3] == tempID[3]) {
		return true;
	}
	return false;
}

void FreeArray() {
	for (int i = 0; i < 4; i++) {
		tempID[i] = 0;
	}
}
void FreeArray2() {
	for (int i = 0; i < 4; i++) {
		tempID2[i] = 0;
	}
}

void FreeStruct() {
	unsigned int size = CountElements();
	for (unsigned int i = 1; i < size; i++)
		card[i] = EmptyStruct;
}

void dump_byte_array(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
		Serial.print(buffer[i] < 0x10 ? " 0" : " ");
		Serial.print(buffer[i], HEX);
	}
}
unsigned int ByteToInt(unsigned char highB, unsigned char lowB) {
	unsigned int value;
	value = highB;
	value = value << 8;
	value |= lowB;
	return value;
}