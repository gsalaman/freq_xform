// Simple Frequency Transform example.

// FHT defines.  This library defines an input buffer for us called fht_input of signed integers.  
#define LIN_OUT 1
#define FHT_N   32
#include <FHT.h>

//  We're using A5 as our audio input pin.
#define AUDIO_PIN A5

// These are the raw samples from the audio input.
#define SAMPLE_SIZE FHT_N
int sample[SAMPLE_SIZE] = {0};

//  Audio samples from the ADC are "centered" around 2.5v, which maps to 512 on the ADC.
#define SAMPLE_BIAS 512

// We have half the number of frequency bins as samples.
#define FREQ_BINS (SAMPLE_SIZE/2)

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
  }

  end_time = micros();

  return (end_time - start_time);
  
}

// This function does the FHT to convert the time-based samples (in the sample[] array)
// to frequency bins.  The FHT library defines an array (fht_input[]) where we put our 
// input values.  After doing it's processing, fht_input will contain raw output values...
// we can use fht_lin_out() to convert those to magnitudes.
void doFHT( void )
{
  int i;
  int temp_sample;
  
  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove DC bias
    temp_sample = sample[i] - SAMPLE_BIAS;

    // Load the sample into the input array
    fht_input[i] = temp_sample;
    
  }
  
  fht_window();
  fht_reorder();
  fht_run();

  // Their lin mag functons corrupt memory!!!  Gonna try this for the 32 point one...we may be okay.
  fht_mag_lin();  
}

void setup() 
{
  Serial.begin(9600);  
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
    Serial.println(fht_lin_out[i]);
    bin = bin + hz_per_bin;
  }
  Serial.println("=========");
}

void loop() 
{
  float         hz_per_bin;
  float         us_per_sample;
  float         freq_range_hz;
  unsigned long sample_time_us;
  int           dc_bias;
  unsigned long start_xform_time;
  unsigned long end_xform_time;
  
  sample_time_us = collect_samples();

  us_per_sample = (float) sample_time_us / SAMPLE_SIZE;

  // The frequency transform below takes our samples and outputs them in an array of "frequency bins".
  // This array is half the size of our input sample rate, so for 32 samples, we have 16 bins.
  // These bins range from 0 Hz to half of our sample rate (Nyquist's theorm).
  // We're going to calculate that range and the bin size next.

  //  us_per_sample is in microseconds (10^-6).  Move that 10^-6 to the numerator, and you get the 1000000.
  //  The 2.0 in the denominator is because our range is half the sample rate.
  freq_range_hz = 1000000.0 /(2.0 * us_per_sample);
    
  // do the FHT to populate our frequency display
  start_xform_time = micros();
  doFHT();
  end_xform_time = micros();

  // ...and display the results.
  print_time_samples(sample_time_us);
  print_freq(freq_range_hz);

  Serial.print("Time to do the xform (us): ");
  Serial.println(end_xform_time - start_xform_time);

  // wait for a key for next iteration through the loop
  Serial.println("hit enter for next sample collection");
  while (!Serial.available());
  while (Serial.available()) Serial.read();
  
}
