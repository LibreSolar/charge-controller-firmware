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
#include "sx1276-mbed-hal.h"
#include "thingset.h"
#include "hardware.h"
#include "eeprom.h"
#include "leds.h"

SPI spi_uext(PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL);
void send_dataset2();

extern Serial serial;

int last_call = 0;                  // Initialise the sending interval timer
int last_call_2 = 0;                // Initialise the sending interval timer for sending the 2nd packet
const int interval_1 = 300;         // Sending interval for lora_1 message in seconds (should be 5 minutes in production)
//const int interval_1 = 30;
const int interval_2 = 43200;       // Sending interval for lora_2 message in seconds (should be 12 hours)
//const int interval_2 = 90;
const int temp_interval = 10;       // Temporary interval in seconds to send a new message shortly after receiving a message
int wait_time;
int num_timeouts = 0;

// ThingSet instance and buffers
extern ThingSet ts;
extern const int PUB_CHANNEL_LORA_DAILY;
extern const int PUB_CHANNEL_LORA_REGULARLY;
uint8_t lora_resp[55] = {0xff, 0xff, 0x00, 0x00};       // These bytes are required by the radiohead library on the hub
uint8_t lora_req[51];

extern load_output_t load;
extern uint32_t deviceID;

#define DEBUG_MESSAGE 1

/* Set this flag to '1' to use the LoRa modulation or to '0' to use FSK modulation */
#define USE_MODEM_LORA          1
#define USE_MODEM_FSK           !USE_MODEM_LORA
#define RF_FREQUENCY            867000000           // Hz
#define TX_OUTPUT_POWER         20                  // 20 dBm

#if USE_MODEM_LORA == 1

#define LORA_BANDWIDTH          125000  // LoRa default, details in SX1276::BandwidthMap
//#define LORA_SPREADING_FACTOR   LORA_SF8
#define LORA_SPREADING_FACTOR   LORA_SF12
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

#define RX_TIMEOUT_VALUE    5000    // in ms

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
SX1276Generic Radio(NULL, RFM95_SX1276,
                    PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL, PIN_UEXT_SDA,
                    PIN_UEXT_TX, PIN_UEXT_RX, NC, NC, NC, NC);


// Add LED for uplink messages
void OnTxDone(void *radio, void *userThisPtr, void *userData)
{
    Radio.Sleep( );
    State = TX;
    if (DEBUG_MESSAGE)
        serial.printf("> OnTxDone\n");
}

// Add LED for Downlink messages and trigger a new uplink message in 10 seconds or so for confirmation
void OnRxDone(void *radio, void *userThisPtr, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Sleep();
    State = RX;
    num_timeouts = 0;
    if (DEBUG_MESSAGE) {
        serial.printf("> OnRxDone: RssiValue=%d dBm, SnrValue=%d, Payload=%d\n", rssi, snr, payload);
        for (int i = 0; i < size - 4; i++) {
            serial.putc(payload[i]);
        }
    }
    if (size > 4) {
        // TODO: Implement LoRa protocol to make sure that only messages
        //       with matching deviceID are received
        ts.process(payload + 4, size - 4, lora_resp, sizeof(lora_resp));
    }
    wait_time = temp_interval;
    trigger_rx_led();
}

void OnTxTimeout(void *radio, void *userThisPtr, void *userData)
{
    Radio.Sleep( );
    State = TX_TIMEOUT;
    if(DEBUG_MESSAGE)
        serial.printf("> OnTxTimeout\n");
}

void OnRxTimeout(void *radio, void *userThisPtr, void *userData)
{
    if (num_timeouts < 1){
        if (DEBUG_MESSAGE)
            serial.printf("Num RX Timeouts: %d\n", num_timeouts);
        Radio.Rx(RX_TIMEOUT_VALUE);
        num_timeouts++;
    }
    else
    {
        Radio.Sleep( );
        State = RX_TIMEOUT;
        if (DEBUG_MESSAGE)
            serial.printf("> OnRxTimeout\n");
        num_timeouts = 0;
        if (time(NULL) > last_call_2 + interval_2)
        {
            send_dataset2();
        }
    }
}

void OnRxError(void *radio, void *userThisPtr, void *userData)
{
    Radio.Sleep( );
    State = RX_ERROR;
    if (DEBUG_MESSAGE)
        serial.printf("> OnRxError\n");
}

void lora_init(void)
{
    serial.printf("Cloud Solar LoRa implementation\n");
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
    if (Radio.Init(&RadioEvents) == false) {
        serial.printf("Radio could not be detected!\n");
        return;
    }

    switch(Radio.DetectBoardType()) {
        case SX1276MB1LAS:
            if (DEBUG_MESSAGE)
                serial.printf(" > Board Type: SX1276MB1LAS <");
            break;
        case SX1276MB1MAS:
            if (DEBUG_MESSAGE)
                serial.printf(" > Board Type: SX1276MB1LAS <");
            break;
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

    Radio.SetChannel(RF_FREQUENCY);

#if USE_MODEM_LORA == 1

    if (LORA_FHSS_ENABLED)
        serial.printf(" > LORA FHSS Mode <\n");
    if (!LORA_FHSS_ENABLED)
        serial.printf(" > LORA Mode <\n");

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

}

void lora_process(void)
{
    // if the time interval to send data has passed
    if (time(NULL) > last_call + wait_time)
    {
        // Send the next LoRa 1 UPLINK frame
        int len = ts.pub_msg_cbor(lora_resp + 4, sizeof(lora_resp) - 4, PUB_CHANNEL_LORA_REGULARLY);
        if (DEBUG_MESSAGE)
            serial.printf("Sending dataset 1...\n");
        Radio.Send(lora_resp, len + 4);
        // We wait to see if there is a response from the hub
        Radio.Rx(RX_TIMEOUT_VALUE);
        // Update last send time
        last_call = time(NULL);
        wait_time = interval_1;
        trigger_tx_led();
    }
}

void send_dataset2(void)
{
    // Send the next LoRa 2 UPLINK frame
    int len = ts.pub_msg_cbor(lora_resp + 4, sizeof(lora_resp) - 4, PUB_CHANNEL_LORA_DAILY);
    if (DEBUG_MESSAGE)
        serial.printf("Sending dataset 2...\n");
    Radio.Send(lora_resp, len + 4);
    // We wait to see if there is a response from the hub
    Radio.Rx( RX_TIMEOUT_VALUE );
    // Update last send time
    last_call_2 = time(NULL);
    trigger_tx_led();
}


#endif /* LORA_ENABLED */