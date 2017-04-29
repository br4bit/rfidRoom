#include <Scheduler.h>
#include <pt.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 53
#define RST_PIN 5
#define RED_LED 31
#define YELLOW_LED 32
#define GREEN_LED 33

struct Room
{
	byte id[4];
	unsigned int number;
};
// Empty struct used for reinitialize the struct because can't dealloc struct
static struct Room EmptyStruct;
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
// Init array that will store new NUID 
byte tempID[4];
unsigned int addressLocation; //last address of value in memory
Room card[10];
unsigned int piccSize = 0;


void setup()
{
	pinMode(RED_LED, OUTPUT);
	pinMode(YELLOW_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
	Serial.begin(9600);
	SPI.begin(); // Init SPI bus
	rfid.PCD_Init(); // Init MFRC522 
	//Auth Key
	for (byte i = 0; i < 6; i++) {
		key.keyByte[i] = 0xFF;
	}
	LoadStructFromEEPROM();
	Serial.println("Master Card:...");
	Serial.print("Numero stanza: ");
	Serial.print(card[0].number);
	Serial.println();
	dump_byte_array(card[0].id, 4);
	Serial.println();
	Serial.print("Last value address: ");
	Serial.println(addressLocation);
	Serial.println("Utenti:");
	StampUser();
	digitalWrite(RED_LED, LOW);
	//load struct from eeprom and master to position 0
}


void loop()
{
	FreeArray();
	if (digitalRead(YELLOW_LED == HIGH))
		digitalWrite(YELLOW_LED, LOW);
	if (digitalRead(GREEN_LED == HIGH))
		digitalWrite(GREEN_LED, LOW);
	if (digitalRead(RED_LED == LOW))
		digitalWrite(RED_LED, HIGH);
	CheckCard();
}

void CheckCard() {
	// Look for new cards
	if (!rfid.PICC_IsNewCardPresent())
		return;

	// Select one of the cards
	if (!rfid.PICC_ReadCardSerial())
		return;

	for (byte i = 0; i < 4; i++) {
		tempID[i] = rfid.uid.uidByte[i];
	}

	Serial.print(F("Card UID:"));
	dump_byte_array(rfid.uid.uidByte, rfid.uid.size);
	Serial.println();
	Serial.print(F("PICC type: "));
	MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
	Serial.println(rfid.PICC_GetTypeName(piccType));
	// Check for compatibility
	if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
		&&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
		&&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
		Serial.println(F("This sample only works with MIFARE Classic cards."));
		return;
	}

	if (IsMaster()) {
		BlinkYellow(50);
		FreeArray();
		digitalWrite(RED_LED, LOW);
		FreeStruct();
		//picc size for dumpinfo
		if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI)
			piccSize = 320;
		else if (piccType == MFRC522::PICC_TYPE_MIFARE_1K)
			piccSize = 1024;
		else if (piccType == MFRC522::PICC_TYPE_MIFARE_4K)
			piccSize = 4096;
		DumpCard();
		ClearEEPROM();
		SaveStructToEEPROM();
		StampUser();
		LoadMaster();
		Serial.print("Numero stanza: ");
		Serial.print(card[0].number);
		Serial.println();
		Serial.print("Last value address: ");
		Serial.println(addressLocation);
		Serial.print("Master Card: ");
		dump_byte_array(card[0].id, 4);
		Serial.println();
	}
	else if (IsGuest()) {
		FreeArray();
		digitalWrite(GREEN_LED, HIGH);
		digitalWrite(RED_LED, LOW);
		Serial.println("apri porta card trovata");
		delay(800);
	}
	else {
		Serial.println("card non presente");
	}
	rfid.PICC_HaltA();
	rfid.PCD_StopCrypto1();
}

void BlinkYellow(unsigned int time) {
	if (digitalRead(YELLOW_LED) == LOW) {
		for (int i = 0; i < time; i++) {
			delay(50);
			OnOffBlink(200, 100);
		}
	}
}

void LoadMaster() {
	EEPROM.get(EEPROM.length() - 6, card[0]);
}

