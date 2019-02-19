// frequency transform examples.
// Bit banging ADC

// FHT defines.  This library defines an input buffer for us called fht_input of signed integers.  
#define LIN_OUT 1
#define FHT_N   64
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

#define BIT_BANG_ADC

void setupADC( void )
{

   // Prescalar is the last 3 bits of the ADCSRA register.  
   // Here are my measured sample rates (and resultant frequency ranges):
   // Prescalar of 128 gives ~10 KHz sample rate (5 KHz range)    mask  111
   // Prescalar of 64 gives ~20 KHz sample rate (10 KHz range)    mask: 110    
   // Prescalar of 32 gives ~40 KHz sample rate (20 KHz range)    mask: 101
   
    ADCSRA = 0b11100111;      // Upper bits set ADC to free-running mode.

    // A5, internal reference.
    ADMUX =  0b00000101;

    delay(50);  //wait for voltages to stabalize.  

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
    #ifdef BIT_BANG_ADC
    while(!(ADCSRA & 0x10));        // wait for ADC to complete current conversion ie ADIF bit set
    ADCSRA = ADCSRA | 0x10;        // clear ADIF bit so that ADC can do next operation (0xf5)
    sample[i] = ADC;
    #else
    sample[i] = analogRead(AUDIO_PIN);
    #endif
  }

  end_time = micros();

  return (end_time - start_time);
  
}

int calc_dc_bias( void )
{
  int i;
  unsigned long total=0;
  unsigned long dc_bias;

  for (i = 0; i < SAMPLE_SIZE; i++)
  {
    total = total + sample[i];
  }

  dc_bias = total / SAMPLE_SIZE;

  if (dc_bias > 1023) Serial.println("*****  DC BIAS OVERFLOW!!!!  *****");

  return dc_bias;
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

void glenn_dc_bias_FHT( int dc_bias )
{
  int i;
  int temp_sample;
  
  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove DC bias
    temp_sample = sample[i] - dc_bias;

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

  #ifdef BIT_BANG_ADC
  setupADC();
  #endif
  
}

void print_time_samples( unsigned long sample_time )
{
  int i;
  unsigned long total = 0;
  

  Serial.print("Time to collect samples: ");
  Serial.print(sample_time);
  Serial.print(" us, or ");
  Serial.print(sample_time/SAMPLE_SIZE);
  Serial.println(" us per sample");

  Serial.println("Raw Samples:");
  for (i=0; i<SAMPLE_SIZE; i++)
  {
    total = total + sample[i];
    Serial.println(sample[i]);
  }

  Serial.print("==>DC Bias: ");
  Serial.println(total / SAMPLE_SIZE);
  Serial.println("------------");
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
  unsigned long start_fft_us;
  unsigned long end_fft_us;
  
  sample_time_us = collect_samples();

  Serial.print("*** sample_time_us: ");
  Serial.println(sample_time_us);

  us_per_sample = (float) sample_time_us / SAMPLE_SIZE;

  Serial.print("*** us_per_sample: ");
  Serial.println(us_per_sample);
  
  freq_range_hz = 1000000.0 /(2.0 * us_per_sample);
  
  Serial.print("*** freq_range_hz: ");
  Serial.println(freq_range_hz);
  
  // do the FHT to populate our frequency display
  start_fft_us = micros();
  doFHT();
  end_fft_us = micros();
  
  // ...and display the results.
  print_time_samples(sample_time_us);
  print_freq(freq_range_hz);

  dc_bias = calc_dc_bias();

  glenn_dc_bias_FHT(dc_bias);
  Serial.println("Glenn DC BIAS adjusted");
  print_freq(freq_range_hz);

  Serial.print("Time to calc FHT (us): ");
  Serial.println(end_fft_us - start_fft_us);
  
  // wait for a key for next iteration through the loop
  Serial.println("hit enter for next sample collection");
  while (!Serial.available());
  while (Serial.available()) Serial.read();
  
}
