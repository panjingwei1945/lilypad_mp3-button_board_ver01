// "Trigger" example sketch for Lilypad MP3 Player
// Mike Grusin, SparkFun Electronics
// http://www.sparkfun.com

// This sketch (which is preloaded onto the board by default),
// will play a specific audio file when one of the five trigger
// inputs (labeled T1 - T5) is momentarily grounded.

// You can place up to five audio files on the micro-SD card.
// These files should have the desired trigger number (1 to 5)
// as the first character in the filename. The rest of the
// filename can be anything you like. Long file names will work,
// but will be translated into short 8.3 names. We recommend using
// 8.3 format names without spaces, but the following characters
// are OK: .$%'-_@~`!(){}^#&. The VS1053 can play a variety of 
// audio formats, see the datasheet for information.

// By default, a new trigger will interrupt a playing file, except
// itself. (In other words, a new trigger won't restart an 
// already-playing file). You can easily change this behavior by
// modifying the global variables "interrupt" and "interruptself"
// below.

// This sketch can output serial debugging information if desired
// by changing the global variable "debugging" to true. Note that
// this will take away trigger inputs 4 and 5, which are shared 
// with the TX and RX lines. You can keep these lines connected to
// trigger switches and use the serial port as long as the triggers
// are normally open (not grounded) and remain ungrounded while the
// serial port is in use.

// Uses the SdFat library by William Greiman, which is supplied
// with this archive, or download from http://code.google.com/p/sdfatlib/

// Uses the SFEMP3Shield library by Bill Porter, which is supplied
// with this archive, or download from http://www.billporter.info/

// License:
// We use the "beerware" license for our firmware. You can do
// ANYTHING you want with this code. If you like it, and we meet
// someday, you can, but are under no obligation to, buy me a
// (root) beer in return.

// Have fun! 
// -your friends at SparkFun

// Revision history:
// 1.0 initial release MDG 2012/11/01


// We'll need a few libraries to access all this hardware!

#include <SPI.h>            // To talk to the SD card and MP3 chip
#include <SdFat.h>          // SD card file system
#include <SFEMP3Shield.h>   // MP3 decoder chip
#include <RCM.h>
#include <Wire.h>
#include <EEPROM.h>

//#define __MP3_DEBUGGING //Comment this line when release the programe.


#define vol_addr 0
#define vol_step 10
unsigned char vol = 0;
#define MAX_MP3_FILE 5
#define sync_error  2
#define max_push 50 
#define MP3_FREE 0
#define MP3_PLAYING 1
#define FIRMWARE_VER "firmware VER1.2_20151113"
const unsigned int SerialNum = 2;
unsigned long push_index = 0;
unsigned long push_count = 0;
unsigned short push_millis[max_push] = {};
unsigned char MP3_status = MP3_FREE;
unsigned char t = 0;              // current trigger

String temp_string;

unsigned char task_mode;

struct
{
	unsigned char year;
	unsigned char month;
	unsigned char date;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned long temp_sys_sync_ms;
}sync_time;

// Constants for the trigger input pins, which we'll place
// in an array for convenience:

const int button = A0;
const int vol_plus = A3;
const int vol_minus = 3;

int trigger[3] = { button,vol_plus,vol_minus};

// And a few outputs we'll be using:


const int EN_GPIO1 = A2; // Amp enable + MIDI/MP3 mode select
const int SD_CS = 9;     // Chip Select for SD card


const int led_r = 10;
const int led_g = A1;
const int led_b = 5;

						 // Create library objects:

SFEMP3Shield MP3player;
SdFat sd;


// Set interrupt = false if you would like a triggered file to
// play all the way to the end. If this is set to true, new
// triggers will stop the playing file and start a new one.

boolean interrupt = false;


// We'll store the five filenames as arrays of characters.
// "Short" (8.3) filenames are used, followed by a null character.

char filename[MAX_MP3_FILE][13];
unsigned int mp3_num = 0;

SdFile logFile;
const char logFileName[] = "log.txt";



