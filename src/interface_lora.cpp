/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#ifdef LORA_ENABLED     // otherwise don't compile code to reduce firmware size

#include "pcb.h"
#include "interface.h"
#include "data_objects.h"

#include "sx1276-mbed-hal.h"

//SPI spi_uext(PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL);

extern Serial serial;

/*
 * Callback functions prototypes
 */
/*!
 * @brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void *radio, void *userThisPtr, void *userData);
 
/*!
 * @brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(void *radio, void *userThisPtr, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
 
/*!
 * @brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void *radio, void *userThisPtr, void *userData);
 
/*!
 * @brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void *radio, void *userThisPtr, void *userData);
 
/*!
 * @brief Function executed on Radio Rx Error event
 */
void OnRxError(void *radio, void *userThisPtr, void *userData);
 
/*!
 * @brief Function executed on Radio Fhss Change Channel event
 */
void OnFhssChangeChannel(void *radio, void *userThisPtr, void *userData, uint8_t channelIndex);
 
/*!
 * @brief Function executed on CAD Done event
 */
void OnCadDone(void *radio, void *userThisPtr, void *userData);

#define DEBUG_MESSAGE 0

/* Set this flag to '1' to use the LoRa modulation or to '0' to use FSK modulation */
#define USE_MODEM_LORA          1
#define USE_MODEM_FSK           !USE_MODEM_LORA
#define RF_FREQUENCY            RF_FREQUENCY_868_1  // Hz
#define TX_OUTPUT_POWER         14                  // 14 dBm
 
#if USE_MODEM_LORA == 1
 
#define LORA_BANDWIDTH          125000  // LoRa default, details in SX1276::BandwidthMap
#define LORA_SPREADING_FACTOR   LORA_SF7
//#define LORA_SPREADING_FACTOR   LORA_SF12
#define LORA_CODINGRATE         LORA_ERROR_CODING_RATE_4_5
 
#define LORA_PREAMBLE_LENGTH    8       // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT     5       // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_FHSS_ENABLED       false  
#define LORA_NB_SYMB_HOP        4     
#define LORA_IQ_INVERSION_ON    false
#define LORA_CRC_ENABLED        true
    
#elif USE_MODEM_FSK == 1
 
#define FSK_FDEV                25000     // Hz
#define FSK_DATARATE            19200     // bps
#define FSK_BANDWIDTH           50000     // Hz
#define FSK_AFC_BANDWIDTH       83333     // Hz
#define FSK_PREAMBLE_LENGTH     5         // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON   false
#define FSK_CRC_ENABLED         true
    
#else
    #error "Please define a modem in the compiler options."
#endif 
 
 
#define RX_TIMEOUT_VALUE    3500    // in ms
 
//#define BUFFER_SIZE       32        // Define the payload size here
#define BUFFER_SIZE         64        // Define the payload size here
 
/*
 *  Global variables declarations
 */
typedef enum
{
    LOWPOWER = 0,
    IDLE,
    
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    
    TX,
    TX_TIMEOUT,
    
    CAD,
    CAD_DONE
} AppStates_t;
 
volatile AppStates_t State = LOWPOWER;
 
/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;
 
/*
 *  Global variables declarations
 */
//SX1276Generic *Radio;
SX1276Generic Radio(NULL, RFM95_SX1276,
            PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL, PIN_UEXT_SDA,
            //PIN_UEXT_TX, PIN_UEXT_RX, NC, NC, NC, NC);      // prototype
            PIN_UEXT_RX, PIN_UEXT_TX, NC, NC, NC, NC);      // final

 
const uint8_t PingMsg[] = { 0xff, 0xff, 0x00, 0x00, 'P', 'I', 'N', 'G'};// "PING";
const uint8_t PongMsg[] = { 0xff, 0xff, 0x00, 0x00, 'P', 'O', 'N', 'G'};// "PONG";
 
