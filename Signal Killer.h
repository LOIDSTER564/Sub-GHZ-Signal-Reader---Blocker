#pragma once
#include <furi.h>
#include <subghz/subghz.h>
#include <notification/notification.h>

typedef struct {
    SubGhz* subghz;
    NotificationApp* notifications;
    uint32_t frequency;
    uint32_t jam_frequency;
    float strength;
    bool scanning;
    bool jamming;
} SignalKiller;