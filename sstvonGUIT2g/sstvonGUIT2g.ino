/* SSTV decoder, receiver on ESP32-S3 display Guition 4,3"
* Prepared by Stojan Markic, S52UV , Sept 2026
* Ver01 prvzet single channel scope on 3,5 inch na en kanal in SSTV osnova
* Ver02 sledenje hitrosti in vključuje grafiko, že prikaže sliko na pravi SSTV signal?
* Ver02a dodana diagnostika nad sempli
* Ver02b sampling to 30kHz and new int16_t getNextSSTVSample()
* Ver02c dodam izpis SSTVmode
* Ver02d dodam osciloscope priredim y na GUITION 480 x 272
* Ver02e dodam touch za stop
* Ver02f stop za vedno in uvedem continue
* Ver02g cleaned up some comments
*/
#include <Arduino.h>
#include <stdio.h>
#include "sstv_decoder.h"
#include "half_band_filter2.h"
#include "cordic.h"
#include "ESP32_SSTV.h"

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "driver/adc.h"
#include "hal/adc_ll.h"
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>
#include "src/tamc_gt911/TAMC_GT911.h" //https://github.com/TAMCTec/gt911-arduino

#define TOUCH_SDA  (gpio_num_t)8
#define TOUCH_SCL  (gpio_num_t)4
#define TOUCH_INT  (gpio_num_t)3
#define TOUCH_RST  (gpio_num_t)38
#define TOUCH_WIDTH  480
#define TOUCH_HEIGHT 272

//LCD  JC4827W543C
#define LCD_BL  (gpio_num_t)1
#define LCD_CS  (gpio_num_t)45
#define LCD_SCK (gpio_num_t)47
#define LCD_D0  (gpio_num_t)21
#define LCD_D1  (gpio_num_t)48
#define LCD_D2  (gpio_num_t)40
#define LCD_D3  (gpio_num_t)39
#define LCD_RST (gpio_num_t)-1

#define NUM_SAMPLES 2048 //1024           // Number of samples per capture

// ADC1 Channel 0 is GPIO1 on ESP32-S3
#define ADC_CHANNEL ADC1_CHANNEL_5  // reads pin6
#define ADC_CHANNEL2 ADC1_CHANNEL_5  // reads pin6
#define ADC_ATTEN   ADC_ATTEN_DB_11 // 11dB attenuation for ~0-3.1V range
// Define the ADC midpoint
#define ADC_MIDPOINT 1950 //512


Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS,LCD_SCK,LCD_D0,LCD_D1,LCD_D2,LCD_D3);
// Example: Changing the last parameter from false to true to force BGR/RGB swap
Arduino_GFX *g = new Arduino_NV3041A(bus, LCD_RST, 0 /* rotation */, true /* ips */, 480 /* w */, 272 /* h */, 0 /* col_offset1 */, 0 /* row_offset1 */, 0 /* col_offset2 */, 0 /* row_offset2 */); //, true /* is_bgr */);

Arduino_Canvas *gfx = new Arduino_Canvas(480, 272, g, 0, 0, 0);

// for touch
bool tp_control = true;
bool tp_band = false;
uint16_t in_left = 999; 
bool l_release = false; 
bool t_release = false; 
bool l_press = false; 
bool t_press = false; 
uint8_t lkey = 1;

TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);
int tp_x,tp_y,tp_size;
// Touch coordinates
int x = 0;
int y = 0;
int z = 0;

int16_t getNextSSTVSample();  //prototype declaration
bool imageInProgress = false;
bool imageComplete = false;
bool stopped = false;

//c_sstv_decoder_ESP32 decoder(15000.0f, gfx);
c_sstv_decoder_ESP32 decoder(14925.0f, gfx);

uint16_t touchX, touchY;

int oldsamples1[NUM_SAMPLES];
//int oldsamples2[NUM_SAMPLES];
volatile int index1 = 0;
unsigned long currentmicro;
unsigned long cur_ms   = 0;
unsigned int difmicro;
bool redraw = false;
volatile bool filled = false;
int picturenum = 0;

int fps;
int dummy;
String Modesstv = "Not received";

//double buffer alternating
#define BUF_SIZE 2048 //480
#define SSTV_BLOCK_SIZE 1024
volatile int16_t bufA[BUF_SIZE];
volatile int16_t bufB[BUF_SIZE];
// Processed 15 kHz samples
int16_t sstvSamples[SSTV_BLOCK_SIZE];
volatile uint32_t bufferOverrunA = 0;
volatile uint32_t bufferOverrunB = 0;

volatile bool useA = true;
volatile uint16_t idx = 0;
volatile bool readyA = false;
volatile bool readyB = false;

// -------- FAST ADC READ --------
int IRAM_ATTR local_adc1_read(int channel) {
    uint16_t adc_value;
    adc_ll_rtc_enable_channel(ADC_NUM_1, channel);
    adc_ll_rtc_start_convert(ADC_NUM_1, channel);
    
    while (adc_ll_rtc_convert_is_done(ADC_NUM_1) == 0);
    adc_value = adc_ll_rtc_get_convert_value(ADC_NUM_1);
    
    return adc_value;
   }

 
