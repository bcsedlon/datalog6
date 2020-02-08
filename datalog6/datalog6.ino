#include "Arduino.h"
/*
SD CARD SPI:
MOSI - pin 11
MISO - pin 12
CLK  - pin 13
CS   - pin 10
*/

#define ENTER '\n'

#define SD_CS_PIN 10
#define LED_PIN 6

#include "LowPower.h"

#include <SPI.h>
#include <SD.h>

#include <Wire.h>
#include "RTClib.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE0_PIN 2
#define ONE_WIRE1_PIN 3
#define ONE_WIRE2_PIN 5

#define ONE_WIRE_BUS_NUM 3
#define ONE_WIRE_DEV_NUM 2
OneWire ds18x20s[] = { ONE_WIRE0_PIN, ONE_WIRE1_PIN, ONE_WIRE2_PIN };
//const int oneWireCount = sizeof(ds18x20s)/sizeof(OneWire);
//DallasTemperature sensors[oneWireCount];
DallasTemperature sensors[ONE_WIRE_BUS_NUM];
DeviceAddress deviceAddresses [ONE_WIRE_BUS_NUM][ONE_WIRE_DEV_NUM];

//DS1307 rtc;
RTC_DS1307 rtc;

bool sdError;
int dallasError;

bool debug;
bool led;
char fileName[16];
char buf[128];

int minute = -1;
bool sdWrite;

#include <EEPROM.h>

#define DEVICESNAME_NUM 16
//struct DeviceName {
//	DeviceAddress address;
//	char name[8];
//};
#include "DeviceName.h"

int getDeviceName(DeviceName &dn) {
	DeviceName d;
	for(int i = 0; i < DEVICESNAME_NUM; i++) {
		EEPROM.get(sizeof(DeviceName) * i, d);
		if(memcmp(d.address, dn.address, sizeof(DeviceAddress)) == 0) {
			EEPROM.get(sizeof(DeviceName) * i, dn);
			return i;
		}
	}
	//dn.name[0] = 0;
	getAddress(dn.name, 0, dn.address);
	dn.name[17] = 0;
	return -1;
}

int getDeviceName(DeviceName dn, int pos) {
	EEPROM.get(sizeof(DeviceName) * pos, dn);
	return pos;
}

int getAddress(char* buf, int pos, DeviceAddress deviceAddress) {
	int i;
	for (i = 0; i < 8; i++) {
		sprintf(buf + pos + i*2, "%02X", deviceAddress[i]);
	}
	return i*2;
}

int getFileName(char* buf, DateTime dt) {
	return sprintf(buf, "%04u%02u%02u.CSV", min(dt.year(), 9999), min(dt.month(), 99), min(dt.day(), 99));
	//return snprintf(buf, "%04u%02u%02u.csv", 8 + 3 + 1, dt.year(), dt.month(), dt.day());
}

