/*
 * Grand Central M4 - 4 Speaker PCB (74HC4066 + 4x LM386N-1)
 * ---------------------------------------------------------
 * Same signal engine as the tested single_speaker.ino (96 kHz,
 * 1024-pt sine table, direct DAC writes). The same stimulus is
 * written to BOTH DACs; the 4066 picks which speakers hear it.
 *
 * Board routing (from the schematic):
 *   A0 -> 1k/47n -> INPUT1 -> switch A (ctrl CNTRL1) -> U1 -> SPKR
 *                          -> switch D (ctrl CNTRL4) -> U4 -> SPKR3
 *   A1 -> 1k/47n -> INPUT2 -> switch B (ctrl CNTRL2) -> U3 -> SPKR1
 *                          -> switch C (ctrl CNTRL3) -> U2 -> SPKR2
 *   4066 pin 14 -> 3.3V, pin 7 -> GND
 *
 * Channel numbers below follow CNTRL1..4.
 * CHECK CTRL_PINS against your PCB traces before first power-up.
 *
 * Serial (115200 baud, newline):
 *   s0 s1 s2      select stimulus (sine / noise / noise pulse)
 *   f<Hz>         sine frequency        (1..20000, default 1000)
 *   a<0..1>       sine amplitude        (default 0.5)
 *   n<0..1>       noise amplitude       (default 0.3)
 *   p<ms>         pulse ON  time in ms  (default 50)
 *   r<ms>         pulse period in ms    (default 500)
 *   on1..on4      close that channel's switch
 *   off1..off4    open that channel's switch
 *   on            all 4 ON
 *   off / stop    all 4 OFF
 *   ?             show settings
 *
 * All channels start OFF. Each channel auto-opens after 60 s ON.
 */

#include <Arduino.h>

const uint32_t SAMPLE_RATE = 96000;

// ---- 74HC4066 switch control ----
// Index 0..3 = CNTRL1..CNTRL4 (switch A, B, C, D)
const uint8_t       CTRL_PINS[4] = {2, 3, 4, 5};   // CHECK against PCB
const char*         CH_NAME[4]   = {"SPKR (A0)", "SPKR1 (A1)", "SPKR2 (A1)", "SPKR3 (A0)"};
const unsigned long MAX_ON_MS    = 60000UL;        // 60 s safety auto-stop
bool                chOn[4]      = {false, false, false, false};
unsigned long       chOnTime[4]  = {0, 0, 0, 0};

void setCh(uint8_t i, bool on){
  digitalWrite(CTRL_PINS[i], on ? HIGH : LOW);
  chOn[i] = on; if(on) chOnTime[i] = millis();
  Serial.print("ch"); Serial.print(i+1); Serial.print(" ");
  Serial.print(CH_NAME[i]); Serial.println(on ? " ON" : " OFF");
}
void setAll(bool on){ for(uint8_t i=0;i<4;i++) setCh(i, on); }

volatile uint8_t  stimulus    = 0;
volatile int32_t  sineAmpQ15  = (int32_t)(0.5*32768);
volatile int32_t  noiseAmpQ15 = (int32_t)(0.3*32768);
volatile uint32_t phaseAcc=0, phaseInc=0;
volatile uint32_t rng=0x1234ABCD;
volatile uint32_t pulseOnSamples=0, pulsePerSamples=0, pulseCtr=0;

