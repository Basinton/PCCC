#if !defined(ESP8266)
  #error This code is designed to run on ESP8266 and ESP8266-based boards! Please check your Tools->Board setting.
#endif

// These define's must be placed at the beginning before #include "ESP8266TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         1
#define _TIMERINTERRUPT_LOGLEVEL_     1

// Select a Timer Clock
#define USING_TIM_DIV1                false           // for shortest and most accurate timer
#define USING_TIM_DIV16               true           // for medium time and medium accurate timer
#define USING_TIM_DIV256              false            // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"
#include "RF24.h"
#include <SPI.h>

// Init ESP8266 timer 1
ESP8266Timer ITimer;

//=======================================================================
#define TIMER_CYCLE 10
#define PROCESS_CYCLE 50


#define BUILTIN_LED     2
#define CE_PIN 0
#define CSN_PIN 15

#define BUTTON 16
#define LED 5
#define LEDBUILTIN 2
#define BUZZER 4
#define RF  1

#define INIT 0
#define NORMAL  1
#define ALARM  2
#define BACK_TO_NORMAL 3

#define NO_OF_RESEND 4

String mode = "TES";
int test_count = 0;

int timer1_flag = 0, timer1_counter = 0;
int timer2_flag = 0, timer2_counter = 0;
int timer3_flag = 0, timer3_counter = 0;
int timer_process_flag = 0, timer_process_counter = 0; 
int timer_led_flag = 0, timer_led_counter = 0;

int resend_count = 0;
int button_count = 0;
int rf_count = 0;

int status = INIT;
int statusLed = LOW;
int statusLedBuiltin = LOW; 
int statusBuzzer = LOW;

const char msg_alarm[] = "Alarm";
const char msg_stop[] = "Stop";
char received[50];
RF24 radio(CE_PIN, CSN_PIN);

void readButton(){  
    if(digitalRead(BUTTON) == 0){
      button_count++;
    } else button_count = 0;
}

void readRF(){  
    if(digitalRead(RF) == 1){
      rf_count++;
    } else rf_count = 0;
}

bool isButtonPress(){
  return button_count == 1;
}

bool isRfReceived(){
  return rf_count == 1;  
}

bool isButtonLongPress(int duration) {
  return button_count == duration/PROCESS_CYCLE;
}

void setTimer1(int duration){
  timer1_counter = duration/TIMER_CYCLE;
  timer1_flag = 0;
}

void setTimer2(int duration){
  timer2_counter = duration/TIMER_CYCLE;
  timer2_flag = 0;
}

void setTimer3(int duration){
  timer3_counter = duration/TIMER_CYCLE;
  timer3_flag = 0;
}

void setTimerProcess(int duration){
  timer_process_counter = duration/TIMER_CYCLE;
  timer_process_flag = 0;
}

void setTimerLed(int duration){
  timer_led_counter = duration/TIMER_CYCLE;
  timer_led_flag = 0;
}

void timerRun(){
  if(timer1_counter > 0){
    timer1_counter--;
    if(timer1_counter <= 0) timer1_flag = 1;
  }

  if(timer2_counter > 0){
    timer2_counter--;
    if(timer2_counter <= 0) timer2_flag = 1;
  }

  if(timer3_counter > 0){
    timer3_counter--;
    if(timer3_counter <= 0) timer3_flag = 1;
  }  

  if(timer_process_counter > 0){
    timer_process_counter--;
    if(timer_process_counter <= 0) {
      timer_process_flag = 1;
    }    
  }

  if(timer_led_counter > 0){
    timer_led_counter--;
    if(timer_led_counter <= 0) {
      timer_led_flag = 1;
    }    
  }
}

void IRAM_ATTR TimerHandler()
{
  timerRun();
}
uint8_t address[] = "12345" ;

uint8_t flag_active = 0;

