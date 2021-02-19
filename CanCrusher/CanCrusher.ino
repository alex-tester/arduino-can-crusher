

/*
 Name:		CanCrusher.ino
 Created:	2/19/2021 5:51:08 AM
 Author:	alexandre
*/
const bool isDebug = true;
const bool ignoreEndstops = true;

#pragma region Imported Libraries

#include <EEPROM.h>
#include "DHT.h"
//CORE GRAPHICS LIBRARIES FOR DISPLAYING TEXT
//GRAPHICS LIBRARIES FOR SD CARD IMAGES
//IMAGES MUST BE NO LARGER THAN 160px height/128px wide - 24bit color BMP (reverse if sideways)
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library
#include <Adafruit_ImageReader.h> // Image-reading functions

#pragma endregion


#pragma region Global Variables
#pragma region EEPROM Defs

//logic to handle counter
bool firstrun = false; //this should never be used again
int crushCount = 0;


#pragma endregion
#pragma region Temperature Sensor

#define DHTPIN 43     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11


int currentHumidity = 0;
int currentTemperatureF = 0;
int currentTemperatureC = 0;
int currentHeatIndex = 0;
int heatIndexF = 0; //heat index farenheight
int heatIndexC = 0;
DHT dht(DHTPIN, DHTTYPE);
#pragma endregion

#pragma region Ultrasound Sensor
//ultrasonic sensor
int trigPin = 39; // 5;    // Trigger - purple
int echoPin = 38; //6;    // Echo - blue
long duration, cm, inches;


const int minimumDistanceToEngageCrusher = 10; //CENTIMETERS
int distanceToCrusherBaseCentimeters = 100; //start with high value in case sensor isn't connected

bool isTallBoyCan = false; //different logic if can is x inches away
const int tallBoyCanDistanceThreshold = 1000; //define height of TB in centimeters

#pragma endregion

#pragma region TFT

#define USE_SD_CARD
#define SD_CS         46 //4 // SD card select pin
#define TFT_CS        49 //10
#define TFT_RST       48 // 9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC        47 // 8

#if defined(USE_SD_CARD)
SdFat                SD;         // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys

#else
// SPI or QSPI flash filesystem (i.e. CIRCUITPY drive)
#if defined(__SAMD51__) || defined(NRF52840_XXAA)
Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS,
	PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
#else
#if (SPI_INTERFACES_COUNT == 1)
Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
#else
Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
#endif
#endif
Adafruit_SPIFlash    flash(&flashTransport);
FatFileSystem        filesys;
Adafruit_ImageReader reader(filesys); // Image-reader, pass in flash filesys
#endif

Adafruit_ST7735      tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_Image       img;        // An image loaded into RAM
//Adafruit_GFX         gfx;
int32_t              width = 0, // BMP image dimensions
height = 0;

int sdCardImageCount = 0;
long randNumber;

#pragma endregion

#pragma region Ejector
//STEPPER MOTOR DRIVER FOR EJECTOR
const int stepPinEject = 31;
const int dirPinEject = 30;
const int microsBetweenStepsEject = 600;
const int enableStepperPinEject = 32;
const int numEjectStepsEject = 540; //number of steps to move when ejecting


#pragma endregion

#pragma region MainCrusher
const int stepPin = 24; // 4; //PUL -
const int dirPin = 25; //5; //DIR -
const int enableStepperPin = 26; //6; //ENABLE+ STILL NEED TO CONNECT
const int retractEndstopPin = 22; // 2; //stop retracting once we reach it
const int crushEndstopPin = 23; //

const int microsBetweenStepsRetract = 100; //150 good //600 good
const int microsBetweenStepsCrushTier0 = 100; //150 good //300 good
const int microsBetweenStepsCrushTier1 = 400; //300 good
const int microsBetweenStepsCrushTier2 = 600; //600 good!!!!!
const int numEjectSteps = 8500; //1540; //number of steps to move when ejecting
const int numCrushStepsTier0 = 3000;
const int numCrushStepsTier1 = 3000;
const int numCrushStepsTier2 = 2500; //3000;
const int numCrushStepsTallBoyTier1 = 2000;
const int numCrushStepsTallBoyTier2 = 7000;
//bool isTallBoyCan = false; //different logic if can is x inches away //moved to ultrasoundImports.h
const int endStopBackOffSteps = 400; //how many steps to back off from endstop
const int microsBetweenStepsEndStopBackOff = 400;

bool hasRetracted = false; //only increment counter if we've gone backwards
bool hasCrushed = false; //only increment counter if we've gone backwards


const int fwdBtnPin = 14; //13; //FORWARD
const int revBtnPin = 15; //12; //REVERSE
#pragma endregion


#pragma endregion



#pragma region Temperature/HumiditySensor Methods