const uint16_t sineTable[1024] = {
  2048,2059,2071,2082,2093,2105,2116,2127,2138,2150,2161,2172,2184,2195,2206,2217,
  2229,2240,2251,2262,2274,2285,2296,2307,2318,2329,2341,2352,2363,2374,2385,2396,
  2407,2418,2430,2441,2452,2463,2474,2485,2496,2507,2518,2528,2539,2550,2561,2572,
  2583,2594,2604,2615,2626,2637,2647,2658,2669,2679,2690,2700,2711,2722,2732,2743,
  2753,2763,2774,2784,2795,2805,2815,2825,2836,2846,2856,2866,2876,2886,2896,2906,
  2916,2926,2936,2946,2956,2966,2976,2985,2995,3005,3014,3024,3034,3043,3053,3062,
  3072,3081,3090,3100,3109,3118,3127,3136,3145,3155,3164,3173,3181,3190,3199,3208,
  3217,3225,3234,3243,3251,3260,3268,3277,3285,3294,3302,3310,3318,3327,3335,3343,
  3351,3359,3367,3374,3382,3390,3398,3405,3413,3421,3428,3436,3443,3450,3458,3465,
  3472,3479,3486,3493,3500,3507,3514,3521,3528,3534,3541,3548,3554,3561,3567,3574,
  3580,3586,3592,3598,3604,3610,3616,3622,3628,3634,3640,3645,3651,3657,3662,3667,
  3673,3678,3683,3688,3694,3699,3704,3709,3713,3718,3723,3728,3732,3737,3741,3746,
  3750,3754,3759,3763,3767,3771,3775,3779,3783,3786,3790,3794,3797,3801,3804,3808,
  3811,3814,3817,3821,3824,3827,3829,3832,3835,3838,3840,3843,3846,3848,3850,3853,
  3855,3857,3859,3861,3863,3865,3867,3869,3870,3872,3874,3875,3876,3878,3879,3880,
  3881,3883,3884,3884,3885,3886,3887,3887,3888,3889,3889,3889,3890,3890,3890,3890,
  3890,3890,3890,3890,3890,3889,3889,3889,3888,3887,3887,3886,3885,3884,3884,3883,
  3881,3880,3879,3878,3876,3875,3874,3872,3870,3869,3867,3865,3863,3861,3859,3857,
  3855,3853,3850,3848,3846,3843,3840,3838,3835,3832,3829,3827,3824,3821,3817,3814,
  3811,3808,3804,3801,3797,3794,3790,3786,3783,3779,3775,3771,3767,3763,3759,3754,
  3750,3746,3741,3737,3732,3728,3723,3718,3713,3709,3704,3699,3694,3688,3683,3678,
  3673,3667,3662,3657,3651,3645,3640,3634,3628,3622,3616,3610,3604,3598,3592,3586,
  3580,3574,3567,3561,3554,3548,3541,3534,3528,3521,3514,3507,3500,3493,3486,3479,
  3472,3465,3458,3450,3443,3436,3428,3421,3413,3405,3398,3390,3382,3374,3367,3359,
  3351,3343,3335,3327,3318,3310,3302,3294,3285,3277,3268,3260,3251,3243,3234,3225,
  3217,3208,3199,3190,3181,3173,3164,3155,3145,3136,3127,3118,3109,3100,3090,3081,
  3072,3062,3053,3043,3034,3024,3014,3005,2995,2985,2976,2966,2956,2946,2936,2926,
  2916,2906,2896,2886,2876,2866,2856,2846,2836,2825,2815,2805,2795,2784,2774,2763,
  2753,2743,2732,2722,2711,2700,2690,2679,2669,2658,2647,2637,2626,2615,2604,2594,
  2583,2572,2561,2550,2539,2528,2518,2507,2496,2485,2474,2463,2452,2441,2430,2418,
  2407,2396,2385,2374,2363,2352,2341,2329,2318,2307,2296,2285,2274,2262,2251,2240,
  2229,2217,2206,2195,2184,2172,2161,2150,2138,2127,2116,2105,2093,2082,2071,2059,
  2048,2037,2025,2014,2003,1991,1980,1969,1958,1946,1935,1924,1912,1901,1890,1879,
  1867,1856,1845,1834,1822,1811,1800,1789,1778,1767,1755,1744,1733,1722,1711,1700,
  1689,1678,1666,1655,1644,1633,1622,1611,1600,1589,1578,1568,1557,1546,1535,1524,
  1513,1502,1492,1481,1470,1459,1449,1438,1427,1417,1406,1396,1385,1374,1364,1353,
  1343,1333,1322,1312,1301,1291,1281,1271,1260,1250,1240,1230,1220,1210,1200,1190,
  1180,1170,1160,1150,1140,1130,1120,1111,1101,1091,1082,1072,1062,1053,1043,1034,
  1024,1015,1006,996,987,978,969,960,951,941,932,923,915,906,897,888,
  879,871,862,853,845,836,828,819,811,802,794,786,778,769,761,753,
  745,737,729,722,714,706,698,691,683,675,668,660,653,646,638,631,
  624,617,610,603,596,589,582,575,568,562,555,548,542,535,529,522,
  516,510,504,498,492,486,480,474,468,462,456,451,445,439,434,429,
  423,418,413,408,402,397,392,387,383,378,373,368,364,359,355,350,
  346,342,337,333,329,325,321,317,313,310,306,302,299,295,292,288,
  285,282,279,275,272,269,267,264,261,258,256,253,250,248,246,243,
  241,239,237,235,233,231,229,227,226,224,222,221,220,218,217,216,
  215,213,212,212,211,210,209,209,208,207,207,207,206,206,206,206,
  206,206,206,206,206,207,207,207,208,209,209,210,211,212,212,213,
  215,216,217,218,220,221,222,224,226,227,229,231,233,235,237,239,
  241,243,246,248,250,253,256,258,261,264,267,269,272,275,279,282,
  285,288,292,295,299,302,306,310,313,317,321,325,329,333,337,342,
  346,350,355,359,364,368,373,378,383,387,392,397,402,408,413,418,
  423,429,434,439,445,451,456,462,468,474,480,486,492,498,504,510,
  516,522,529,535,542,548,555,562,568,575,582,589,596,603,610,617,
  624,631,638,646,653,660,668,675,683,691,698,706,714,722,729,737,
  745,753,761,769,778,786,794,802,811,819,828,836,845,853,862,871,
  879,888,897,906,915,923,932,941,951,960,969,978,987,996,1006,1015,
  1024,1034,1043,1053,1062,1072,1082,1091,1101,1111,1120,1130,1140,1150,1160,1170,
  1180,1190,1200,1210,1220,1230,1240,1250,1260,1271,1281,1291,1301,1312,1322,1333,
  1343,1353,1364,1374,1385,1396,1406,1417,1427,1438,1449,1459,1470,1481,1492,1502,
  1513,1524,1535,1546,1557,1568,1578,1589,1600,1611,1622,1633,1644,1655,1666,1678,
  1689,1700,1711,1722,1733,1744,1755,1767,1778,1789,1800,1811,1822,1834,1845,1856,
  1867,1879,1890,1901,1912,1924,1935,1946,1958,1969,1980,1991,2003,2014,2025,2037,
};
const uint8_t SH=32-10; const uint16_t MASK=1023;

