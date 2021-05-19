#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>
#include <Tjpg_Decoder.h>
#include <TFT_eSPI.h>
#include <ArduinoSpotify.h>
#include <ArduinoSpotifyCert.h>
#include <ArduinoJson.h>
//#include <Button2.h>
#include "config.h"

#define ALBUM_ART "/image.jpg"
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
char clientId[] = CLIENT_ID;
char clientSecret[] = CLIENT_SECRET;
unsigned long delayBetweenRequests = 30000; // Time between requests (30 seconds)
unsigned long requestDueTime;               //time when request due
String lastAlbumArtUrl;

HTTPClient http;
WiFiClient wifi;
WiFiClientSecure wifiSecure;
TFT_eSPI tft = TFT_eSPI();
ArduinoSpotify spotify(wifiSecure, clientId, clientSecret, SPOTIFY_REFRESH_TOKEN);
// Button2 buttonR = Button2(35);
// Button2 buttonL = Button2(0);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height())
    return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

// void nextTrack(Button2& btn){
//   //spotify.nextTrack();
//   Serial.println("----------------- next track");
// }

// void previousTrack(Button2& btn){
//   //spotify.previousTrack();
//   Serial.println("------------------- previous track");
// }

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPIFFS.begin(true);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setCallback(tft_output);
  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(true);
  wifiSecure.setCACert(spotify_server_cert);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // buttonR.setClickHandler(nextTrack);
  // buttonL.setClickHandler(previousTrack);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }
  if (SPIFFS.exists("/image.jpg"))
  {
    Serial.println("File exists");
  }
  else
  {
    Serial.println("File doesent exist");
  }
  Serial.println("Refreshing Access token");
  if (!spotify.refreshAccessToken())
  {
    Serial.println("Failed to refresh access token");
  }
  
}



int displayImageUsingFile(char *albumArtUrl)
{

  if(SPIFFS.exists(ALBUM_ART))
  {
    Serial.println("removing existing image");
    SPIFFS.remove(ALBUM_ART);
  }
  File f = SPIFFS.open(ALBUM_ART, "w+");
  if(!f){
    Serial.println("failed to open file");
  }
  bool gotImage = spotify.getImage(albumArtUrl, &f);
  f.close();
  if(gotImage){
    return TJpgDec.drawFsJpg(0,0,ALBUM_ART);
  } else{
    return -2;
  }
}

int displayImage(char *albumArtUrl) {

  uint8_t *imageFile; // pointer that the library will store the image at (uses malloc)
  int imageSize; // library will update the size of the image
  bool gotImage = spotify.getImage(albumArtUrl, &imageFile, &imageSize);

  if (gotImage) {
    Serial.print("Got Image");
    delay(1);
    int jpegStatus = TJpgDec.drawJpg(0, 0, imageFile, imageSize);
    free(imageFile); // Make sure to free the memory!
    return jpegStatus;
  } else {
    return -2;
  }
}


void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    if (!currentlyPlaying.error)
    {
        Serial.println("--------- Currently Playing ---------");


        Serial.print("Is Playing: ");
        if (currentlyPlaying.isPlaying)
        {
        Serial.println("Yes");
        } else {
        Serial.println("No");
        }

        Serial.print("Track: ");
        Serial.println(currentlyPlaying.trackName);
        Serial.print("Track URI: ");
        Serial.println(currentlyPlaying.trackUri);
        Serial.println();

        Serial.print("Artist: ");
        Serial.println(currentlyPlaying.firstArtistName);
        Serial.print("Artist URI: ");
        Serial.println(currentlyPlaying.firstArtistUri);
        Serial.println();

        Serial.print("Album: ");
        Serial.println(currentlyPlaying.albumName);
        Serial.print("Album URI: ");
        Serial.println(currentlyPlaying.albumUri);
        Serial.println();

        // will be in order of widest to narrowest
        // currentlyPlaying.numImages is the number of images that
        // are stored 
        for (int i = 0; i < currentlyPlaying.numImages; i++) {
            Serial.println("------------------------");
            Serial.print("Album Image: ");
            Serial.println(currentlyPlaying.albumImages[i].url);
            Serial.print("Dimensions: ");
            Serial.print(currentlyPlaying.albumImages[i].width);
            Serial.print(" x ");
            Serial.print(currentlyPlaying.albumImages[i].height);
            Serial.println();
        }

        Serial.println("------------------------");
    }
}

void loop() {
  // buttonR.loop();
  //   buttonL.loop();
  if (millis() > requestDueTime)
  {
    
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());

    Serial.println("getting currently playing song:");
    // Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
    CurrentlyPlaying currentlyPlaying = spotify.getCurrentlyPlaying(SPOTIFY_MARKET);
    if (!currentlyPlaying.error)
    {
      printCurrentlyPlayingToSerial(currentlyPlaying);

      // Smallest (narrowest) image will always be last.
      SpotifyImage smallestImage = currentlyPlaying.albumImages[currentlyPlaying.numImages - 2];
      String newAlbum = String(smallestImage.url);
      if (newAlbum != lastAlbumArtUrl) {
        Serial.println("Updating Art");
        //int displayImageResult = displayImageUsingFile(smallestImage.url); File reference example
        tft.fillScreen(TFT_BLACK);
        int displayImageResult = displayImage(smallestImage.url); // Memory Buffer Example - should be much faster
        tft.setCursor(0, 160);
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE);
        tft.println(spotify.getCurrentlyPlaying().trackName);
        Serial.println(spotify.getCurrentlyPlaying().trackName);
        if (displayImageResult == 0) {
          lastAlbumArtUrl = newAlbum;
        } else {
          Serial.print("failed to display image: ");
          Serial.println(displayImageResult);
        }
      }
    }

    requestDueTime = millis() + delayBetweenRequests;
  }

}