uint16_t BufferSize = BUFFER_SIZE;
//uint8_t *Buffer;
 
uint8_t Buffer[BUFFER_SIZE];


//DigitalOut *led3;
 
void dump(const char *title, const void *data, int len, bool dwords)
{
    serial.printf("dump(\"%s\", 0x%x, %d bytes)", title, data, len);
 
    int i, j, cnt;
    unsigned char *u;
    const int width = 16;
    const int seppos = 7;
 
    cnt = 0;
    u = (unsigned char *)data;
    while (len > 0) {
        serial.printf("%08x: ", (unsigned int)data + cnt);
        if (dwords) {
            unsigned int *ip = ( unsigned int *)u;
            serial.printf(" 0x%08x\r\n", *ip);
            u+= 4;
            len -= 4;
            cnt += 4;
            continue;
        }
        cnt += width;
        j = len < width ? len : width;
        for (i = 0; i < j; i++) {
            serial.printf("%2.2x ", *(u + i));
            if (i == seppos)
                serial.printf(" ");
        }
        serial.printf(" ");
        if (j < width) {
            i = width - j;
            if (i > seppos + 1)
                serial.printf(" ");
            while (i--) {
                serial.printf("%s", "   ");
            }
        }
        for (i = 0; i < j; i++) {
            int c = *(u + i);
            if (c >= ' ' && c <= '~')
                serial.printf("%c", c);
            else
                serial.printf(".");
            if (i == seppos)
                serial.printf(" ");
        }
        len -= width;
        u += width;
        serial.printf("\r\n");
    }
    serial.printf("--\r\n");
}

extern Serial serial;



void lora_init(void)
{
//    Buffer = new  uint8_t[BUFFER_SIZE];

//    Radio = new SX1276Generic(NULL, RFM95_SX1276,
//            PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL, PIN_UEXT_SDA,
            //PIN_UEXT_TX, PIN_UEXT_RX, NC, NC, NC, NC);      // prototype
//            PIN_UEXT_RX, PIN_UEXT_TX, NC, NC, NC, NC);      // final
    
    serial.printf("SX1276 Ping Pong Demo Application\n");
    serial.printf("Freqency: %.1f\n", (double)RF_FREQUENCY/1000000.0);
    serial.printf("TXPower: %d dBm\n",  TX_OUTPUT_POWER);
#if USE_MODEM_LORA == 1
    serial.printf("Bandwidth: %d Hz\n", LORA_BANDWIDTH);
    serial.printf("Spreading factor: SF%d\n", LORA_SPREADING_FACTOR);
#elif USE_MODEM_FSK == 1
    serial.printf("Bandwidth: %d kHz\n",  FSK_BANDWIDTH);
    serial.printf("Baudrate: %d\n", FSK_DATARATE);
#endif
    // Initialize Radio driver
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxError = OnRxError;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;    
    if (Radio.Init( &RadioEvents ) == false) {
        while(1) {
            serial.printf("Radio could not be detected!\n");
            wait( 1 );
        }
    }
 
    switch(Radio.DetectBoardType()) {
        case SX1276MB1LAS:
            if (DEBUG_MESSAGE)
                serial.printf(" > Board Type: SX1276MB1LAS <");
            break;
        case SX1276MB1MAS:
            if (DEBUG_MESSAGE)
                serial.printf(" > Board Type: SX1276MB1LAS <");
        case MURATA_SX1276:
            if (DEBUG_MESSAGE)
                serial.printf(" > Board Type: MURATA_SX1276_STM32L0 <");
            break;
        case RFM95_SX1276:
            if (DEBUG_MESSAGE)
                serial.printf(" > HopeRF RFM95xx <\n");
            break;
        default:
            serial.printf(" > Board Type: unknown <");
    }
 
    Radio.SetChannel(RF_FREQUENCY ); 
 
#if USE_MODEM_LORA == 1
    
    if (LORA_FHSS_ENABLED)
        serial.printf("             > LORA FHSS Mode <\n");
    if (!LORA_FHSS_ENABLED)
        serial.printf("             > LORA Mode <\n");
 
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                         LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                         LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                         LORA_CRC_ENABLED, LORA_FHSS_ENABLED, LORA_NB_SYMB_HOP, 
                         LORA_IQ_INVERSION_ON, 2000 );
    
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                         LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                         LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0,
                         LORA_CRC_ENABLED, LORA_FHSS_ENABLED, LORA_NB_SYMB_HOP, 
                         LORA_IQ_INVERSION_ON, true );
                         