static inline uint32_t xs(){ uint32_t x=rng; x^=x<<13; x^=x>>17; x^=x<<5; rng=x; return x; }

float fFreq=1000,fSine=0.5,fNoise=0.3,fPon=50,fPer=500;

void setFreq(float f){ if(f<1)f=1; if(f>20000)f=20000; fFreq=f;
  phaseInc=(uint32_t)(((double)f*4294967296.0)/(double)SAMPLE_RATE+0.5); }
void setSine(float a){ if(a<0)a=0; if(a>1)a=1; fSine=a; sineAmpQ15=(int32_t)(a*32768.0); }
void setNoise(float a){ if(a<0)a=0; if(a>1)a=1; fNoise=a; noiseAmpQ15=(int32_t)(a*32768.0); }
void setPon(float ms){ if(ms<1)ms=1; fPon=ms; pulseOnSamples=(uint32_t)((uint64_t)ms*SAMPLE_RATE/1000ULL); }
void setPer(float ms){ if(ms<1)ms=1; fPer=ms; pulsePerSamples=(uint32_t)((uint64_t)ms*SAMPLE_RATE/1000ULL); }

// fast, non-blocking DAC write (never spins inside the ISR)
static inline void dacWrite(uint16_t v){
  if(!DAC->SYNCBUSY.bit.DATA0) DAC->DATA[0].reg = v;   // A0
  if(!DAC->SYNCBUSY.bit.DATA1) DAC->DATA[1].reg = v;   // A1
}