void setup()
{
	int x, index;
	SdFile file;
	byte result;
	char tempfilename[13];

	// Set the five trigger pins as inputs, and turn on the 
	// internal pullup resistors:

	time_sync();

	for (x = 0; x <= 4; x++)
	{
		pinMode(trigger[x], INPUT);
		digitalWrite(trigger[x], HIGH);
	}



	pinMode(led_r, OUTPUT);
	pinMode(led_g, OUTPUT);
	pinMode(led_b, OUTPUT);
	digitalWrite(led_r, HIGH);
	digitalWrite(led_g, HIGH);
	digitalWrite(led_b, HIGH);   // HIGH = off



	pinMode(EN_GPIO1, OUTPUT);
	digitalWrite(EN_GPIO1, LOW);  // MP3 mode / amp off




#ifdef __MP3_DEBUGGING
		Serial.begin(9600);
		Serial.println(F("Lilypad MP3 Player trigger sketch"));
#endif

	// Initialize the SD card; SS = pin 9, half speed at first

#ifdef __MP3_DEBUGGING
		Serial.print(F("initialize SD card... "));
#endif

	result = sd.begin(SD_CS, SPI_HALF_SPEED); // 1 for success

	if (result != 1) // Problem initializing the SD card
	{
#ifdef __MP3_DEBUGGING
		Serial.print(F("error, halting"));
#endif
		errorBlink(1); // Halt forever, blink LED if present.
	}
	else
#ifdef __MP3_DEBUGGING
		Serial.println(F("success!"));
#endif

	// Start up the MP3 library


#ifdef __MP3_DEBUGGING
		Serial.print(F("initialize MP3 chip... "));
#endif

	result = MP3player.begin(); // 0 or 6 for success

								// Check the result, see the library readme for error codes.

	if ((result != 0) && (result != 6)) // Problem starting up
	{
#ifdef __MP3_DEBUGGING
			Serial.print(F("error code "));
			Serial.print(result);
			Serial.print(F(", halting."));
#endif

		errorBlink(result); // Halt forever, blink red LED if present.
	}
	else
#ifdef __MP3_DEBUGGING
		Serial.println(F("success!"));
#endif


#ifdef __MP3_DEBUGGING
	Serial.println(F("reading root directory"));
#endif

	// Start at the first file in root and step through all of them:

	sd.chdir("/", true);


/**************system setup*****************/
/**************system setup*****************/

	randomSeed(RCM.minute() * 60 + RCM.second());

	while (file.openNext(sd.vwd(), O_READ))
	{
		// get filename

		delay(100);
		file.getFilename(tempfilename);

		// Does the filename start with char '1' through '5'?      

		if (tempfilename[0] >= '1' && tempfilename[0] <= '0' + MAX_MP3_FILE)
		{
			// Yes! subtract char '1' to get an index of 0 through 4.

			index = tempfilename[0] - '1';

			// Copy the data to our filename array.

			strcpy(filename[index], tempfilename);

			mp3_num++;
			if (mp3_num >= MAX_MP3_FILE)
			{
				break; //The number of music file must no more than 10. 
			}
		}
		file.close();
	}

#ifdef __MP3_DEBUGGING
	Serial.println(F("done reading root directory"));
#endif


		
		if (!logFile.open(logFileName, O_RDWR | O_CREAT | O_AT_END)) {
				sd.errorHalt("opening log.txt for write failed");
			}


	temp_string = FIRMWARE_VER;
#ifdef __MP3_DEBUGGING
	report_log(temp_string);
#endif
	write_log(temp_string);
	log_sync();

	temp_string = String("Device_Serail_number: ") + SerialNum;
#ifdef __MP3_DEBUGGING
	report_log(temp_string);
#endif
	write_log(temp_string);
	log_sync();

	temp_string = "Power on";
#ifdef __MP3_DEBUGGING
	report_log(temp_string);
#endif
	write_log(temp_string);
	log_sync();

	temp_string = String("Find ") + mp3_num + "audio files:";
#ifdef __MP3_DEBUGGING
	report_log(temp_string);
#endif
	write_log(temp_string);
	log_sync();


		for (x = 0; x < mp3_num; x++)
		{
			temp_string = String(filename[x]);
#ifdef __MP3_DEBUGGING
			report_log(temp_string);
#endif
			write_log(temp_string);
			log_sync();
		}

	// Set the VS1053 volume. 0 is loudest, 255 is lowest (off):
	vol = EEPROM.read(vol_addr);
	MP3player.setVolume(vol, vol);


#ifdef __MP3_DEBUGGING
	Serial.println(String("Set volume to ") + String(vol));
#endif

	// Turn on the amplifier chip:

	digitalWrite(EN_GPIO1, HIGH);
	delay(2);
}


