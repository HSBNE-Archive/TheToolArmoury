//----------------------------------
// RS 485 - Slave
// By Luke Hovington
// 2014-05-08
//----------------------------------

#define SLAVE_ADDR 02

//#define LED_R 9
//#define LED_B 10
//#define LED_G 11

#define LED_R 10
#define LED_B 9
#define LED_G 6

#define AUX_1 5
#define AUX_2 6
#define AUX_3 3

//#define pinCONTROL 4
#define pinCONTROL 5

typedef struct __attribute__((__packed__)) {
  uint16_t addressTo;
  uint16_t addressFrom;
  uint8_t  data_type;
  uint8_t  code;
  uint8_t  data;
} transmit_t;

typedef enum {
  START_A = 0,
  START_5,
  DATA,
  PARSE
} parseInput;

void sendMSG(transmit_t *data);
void parseMessage(transmit_t *data, uint16_t crc);
void sendACK_NAK(transmit_t *data, bool ack);

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R,HIGH);
  digitalWrite(LED_G,HIGH);
  digitalWrite(LED_B,HIGH);
  
  pinMode(pinCONTROL,OUTPUT);
  digitalWrite(pinCONTROL,LOW);
  
  Serial.begin(9600);
}

void loop() {
  static transmit_t  incomingData;
  static parseInput  state = START_A;
  static uint8_t     count, byte_receive;
  static uint16_t    crc;
  
  while (Serial.available() > 0) {
    byte_receive=Serial.read();
    
    switch (state) {
      case START_A:
        if(byte_receive == 0xAA) {
          state = START_5;
        }
        break;
      
      case START_5:
        if(byte_receive == 0x55) {
          count = 0;
          state = DATA;
        }
        break;
        
      case DATA:
        if (count < sizeof(transmit_t)) {
          ((uint8_t*)&incomingData)[count++] = byte_receive;
        }
        else
        {
          ((uint8_t*)&crc)[(count++) - sizeof(transmit_t)] = byte_receive;
        }
        
        // If this is not addressed to us, don't listem anymore
        if(count >= sizeof(uint16_t) && incomingData.addressTo != SLAVE_ADDR) {
          state = START_A;
        }
        
        // if we have all the data and the crc go to the next stage
        if(count >= sizeof(transmit_t)+2) {
          // Parse Data and respond
          state = START_A;
          parseMessage(&incomingData, crc);
        }
        break;
      
      default:
        state = START_A;
        break;
    }
  }
}


//------------------------
// FUNCTIONS
//------------------------

void parseMessage(transmit_t *data, uint16_t crc) {
  uint16_t calCRC = calc_CRC((uint8_t*) data, sizeof(transmit_t));
  uint8_t tmp;
  
//  Serial.write(0xAA);
//  Serial.write(0xAA);
//  for (tmp = 0; tmp < sizeof(transmit_t); tmp++) {
//    Serial.write(((uint8_t*) data)[tmp]);
//  }
//  Serial.write(0xBB);
//  Serial.write(0xBB);
//  Serial.write(calCRC >> 8);
//  Serial.write(calCRC & 0xff);
//  Serial.write(0xCC);
//  Serial.write(0xCC);
  
  sendACK_NAK(data, calCRC == crc);
  
  // go no further if the package was corrupt
  if(calCRC != crc) {
    return;
  }
  
  // Parse the data to see what pin we need to change
  switch (data->code) {
    case 1:
      tmp = LED_R;
      break;
    case 2:
      tmp = LED_G;
      break;
    case 3:
      tmp = LED_B;
      break;
    default:
      tmp = 0;
      break;
  }
  
  switch (data->data_type) {
    // Fade the output
    case 'F':
      if (tmp != 0) {
        analogWrite(tmp, data->data);
      }
      break;
    
    // Switch the output
    case 'S':
      if (tmp != 0) {
        digitalWrite(tmp, data->data != 1);
      }
      break;
      
    default:
      break;
  }
}

uint16_t calc_CRC(uint8_t *p, uint16_t packetLength) {
  uint16_t crc = 0;
  static uint8_t i, j;
  for (i = 0; i < packetLength; i++) {
    crc = crc ^ p[i] << 8;
    for(j=0;j < 8; j++) {
      if (crc & 0x8000)
        crc = crc << 1 ^ 0x1021;
      else
        crc = crc << 1;
    }
  }
  return crc;
}

void sendMSG(transmit_t *data) {
  // Create a crc from the outgoing data
  uint16_t crc = calc_CRC((uint8_t*) data, sizeof(data));
  uint16_t i;
  
  // Clear the TX buffer
  UCSR0A=UCSR0A |(1 << TXC0);
  
  digitalWrite(pinCONTROL,HIGH);
  delay(1);
  
  // Start bytes
  Serial.write(0xAA);
  Serial.write(0x55);
  
  // Send Data
  for(i=0;i<sizeof(transmit_t);i++) {
    Serial.write(((uint8_t*) data)[i]);
  }
  
  // And lastly the CRC
  for(i=0;i<sizeof(crc);i++) {
    Serial.write(((uint8_t*) &crc)[i]);
  }
  
  // Wait till the TX buffer is empty
  while (!(UCSR0A & (1 << TXC0)));
  digitalWrite(pinCONTROL,LOW);
}

void sendACK_NAK(transmit_t *data, bool ack) {
  uint8_t      i;
  transmit_t   dataACK;
  uint16_t     checksum_ACK;
  
  // Make a copy
  for(i=0;i<sizeof(dataACK);i++) {
    ((uint8_t *) &dataACK)[i] = ((uint8_t *) data)[i];
  }
  
  // Is this a ACK or NAK
  if(ack) {
    dataACK.data_type = 0x06;
  } else {
    dataACK.data_type = 0x15;
  }
  
  // Swap the addresses around
  dataACK.addressTo = dataACK.addressFrom;
  dataACK.addressFrom = SLAVE_ADDR;
  
  sendMSG(&dataACK);
}