void fsm() {
  switch (status) {
    case INIT:
      status = NORMAL;

      // set the RX address of the TX node into a RX pipe 
      // additional setup specific to the node's role
      radio.startListening();  // put radio in RX mode      
      break;
    case NORMAL:
      statusLed = LOW;
      statusBuzzer = LOW;
      if (isButtonPress() || isRfReceived()) {
        // Serial.println("button pressed");
        radio.stopListening();   
        radio.write(&msg_alarm, sizeof(msg_alarm));
        status = ALARM;               
        flag_active = 0;
        setTimer2(30000);
        setTimer1(500);
        statusLed = HIGH;
        resend_count = 0;
        radio.startListening();   
        break;             
      }

      if (radio.available()) {
        radio.read(&received, sizeof(received));
        // Serial.println(received);
        if (strcmp(received, msg_alarm) == 0) {
          // Serial.println("Received alarm");          
          radio.stopListening(); 
          radio.write(&msg_alarm, sizeof(msg_alarm));         
          status = ALARM;
          flag_active = 0;
          setTimer2(30000);
          setTimer1(500);
          resend_count = NO_OF_RESEND;
          radio.startListening();
        }
      }
      break;
    case ALARM:
      statusBuzzer = HIGH;
      if(button_count == 0) flag_active = 1;
      if(timer1_flag){
        if(resend_count < NO_OF_RESEND){
          radio.stopListening();
          radio.write(&msg_alarm, sizeof(msg_alarm));
          resend_count++;   
          radio.startListening();             
          setTimer1(500);   
        } 
      }

      if (isButtonLongPress(2000) && flag_active == 1) {
        // Serial.println("button long pressed");
        radio.stopListening();
        radio.write(&msg_stop, sizeof(msg_stop));         
        status = BACK_TO_NORMAL;
        setTimer1(3000);
        setTimer3(500);        
        resend_count = 0;
        radio.startListening(); 
        break;
      }

      if (radio.available()) {
        radio.read(&received, sizeof(received));
        // Serial.println(received);
        if (strcmp(received, msg_stop) == 0) {          
          // Serial.println("Received stop.");
          radio.stopListening(); 
          radio.write(&msg_stop, sizeof(msg_stop));         
          status = BACK_TO_NORMAL;      
          resend_count = NO_OF_RESEND;          
          setTimer1(3000);
          radio.startListening();
        }        
      }

      if (timer2_flag == 1) {
        status = BACK_TO_NORMAL;
        setTimer1(3000);
      }

      break;
    case BACK_TO_NORMAL:
      statusBuzzer = LOW;
      statusLed = LOW;
      if(timer1_flag) status = NORMAL;
      if(timer3_flag){
        if(resend_count < NO_OF_RESEND){
          radio.stopListening(); 
          radio.write(&msg_stop, sizeof(msg_stop));   
          resend_count++;                    
          setTimer3(500);
          radio.startListening();                    
        }
      }
      break;      
    default:
      break;
  }
}

//=======================================================================
//                               Setup
//=======================================================================
void setup()
{
  pinMode(BUTTON, INPUT);
  pinMode(LED ,OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LEDBUILTIN ,OUTPUT);
  pinMode(RF, INPUT_PULLUP);
  digitalWrite(LED, LOW);
  digitalWrite(LEDBUILTIN, LOW);
  digitalWrite(BUZZER, LOW);
    
  // Serial.begin(115200);
  while(!Serial);
  if (!radio.begin()) {
    // Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }

  radio.setAutoAck(0);
  radio.openWritingPipe(address);
  radio.openReadingPipe(1, address);

  delay(500);
  // Interval in microsecs
  if (ITimer.attachInterruptInterval(TIMER_CYCLE * 1000, TimerHandler))
  {
    // Serial.println("Starting ITimer OK");
  }  
  setTimerLed(500);
  setTimerProcess(50);
}

void loop(){
  if(timer_process_flag){
    setTimerProcess(50);
    if(timer_led_flag){
      setTimerLed(500);
      statusLedBuiltin = !statusLedBuiltin;
      digitalWrite(LEDBUILTIN, statusLedBuiltin);
    }
    readButton();
    readRF();
    fsm();
    digitalWrite(LED, statusLed);
    digitalWrite(BUZZER, statusBuzzer);    
  }
}

