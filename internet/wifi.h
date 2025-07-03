#ifndef WIFI_H
#define WIFI_H
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int wifi_connect(const char* ssid, const char* password);

#ifdef __cplusplus
}
#endif

#endif
