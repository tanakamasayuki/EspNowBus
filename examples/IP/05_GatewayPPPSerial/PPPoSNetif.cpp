#include "PPPoSNetif.h"

#include <stdarg.h>

bool PPPoSNetif::begin(Stream &io)
{
    return begin(io, Config{});
}

bool PPPoSNetif::begin(Stream &io, const Config &cfg)
{
    end();
    io_ = &io;
    config_ = cfg;

    if (config_.rxBufferSize == 0)
        config_.rxBufferSize = 256;

    rxBuffer_ = static_cast<uint8_t *>(malloc(config_.rxBufferSize));
    if (!rxBuffer_)
        return false;

    esp_netif_init();

    esp_netif_config_t netifCfg = ESP_NETIF_DEFAULT_PPP();
    netif_ = esp_netif_new(&netifCfg);
    if (!netif_)
    {
        end();
        return false;
    }

    lwipNetif_ = static_cast<struct netif *>(esp_netif_get_netif_impl(netif_));
    if (!lwipNetif_)
    {
        end();
        return false;
    }

    pcb_ = pppapi_pppos_create(lwipNetif_, &PPPoSNetif::outputCallback, &PPPoSNetif::statusCallback, this);
    if (!pcb_)
    {
        end();
        return false;
    }

    if (config_.setAsDefault)
    {
        pppapi_set_default(pcb_);
        esp_netif_set_default_netif(netif_);
    }

    err_t err = pppapi_connect(pcb_, 0);
    if (err != ERR_OK)
    {
        end();
        return false;
    }

    started_ = true;
    connected_ = false;
    logf("[PPPoSNetif] started rxBuffer=%u", static_cast<unsigned>(config_.rxBufferSize));
    return true;
}

void PPPoSNetif::end()
{
    if (pcb_)
    {
        pppapi_close(pcb_, 0);
        pppapi_free(pcb_);
        pcb_ = nullptr;
    }

    if (netif_)
    {
        esp_netif_destroy(netif_);
        netif_ = nullptr;
    }

    lwipNetif_ = nullptr;

    if (rxBuffer_)
    {
        free(rxBuffer_);
        rxBuffer_ = nullptr;
    }

    io_ = nullptr;
    started_ = false;
    connected_ = false;
}

void PPPoSNetif::poll()
{
    if (!started_ || !pcb_ || !io_ || !rxBuffer_)
        return;

    size_t rxLen = 0;
    while (io_->available() && rxLen < config_.rxBufferSize)
    {
        int c = io_->read();
        if (c < 0)
            break;
        rxBuffer_[rxLen++] = static_cast<uint8_t>(c);
    }

    if (rxLen > 0)
        pppos_input_tcpip(pcb_, rxBuffer_, rxLen);
}

bool PPPoSNetif::started() const
{
    return started_;
}

bool PPPoSNetif::connected() const
{
    return connected_;
}

esp_netif_t *PPPoSNetif::netif() const
{
    return netif_;
}

u32_t PPPoSNetif::outputCallback(ppp_pcb *pcb, const void *data, u32_t len, void *ctx)
{
    (void)pcb;
    PPPoSNetif *self = static_cast<PPPoSNetif *>(ctx);
    if (!self || !self->io_ || !data || len == 0)
        return 0;

    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    size_t written = self->io_->write(bytes, len);
    return static_cast<u32_t>(written);
}

void PPPoSNetif::statusCallback(ppp_pcb *pcb, int errCode, void *ctx)
{
    (void)pcb;
    PPPoSNetif *self = static_cast<PPPoSNetif *>(ctx);
    if (!self)
        return;

    if (errCode == PPPERR_NONE)
    {
        self->connected_ = true;
        self->logf("[PPPoSNetif] connected");
    }
    else
    {
        self->connected_ = false;
        self->logf("[PPPoSNetif] status err=%d", errCode);
    }
}

void PPPoSNetif::logf(const char *fmt, ...) const
{
    if (!config_.logger || !fmt)
        return;

    char buffer[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    config_.logger->println(buffer);
}
