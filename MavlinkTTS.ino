// SerialSpeak - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain January 2025

// Reads from the serial port and plays what's typed over the output
// asynchronously.  Can queue up work while still speaking.
// Demonstrates dictionary and voice usage

#include <BackgroundAudioSpeech.h>
#include <libespeak-ng/voice/en_029.h>
#include <libespeak-ng/voice/en_gb_scotland.h>
#include <libespeak-ng/voice/en_gb_x_gbclan.h>
#include <libespeak-ng/voice/en_gb_x_gbcwmd.h>
#include <libespeak-ng/voice/en_gb_x_rp.h>
#include <libespeak-ng/voice/en.h>
#include <libespeak-ng/voice/en_shaw.h>
#include <libespeak-ng/voice/en_us.h>
#include <libespeak-ng/voice/en_us_nyc.h>


#include <MAVLink.h>
#include <mavlink/common/mavlink_msg_rc_channels.h>
#include <mavlink/common/mavlink_msg_mission_current.h>
#include <mavlink/common/mavlink_msg_command_long.h>
#include <mavlink/common/mavlink_msg_ais_vessel.h>




BackgroundAudioVoice v[] = {
  voice_en_029,
  voice_en_gb_scotland,
  voice_en_gb_x_gbclan,
  voice_en_gb_x_gbcwmd,
  voice_en,
  voice_en_shaw,
  voice_en_us,
  voice_en_us_nyc
};


#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(15, 16, 7);  // BCLK, LRCLK, DOUT (,MCLK)
BackgroundAudioSpeech BMP(audio);

void usage() {
  Serial.printf("Simple Speech Example\r\n");
  Serial.printf("Enter a line with *# to change voice (i.e. *3 to go to 3rd voice in list)\r\n");
  Serial.printf("Installed voices:\r\n");
  for (int i = 0; i < (int)sizeof(v) / (int)sizeof(v[0]); i++) {
    Serial.printf("*%d: %s\r\n", i, v[i].name);
  }
  Serial.printf("!rate <rate in wpm , 175 default>\r\n");
  Serial.printf("!pitch <pitch adjustment 0-99, 50 default>\r\n");
  Serial.printf("!gap <word gap in 10-ms>\r\n");
  Serial.printf("Otherwise, any typed words will be read aloud.\r\n\r\n");
}



String mavmessage;
String mavmessageold;
uint8_t gps_Fix = 0;
int32_t gps_Lat = 0;
int32_t gps_Long = 0;
uint16_t gps_Vel = 0;
uint16_t gps_Head = 0;
uint16_t sys_Bat = 0;
uint16_t sys_Bcur = 0;
int8_t sys_Brem = 0;
int wp_number;
int wp_numberold;
int wp_total;
int mission_state;
int mission_mode;
float HDOP;
float VOLTS = 0;
float AMPS = 0;
int flightmode = 0;
long GPSLAT = 0.1234567;
long GPSLON = 0.1234567;
int satelites = 0;
int satelitesold = 0;
int comdroprate = 60;
int BASEMODE = 0;
float roll = 0;
float pitch = 0;
float yaw = 0;
float gspeed = 0;
float heading = 0;
float fcmodein;
float navbearing = 0;
float wpdist = 0;
float xtrackerror = 0;
int TIME_TO_SLEEP = 0;

void setup() {
  Serial.begin(115200);
 /*
  rate in wpm , 175 default
  pitch adjustment 0-99, 50 default
  word gap in 10-ms
*/
  BMP.setVoice(v[1]);
  //BMP.setRate(175);
 // BMP.setPitch(50);
  //BMP.setWordGap(100);

  delay(3000);
  Serial.printf("Ready!\r\n");
  BMP.begin();
  BMP.speak("I ");
  BMP.speak("have ");
  BMP.speak("awoken ");
}


void loop() {
  // BMP.speak("I ");
  FetchMavlinkSerial();


  delay(10);
}