void setupTimer(uint32_t sr){
  GCLK->PCHCTRL[TC3_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
  while(!(GCLK->PCHCTRL[TC3_GCLK_ID].reg & GCLK_PCHCTRL_CHEN));
  TC3->COUNT16.CTRLA.reg=TC_CTRLA_SWRST; while(TC3->COUNT16.SYNCBUSY.bit.SWRST);
  TC3->COUNT16.WAVE.reg=TC_WAVE_WAVEGEN_MFRQ;
  uint32_t cc=(120000000UL/sr)-1; TC3->COUNT16.CC[0].reg=(uint16_t)cc;
  while(TC3->COUNT16.SYNCBUSY.bit.CC0);
  TC3->COUNT16.INTENSET.reg=TC_INTENSET_MC0;
  NVIC_EnableIRQ(TC3_IRQn); NVIC_SetPriority(TC3_IRQn,0);
  TC3->COUNT16.CTRLA.bit.ENABLE=1; while(TC3->COUNT16.SYNCBUSY.bit.ENABLE);
}

void TC3_Handler(){
  if(!TC3->COUNT16.INTFLAG.bit.MC0) return;
  TC3->COUNT16.INTFLAG.reg=TC_INTFLAG_MC0;

  int32_t centered=0, amp=0;
  switch(stimulus){
    case 0:
      phaseAcc+=phaseInc;
      centered=(int32_t)sineTable[(phaseAcc>>SH)&MASK]-2048;
      amp=sineAmpQ15;
      break;
    case 1:
      centered=(int32_t)(xs()&0x0FFF)-2048;
      amp=noiseAmpQ15;
      break;
    case 2:
      if(pulseCtr<pulseOnSamples){ centered=(int32_t)(xs()&0x0FFF)-2048; amp=noiseAmpQ15; }
      else { centered=0; amp=0; }
      pulseCtr++;
      if(pulseCtr>=pulsePerSamples) pulseCtr=0;
      break;
  }
  int32_t out=2048 + ((centered*amp)>>15);
  if(out<0)out=0; if(out>4095)out=4095;
  dacWrite((uint16_t)out);
}

char buf[24]; uint8_t bi=0;
void handleLine(char* s){
  // word commands first (so "off" is not read as an 'o' command)
  if(strcmp(s,"on")==0){ setAll(true); return; }
  if(strcmp(s,"off")==0 || strcmp(s,"stop")==0){ setAll(false); return; }
  if(strncmp(s,"on",2)==0 && s[2]>='1' && s[2]<='4' && s[3]==0){ setCh(s[2]-'1', true); return; }
  if(strncmp(s,"off",3)==0 && s[3]>='1' && s[3]<='4' && s[4]==0){ setCh(s[3]-'1', false); return; }

  char c=s[0]; float v=atof(s+1);
  switch(c){
    case 's': stimulus=(uint8_t)v; pulseCtr=0; Serial.print("stimulus="); Serial.println(stimulus); break;
    case 'f': setFreq(v);  Serial.print("freq=");     Serial.println(v); break;
    case 'a': setSine(v);  Serial.print("sineAmp=");  Serial.println(v); break;
    case 'n': setNoise(v); Serial.print("noiseAmp="); Serial.println(v); break;
    case 'p': setPon(v);   Serial.print("pulseOn ms=");Serial.println(v); break;
    case 'r': setPer(v);   Serial.print("period ms="); Serial.println(v); break;
    case '?':
      Serial.println("--- settings ---");
      Serial.print("stimulus="); Serial.println(stimulus);
      Serial.print("freq="); Serial.println(fFreq);
      Serial.print("sineAmp="); Serial.println(fSine);
      Serial.print("noiseAmp="); Serial.println(fNoise);
      Serial.print("pulseOn ms="); Serial.println(fPon);
      Serial.print("period ms="); Serial.println(fPer);
      for(uint8_t i=0;i<4;i++){
        Serial.print("ch"); Serial.print(i+1); Serial.print(" "); Serial.print(CH_NAME[i]);
        Serial.println(chOn[i] ? " ON" : " OFF");
      }
      break;
  }
}

void setup(){
  for(uint8_t i=0;i<4;i++){
    pinMode(CTRL_PINS[i], OUTPUT);
    digitalWrite(CTRL_PINS[i], LOW);  // all silent at boot (fail-safe)
  }

  analogWriteResolution(12);
  analogWrite(A0, 2048);        // let the core configure DAC0, then we write registers
  analogWrite(A1, 2048);        // same for DAC1
  Serial.begin(115200);
  setFreq(1000); setSine(0.5); setNoise(0.3); setPon(50); setPer(500);
  setupTimer(SAMPLE_RATE);
  Serial.println("Ready. s0=sine s1=noise s2=noisePulse");
  Serial.println("f<Hz> a<sine> n<noise> p<onMs> r<perMs> ?=show");
  Serial.println("on1..on4 / off1..off4, on = all, off = all (start OFF)");
}

void loop(){
  for(uint8_t i=0;i<4;i++){
    if(chOn[i] && (millis()-chOnTime[i] > MAX_ON_MS)){
      Serial.print("safety timeout ch"); Serial.println(i+1);
      setCh(i, false);
    }
  }

  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'||c=='\r'){ if(bi>0){ buf[bi]=0; handleLine(buf); bi=0; } }
    else if(bi<sizeof(buf)-1){ buf[bi++]=c; }
  }
}
