#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "wifi.h"
#include "../../vm_pages.h"

static uint64_t wifi_mapped_base = 0;
static bool wifi_initialized = false;

static void wifi_write_str(uint32_t offset, const char* str, size_t max_len) {
    volatile char* reg = (volatile char*)(wifi_mapped_base + offset);
    size_t len = strlen(str);
    if (len >= max_len) len = max_len - 1;
    
    for (size_t i = 0; i < len; i++) {
        reg[i] = str[i];
    }
    reg[len] = 0;
}

static uint32_t wifi_read32(uint32_t offset) {
    return *(volatile uint32_t*)(wifi_mapped_base + offset);
}

static void wifi_write32(uint32_t offset, uint32_t val) {
    *(volatile uint32_t*)(wifi_mapped_base + offset) = val;
}

static void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 10000; i++);
}

static bool wifi_wait_status(uint32_t target, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        uint32_t status = wifi_read32(0x04);
        if (status == target) return true;
        if (status == STATUS_FAILED) return false;
        delay_ms(100);
        elapsed += 100;
    }
    return false;
}

static void wifi_reset_chip(void) {
    wifi_write32(0x00, 0xFF);
    delay_ms(100);
    wifi_write32(0x00, 0x00);
    delay_ms(500);
}

static bool wifi_init_hardware(void) {
    if (!wifi_initialized) {
        // map wifi device memory using vm subsystem
        wifi_mapped_base = 0x10000000;  // virtual address
        if (vm_map(wifi_mapped_base, WIFI_BASE, PROT_READ | PROT_WRITE) != 0) {
            return false;
        }
        wifi_initialized = true;
    }
    
    wifi_reset_chip();
    
    if (wifi_read32(0x04) != STATUS_IDLE) {
        return false;
    }
    
    uint32_t mac_low = wifi_read32(0x88);
    uint32_t mac_high = wifi_read32(0x8C);
    
    if (mac_low == 0 && mac_high == 0) {
        wifi_write32(0x88, 0x12345678);
        wifi_write32(0x8C, 0x9ABC0000);
    }
    
    return true;
}

static bool wifi_scan_network(const char* ssid) {
    wifi_write_str(0x08, ssid, 32);
    wifi_write32(0x00, CMD_SCAN);
    
    if (!wifi_wait_status(STATUS_SCANNING, 5000)) return false;
    if (!wifi_wait_status(STATUS_IDLE, 15000)) return false;
    
    return true;
}

static bool wifi_authenticate(const char* ssid, const char* password) {
    wifi_write_str(0x08, ssid, 32);
    wifi_write_str(0x48, password ? password : "", 64);
    
    wifi_write32(0x00, CMD_CONNECT);
    
    if (!wifi_wait_status(STATUS_CONNECTING, 5000)) return false;
    if (!wifi_wait_status(STATUS_CONNECTED, 30000)) return false;
    
    return true;
}

static bool wifi_get_ip(void) {
    wifi_write32(0x00, CMD_GET_IP);
    delay_ms(2000);
    
    uint32_t ip = wifi_read32(0x90);
    return ip != 0;
}

static bool wifi_test_connection(void) {
    wifi_write32(0x90, 0x08080808);
    wifi_write32(0x00, CMD_PING);
    delay_ms(3000);
    
    uint32_t status = wifi_read32(0x04);
    return status == STATUS_CONNECTED;
}

int wifi_connect(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) return -1;
    
    if (!wifi_init_hardware()) return -2;
    
    if (wifi_read32(0x04) == STATUS_CONNECTED) {
        wifi_write32(0x00, CMD_DISCONNECT);
        wifi_wait_status(STATUS_IDLE, 5000);
    }
    
    if (!wifi_scan_network(ssid)) return -3;
    
    if (!wifi_authenticate(ssid, password)) return -4;
    
    if (!wifi_get_ip()) return -5;
    
    if (!wifi_test_connection()) return -6;
    
    return 0;
}

void wifi_disconnect(void) {
    if (!wifi_initialized) return;
    
    wifi_write32(0x00, CMD_DISCONNECT);
    wifi_wait_status(STATUS_IDLE, 5000);
}

uint32_t wifi_get_ip_addr(void) {
    if (!wifi_initialized) return 0;
    return wifi_read32(0x90);
}

uint32_t wifi_get_signal_strength(void) {
    if (!wifi_initialized) return 0;
    return wifi_read32(0x94);
}

bool wifi_is_connected(void) {
    if (!wifi_initialized) return false;
    return wifi_read32(0x04) == STATUS_CONNECTED;
}