hw_timer_t *Timer1_Cfg = NULL;
  
void IRAM_ATTR Timer1_ISR() {
  //currentmicro = micros();
    //digitalWrite(interruptPin, HIGH);
    // Toggle GPIO pin
    //digitalWrite(interruptPin, !digitalRead(interruptPin));
     if (useA) {
        bufA[idx] = local_adc1_read(ADC_CHANNEL); // * 4;  //adc1_get_raw(ADC_CHANNEL); //analogRead(6);

    } else {
        bufB[idx] = local_adc1_read(ADC_CHANNEL); // * 4;  //adc1_get_raw(ADC_CHANNEL); //analogRead(6);
        
    }
      
    idx++;

    if (idx >= BUF_SIZE) {
        if (useA) readyA = true;
        else      readyB = true;
        useA = !useA;
        idx = 0;
        //filled = true;
    }
    //difmicro = micros() - currentmicro;
    //digitalWrite(interruptPin, LOW);
}

void takesamples() {

   //difmicro = micros() - currentmicro;
    gfx->fillRect(0, 30, 250, 40, RGB565_DARKGREEN);
    //gfx->setCursor(10, 30);
    //gfx->print(String(oldsamples1[0]) + "," + String(oldsamples2[0])+ ",t=" + String(difmicro));
}

void setup() {
  Serial.begin(115200);
  delay(800);
  // Initialize Display
  gfx->begin(60000000);
  //gfx->invertDisplay(true);  // za obrnit barve? NE! ne vpliva glej v global bus seting za display
  gfx->setTextWrap(false);
  gfx->fillRect(0,0,480,272,BLACK);
  gfx->flush(); //gfx->flush();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, 1);

  // Initialize touch
  tp.begin(); //za C verzijo
  //touch.begin() //za R verzijo
  tp.setRotation(ROTATION_INVERTED); //za C verzijo ze v beginu

  // 1. Configure ADC Width (12-bit)
    adc1_config_width(ADC_WIDTH_BIT_12);
    // 2. Configure Attenuation (defines input voltage range)
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    delay(10); //give it some time
    uint16_t raw = adc1_get_raw(ADC1_CHANNEL_5);   // kot analogRead(6); //dummy read

  //gfx->setRotation(1);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(340, 7);
  gfx->print("SSTV2d");
  gfx->setCursor(340, 30);
  gfx->print("by S52UV");
  gfx->setTextColor(RGB565_YELLOW);
  gfx->fillRect(0, 8, 320, 20, RGB565_BLACK);
    gfx->setCursor(10, 10);
    //gfx->print(String(touchX) + "," + String(touchY));
    gfx->print(Modesstv);
    gfx->fillRect(325, 50, 70, 30, RGB565_BLACK);
    gfx->setCursor(330, 52);
    gfx->print("Receiving!");
  gfx->flush();
  
  
  Timer1_Cfg = timerBegin(0, 40, true);  // 80 je 1Mhz  40 je 2Mhz je dobro za 24kHz audio
  timerAttachInterrupt(Timer1_Cfg, &Timer1_ISR, true);
  timerAlarmWrite(Timer1_Cfg, 67, true);  //za cca 30k
  timerAlarmEnable(Timer1_Cfg); // Enable the alarm

}

void drawgraph2(){

  gfx->fillRect(0, 100, 480, 220, RGB565(64, 64, 64));
  int last_y = 210; // center line (1.65V bias)
  int val, y;
    
    for (int i = 0; i < 480; i++) {
    val = oldsamples1[i] - 2048; // 12-bit ADC value
    y = 210 - map(val, -2048, 2048, -100, 100);
    //y = 210 - map(val, -2048, -1800, -100, 100);
    gfx->drawLine(i, last_y, i + 1, y, RGB565_YELLOW);
    last_y = y;
  }
  
  // Draw center line
  gfx->drawLine(0, 210, 479, 210, RGB565_DARKGREY);
  gfx->fillRect(0, 30, 250, 40, RGB565_DARKGREEN);
    gfx->setCursor(10, 30);
    //gfx->print(String(oldsamples1[0]) + "," + String(oldsamples2[0])); //+ ",t=" + String(difmicro));
    gfx->print(String(fps) + "," + String(oldsamples1[0]) + "," + ",t=" + String(difmicro));
    
}

void drawinfo(){
  
  gfx->fillRect(0, 30, 250, 40, RGB565_DARKGREEN);
    gfx->setCursor(10, 30);
    //gfx->print(String(count));
    //count++;
    gfx->print(String(fps) + "," + String(picturenum) + ",t=" + String(difmicro));
    
  //gfx->flush();
}

void processbufA() {
for (int i = 0; i < BUF_SIZE; i++) {
     oldsamples1[i]= bufA[i];
   }
   filled = false;
  }

void processbufB() {
for (int i = 0; i < BUF_SIZE; i++) {
     oldsamples1[i]= bufB[i];
   }
   filled = false;
  }

