// Converting Teensy to use standard audio out and old FFT.
//
// 1st experiment:  looks like plain old analogRead takes about 10us...fast enough.
// 256 pt FFT takes ~27 ms...a little slower than I want.
// 128 takes 12ms...closer.
// 64 takes about 6.  Disco!
//
// Ideally, I want 21 frequency bins, going to about 3 KHz.
// 64 points would give us 32 bins...11 more than I need...but not too bad.
// Doing 150 Hz/bin (times 21) gives me a little over 3 KHz range...meaning I want to sample at about 6 KHz, or about 167us. 
// Doing an ugly 150us delay between samples gets me close.
//
// Next issue:  we've got a significant DC bias in our samples...probably because arduino voltage ref is 1.1v rather than 1.15 (3.3/2)
// Just changing the constant.

#include <SmartLEDShieldV4.h>  // comment out this line for if you're not using SmartLED Shield V4 hardware (this line needs to be before #include <SmartMatrix3.h>)
#include <SmartMatrix3.h>
#include <FastLED.h>

#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN; // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels, or use SMARTMATRIX_HUB75_64ROW_MOD32SCAN for common 64x64 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);


#include "arduinoFFT.h"

arduinoFFT FFT = arduinoFFT(); /* Create FFT object */

CRGB palette[21];

//  We're using A2 as our audio input pin.
#define AUDIO_PIN A2

// These are the raw samples from the audio input.
#define SAMPLE_SIZE 128
int sample[SAMPLE_SIZE] = {0};

#define SAMPLE_BIAS 512

// We have half the number of frequency bins as samples.
#define FREQ_BINS (SAMPLE_SIZE/2)

// These are the input and output vectors for the FFT.
double vReal[SAMPLE_SIZE];
double vImag[SAMPLE_SIZE];

#define MAX_FREQ_MAG 200

void init_palette( void )
{
  fill_gradient_RGB(palette, 7, CRGB::Purple, CRGB::Blue);
  fill_gradient_RGB(&(palette[7]), 7, CRGB::Green, CRGB::Yellow);
  fill_gradient_RGB(&(palette[14]),7, CRGB::Yellow, CRGB::Red);
}

// This function fills our buffer with audio samples.
unsigned long collect_samples( void )
{
  int i;
  unsigned long start_time;
  unsigned long end_time;

  start_time = micros();
  
  for (i = 0; i < SAMPLE_SIZE; i++)
  {
    sample[i] = analogRead(AUDIO_PIN);

    // Here's my ugly blocking delay.
    delayMicroseconds(30);
  }

  end_time = micros();

  return (end_time - start_time);
  
}

// This function does the FFT to convert the time-based samples (in the sample[] array)
// to frequency bins.  
void doFFT( void )
{
  int i;
  int temp_sample;
  
  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove DC bias
    temp_sample = sample[i] - SAMPLE_BIAS;

    // Load the sample into the input array
    vReal[i] = temp_sample;
    vImag[i] = 0;
    
  }
  
  FFT.Windowing(vReal, SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLE_SIZE, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLE_SIZE);
 
}

void setup() 
{
  Serial.begin(9600);  
  
  init_palette();
  
  // Initialize Matrix
  matrix.addLayer(&backgroundLayer); 
  matrix.begin();

  matrix.setBrightness(255);

}

void print_time_samples( unsigned long sample_time )
{
  int i;

  Serial.print("Time to collect samples: ");
  Serial.print(sample_time);
  Serial.print(" us, or ");
  Serial.print(sample_time/SAMPLE_SIZE);
  Serial.println(" us per sample");

  Serial.println("Raw Samples:");
  for (i=0; i<SAMPLE_SIZE; i++)
  {
    Serial.println(sample[i]);
  }
}

void print_freq( float freq_range_hz )
{
  int i;
  float bin=0.0;
  float hz_per_bin;

  hz_per_bin = freq_range_hz / FREQ_BINS;

  Serial.print("Freq results: (");
  Serial.print(freq_range_hz);
  Serial.println(" Hz wide)");
  
  for (i=0; i< FREQ_BINS; i++)
  {
    Serial.print(bin);
    Serial.print(" Hz = ");
    Serial.println(vReal[i]);
    bin = bin + hz_per_bin;
  }
  Serial.println("=========");
}

void display_freq_raw( void )
{
  int i;
  int mag;
  int bin;
  int x;
  rgb24 color = {0xFF, 0, 0};
  rgb24 black = {0,0,0};

  backgroundLayer.fillScreen(black);

  for (i = 0; i < 21; i++)
  {
    mag = constrain(vReal[i], 0, MAX_FREQ_MAG);
    mag = map(mag, 0, MAX_FREQ_MAG, 0, 31);

    x = i*3;

    backgroundLayer.drawRectangle(x, 32, x+2, 31-mag, palette[i]);
  }
  backgroundLayer.swapBuffers();
  
}

void loop() 
{
  collect_samples();

  doFFT();

  display_freq_raw();
  
}
