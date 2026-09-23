#include "SyncTimer.h"

SyncTimer* SyncTimer::_instance = nullptr;

SyncTimer::SyncTimer(uint8_t inputPin)
    : _inputPin(inputPin)
    , _timer(nullptr)
    , _timerNum(1)
    , _lastEdgeTime(0)
    , _period(0)
    , _synchronized(false)
    , _standaloneMode(false)
    , _divider(1)
    , _ppqn(1)
    , _tickCount(0)
    , _callbackPending(true)
    , _callback(nullptr)
    , _tickPeriod(0)
    , _lastTickTime(0)
{
    _instance = this;
}

bool SyncTimer::begin(uint16_t divider, TimerCallback callback, uint16_t ppqn) {
    if (divider == 0) return false;
    if (ppqn == 0) return false;           // Validate PPQN
    _divider = divider;
    _callback = callback;
    _ppqn = ppqn; 
    _standaloneMode = false;      // NEW: external mode
    
    pinMode(_inputPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_inputPin), onExternalRisingEdge, RISING);
    
    _timer = timerBegin(_timerNum, 80, true);
    if (!_timer) return false;
    
    timerAttachInterrupt(_timer, onTimerTick, true);
    timerStop(_timer);
    timerAlarmDisable(_timer);
    
    return true;
}

void SyncTimer::destroy() {
    if (_timer) {
        timerStop(_timer);
        timerAlarmDisable(_timer);
        timerDetachInterrupt(_timer);
        timerEnd(_timer);
        _timer = nullptr;
    }
    
    detachInterrupt(digitalPinToInterrupt(_inputPin));
    _synchronized = false;
    _standaloneMode = false;
    _callbackPending = false;
    _tickCount = 0;
}

// NEW: Standalone mode without external clock
bool SyncTimer::beginStandalone(uint16_t divider, TimerCallback callback, float frequencyHz) {
    if (divider == 0) return false;
    if (frequencyHz <= 0 || frequencyHz > 1000) return false;  // Limit to 10kHz max
    
    _divider = divider;
    _callback = callback;
    _standaloneMode = true;       // NEW: standalone mode flag
    _synchronized = true;         // In standalone mode, always synchronized
    
    // Detach any external interrupt (not needed)
    detachInterrupt(digitalPinToInterrupt(_inputPin));
    
    // Calculate timer period for the desired frequency
    // Timer tick rate = frequencyHz * divider
    // Period between callbacks = 1 / (frequencyHz * divider) in seconds
    // In microseconds: 1,000,000 / (frequencyHz * divider)
    uint64_t tickPeriodUs = 1000000 / (frequencyHz * divider);
    
    if (tickPeriodUs == 0) return false;
    
    // Reset tick count
    _tickCount = 0;
    _callbackPending = false;
    
    _timer = timerBegin(_timerNum, 80, true);
    if (!_timer) return false;
    
    timerAttachInterrupt(_timer, onTimerTick, true);
    timerWrite(_timer, 0);
    timerAlarmWrite(_timer, tickPeriodUs, true);
    timerAlarmEnable(_timer);
    timerStart(_timer);
    
    return true;
}

void IRAM_ATTR SyncTimer::onExternalRisingEdge() {
    if (!_instance) return;
    
    // Skip if in standalone mode
    if (_instance->_standaloneMode) return;
    
    uint64_t now = micros();
    
    if (_instance->_lastEdgeTime != 0) {
        uint64_t pulsePeriod = now - _instance->_lastEdgeTime;  // Time between external pulses
        
        if (pulsePeriod >= 200000 / _instance->_ppqn && pulsePeriod <= 10000000) {
            // Store the pulse period (time between external clock pulses)
            _instance->_period = pulsePeriod;
            _instance->_synchronized = true;
            
            // Calculate quarter note period = pulsePeriod * PPQN
            // Because each pulse is 1/PPQN of a quarter note
            uint64_t quarterNotePeriod = pulsePeriod * _instance->_ppqn;
            
            // Calculate timer interval: quarter note period divided by divider
            uint64_t intervalUs = quarterNotePeriod / _instance->_divider;
            
            if (intervalUs > 0) {
                timerStop(_instance->_timer);
                timerAlarmDisable(_instance->_timer);
                timerWrite(_instance->_timer, 0);
                timerAlarmWrite(_instance->_timer, intervalUs, true);
                timerAlarmEnable(_instance->_timer);
                timerStart(_instance->_timer);
            }
            
            // Reset tick count for new period
            _instance->_tickCount = 0;
            
            // If callback pending (missed callbacks), trigger tick 0 now
            if (_instance->_callbackPending && _instance->_callback) {
                _instance->_callback(0);
            }
        }
    }

    _instance->_lastEdgeTime = now;
    // Always set pending true for next period
    _instance->_callbackPending = true;
}

void IRAM_ATTR SyncTimer::onTimerTick() {
    if (!_instance) return;

    uint64_t now = micros();

    _instance->_tickPeriod = now - _instance->_lastTickTime;
    _instance->_lastTickTime = now;
    if(!_instance->_standaloneMode){
        _instance->_tickCount++;
    }
    
    if (_instance->_tickCount <= _instance->_divider) {
        if (_instance->_callback) {
            _instance->_callback(_instance->_tickCount);
        }
    }
    if(_instance->_standaloneMode){
        _instance->_tickCount++;
    }
        
    // When we complete all ticks, clear pending flag
    if (_instance->_tickCount >= _instance->_divider) {
        _instance->_callbackPending = false;
        
        // In standalone mode, reset tick count and continue
        if (_instance->_standaloneMode) {
            _instance->_tickCount = 0;
        } else {
            timerStop(_instance->_timer);
            timerAlarmDisable(_instance->_timer);
        }
    }
}

bool SyncTimer::isSynchronized() {
    // In standalone mode, always return true
    if (_standaloneMode) return true;
    return _synchronized;
}

uint16_t SyncTimer::getDivider() {
    return _divider;
}

uint16_t SyncTimer::getTickCount() {
    return _tickCount;
}