void loop()
{
	int x;
	byte result;
	if (digitalRead(vol_plus) == LOW)
	{
		x = 0;
		digitalWrite(led_r, LOW);
		while (x < 50)
		{
			if (digitalRead(vol_plus) == HIGH)
				x++;
			else
				x = 0;
			delay(1);
		}
		digitalWrite(led_r, HIGH);
		vol = (vol > vol_step) ? (vol - vol_step) : 0;
		EEPROM.write(vol_addr, vol);
		MP3player.setVolume(vol, vol);

		temp_string = String("Set volume to ") + vol + " (vol+)";
#ifdef __MP3_DEBUGGING
		report_log(temp_string); 
#endif
	}


	if (digitalRead(vol_minus) == LOW)
	{
		x = 0;
		digitalWrite(led_b, LOW);
		while (x < 50)
		{
			if (digitalRead(vol_minus) == HIGH)
				x++;
			else
				x = 0;
			delay(1);
		}
		digitalWrite(led_b, HIGH);
		
		vol = ((vol + vol_step) < 255) ? (vol + vol_step) : 255;
		EEPROM.write(vol_addr,vol);
		MP3player.setVolume(vol, vol);
#ifdef __MP3_DEBUGGING
		report_log(String("Set volume to ") + vol + " (vol-)"); //cause error
#endif
	}

	if (!interrupt)
	{
		if ((MP3_status == MP3_PLAYING) && (MP3player.isPlaying() == 0))
		{
			unsigned long temp_ending_time = millis();
			delay(100);
			MP3_status = MP3_FREE;
#ifdef __MP3_DEBUGGING
			report_log( String("Play") + "	" + filename[t], 0);
#endif
			write_log( String("Play") + "	" + filename[t], 0);
			log_sync();
			for (int i = 0; i < push_index; i++)
			{
#ifdef __MP3_DEBUGGING
				report_log(String("push ") + (i+1), push_millis[i]);
#endif
				write_log(String("push ") + (i+1), push_millis[i]);
				log_sync();
			}


#ifdef __MP3_DEBUGGING
				report_log(String("Trial done "), temp_ending_time - sync_time.temp_sys_sync_ms);
#endif
				write_log(String("Trial done "), temp_ending_time - sync_time.temp_sys_sync_ms);
				log_sync();
				

			if (push_count > max_push)
			{
#ifdef __MP3_DEBUGGING
				report_log(String("total push num is ") + push_count);
#endif
				write_log(String("total push num is ") + push_count );
				log_sync();
			}
			else
			{
#ifdef __MP3_DEBUGGING
				report_log(String("total push num is ") + push_count);
#endif
				write_log(String("total push num is ") + push_count);
				log_sync();
			}


			push_count = 0;
			push_index = 0;

		}
	}
		
	
	if (digitalRead(button) == LOW)
		{

			x = 0;
			digitalWrite(led_g, LOW);
			while (x < 50)
			{
				if (digitalRead(button) == HIGH)
					x++;
				else
					x = 0;
				delay(1);
			}
			digitalWrite(led_g, HIGH);



				if (interrupt)
				{
 
					push_index = 0;
					push_count = 0;

					if (MP3player.isPlaying())
					{
#ifdef __MP3_DEBUGGING
						report_log("stopping playback");
#endif
						MP3player.stopTrack();
					}

					t = random(mp3_num);
					time_sync();
					result = MP3player.playMP3(filename[t]);
#ifdef __MP3_DEBUGGING
						if (result != 0)
						{
							Serial.print(F("error "));
							Serial.print(result);
							Serial.print(F(" when trying to play track "));
						} else {
							report_log( String("Play") + "	" + filename[t]);
						}
					
#endif
	
				} 
				else 
				{

					if (MP3player.isPlaying() == 0)
					{
						t = random(mp3_num);
#ifdef __MP3_DEBUGGING
							if (result != 0)
							{
								Serial.print(F("error "));
								Serial.print(result);
								Serial.print(F(" when trying to play track "));
							}
							else
							{
								report_log( String("Play") + "	" + filename[t]);
							}
					
#endif
						push_count = 0;
						push_index = 0;
						time_sync();
						result = MP3player.playMP3(filename[t]);
						MP3_status = MP3_PLAYING;
					} 
					else 
					{
						record_push();
#ifdef __MP3_DEBUGGING
						report_log(String("push") + push_count);
#endif
					}
				}

		}

}

