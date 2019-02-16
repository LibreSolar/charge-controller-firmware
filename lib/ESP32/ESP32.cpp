/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

#include "ESP32.h"
#include "mbed.h"

ESP32::ESP32(UARTSerial &s) :
    _serial(s),
    _at(&_serial, "\r\n", 200, 4000)
{

}

ESP32::~ESP32()
{ }

// Soft reset of ESP32
int ESP32::reset(void)
{
    _at.send("AT+RST");

    char buf[100];
    _at.read(buf, sizeof(buf));
    printf(buf);

    return _at.recv("ready");
}

void ESP32::print_firmware()
{
    char buf[100];
    _at.send("AT+GMR");
    _at.read(buf, sizeof(buf));
    printf(buf);
}

int ESP32::set_wifi_mode(ESP32_wifi_mode_t mode)
{
    _at.flush();
    _at.send("AT+CWMODE=%d", mode);
    return _at.recv("OK");
}

void ESP32::list_APs(char *buf, int size)
{
    _at.set_timeout(5000);  // use long timeout
    _at.send("AT+CWLAP");
    _at.read(buf, size);
}

int ESP32::join_AP(const char *ssid, const char *pwd)
{
    _at.set_timeout(2000);
    _at.send("AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    char buf[100];
    _at.read(buf, sizeof(buf));
    printf(buf);

    return _at.recv("OK");
}

int ESP32::quit_AP(void)
{
    _at.send("AT+CWQAP");
    return _at.recv("OK");
}

int ESP32::get_IP(char *ip, int size)
{
    int ip_arr[4] = {0};

    _at.set_timeout(500);
    _at.flush();
    _at.send("AT+CIFSR");
    // valid answer:
    // +CIFSR:STAIP,"192.168.178.50"
    // +CIFSR:STAMAC,"30:ae:a4:c3:70:88"
    //
    // OK

    // hack to obtain IP address, as _at.recv seems to contain a bug (it returns 192.168.0.4 instead of 192.168.0.43)
    char buf[100];
    char *ip_start = buf;
    _at.read(buf, sizeof(buf));
    //printf(buf);
    for (unsigned int i = 0; i < sizeof(buf); i++) {
        if (strncmp(&buf[i], "busy now", 8) == 0) {
            printf("IP busy now!!\n");
            snprintf(ip, size, "ERROR");
            _at.flush();
            return 0;
        }
        else if (buf[i] < '0' || buf[i] > '9') {   // find first numeric character
            ip_start++;
        }
        else
            break;
    }
    int res = sscanf(ip_start, "%d.%d.%d.%d", &ip_arr[0], &ip_arr[1], &ip_arr[2], &ip_arr[3]);

    // TODO: figure out bug in _at.recv and correct it
    //int res = _at.recv("%d.%d.%d.%d\r", &ip_arr[0], &ip_arr[1], &ip_arr[2], &ip_arr[3]);

    if (res > 0)
        snprintf(ip, size, "%d.%d.%d.%d", ip_arr[0], ip_arr[1], ip_arr[2], ip_arr[3]);
    else
        snprintf(ip, size, "ERROR");

    return res;
}

int ESP32::ping(const char *ip)
{
    _at.set_timeout(2000);
    _at.send("AT+PING=\"%s\"", ip);
    return _at.recv("OK");
}

int ESP32::set_single()
{
    _at.send("AT+CIPMUX=0");
    return _at.recv("OK");
}

int ESP32::set_multiple()
{
    _at.send("AT+CIPMUX=1");
    return _at.recv("OK");
}

int ESP32::get_conn_status()
{
    int status = -1;
    _at.set_timeout(2000);
    _at.flush();
    _at.send("AT+CIPSTATUS");
    //char buf[100];
    //_at.read(buf, sizeof(buf));
    //printf(buf);
    _at.recv("STATUS:%d", &status);
    return status;
}

int ESP32::set_ip_mode(ESP32_ip_mode_t mode)
{
    _at.set_timeout(500);
    _at.send("AT+CIPMODE=%d", mode);
    return _at.recv("OK");
}

int ESP32::start_TCP_conn(const char *ip, const char *port, bool ssl)
{
    _at.set_timeout(2000);
    if (ssl) {
        printf("Setting SSL conf... ");
        _at.send("AT+CIPSSLCCONF=0");
        int res = _at.recv("OK");
        printf((res > 0) ? "OK\n" : "ERROR\n");
        _at.send("AT+CIPSTART=\"SSL\",\"%s\",%s", ip, port);
    }
    else {
        _at.send("AT+CIPSTART=\"TCP\",\"%s\",%s", ip, port);
    }
    return _at.recv("OK");
}

int ESP32::close_TCP_conn()
{
    _at.set_timeout(500);
    _at.send("AT+CIPCLOSE");
    return _at.recv("OK");
}

int ESP32::send_URL(char *url, char *host)
{
    char request[500];
    snprintf(request, sizeof(request), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url, host);

    _at.send("AT+CIPSEND=%d", strlen(request) + 1);
    _at.recv("> ");

    _at.send(request);
    return _at.recv("SEND OK");
}

int ESP32::send_TCP_data(uint8_t *data, int length)
{
    _at.send("AT+CIPSEND=%d", length);
    _at.recv("> ");

    for (int i = 0; i < length; i++) {
        _at.putc(data[i]);
    }
    return _at.recv("SEND OK");
}

int ESP32::start_TCP_server(int port)
{
    _at.set_timeout(500);
    _at.send("AT+CIPSERVER=1,%d", port);
    return _at.recv("OK");
}

int ESP32::close_TCP_server()
{
    _at.set_timeout(500);
    _at.send("AT+CIPSERVER=0");
    return _at.recv("OK");
}