void checkCurrentTemps()
{
	// Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  //float h = dht.readHumidity();
	currentHumidity = round(dht.readHumidity());
	// Read temperature as Celsius (the default)
	//float t = dht.readTemperature();
	//currentTemperatureC = round(dht.readTemperature());
	// Read temperature as Fahrenheit (isFahrenheit = true)
	//float f = dht.readTemperature(true);
	currentTemperatureF = round(dht.readTemperature(true));

	// Check if any reads failed and exit early (to try again).
	//if (isnan(h) || isnan(t) || isnan(f)) {
	if (isnan(currentHumidity) || isnan(currentTemperatureF) || isnan(currentTemperatureC)) {
		Serial.println(F("Failed to read from DHT sensor!"));
		return;
	}

	// Compute heat index in Fahrenheit (the default)
	heatIndexF = round(dht.computeHeatIndex(currentTemperatureF, currentHumidity));
	// Compute heat index in Celsius (isFahreheit = false)
	//heatIndexC = round(dht.computeHeatIndex(currentTemperatureC, currentHumidity, false));
}

#pragma endregion



#pragma region Ultrasound Methods
//ULTRASONIC SENSOR


//if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {break;}
int getDistanceToCrusherBaseCentimeters()
{






	// The sensor is triggered by a HIGH pulse of 10 or more microseconds.
	  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
	digitalWrite(trigPin, LOW);
	delayMicroseconds(5);
	digitalWrite(trigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);

	// Read the signal from the sensor: a HIGH pulse whose
	// duration is the time (in microseconds) from the sending
	// of the ping to the reception of its echo off of an object.
	pinMode(echoPin, INPUT);
	duration = pulseIn(echoPin, HIGH);

	// Convert the time into a distance
	//cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343
	//inches = (duration/2) / 74;   // Divide by 74 or multiply by 0.0135
	distanceToCrusherBaseCentimeters = (duration / 2) / 29.1; //centimeters
	Serial.print(distanceToCrusherBaseCentimeters);
	Serial.println("cm - ultrasonic sensor");
	if (distanceToCrusherBaseCentimeters >= tallBoyCanDistanceThreshold)
	{
		isTallBoyCan = true;

		Serial.println("isTallBoy: TRUE");
	}
	else
	{
		isTallBoyCan = false;
	}









	//OVERRIDE FOR MANUAL ENGAGING
	if (digitalRead(revBtnPin) == LOW)
	{
		//digitalWrite(enableStepperPin,LOW); //ENABLE Stepper
		//delay(1000);
		Serial.print(revBtnPin);
		Serial.println(" is LOW - RETRACTING");
		//retract();
		//digitalWrite(enableStepperPin,HIGH);
		distanceToCrusherBaseCentimeters = 1;
	}
	if (digitalRead(fwdBtnPin) == LOW)
	{
		//    bool success = false;
		Serial.print(fwdBtnPin);
		Serial.println(" is LOW - CRUSHING");
		//      if (isTallBoyCan)
		//      {
		//      crush(numCrushStepsTallBoyTier1, microsBetweenStepsCrushTier1);
		//      crush(numCrushStepsTallBoyTier2, microsBetweenStepsCrushTier2);
		//      }
		//      else
		//      {
		//      
		//      success = crushTrueIfSuccess(numCrushStepsTier0, microsBetweenStepsCrushTier0);
		//      if (success) { success = crushTrueIfSuccess(numCrushStepsTier1, microsBetweenStepsCrushTier1);}
		//      if (success) { success = crushTrueIfSuccess(numCrushStepsTier2, microsBetweenStepsCrushTier2);}
		//      }
		//
		//      retractOLD();
		distanceToCrusherBaseCentimeters = 1;


	}


	return  distanceToCrusherBaseCentimeters;
}
#pragma endregion



#pragma region TFT Methods


int printDirectory(File dir, int numTabs) {
	int fileCountLocal = 0;
	while (true) {
		File entry = dir.openNextFile();
		if (!entry) {
			// no more files
			//break;
			return fileCountLocal;
		}
		for (uint8_t i = 0; i < numTabs; i++) {
			Serial.print('\t');
		}

		Serial.print(entry.name());
		if (entry.isDirectory()) {
			//Serial.println("/");
			//printDirectory(entry, numTabs + 1);
		}
		else {
			fileCountLocal++;
			// files have sizes, directories do not
			Serial.print("\t\t");
			Serial.println(entry.size(), DEC);
		}
		entry.close();
	}
}

void loadRandomImg()
{
	tft.setRotation(1);
	tft.fillScreen(ST77XX_WHITE);
	randNumber = random(1, sdCardImageCount);
	Serial.print("random number: ");
	Serial.println(randNumber);
	Serial.print("Loading crusherbmp/");
	Serial.print(randNumber);
	Serial.println(".bmp to screen...");
	//Serial.println();
	if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
		return;
	}
	String filename = String("crusherbmp/" + String(randNumber) + ".bmp");
	char* chrFilename = strdup(filename.c_str());
	ImageReturnCode ret = reader.drawBMP(chrFilename, tft, 0, 0);
	reader.printStatus(ret);   // How'd we do?
	free(chrFilename);

}

