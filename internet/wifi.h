#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WIFI_BASE 0xE0000000
#define WIFI_CMD (WIFI_BASE + 0x00)
#define WIFI_STATUS (WIFI_BASE + 0x04)
#define WIFI_SSID (WIFI_BASE + 0x08)
#define WIFI_PSK (WIFI_BASE + 0x48)
#define WIFI_MAC (WIFI_BASE + 0x88)
#define WIFI_IP (WIFI_BASE + 0x90)
#define WIFI_SIGNAL (WIFI_BASE + 0x94)

#define CMD_SCAN 0x01
#define CMD_CONNECT 0x02
#define CMD_DISCONNECT 0x03
#define CMD_GET_IP 0x04
#define CMD_PING 0x05

#define STATUS_IDLE 0x00
#define STATUS_SCANNING 0x01
#define STATUS_CONNECTING 0x02
#define STATUS_CONNECTED 0x03
#define STATUS_FAILED 0x04
#define STATUS_DISCONNECTED 0x05

int wifi_connect(const char* ssid, const char* password);
void wifi_disconnect(void);
uint32_t wifi_get_ip_addr(void);
uint32_t wifi_get_signal_strength(void);
bool wifi_is_connected(void);
