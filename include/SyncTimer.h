#ifndef SYNC_TIMER_H
#define SYNC_TIMER_H

#include <Arduino.h>

typedef void (*TimerCallback)(uint16_t tickCount);

class SyncTimer {
public:
    SyncTimer(uint8_t inputPin);
    void destroy();
    
    // Normal mode: synchronize to external clock pulse
    bool begin(uint16_t divider, TimerCallback callback, uint16_t ppqn = 1);

    // Standalone mode: run without external clock (fixed frequency)
    // frequencyHz: desired callback frequency in Hz (e.g., 1000 for 1kHz)
    bool beginStandalone(uint16_t divider, TimerCallback callback, float frequencyHz);
    
    bool isSynchronized();
    uint64_t getTickPeriod() const { return _tickPeriod; };
    uint64_t getBasePeriod() const { return _period; };
    uint16_t getDivider();
    uint16_t getTickCount();

private:
    uint8_t _inputPin;
    hw_timer_t* _timer;
    uint8_t _timerNum;
    uint16_t _ppqn;  // Pulses per quarter note (default 1)
    
    volatile uint64_t _lastEdgeTime;
    volatile uint64_t _lastTickTime;
    volatile uint64_t _period;
    volatile uint64_t _tickPeriod;
    volatile bool _synchronized;
    volatile bool _standaloneMode;      // NEW: true if running without external clock
    
    uint16_t _divider;
    volatile uint16_t _tickCount;
    volatile bool _callbackPending;
    
    TimerCallback _callback;
    
    static SyncTimer* _instance;
    
    static void IRAM_ATTR onExternalRisingEdge();
    static void IRAM_ATTR onTimerTick();

};

#endif