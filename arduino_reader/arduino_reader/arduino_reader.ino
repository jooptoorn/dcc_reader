#include "dcc_reader.h"

#define RX_PIN 2
#define DEBUG_PIN 7

unsigned long bit_time = 0;

dcc_message_t dcc_msg;
unsigned char debug_byte;
int dcc_speed;

void setup() {
  // Setup GPIO
  pinMode(RX_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(DEBUG_PIN, LOW);
  Serial.begin(9600);
  // Attach interrupt
  attachInterrupt(digitalPinToInterrupt(RX_PIN), rx_pin_isr, CHANGE);
}

void loop() {
  // put your main code here, to run repeatedly: 
  
  //disable interrupts
  // noInterrupts();
  // if(dcc_msg.msg_type != dcc_msg_idle)
  // {}
  	// Serial.print("Loc ");
    // Serial.print(dcc_msg.addr);
    // Serial.print(" : ");

    // Serial.print(dcc_msg.cmd);
      Serial.print(" Speed ");
      Serial.print(dcc_speed);
      // Serial.print("cmd byte ");
      // Serial.print(0<(debug_byte&(1<<7)));
      // Serial.print(0<(debug_byte&(1<<6)));
      // Serial.print(0<(debug_byte&(1<<5)));
      // Serial.print(0<(debug_byte&(1<<4)));
      // Serial.print(0<(debug_byte&(1<<3)));
      // Serial.print(0<(debug_byte&(1<<2)));
      // Serial.print(0<(debug_byte&(1<<1)));
      // Serial.print(0<(debug_byte&(1<<0)));
    Serial.print("\r\n");

  // }
  // interrupts();
  delay(300);
}

void rx_pin_isr(void){
  //timing vars
  static unsigned long prev_time = 0;
  unsigned long this_time = micros();
  // get the bit timing, wrap around overflow
  if(prev_time > this_time)
  {
    unsigned long delta_t = 0xffffffff - prev_time;
    bit_time = this_time + delta_t;
  }
  else
  {
    bit_time = this_time - prev_time;
  }
  
  dcc_halfbit_t halfbit = dcc_feed_halfbit(bit_time);
  dcc_reader_msg_type ret = dcc_feed_bit(halfbit, &dcc_msg);
  if(ret == dcc_msg_sdi || ret == dcc_msg_aoi)
  {
    debug_byte = dcc_msg.cmd_arg[0];
    dcc_speed = dcc_msg.speed;
  }
  // if(ret == dcci)
  //   digitalWrite(DEBUG_PIN, HIGH);
  // else
  //   digitalWrite(DEBUG_PIN, LOW);

  prev_time = this_time;
}
