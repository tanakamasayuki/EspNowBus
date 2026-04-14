#pragma once

#include <Arduino.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>

#if !CONFIG_LWIP_PPP_SUPPORT
#error "This example requires CONFIG_LWIP_PPP_SUPPORT"
#endif

#include <lwip/netif.h>
#include <netif/ppp/pppapi.h>
#include <netif/ppp/pppos.h>

class PPPoSNetif
{
public:
    struct Config
    {
        size_t rxBufferSize = 256;
        bool setAsDefault = true;
        Print *logger = nullptr;
    };

    PPPoSNetif() = default;

    bool begin(Stream &io);
    bool begin(Stream &io, const Config &cfg);
    void end();
    void poll();

    bool started() const;
    bool connected() const;
    esp_netif_t *netif() const;

private:
    Stream *io_ = nullptr;
    Config config_{};
    bool started_ = false;
    bool connected_ = false;
    uint8_t *rxBuffer_ = nullptr;

    esp_netif_t *netif_ = nullptr;
    struct netif *lwipNetif_ = nullptr;
    ppp_pcb *pcb_ = nullptr;

    static u32_t outputCallback(ppp_pcb *pcb, const void *data, u32_t len, void *ctx);
    static void statusCallback(ppp_pcb *pcb, int errCode, void *ctx);

    void logf(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
};