int getDateTime(char* buf, int pos, DateTime dt) {
	//dt.toString(buf + pos)
	return sprintf(buf + pos, "%04u-%02u-%02u %02u:%02u:%02u", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
}

int getInt(int *pi) {
	while(!Serial.available());
	String s = Serial.readStringUntil(ENTER);
	if(s.length()) {
		*pi = s.toInt();
		Serial.println(*pi);
		return 1;
	}
	Serial.println(*pi);
	return 0;
}

int getCharArray(char *p) {
	while(!Serial.available());
	String s = Serial.readStringUntil(ENTER);
	if(s.length()) {
		s.toCharArray(p, 18);
		p[17] = 0;
		Serial.println(p);
		return 1;
	}
	Serial.println(p);
	return 0;
}

int printConfirm(String s) {
	while(1) {
		Serial.print(s);
		Serial.print(F(" Y/N?:"));
		while(!Serial.available());
		String ss = Serial.readStringUntil(ENTER);
		Serial.println(ss);
		if(ss.equalsIgnoreCase("Y")) {
			return 1;
		}
		if(ss.equalsIgnoreCase("N")) {
			return 0;
		}
	}
	return 0;
}

int getConfigString(char* buf, int pos, DateTime dt) {
	pos += getDateTime(buf, pos, dt);
	for(int i = 0; i < ONE_WIRE_BUS_NUM; i++) {
		for(int j = 0; j < ONE_WIRE_DEV_NUM; j++) {
			buf[pos++] = ';';
			//pos += getAddress(buf, pos, deviceAddresses[i][j]);
			DeviceName dn;
			memcpy(dn.address, deviceAddresses[i][j], 8);
			getDeviceName(dn),
			strcpy(buf + pos, dn.name);
			pos += strlen(dn.name);
		}
	}
	return pos;
}

void searchDevices() {
	for (int i = 0; i < ONE_WIRE_BUS_NUM; i++) {;
		sensors[i].setOneWire(&ds18x20s[i]);
		sensors[i].begin();
		Serial.print(F("BUS:"));
		Serial.println(i);

		sensors[i].requestTemperatures();
		for(int j = 0; j < ONE_WIRE_DEV_NUM; j++) {
			DeviceAddress da;
			DeviceName dn;
			if (sensors[i].getAddress(da, j)) {
				memcpy(deviceAddresses[i][j], da, sizeof(da));
				Serial.print(F("NUM:"));
				Serial.print(j);
				Serial.print(F("\tID:"));
				getAddress(buf, 0, deviceAddresses[i][j]);
				Serial.print(buf);
				Serial.print(F("\tNAME:"));
				memcpy(dn.address, da, 8);
				getDeviceName(dn);
				Serial.print(dn.name);
				Serial.print(F("\tT:"));
				sensors[i].setResolution(deviceAddresses[i][j], 12);
				float f = sensors[i].getTempC(deviceAddresses[i][j]);
				Serial.println(f);
			}
			else {
				for(int k = 0; k < 8; k++)
					deviceAddresses[i][j][k] = 0;
			}
		}
	}
}

void printDeviceName(DeviceName dn, int pos) {
	 Serial.print(pos);
	 Serial.print('\t');
	 getAddress(buf, 0, dn.address);
	 Serial.print(buf);
	 Serial.print('\t');
	 Serial.println(dn.name);
}
/*
void printDirectory(File dir, int numTabs) {
	while (true) {
		File entry =  dir.openNextFile();
		if (!entry)
			break;
		for (uint8_t i = 0; i < numTabs; i++)
			Serial.print('\t');

		Serial.print(entry.name());
		if (entry.isDirectory()) {
			Serial.println("/");
			printDirectory(entry, numTabs + 1);
		} else {
			Serial.print("\t");
			Serial.println(entry.size(), DEC);
		}
		entry.close();
	}
}
*/
void printDirectory(int index = -1) {
	int i = 0;
	File dir = SD.open("/");
	if(dir) {
		while(true) {
			File entry = dir.openNextFile();
			if (!entry)
				break;
			if(index < 0) {
				Serial.print(i);
				Serial.print('\t');
				Serial.print(entry.name());
				Serial.print('\t');
				Serial.println(entry.size(), DEC);
			}
			else if(i == index) {
				Serial.println(entry.name());
				while (entry.available()) {
				      Serial.write(entry.read());
				}
				entry.close();
				break;
			}
			entry.close();
			i++;
		}
		dir.close();
	}
}

void setup() {
	Serial.begin(9600);
	while (!Serial) {
	}
	Serial.println(F("DATALOG6"));
	pinMode(LED_PIN, OUTPUT);

	Wire.begin();
	rtc.begin();
	if(! rtc.isrunning()) {
		Serial.println(F("RTC SET"));
		//rtc.adjust(DateTime(__DATE__, __TIME__));
		rtc.adjust(DateTime(2020, 1, 1, 0, 0, 0));
	}
	else {
		getDateTime(buf, 0, rtc.now());
		Serial.println(buf);
	}

	Serial.println(F("SD INIT"));
	if (!SD.begin(SD_CS_PIN)) {
		Serial.println(F("SD ERROR"));
		sdError = true;
		//while (1) {
		//	digitalWrite(LED_PIN, !digitalRead(LED_PIN));
		//	delay(250);
		//}
	}

	searchDevices();

	DateTime now = rtc.now();

	getFileName(fileName, now);
	Serial.println(fileName);
	getConfigString(buf, 0, now);
	Serial.println(buf);

	File dataFile = SD.open(fileName, FILE_WRITE);
	if (dataFile) {
		dataFile.println(buf);
		dataFile.close();
		sdError = false;
	}
	else
		sdError = true;
}

void loop() {
	if(debug) {
		Serial.println('>');
	}

	if(sdError) {
		Serial.println(F("SD INIT"));
		if (!SD.begin(SD_CS_PIN)) {
			Serial.println(F("SD ERROR"));
		}
	}

	DateTime now = rtc.now();
	int m = now.minute();
	if(m % 10 == 0 && m != minute) {
		minute = m;
		sdWrite = true;
	}
	else
		sdWrite = false;

	//if(millis() - lastMillis > 1000) {
	//	lastMillis = millis();
	//	return;
	//}

	for (int i = 0; i < ONE_WIRE_BUS_NUM; i++) {
	    sensors[i].requestTemperatures();
	}

	int pos = 0;
	char newFileName[16];
	getFileName(newFileName, now);
	if(strcmp(newFileName, fileName)) {
		strcpy(fileName, newFileName);

		//dataString = getConfigString(now);
		//dataString += "\n\r";.
		searchDevices();
		pos = getConfigString(buf, 0, now);
		if(debug) {
			if(sdWrite)
				Serial.println(fileName);
			Serial.println(buf);
		}

		if(sdWrite) {
			File dataFile = SD.open(fileName, FILE_WRITE);
			if (dataFile) {
				dataFile.println(buf);
				dataFile.close();
				sdError = false;
			}
			else
				sdError = true;
		}
	}

	dallasError = 0; //false;
	pos = 0;
	pos += getDateTime(buf, pos, now);
	for(int i = 0; i < ONE_WIRE_BUS_NUM; i++) {
		for(int j = 0; j < ONE_WIRE_DEV_NUM; j++) {
			float f = sensors[i].getTempC(deviceAddresses[i][j]);
			if(f == -127.0)
				f = sensors[i].getTempC(deviceAddresses[i][j]);
			if(f == -127.0)
				f = sensors[i].getTempC(deviceAddresses[i][j]);
			if(f == -127.0)
				dallasError++; // = true;
			buf[pos++] = ';';
			char* p = dtostrf(f, 0, 1, buf + pos);
			pos += strlen(p);
		}
	}

	if(debug) {
		if(sdWrite)
			Serial.println(fileName);
		Serial.println(buf);
	}

	if(sdWrite) {
		File dataFile = SD.open(fileName, FILE_WRITE);
		if (dataFile) {
			dataFile.println(buf);
			dataFile.close();
			sdError = false;
		}
		else
			sdError = true;
	}

	if(debug) {
		if(sdError)
			Serial.println(F("SD ERROR"));
		if(dallasError) {
			Serial.print(F("SENSORS ERROR:"));
			Serial.println(dallasError);
		}
	}
	pinMode(LED_PIN, OUTPUT);
	int blinkNum = 1;
	int blinkTime = 5;
	if(sdError) {
		blinkNum = 3;
		blinkTime = 505;
	}
	else if (dallasError)
		blinkNum = dallasError + 1;

	for(int i = 0; i < blinkNum; i++) {
		digitalWrite(LED_PIN, true);
		delay(blinkTime);
		digitalWrite(LED_PIN, false);
		delay(495);
	}

	if(Serial.available()) {
		now = rtc.now();
		char ch = Serial.read();
		while(Serial.available()) Serial.read();

		if(ch == 't' || ch == 'T') {
			Serial.println(F("TIME SET"));
			int year, month, day, hour, min, sec;
			year = now.year();
			month = now.month();
			day = now.day();
			hour = now.hour();
			min = now.minute();
			sec = now.second();
			Serial.print(F("YEAR:"));
			getInt(&year);
			Serial.print(F("\nMONTH:"));
			getInt(&month);
			Serial.print(F("\nDAY:"));
			getInt(&day);
			Serial.print(F("\nHOUR:"));
			getInt(&hour);
			Serial.print(F("\nMIN:"));
			getInt(&min);
			Serial.print(F("\nSEC:"));
			getInt(&sec);
			Serial.println();

			DateTime dt(year, month, day, hour, min, sec);
			rtc.adjust(dt);
			getDateTime(buf, 0, rtc.now());
			Serial.println(buf);
		}
		else if(ch == 'd' || ch == 'D') {
			debug = !debug;
			if(debug)	{
				Serial.println(F("DEBUG ON"));
				pos = getConfigString(buf, 0, now);
				Serial.println(buf);
			}
			else
				Serial.println(F("DEBUG OFF"));
		}
		else if(ch == 's' || ch == 'S') {
			Serial.println(F("SEARCH DEVICES"));
			searchDevices();
		}
		else if(ch == 'l' || ch == 'L') {
			Serial.println(F("SD LIST"));
			//printDirectory(SD.open("/"), 0);
			Serial.println(F("INDEX\tNAME\tSIZE"));
			printDirectory();
			int n = 0;
			Serial.println(F("INDEX?"));
			if(getInt(&n))
				printDirectory(n);
		}
		else if(ch == 'n' || ch == 'N') {
			DeviceName dn;
			for(int i = 0; i < DEVICESNAME_NUM; i++) {
				EEPROM.get(sizeof(DeviceName) * i, dn);
				printDeviceName(dn, i);
			 }
		}
		else if(ch == 'e' || ch == 'E') {
			int n;
			Serial.println(F("POS?"));
			getInt(&n);
			DeviceName dn;
			EEPROM.get(sizeof(DeviceName) * n, dn);
			printDeviceName(dn, n);

			Serial.println(F("NAME:"));
			getCharArray(dn.name);\
			int bus, index;
			Serial.println(F("BUS:"));
			getInt(&bus);
			Serial.println(F("NUM:"));
			getInt(&index);
			memcpy(dn.address, deviceAddresses[bus][index], 8);

			printDeviceName(dn, n);
			if(printConfirm(F("SAVE"))) {
				EEPROM.put(sizeof(DeviceName) * n, dn);
			}
		}
		else {
			Serial.println(F("T:TIME SET"));
			Serial.println(F("S:SEARCH DEVICES"));
			Serial.println(F("L:LIST SD CARD"));
			Serial.println(F("N:DISPLAY NAMES"));
			Serial.println(F("E:EDIT NAME"));
			Serial.println(F("D:DEBUG"));
		}
		while(Serial.available()) Serial.read();
	}

	Serial.flush();
	pinMode(LED_PIN, INPUT);

	if(sdError)
		minute = -1;
	LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
}
