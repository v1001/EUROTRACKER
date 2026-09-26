#ifndef SYNC_TIMER_H
#define SYNC_TIMER_H

#include <Arduino.h>

typedef void (*TimerCallback)(uint16_t tickCount);

class SyncTimer {
public:
    SyncTimer(uint8_t inputPin);
    void destroy();

    // Normal mode: synchronize to external clock pulse (soft PLL)
    bool begin(uint16_t divider, TimerCallback callback, uint16_t ppqn = 1);

    // Standalone mode: run without external clock (fixed frequency)
    bool beginStandalone(uint16_t divider, TimerCallback callback, float frequencyHz);

    bool isSynchronized();
    uint64_t getTickPeriod() const { return _tickPeriod; };
    uint64_t getBasePeriod() const { return _period; };
    uint16_t getDivider();
    uint16_t getTickCount();

private:
    // State machine
    static const uint8_t SYNC_IDLE   = 0;   // no pulse received, timer stopped
    static const uint8_t SYNC_ARMED  = 1;   // one pulse received, waiting for second
    static const uint8_t SYNC_LOCKED = 2;   // PLL running

    uint8_t _inputPin;
    hw_timer_t* _timer;
    uint8_t _timerNum;
    uint16_t _ppqn;

    volatile uint64_t _lastEdgeTime;    // time of last external pulse (µs)
    volatile uint64_t _lastTickTime;    // time of last internal timer tick (µs)
    volatile uint64_t _period;          // pulse period from last two edges (µs)
    volatile uint64_t _tickPeriod;      // last measured tick period (µs)
    volatile bool _synchronized;
    volatile bool _standaloneMode;

    uint16_t _divider;
    volatile uint16_t _tickCount;       // 0.._divider-1, for the sequencer
    volatile uint8_t  _state;
    volatile uint16_t _ticksSincePulse; // ticks fired since last pulse
    volatile uint32_t _t_int;           // current internal tick period (µs)

    TimerCallback _callback;

    static SyncTimer* _instance;

    static void IRAM_ATTR onExternalRisingEdge();
    static void IRAM_ATTR onTimerTick();
};

#endif