void centerTftText(const String& buf, int x, int y)
{
	int16_t x1, y1;
	uint16_t w, h;
	tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
	tft.setCursor(x - w / 2, y);
	tft.print(buf);
}


void outputStats() {
	if (!isDebug)
	{
		if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
			return;
		}
	}
	tft.fillScreen(ST77XX_BLACK);
	tft.setTextWrap(false);
	tft.setRotation(1);

	int crushCountSize = 6; //text size
	tft.setTextSize(crushCountSize); //each size counts at 10 units in size (5 = 50)
	int crushCountUnitSize = crushCountSize * 10 / 2;
	//tft.setCursor((tft.width() / 2) - (String(crushCount).length() / 2) , tft.height()/2 - crushCountUnitSize); // display will be halfway down screen
	//tft.setCursor(tft.width() / 2 - crushCountUnitSize , tft.height()/2 ); // display will be halfway down screen - looks nice
	//tft.setCursor(tft.width() / 2 - crushCountUnitSize , 10 ); // display will be halfway down screen

	tft.setTextColor(ST77XX_RED, ST77XX_BLACK); // red on black
	centerTftText(String(crushCount), tft.width() / 2, 10); //WORKING - SHOWS TEXT ALONG TOP OF DISPLAY
	//tft.println(crushCount);
	int scrollingTextSize = 5;
	tft.setTextSize(scrollingTextSize); //each size counts at 10 units in size (5 = 50)
	//
	String text = "    CANS CRUSHED "; // sample text

	const int width = 5; // width of the marquee display (in characters)

	// Loop once through the string

	for (int offset = 0; offset < text.length(); offset++)

	{

		// Construct the string to display for this iteration
		// tft.fillScreen(ST77XX_BLACK);
		String t = "";

		for (int i = 0; i < width; i++)

			t += text.charAt((offset + i) % text.length());

		// Print the string for this iteration

		//tft.setCursor(0, tft.height()/2-10); // display will be halfway down screen
		//tft.setCursor(0, 10 );  //looks nice
		tft.setCursor(0, tft.height() / 2);

		tft.println(t);

		// Short delay so the text doesn't move too fast
		if (!isDebug)
		{
			if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
				return;
			}
		}
		delay(200);

	}

}

void prepareStatusScreen()
{
	tft.fillScreen(ST77XX_BLACK);
	tft.setTextWrap(false);
	tft.setRotation(1);
}
int getTextWidthBoundry(const String& buf, int previousTextSize, int textSize)
{


	tft.setTextSize(textSize);

	int x = tft.width() / 2;
	int y = tft.height() / 2;
	int16_t x1, y1;
	uint16_t w, h;
	tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string

	tft.setTextSize(previousTextSize);

	return w;
}

void centerTftTextWithDegrees(const String& buf, int textSize, int degreeSymbolTextSize)
{
	int x = tft.width() / 2;
	int y = tft.height() / 2;
	int16_t x1, y1;
	uint16_t w, h;
	tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
	String degreeSymbol = String((char)247);
	uint16_t degreeBoundary = getTextWidthBoundry(String(degreeSymbol), textSize, degreeSymbolTextSize);
	tft.setCursor(x - (w + degreeBoundary) / 2, y);
	tft.print(buf);
	tft.setTextSize(degreeSymbolTextSize);
	tft.print(degreeSymbol);
	tft.setTextSize(textSize);
}
void showScrollingText(String text)
{
	const int scrollingTextSize = 5;
	tft.setTextSize(scrollingTextSize);
	const int width = 5; // width of the marquee display (in characters)

	// Loop once through the string

	for (int offset = 0; offset < text.length(); offset++)

	{

		String t = "";

		for (int i = 0; i < width; i++)

			t += text.charAt((offset + i) % text.length());

		// Print the string for this iteration

		//tft.setCursor(0, tft.height()/2-10); // display will be halfway down screen
		tft.setCursor(0, 10);  //looks nice

		tft.println(t);

		// TEMPORARILY REMOVING TO TEST TFT SHIT
	//    if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
	//      return;
	//    }

		delay(200);

	}
}