int16_t getNextSSTVSample()
{
    static uint16_t sample_number = SSTV_BLOCK_SIZE;

    // We need a new processed block
    if (sample_number >= SSTV_BLOCK_SIZE)
    {
        volatile int16_t *rawSamples = nullptr;

        // Wait until one of the raw 2048-sample buffers is ready
        while (!readyA && !readyB)
        {
            delay(0);
        }
        // Prefer A if available
        if (readyA)
        {
            readyA = false;
            rawSamples = bufA;
        }
        else
        {
            readyB = false;
            rawSamples = bufB;
        }

        // Convert 2048 raw 30-kHz samples
        // into 1024 processed 15-kHz samples

        static int32_t dc = 0;

        for (uint16_t i = 0; i < SSTV_BLOCK_SIZE; i++)
        {
            // Combine two consecutive 30-kHz samples
            int32_t sample =
                (int32_t)rawSamples[i * 2] +
                (int32_t)rawSamples[i * 2 + 1];

            // Dynamic DC tracking -- same algorithm as Pico
            dc += (sample - dc) / 2;

            // Remove DC
            sample -= dc;

            // Convert to decoder's 16-bit scale
            sample <<= 2;

            // Limit to int16_t just in case
            if (sample > 32767)
                sample = 32767;
            else if (sample < -32768)
                sample = -32768;

            sstvSamples[i] = (int16_t)sample;
        }

        sample_number = 0;
    }

    return sstvSamples[sample_number++];
}

void loop() {

    bool finished = decoder.decode_image_non_blocking(
        40,
        true,
        imageInProgress
    );

    if (finished){
        //Serial.println("SSTV image complete!");
        stopped = false;
        gfx->fillRect(325, 50, 130, 30, RGB565_BLACK);
        gfx->setCursor(330, 52);
        gfx->print("Image done");
        gfx->fillRoundRect(330,90,100,50,5,RGB565_RED);
        gfx->setCursor(355,110);
        gfx->print("STOP");
        gfx->flush();
        cur_ms=millis();
        while ((millis()-cur_ms) < 3000){
            t_touched();
       //Serial.println("Touch Pressed:" + String(touchX) + "," + String(touchY));
       gfx->fillRect(325, 205, 100, 30, RGB565_BLACK);
       gfx->setCursor(330, 210);
       gfx->print(x);gfx->print(":");gfx->print(y);
       //gfx->drawCircle(touchX, touchY, 2, RGB565_WHITE);
       
       if(x > 330 && x < 430 && y > 90 && y < 140) {
        stopped = true;
        gfx->fillRect(325, 50, 130, 30, RGB565_BLACK);
        gfx->setCursor(330, 52);
        gfx->print("Stopped!");
        }
      gfx->flush();
      delay(100);
      }
        
        if (stopped){
        gfx->fillRoundRect(330,90,100,50,5,RGB565_BLACK);
        gfx->fillRoundRect(330,145,100,50,5,RGB565_GREEN);
        gfx->setCursor(355,165);
        gfx->print("NEXT");

        bool waitcont = true;
        while (waitcont){
            t_touched();
       //Serial.println("Touch Pressed:" + String(touchX) + "," + String(touchY));
       gfx->fillRect(325, 205, 100, 30, RGB565_BLACK);
       gfx->setCursor(330, 210);
       gfx->print(x);gfx->print(":");gfx->print(y);
       //gfx->drawCircle(touchX, touchY, 2, RGB565_WHITE);      
       if(x > 330 && x < 430 && y > 145 && y < 200) {
        stopped = false;
        waitcont = false;
        gfx->fillRect(325, 50, 130, 30, RGB565_BLACK);
        gfx->setCursor(330, 52);
        gfx->print("Receiving!");
        gfx->fillRoundRect(330,145,100,50,5,RGB565_BLACK);
        }
      gfx->flush();
      delay(100);
       }
       // delay(20000);  //hold picture for this time at least
     }
        stopped = false;
        gfx->fillRect(325, 50, 130, 30, RGB565_BLACK);
        gfx->setCursor(330, 52);
        gfx->print("Receivivg!");
        gfx->fillRoundRect(330,90,100,50,5,RGB565_BLACK);
        //gfx->setCursor(350,80);
        //gfx->print("STOP");
        gfx->flush(); 
   }
  //delay(10);  //adjust refresh rate
} //end of loop

void t_touched(){ 
    static bool start = true;
    static int l_key=0;
    static int b_key=0;
    if(start){t_press=true;l_key=1;start=false;}
    tp.read();
    if(tp.isTouched){
      tp_x=tp.points[0].x;tp_y=tp.points[0].y;tp_size=tp.points[0].size;
      //x = tp_x; y = tp_y;  //za R display
      //x = tp_y; y = tp_x;  //za C display obrnem koordinati da dela enako kot R display
      x = tp_x; y = tp_y;  //na drugem C displaju spet obrnem koordinati da dela enako kot R display
      l_key=17;t_press=true;  //ena mora bit izbrana     
    }
    else{
      if(t_press&&l_key!=0){lkey=l_key;l_key=0;t_press=false;t_release=true;}
    }
}