#elif USE_MODEM_FSK == 1
 
    serial.printf("              > FSK Mode <");
    Radio.SetTxConfig( MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV, 0,
                         FSK_DATARATE, 0,
                         FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
                         FSK_CRC_ENABLED, 0, 0, 0, 2000 );
    
    Radio.SetRxConfig( MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                         0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                         0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, FSK_CRC_ENABLED,
                         0, 0, false, true );
                         
#else
 
#error "Please define a modem in the compiler options."
 
#endif
        
    Radio.Rx( RX_TIMEOUT_VALUE );
     
}

void lora_process(void)
{
    uint8_t i;
    bool isMaster = true;

    //serial.printf( "Sending frame...\n" );
    // Send the next PING frame            
    memcpy(Buffer, PingMsg, sizeof(PingMsg));
    // We fill the buffer with numbers for the payload 
    for( i = sizeof(PingMsg); i < BufferSize; i++ )
    {
        Buffer[i] = i - sizeof(PingMsg);
    }
    wait_ms( 10 ); 
    Radio.Send( Buffer, BufferSize );
//    Radio.Rx( RX_TIMEOUT_VALUE );


/*

    if (DEBUG_MESSAGE)
        serial.printf("Starting Ping-Pong loop\n");     
    
    switch( State )
    {
    case RX:
        //*led3 = 0;
        if( isMaster == true )
        {
            if( BufferSize > 0 )
            {
                if( memcmp(Buffer, PongMsg, sizeof(PongMsg)) == 0 )
                {
                    //*led = !*led;
                    serial.printf( "...Pong" );
                    // Send the next PING frame            
                    memcpy(Buffer, PingMsg, sizeof(PingMsg));
                    // We fill the buffer with numbers for the payload 
                    for( i = sizeof(PingMsg); i < BufferSize; i++ )
                    {
                        Buffer[i] = i - sizeof(PingMsg);
                    }
                    wait_ms( 10 ); 
                    Radio.Send( Buffer, BufferSize );
                }
                else if( memcmp(Buffer, PingMsg, sizeof(PingMsg)) == 0 )
                { // A master already exists then become a slave
                    serial.printf( "...Ping" );
                    //*led = !*led;
                    isMaster = false;
                    // Send the next PONG frame
                    memcpy(Buffer, PongMsg, sizeof(PongMsg));        
                    // We fill the buffer with numbers for the payload 
                    for( i = sizeof(PongMsg); i < BufferSize; i++ )
                    {
                        Buffer[i] = i - sizeof(PongMsg);
                    }
                    wait_ms( 10 ); 
                    Radio.Send( Buffer, BufferSize );
                }
                else // valid reception but neither a PING or a PONG message
                {    // Set device as master ans start again
                    isMaster = true;
                    Radio.Rx( RX_TIMEOUT_VALUE );
                }    
            }
        }
        else
        {
            if( BufferSize > 0 )
            {
                if( memcmp(Buffer, PingMsg, sizeof(PingMsg)) == 0 )
                {
                    //*led = !*led;
                    serial.printf( "...Ping" );
                    // Send the reply to the PING string
                    memcpy(Buffer, PongMsg, sizeof(PongMsg));
                    // We fill the buffer with numbers for the payload 
                    for( i = sizeof(PongMsg); i < BufferSize; i++ )
                    {
                        Buffer[i] = i - sizeof(PongMsg);
                    }
                    wait_ms( 10 );  
                    Radio.Send( Buffer, BufferSize );
                }
                else // valid reception but not a PING as expected
                {    // Set device as master and start again
                    isMaster = true;
                    Radio.Rx( RX_TIMEOUT_VALUE );
                }    
            }
        }
        State = LOWPOWER;
        break;
    case TX:    
        //*led3 = 1;
        if( isMaster == true )  
        {
            serial.printf("Ping..." );
        }
        else
        {
            serial.printf("Pong..." );
        }
        Radio.Rx( RX_TIMEOUT_VALUE );
        State = LOWPOWER;
        break;
    case RX_TIMEOUT:
        if( isMaster == true )
        {
            // Send the next PING frame
            memcpy(Buffer, PingMsg, sizeof(PingMsg));
            for( i = sizeof(PingMsg); i < BufferSize; i++ )
            {
                Buffer[i] = i - sizeof(PingMsg);
            }
            wait_ms( 10 ); 
            Radio.Send( Buffer, BufferSize );
        }
        else
        {
            Radio.Rx( RX_TIMEOUT_VALUE );  
        }             
        State = LOWPOWER;
        break;
    case RX_ERROR:
        // We have received a Packet with a CRC error, send reply as if packet was correct
        if( isMaster == true )
        {
            // Send the next PING frame
            memcpy(Buffer, PingMsg, sizeof(PingMsg));
            for( i = 4; i < BufferSize; i++ )
            {
                Buffer[i] = i - 4;
            }
            wait_ms( 10 );  
            Radio.Send( Buffer, BufferSize );
        }
        else
        {
            // Send the next PONG frame
            memcpy(Buffer, PongMsg, sizeof(PongMsg));
            for( i = sizeof(PongMsg); i < BufferSize; i++ )
            {
                Buffer[i] = i - sizeof(PongMsg);
            }
            wait_ms( 10 );  
            Radio.Send( Buffer, BufferSize );
        }
        State = LOWPOWER;
        break;
    case TX_TIMEOUT:
        Radio.Rx( RX_TIMEOUT_VALUE );
        State = LOWPOWER;
        break;
    case LOWPOWER:
        sleep();
        break;
    default:
        State = LOWPOWER;
        break;
    }
    */
}