void newShowTemperature()
{
	//160px height/128px wide
  // TEMPORARILY REMOVING TO TEST TFT SHIT
  //    if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
  //      return;
  //    }

	delay(500);

	prepareStatusScreen();
	// TEMPORARILY REMOVING TO TEST TFT SHIT
	//    if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) {
	//      return;
	//    }
	  //Serial.print(int(tempSensors.toFahrenheit(tempSensors.getTempCByIndex(0))));




	const int temperatureTextSize = 6; //text size
	tft.setTextSize(temperatureTextSize); //each size counts at 10 units in size (5 = 50)

	tft.setTextColor(ST77XX_RED, ST77XX_BLACK); // red on black

	const int degreeSymbolTextSize = 3;



	centerTftTextWithDegrees(String(currentTemperatureF), temperatureTextSize, degreeSymbolTextSize);
	String text = "    CURRENT TEMPERATURE "; // sample text
	showScrollingText(text);



	tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK); // red on black
	prepareStatusScreen();
	centerTftTextWithDegrees(String(heatIndexF), temperatureTextSize, degreeSymbolTextSize);
	text = "    HEAT INDEX (FEELS LIKE) "; // sample text
	showScrollingText(text);
	prepareStatusScreen();
	//  centerTftText(String(currentTemperatureF), tft.width() / 2, tft.height() / 2);
	//  secondaryTextSize = 3; //text size
	//  tft.setTextSize(secondaryTextSize); //each size counts at 10 units in size (5 = 50)
	//  tft.print((char)247);
	//  tft.setTextSize(crushCountSize);
	//  text = "    CURRENT TEMPERATURE "; // sample text
	//  showScrollingText(text);


	  //tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
	  //prepareStatusScreen();
	  //text = "    CURRENT TEMPERATURE CELCIUS "; // sample text
	  //centerTftText(String(String(currentTemperatureC) + (char)247), tft.width() / 2, tft.height() / 2);
	  //showScrollingText(text);
	tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK); // red on black
	prepareStatusScreen();
	text = "    CURRENT HUMIDITY "; // sample text
	centerTftText(String(String(currentHumidity) + "%"), tft.width() / 2, tft.height() / 2);
	showScrollingText(text);
}
#pragma endregion

#pragma region Ejector Methods

void eject() {
	digitalWrite(enableStepperPinEject, LOW); //Enable Stepper
	digitalWrite(dirPinEject, LOW); // Enables the motor to move in a particular direction
	digitalWrite(enableStepperPinEject, LOW); //Enable Stepper
	delay(1000);
	for (int x = 0; x < numEjectStepsEject; x++) {
		digitalWrite(stepPinEject, HIGH);
		delayMicroseconds(microsBetweenStepsEject);
		digitalWrite(stepPinEject, LOW);
		delayMicroseconds(microsBetweenStepsEject);

	}
	delay(1000);
	digitalWrite(dirPinEject, HIGH); //Changes the rotations direction
	for (int x = 0; x < numEjectStepsEject; x++) {
		digitalWrite(stepPinEject, HIGH);
		delayMicroseconds(microsBetweenStepsEject);
		digitalWrite(stepPinEject, LOW);
		delayMicroseconds(microsBetweenStepsEject);
	}

	digitalWrite(enableStepperPinEject, HIGH); //Disnable Stepper
}
#pragma endregion


#pragma region Main Crusher Methods







bool hasEndstopReached(int endstopPin)
{
	if (digitalRead(endstopPin) == LOW) //endstop switch has been reached
	{
		Serial.print("endstop switch has been reached!");
		Serial.println();
		return true;
	}
	else
	{

		return false;
	}

}

void moveStepperUNSAFE(int stepsToMove, int microsBetweenEachStep, int directionOfSteps) //directionOfSteps: 1 = HIGH, 0 = LOW (retract)
{

	if (directionOfSteps == 0) //retract
	{
		digitalWrite(dirPin, LOW);
	}
	else //crush
	{
		digitalWrite(dirPin, HIGH);
	}
	for (int x = 0; x < stepsToMove; x++) {

		//if (!hasEndstopReached(endStopPinForSteps))
		//if (!hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		//{
		digitalWrite(stepPin, HIGH);
		//digitalWrite(stepPin,LOW);
		delayMicroseconds(microsBetweenEachStep);
		//}
		//else {break;} //endstop has been reached

		//if (!hasEndstopReached(endStopPinForSteps))
		//if (!hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		//{
		digitalWrite(stepPin, LOW);
		delayMicroseconds(microsBetweenEachStep);
		//}
		//else {break;} //endstop has been reached

	}
}

bool hasEndstopReachedWithBuffer(int endstopPin, int currentPosition, int currentStepDirection)
{
	if (hasCrushed)
	{
		if (currentStepDirection == 1) //means we're trying to crush again
		{
			Serial.println("has crushed is true and step direction is 1");
			return true;
		}

	}

	if (hasCrushed == false && currentStepDirection == 0) //means we're trying to eject again
	{

		if (!hasRetracted)
		{
			Serial.println("has crushed is false and step direction is 0");
			return true;
		}

	}

	//  if (currentPosition < 500)
	//  {
	//    return false;
	//
	//  }
	//  else if (digitalRead(endstopPin) == LOW) //endstop switch has been reached
	//if (digitalRead(endstopPin) == LOW) //endstop switch has been reached
	if (digitalRead(endstopPin) == HIGH) //endstop has been reached
	{
		Serial.print("endstop switch has been reached!");
		Serial.println();
		return true;
	}
	else
	{

		return false;
	}

}