void OnOffBlink(int tOn, int tOff) {
	if (digitalRead(RED_LED) == HIGH)
		digitalWrite(RED_LED, LOW);

	static int timer = tOn;
	static long previousMillis;
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
	for (int i = 1; i <= CountElements(); i++) {
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
	int i = 1;
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

int CountElements() {
	int count = 0;
	for (int i = 1; i < sizeof(card) / 10; i++) {
		if (card[i].number != 0) {
			count++;
		}
	}
	return count;
}

void SaveStructToEEPROM() {
	int n = CountElements();
	unsigned int address = 1;
	int i = 1;
	if (addressLocation == 0) {
		while (i <= n) {
			EEPROM.put(address, card[i]);
			address = address + sizeof(card[i]);
			i++;
		}
	}
	else {
		for (int i = 1; i <= n; i++) {
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
	byte blockAddr = 4;
	byte trailerBlock = 7;
	MFRC522::StatusCode status;
	byte buffer[18];
	byte size = sizeof(buffer);
	int j = 1;
	unsigned int roomNumber;
	unsigned int totalBlocks = piccSize / 16;
	Serial.println(F("Total blocks: "));
	Serial.print(totalBlocks);
	Serial.println();
	// Authenticate using key A
	Serial.println(F("Authenticating...Trailer block: "));
	Serial.print(trailerBlock);
	Serial.println();
	status = (MFRC522::StatusCode) rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid.uid));
	if (status != MFRC522::STATUS_OK) {
		Serial.print(F("PCD_Authenticate() failed: "));
		Serial.println(rfid.GetStatusCodeName(status));
		return;
	}
	while (blockAddr < totalBlocks) {
		BlinkYellow(30);
		if (blockAddr + 1 % 4 == 0) {
			trailerBlock = trailerBlock + 4;
			blockAddr = blockAddr + 1;
			// Authenticate using key A
			Serial.println(F("Authenticating....Trailer block: "));
			Serial.print(trailerBlock);
			Serial.println();
			status = (MFRC522::StatusCode) rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid.uid));
			if (status != MFRC522::STATUS_OK) {
				Serial.print(F("PCD_Authenticate() failed: "));
				Serial.println(rfid.GetStatusCodeName(status));
				return;
			}
		}
		else {
			// Read data from block
			status = (MFRC522::StatusCode) rfid.MIFARE_Read(blockAddr, buffer, &size);
			if (status != MFRC522::STATUS_OK) {
				Serial.print(F("MIFARE_Read() failed: "));
				Serial.println(rfid.GetStatusCodeName(status));
				break;
			}
			if (buffer[0] == 0 &&
				buffer[1] == 0 &&
				buffer[2] == 0 &&
				buffer[3] == 0 &&
				buffer[4] == 0 &&
				buffer[5] == 0) {
				Serial.println("blocco fermato: ");
				Serial.print(blockAddr);
				Serial.println();
				break;
			}
			byteHigh = buffer[4];
			byteLow = buffer[5];
			roomNumber = ByteToInt(byteHigh, byteLow);
			if (roomNumber == card[0].number) {
				//load id from buff to struct
				for (int i = 0; i < 4; i++)
					card[j].id[i] = buffer[i];
				//load numer room to struct
				card[1].number = roomNumber;
				blockAddr = blockAddr + 1;
				FreeBuffer(buffer);
				j++;
			}
			else
				blockAddr = blockAddr + 1;

			FreeBuffer(buffer);
		}
	}
	rfid.PICC_HaltA();
	rfid.PCD_StopCrypto1();
	Serial.println("Dump completato");
}

void ClearEEPROM() {
	for (int i = 0; i < addressLocation; i++) {
		EEPROM.write(i, 0);
	}

	addressLocation = 0;
}

void FreeBuffer(byte *buff) {
	for (int i = 0; i < 18; i++)
		buff[i] = 0;
}

bool IsGuest() {
	int n = CountElements();
	for (int i = 1; i <= n; i++) {
		if (card[i].id[0] == tempID[0] &&
			card[i].id[1] == tempID[1] &&
			card[i].id[2] == tempID[2] &&
			card[i].id[3] == tempID[3] &&
			card[i].number == card[0].number) {
			Serial.println("Trovata card");
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
		Serial.println("Trovato master");
		return true;
	}
	return false;
}

void FreeArray() {
	for (int i = 0; i < 4; i++) {
		tempID[i] = 0;
	}
}

void FreeStruct() {
	int size = CountElements();
	for (int i = 0; i < size; i++)
		card[i] = EmptyStruct;
}

void dump_byte_array(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
		Serial.print(buffer[i] < 0x10 ? " 0" : " ");
		Serial.print(buffer[i], HEX);
	}
}
unsigned int ByteToInt(unsigned char highB, unsigned char lowB) {
	int value;
	value = highB;
	value = value << 8;
	value |= lowB;
	return value;
}