void OnTxDone(void *radio, void *userThisPtr, void *userData)
{
    Radio.Sleep( );
    State = TX;
    if (DEBUG_MESSAGE)
        serial.printf("> OnTxDone\n");
}
 
void OnRxDone(void *radio, void *userThisPtr, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Sleep( );
    BufferSize = size;
    memcpy( Buffer, payload, BufferSize );
    State = RX;
    if (DEBUG_MESSAGE)
        serial.printf("> OnRxDone: RssiValue=%d dBm, SnrValue=%d\n", rssi, snr);
    //dump("Data:", payload, size);
}
 
void OnTxTimeout(void *radio, void *userThisPtr, void *userData)
{
    //*led3 = 0;
    Radio.Sleep( );
    State = TX_TIMEOUT;
    if(DEBUG_MESSAGE)
        serial.printf("> OnTxTimeout\n");
}
 
void OnRxTimeout(void *radio, void *userThisPtr, void *userData)
{
    //*led3 = 0;
    Radio.Sleep( );
    Buffer[BufferSize-1] = 0;
    State = RX_TIMEOUT;
    if (DEBUG_MESSAGE)
        serial.printf("> OnRxTimeout\n");
}
 
void OnRxError(void *radio, void *userThisPtr, void *userData)
{
    Radio.Sleep( );
    State = RX_ERROR;
    if (DEBUG_MESSAGE)
        serial.printf("> OnRxError\n");
}
 
#endif /* LORA_ENABLED */