void time_sync(void)
{
	unsigned char temp_sec = RCM.second();
	while (RCM.second() == temp_sec);
	sync_time.temp_sys_sync_ms = millis();
	sync_time.second = RCM.second();
	sync_time.minute = RCM.minute();
	sync_time.hour = RCM.hour();
	sync_time.date = RCM.date();
	sync_time.month = RCM.month();
	sync_time.year = RCM.year();
}


void record_push()
{
	push_count++;
	if (push_index < max_push)
	{
		push_millis[push_index] = millis() - sync_time.temp_sys_sync_ms;
		push_index++;
	}
	else
	{
#ifdef __MP3_DEBUGGING
		report_log("push_count overflowing.");
#endif
	}
}

void write_log(String str) {
	String res;
	res = get_real_time() + "	" + str;
	logFile.println(res);
}

void write_log(String str, unsigned long ms) {
	String res;
	res = get_report_time(ms) + "	" + str;
	logFile.println(res);
}

void log_sync()
{
	logFile.sync();
}

void report_log(String str) {
#ifdef __MP3_DEBUGGING
		String res;
		res = get_real_time() + "	" + str;
		Serial.println(res);
#endif
}

void report_log(String str, unsigned long ms) {
#ifdef __MP3_DEBUGGING
		String res;
		res = get_report_time(ms) + "	" + str;
		Serial.println(res);
#endif
}



String get_report_time(unsigned long ms)
{
	String res;
	unsigned long res_millis = ms % 1000;
	unsigned long temp_second = (ms / 1000) + sync_time.second;
	unsigned long temp_minute = (temp_second / 60) + sync_time.minute;
	unsigned long temp_hour = (temp_minute / 60) + sync_time.hour;
	unsigned char res_second = temp_second % 60;
	unsigned char res_minute = temp_minute % 60;
	unsigned char res_hour = temp_hour % 24;
	res += "20";
	res += String(RCM.year());
	res += "/";
	res += String(RCM.month());
	res += "/";
	res += String(RCM.date());
	res += "/";
	res += String(res_hour);
	res += "/";
	res += String(res_minute);
	res += "/";
	res += String(res_second);
	res += "/";
	res += String(res_millis);
	return res;
}


String get_real_time()
{
	String res;
	unsigned long ms = millis() - sync_time.temp_sys_sync_ms;
	unsigned long res_millis = ms % 1000;
	unsigned long temp_second = (ms / 1000) + sync_time.second;
	unsigned long temp_minute = (temp_second / 60) + sync_time.minute;
	unsigned long temp_hour = (temp_minute / 60) + sync_time.hour;
	unsigned char res_second = temp_second % 60;
	unsigned char res_minute = temp_minute % 60;
	unsigned char res_hour = temp_hour % 24;
	res += "20";
	res += String(RCM.year());
	res += "/";
	res += String(RCM.month());
	res += "/";
	res += String(RCM.date());
	res += "/";
	res += String(res_hour);
	res += "/";
	res += String(res_minute);
	res += "/";
	res += String(res_second);
	res += "/";
	res += String(res_millis);
	return res;
}



void errorBlink(int blinks)
{
	// The following function will blink the red LED in the rotary
	// encoder (optional) a given number of times and repeat forever.
	// This is so you can see any startup error codes without having
	// to use the serial monitor window.

	int x;

	while (true) // Loop forever
	{
		for (x = 0; x < blinks; x++) // Blink the given number of times
		{
			digitalWrite(led_r, LOW); // Turn LED ON
			delay(250);
			digitalWrite(led_r, HIGH); // Turn LED OFF
			delay(250);
		}
		delay(1500); // Longer pause between blink-groups
	}
}
