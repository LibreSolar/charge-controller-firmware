#include "interface.h"
#include "config.h"

#ifdef WIFI_ENABLED

#include "thingset.h"
#include <inttypes.h>

#include "pcb.h"
#include "half_bridge.h"
#include "ATCmdParser.h"

#include "ESP32.h"

UARTSerial uext_serial(PIN_UEXT_TX, PIN_UEXT_RX, 115200);

ESP32 wifi(uext_serial);
DigitalOut wifi_enable(PIN_UEXT_SCL);

enum wifi_states {
    STATE_WIFI_INIT,        // initial state
    STATE_WIFI_CONN,        // successfully connected to WiFi AP
    STATE_LAN_CONN,         // local IP address obtained
    STATE_INTERNET_CONN     // internet connection active
};

extern log_data_t log_data;
extern dcdc_port_t hs_port;
extern dcdc_port_t ls_port;
extern load_output_t load;
extern battery_t bat;
extern dcdc_t dcdc;
extern uint32_t deviceID;

int wifi_check_ap_connected()
{
    printf("WiFi: Getting wifi module status... ");
    int status = wifi.get_conn_status();
    printf("%d\n", status);

    if (status == ESP32_STATUS_AP_CONNECTED
        || status == ESP32_STATUS_TCP_ACTIVE
        || status == ESP32_STATUS_TCP_DIS)
    {
        return 1;
    }
    else {
        return 0;
    }
}

int wifi_connect_ap()
{
    if (wifi_check_ap_connected()) {
        return 1;
    }
    else {
        printf("WiFi: Joining network with SSID and PASS... ");
        int res = wifi.join_AP(WIFI_SSID, WIFI_PASS);
        printf("%d\n", res);
        return res;
    }
}

int wifi_check_lan_connected()
{
    char buffer[30];
    printf("WiFi: Getting IP address... ");
    // TODO
    int res = wifi.get_IP(buffer, sizeof(buffer));
    printf("%s\n", buffer);
    return res;
}

int wifi_check_internet_connected()
{
    // TODO
    printf("WiFi: Ping %s... ", EMONCMS_HOST);
    int res = wifi.ping(EMONCMS_HOST);
    printf((res > 0) ? "OK\n" : "ERROR\n");
    return res;
}

void wifi_setup_internet_conn()
{
    int res = 0;

    printf("WiFi: Setting normal transmission mode... ");
    res = wifi.set_ip_mode(ESP32_IP_MODE_NORMAL);
    printf((res > 0) ? "OK\n" : "ERROR\n");

    printf("WiFi: Setting single connection mode... ");
    res = wifi.set_single();
    printf((res > 0) ? "OK\n" : "ERROR\n");
}

int wifi_send_emoncms_data()
{
    char url[500];
    int res = 0;

    printf("WiFi: Starting TCP connection to %s:%s ... ", EMONCMS_HOST, "80");
    res = wifi.start_TCP_conn(EMONCMS_HOST, "80", false);
    printf((res > 0) ? "OK\n" : "ERROR\n");

    if (res <= 0) {
        wifi.close_TCP_conn();
        return 0;
    }

    // create link
    sprintf(url, "/emoncms/input/post?node=%s&json={"
        "vSolar:%.2f,vBat:%.2f,iBat:%.2f,iLoad:%.2f,"
        "dcdcEn:%d,loadEn:%d,chgState:%d,tempInt:%.1f,nFullChg:%d,nDeepDis:%d,"
        "eInputDay_Wh:%.2f,eOutputDay_Wh:%.2f,SOC:%d,day:%d,SOH:%d,qDis_Ah:%.3f,qBatUse_Ah:%.2f}",
        EMONCMS_NODE, hs_port.voltage, ls_port.voltage, ls_port.current, load.current,
        half_bridge_enabled(), load.enabled, bat.state, dcdc.temp_mosfets, bat.num_full_charges, bat.num_deep_discharges,
        bat.input_Wh_day, bat.output_Wh_day, bat.soc, log_data.day_counter, bat.soh, bat.discharged_Ah, bat.useable_capacity);
    strcat(url, "&apikey=" EMONCMS_APIKEY);
    //printf("%s\n", url);

    wait(0.1);
    printf("WiFi: Sending data... ");
    res = wifi.send_URL(url, (char *)EMONCMS_HOST);
    printf((res > 0) ? "OK\n" : "ERROR\n");

    wifi.close_TCP_conn();

    return 0;
}

void wifi_init(void)
{
    int res = 0;

    //wifi_enable = 0;
    //wait(0.5);
    wifi_enable = 1;

    printf("WiFi: Resetting wifi module... ");
    res = wifi.reset();
    printf((res > 0) ? "OK\n" : "ERROR\n");

    printf("WiFi: ESP8266 module firmware... \n");
    wifi.print_firmware();

    printf("WiFi: Setting wifi station mode... ");
    res = wifi.set_wifi_mode(ESP32_WIFI_MODE_STATION);
    printf((res > 0) ? "OK\n" : "ERROR\n");

    //printf("WiFi: Listing APs... \n");
    //char buf[1000];
    //wifi.list_APs(buf, sizeof(buf));
    //printf(buf);

    wifi_connect_ap();
}

void wifi_process()
{
    static wifi_states state = STATE_WIFI_INIT;

    if (time(NULL) % 5 == 0) {

        printf("WiFi state: %d\n", state);

        switch (state) {
        case STATE_WIFI_INIT:
            if (wifi_connect_ap()) {
                state = STATE_WIFI_CONN;
            }
            break;
        case STATE_WIFI_CONN:
            if (wifi_check_ap_connected()) {        // check if still connected to AP
                if (wifi_check_lan_connected()) {   // check if already got IP address
                    state = STATE_LAN_CONN;
                }
            }
            else {
                state = STATE_WIFI_INIT;
            }
            break;
        case STATE_LAN_CONN:
            if (wifi_check_lan_connected()) {       // check if still got a local IP
                wifi_setup_internet_conn();
                state = STATE_INTERNET_CONN;
            }
            else {
                state = STATE_WIFI_CONN;
            }
            break;
        case STATE_INTERNET_CONN:
            if (wifi_check_internet_connected()) {
#if EMONCMS_ENABLED == true
                wifi_send_emoncms_data();
#endif
            }
            else {
                // ToDo
                //wifi_init();    // reset
                //state = STATE_WIFI_CONN;
            }
            break;
        }

    }
}

#endif /* WIFI_ENABLED */