void endStopBackOff(int dir)
{
	moveStepperUNSAFE(endStopBackOffSteps, microsBetweenStepsEndStopBackOff, dir);
}

void moveStepper(int stepsToMove, int microsBetweenEachStep, int endStopPinForSteps, int directionOfSteps) //directionOfSteps: 1 = HIGH, 0 = LOW (retract)
{
	int backOffDirection = 0;
	if (directionOfSteps == 0) //retract
	{
		digitalWrite(dirPin, LOW);
		backOffDirection = 1;
	}
	else //crush
	{
		digitalWrite(dirPin, HIGH);
	}
	for (int x = 0; x < stepsToMove; x++) {

		//if (!hasEndstopReached(endStopPinForSteps))
		if (!hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		{
			digitalWrite(stepPin, HIGH);
			//digitalWrite(stepPin,LOW);
			delayMicroseconds(microsBetweenEachStep);
		}
		else
		{
			endStopBackOff(backOffDirection);
			break;
		} //endstop has been reached

		//if (!hasEndstopReached(endStopPinForSteps))
		if (!hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		{
			digitalWrite(stepPin, LOW);
			delayMicroseconds(microsBetweenEachStep);
		}
		else
		{
			endStopBackOff(backOffDirection);
			break;
		} //endstop has been reached

	}
}



bool moveStepperTrueIfSuccess(int stepsToMove, int microsBetweenEachStep, int endStopPinForSteps, int directionOfSteps) //directionOfSteps: 1 = HIGH, 0 = LOW (retract)
{
	int backOffDirection = 0;
	if (directionOfSteps == 0) //retract
	{
		digitalWrite(dirPin, LOW);
		backOffDirection = 1;
	}
	else //crush
	{
		digitalWrite(dirPin, HIGH);
	}
	for (int x = 0; x < stepsToMove; x++) {

		//if (!hasEndstopReached(endStopPinForSteps))
		if (ignoreEndstops || !hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		{
			digitalWrite(stepPin, HIGH);
			//digitalWrite(stepPin,LOW);
			delayMicroseconds(microsBetweenEachStep);
		}
		else
		{
			endStopBackOff(backOffDirection);
			//break;
			return false;
		} //endstop has been reached

		//if (!hasEndstopReached(endStopPinForSteps))
		if (ignoreEndstops || !hasEndstopReachedWithBuffer(endStopPinForSteps, x, directionOfSteps))
		{
			digitalWrite(stepPin, LOW);
			delayMicroseconds(microsBetweenEachStep);
		}
		else
		{
			endStopBackOff(backOffDirection);
			return false;
			//break;
		} //endstop has been reached

	}
	return true;
}
bool crushTrueIfSuccess(int numCrushSteps, int microsBetweenStepsCrushTier) {

	Serial.print("ayyyyyyyyy crushing");
	Serial.println();
	bool success = moveStepperTrueIfSuccess(numCrushSteps, microsBetweenStepsCrushTier, retractEndstopPin, 1);
	if (numCrushSteps == numCrushStepsTier2)
	{
		if (hasCrushed == false) //allow two iterations in crush direction
		{
			//crushCount++;
			//writeIntIntoEEPROM(0, crushCount);
			//hasCrushed = true;

		}

	}
	return success;
}

void crush(int numCrushSteps, int microsBetweenStepsCrushTier) {

	Serial.print("ayyyyyyyyy crushing");
	Serial.println();
	moveStepper(numCrushSteps, microsBetweenStepsCrushTier, retractEndstopPin, 1);
	if (numCrushSteps == numCrushStepsTier2)
	{
		if (hasCrushed == false) //allow two iterations in crush direction
		{
			crushCount++;
			writeIntIntoEEPROM(0, crushCount);
			hasCrushed = true;

		}

	}
}








void retract() {

	Serial.print("ayyyyyyyyy retracting");
	Serial.println();

	moveStepper(numEjectSteps, microsBetweenStepsRetract, retractEndstopPin, 0);
	if (hasCrushed == true)
	{
		hasCrushed = false;
	}
	//moveStepper(numEjectSteps, microsBetweenStepsRetract,retractEndstopPin, 0);
	hasRetracted = true;
	Serial.print("crush count: ");
	Serial.println(crushCount);
}





void retractAndEject() {
	if (hasCrushed == true)
	{
		hasCrushed = false;
	}
	Serial.print("ayyyyyyyyy retracting");
	Serial.println();
	//digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction
	digitalWrite(dirPin, LOW); // Enables the motor to move in a particular direction
	//digitalWrite(enableStepperPin,LOW); //Enable Stepper



	//ejector

	digitalWrite(dirPinEject, LOW); // Enables the motor to move in a particular direction
	digitalWrite(enableStepperPinEject, LOW); //Enable Stepper
	delay(1000);
	int loopCount = 0;
	int ejectAfterRetractingSteps = 1000;
	int stopEjectPosition = numEjectStepsEject + ejectAfterRetractingSteps;
	int ejectAfterRetractingReverseSteps = (ejectAfterRetractingSteps * 3);
	int stopEjectAfterRetractingReverseSteps = numEjectStepsEject + (ejectAfterRetractingSteps * 3);
	for (int x = 0; x < numEjectSteps; x++) {

		digitalWrite(stepPin, HIGH);
		delayMicroseconds(microsBetweenStepsRetract);
		digitalWrite(stepPin, LOW);
		delayMicroseconds(microsBetweenStepsRetract);

		loopCount++;
		if (loopCount > ejectAfterRetractingSteps && loopCount < stopEjectPosition)
		{
			digitalWrite(stepPinEject, HIGH);
			delayMicroseconds(microsBetweenStepsEject);
			digitalWrite(stepPinEject, LOW);
			delayMicroseconds(microsBetweenStepsEject);
		}
		else if (loopCount == stopEjectPosition)
		{
			digitalWrite(dirPinEject, HIGH); //Changes the rotations direction
		}
		else if (loopCount > ejectAfterRetractingReverseSteps && loopCount < stopEjectAfterRetractingReverseSteps)
		{
			digitalWrite(stepPinEject, HIGH);
			delayMicroseconds(microsBetweenStepsEject);
			digitalWrite(stepPinEject, LOW);
			delayMicroseconds(microsBetweenStepsEject);
		}
		else if (loopCount == stopEjectAfterRetractingReverseSteps)
		{
			digitalWrite(enableStepperPinEject, HIGH); //Disable ejector Stepper
		}



	}

}






void retractOLD() {
	if (hasCrushed == true)
	{
		hasCrushed = false;
	}
	Serial.print("ayyyyyyyyy retracting");
	Serial.println();
	//digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction
	digitalWrite(dirPin, LOW); // Enables the motor to move in a particular direction
	//digitalWrite(enableStepperPin,LOW); //Enable Stepper

	delay(1000);
	for (int x = 0; x < numEjectSteps; x++) {

		digitalWrite(stepPin, HIGH);
		delayMicroseconds(microsBetweenStepsRetract);
		digitalWrite(stepPin, LOW);
		delayMicroseconds(microsBetweenStepsRetract);

	}
}



//void crushOLD(int numCrushSteps, int microsBetweenStepsCrushTier) {
//  if (hasCrushed == false)
//  {
//    crushCount++;
//    writeIntIntoEEPROM(0, crushCount);
//    hasCrushed = true;
//  }
//    Serial.print("ayyyyyyyyy crushing");
//    Serial.println();
//  //digitalWrite(dirPin,LOW); // Enables the motor to move in a particular direction
//  digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction
//
//  //digitalWrite(enableStepperPin,LOW); //Enable Stepper
//
//  //delay(200);
//    //for(int x = 0; x < numEjectSteps; x++) {
//    for(int x = 0; x < numCrushSteps; x++) {
//    if (digitalRead(retractEndstopPin) == LOW) //endstop switch has been reached
//    {
//          Serial.print("retract switch (5) has been reached!");
//            Serial.println();
//          break;
//    }
//    digitalWrite(stepPin,HIGH);
//    delayMicroseconds(microsBetweenStepsCrushTier);
//    digitalWrite(stepPin,LOW);
//    delayMicroseconds(microsBetweenStepsCrushTier);
//
//  }
//
//}
#pragma endregion




#pragma region EEPROM Methods
void writeIntIntoEEPROM(int address, int number)
{
	EEPROM.write(address, number >> 8);
	EEPROM.write(address + 1, number & 0xFF);
}
int readIntFromEEPROM(int address)
{
	return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}

#pragma endregion





void setup(void) {
	ImageReturnCode stat; // Status from image-reading functions
	Serial.begin(9600);
#if !defined(ESP32)
	while (!Serial);      // Wait for Serial Monitor before continuing
#endif
	Serial.print(F("Hello! ST77xx TFT Test"));

	// Use this initializer if using a 1.8" TFT screen:
	tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab


	Serial.println(F("Initialized"));

	// The Adafruit_ImageReader constructor call (above, before setup())
	// accepts an uninitialized SdFat or FatFileSystem object. This MUST
	// BE INITIALIZED before using any of the image reader functions!
	Serial.print(F("Initializing filesystem..."));
#if defined(USE_SD_CARD)
	// SD card is pretty straightforward, a single call...
	if (!SD.begin(SD_CS, SD_SCK_MHZ(10))) { // Breakouts require 10 MHz limit due to longer wires
		Serial.println(F("SD begin() failed"));
		for (;;); // Fatal error, do not continue
	}
#else
	// SPI or QSPI flash requires two steps, one to access the bare flash
	// memory itself, then the second to access the filesystem within...
	if (!flash.begin()) {
		Serial.println(F("flash begin() failed"));
		for (;;);
	}
	if (!filesys.begin(&flash)) {
		Serial.println(F("filesys begin() failed"));
		for (;;);
	}
#endif
	Serial.println(F("OK!"));

	uint16_t time = millis();
	tft.fillScreen(ST77XX_BLACK);
	time = millis() - time;


	// Load full-screen BMP file 'parrot.bmp' at position (0,0) (top left).
	// Notice the 'reader' object performs this, with 'tft' as an argument.
  //  Serial.print(F("Loading 4loko.bmp to screen..."));
  //  stat = reader.drawBMP("/4loko.bmp", tft, 0, 0);
  //  reader.printStatus(stat);   // How'd we do?
  //  fourFuckingLOKO();
	// Query the dimensions of image 'miniwoof.bmp' WITHOUT loading to screen:
	Serial.print(F("Querying 4loko.bmp image size..."));
	stat = reader.bmpDimensions("/4loko.bmp", &width, &height);
	reader.printStatus(stat);   // How'd we do?
	if (stat == IMAGE_SUCCESS) { // If it worked, print image size...
		Serial.print(F("Image dimensions: "));
		Serial.print(width);
		Serial.write('x');
		Serial.println(height);
	}

	// Load small BMP 'wales.bmp' into a GFX canvas in RAM. This should fail
	// gracefully on Arduino Uno and other small devices, meaning the image
	// will not load, but this won't make the program stop or crash, it just
	// continues on without it. Should work on Arduino Mega, Zero, etc.
	//Serial.print(F("Loading wales.bmp to canvas..."));
	//stat = reader.loadBMP("/wales.bmp", img);
	//reader.printStatus(stat); // How'd we do?

	//delay(1000); // Pause 2 seconds before moving on to loop()



	if (firstrun == true) //FIRST RUN SETUP
	{
		Serial.println("writing crushcount value to eeprom since first run");
		writeIntIntoEEPROM(0, 0);
	}
	else
	{
		Serial.println("reading crushcount value from eeprom since not first run");
		crushCount = readIntFromEEPROM(0);
	}



	//JUST FOR TESTING THE NEW MEGA BOARD - DELETE AFTERWARDS!
  //  crushCount++;
  //  writeIntIntoEEPROM(0, crushCount);


	//GET COUNT OF FILES ON SD CARD
	File root = SD.open("/crusherbmp");  // open SD card main root
	sdCardImageCount = printDirectory(root, 0);   // print all files names and sizes
	root.close();              // close the opened root
	Serial.print(sdCardImageCount);
	Serial.println(" images found on sd card");

	// temp sensor
	//tempSensors.begin();
	dht.begin();

	//seed using A0 to get random number for sd card images
	randomSeed(analogRead(0));


	//init ultrasonic sensor pins
	pinMode(trigPin, OUTPUT);
	pinMode(echoPin, INPUT);

	//MAIN STEPPER/CRUSHER
	pinMode(stepPin, OUTPUT);
	pinMode(dirPin, OUTPUT);
	pinMode(enableStepperPin, OUTPUT);

	digitalWrite(enableStepperPin, HIGH); //Disnable Stepper
  //  digitalWrite(enableStepperPin,LOW); //ENABLE Stepper
	pinMode(fwdBtnPin, INPUT_PULLUP); //try if input doesnt work
	digitalWrite(fwdBtnPin, HIGH);
	pinMode(revBtnPin, INPUT);
	digitalWrite(revBtnPin, HIGH);

	pinMode(retractEndstopPin, INPUT_PULLUP); //try if input doesnt work
	pinMode(crushEndstopPin, INPUT_PULLUP); //try if input doesnt work
	//pinMode(retractEndstopPin, INPUT); //try if input doesnt work
	digitalWrite(retractEndstopPin, HIGH);
	digitalWrite(crushEndstopPin, HIGH);



	//init ejector stepper 
	pinMode(stepPinEject, OUTPUT);
	pinMode(dirPinEject, OUTPUT);
	pinMode(enableStepperPinEject, OUTPUT);

	digitalWrite(enableStepperPinEject, HIGH); //Disnable Stepper
}

void loop() {

	if (isDebug)
	{
		checkCurrentTemps();
		newShowTemperature();
		outputStats();
	}
	else
	{
		checkEndstops();
		//distanceToCrusherBaseCentimeters = getDistanceToCrusherBaseCentimeters(); //GLOBAL VAR FOR DETERMINING IF WE NEED TO CRUSH SHIT
		if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) { goto crush_that_shit; }
		engageCrushActions(); //TEMP - REMOVE AFTER TESTING

		//  Serial.print(distanceToCrusherBaseCentimeters);
		//  Serial.println("cm - ultrasonic sensor");
		loadRandomImg();
		engageCrushActions(); //TEMP - REMOVE AFTER TESTING
		if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) { goto crush_that_shit; }

		outputStats();
		engageCrushActions(); //TEMP - REMOVE AFTER TESTING
		if (getDistanceToCrusherBaseCentimeters() < minimumDistanceToEngageCrusher) { goto crush_that_shit; }
		//delay(1000);
		checkCurrentTemps();
		newShowTemperature();


	crush_that_shit: //label for goto statement
		if (distanceToCrusherBaseCentimeters < minimumDistanceToEngageCrusher)
		{
			fourFuckingLOKO();
			eject();
		}
	}

}

void engageCrushActionsManual()
{
	//OVERRIDE FOR MANUAL ENGAGING
	if (digitalRead(revBtnPin) == LOW)
	{
		//digitalWrite(enableStepperPin,LOW); //ENABLE Stepper
		//delay(1000);
		Serial.print(revBtnPin);
		Serial.println(" is LOW - RETRACTING");
		retract();
		//digitalWrite(enableStepperPin,HIGH);
		distanceToCrusherBaseCentimeters = 1;
	}
	if (digitalRead(fwdBtnPin) == LOW)
	{
		bool success = false;
		Serial.print(fwdBtnPin);
		Serial.println(" is LOW - CRUSHING");
		if (isTallBoyCan)
		{
			crush(numCrushStepsTallBoyTier1, microsBetweenStepsCrushTier1);
			crush(numCrushStepsTallBoyTier2, microsBetweenStepsCrushTier2);
		}
		else
		{

			success = crushTrueIfSuccess(numCrushStepsTier0, microsBetweenStepsCrushTier0);
			if (success) { success = crushTrueIfSuccess(numCrushStepsTier1, microsBetweenStepsCrushTier1); }
			if (success) { success = crushTrueIfSuccess(numCrushStepsTier2, microsBetweenStepsCrushTier2); }
		}

		//retractOLD();
		retract();
		distanceToCrusherBaseCentimeters = 1;
	}
}


void engageCrushActions()
{
	//OVERRIDE FOR MANUAL ENGAGING
	if (digitalRead(revBtnPin) == LOW)
	{
		//digitalWrite(enableStepperPin,LOW); //ENABLE Stepper
		//delay(1000);
		Serial.print(revBtnPin);
		Serial.println(" is LOW - RETRACTING");
		retract();
		//digitalWrite(enableStepperPin,HIGH);
		distanceToCrusherBaseCentimeters = 1;
	}
	if (digitalRead(fwdBtnPin) == LOW)
	{
		bool success = false;
		Serial.print(fwdBtnPin);
		Serial.println(" is LOW - CRUSHING");
		if (isTallBoyCan)
		{
			crush(numCrushStepsTallBoyTier1, microsBetweenStepsCrushTier1);
			crush(numCrushStepsTallBoyTier2, microsBetweenStepsCrushTier2);
		}
		else
		{

			success = crushTrueIfSuccess(numCrushStepsTier0, microsBetweenStepsCrushTier0);
			if (success) { success = crushTrueIfSuccess(numCrushStepsTier1, microsBetweenStepsCrushTier1); }
			if (success) { success = crushTrueIfSuccess(numCrushStepsTier2, microsBetweenStepsCrushTier2); }
		}

		//retractOLD();
		retract();
		distanceToCrusherBaseCentimeters = 1;
	}
}
void checkEndstops()
{

	Serial.print("retract endstop pin is: ");
	Serial.println(digitalRead(retractEndstopPin));
	Serial.print("crush endstop pin is: ");
	Serial.println(digitalRead(crushEndstopPin));
}

void fourFuckingLOKO()
{

	Serial.println("FOUR FUCKING LOKO");
	ImageReturnCode stat; // Status from image-reading functions
	Serial.print(F("Loading 4loko.bmp to screen..."));
	stat = reader.drawBMP("/4loko.bmp", tft, 0, 0);
	reader.printStatus(stat);   // How'd we do?
	isTallBoyCan = false;
	bool success = false;
	if (isTallBoyCan)
	{
		crush(numCrushStepsTallBoyTier1, microsBetweenStepsCrushTier1);
		crush(numCrushStepsTallBoyTier2, microsBetweenStepsCrushTier2);
	}
	else
	{

		success = crushTrueIfSuccess(numCrushStepsTier0, microsBetweenStepsCrushTier0);
		if (success) { success = crushTrueIfSuccess(numCrushStepsTier1, microsBetweenStepsCrushTier1); }
		if (success) { success = crushTrueIfSuccess(numCrushStepsTier2, microsBetweenStepsCrushTier2); }
	}

	retractOLD();
	//retractAndEject();
	crushCount++;
	writeIntIntoEEPROM(0, crushCount);
}