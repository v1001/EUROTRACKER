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
    , _state(SYNC_IDLE)
    , _ticksSincePulse(0)
    , _t_int(0)
    , _callback(nullptr)
    , _tickPeriod(0)
    , _lastTickTime(0)
    , _wrap_corr_accum(0)
{
    _instance = this;
}

bool SyncTimer::begin(uint16_t divider, TimerCallback callback, uint16_t ppqn) {
    if (divider == 0) return false;
    if (ppqn == 0) return false;
    if (divider % ppqn != 0) return false;

    _divider = divider;
    _callback = callback;
    _ppqn = ppqn;
    _standaloneMode = false;
    _state = SYNC_IDLE;
    _synchronized = false;
    _tickCount = 0;
    _ticksSincePulse = 0;
    _t_int = 0;
    _period = 0;
    _lastEdgeTime = 0;
    _lastTickTime = 0;
    _tickPeriod = 0;
    _wrap_corr_accum = 0;

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
    _state = SYNC_IDLE;
    _tickCount = 0;
    _ticksSincePulse = 0;
    _t_int = 0;
    _period = 0;
    _wrap_corr_accum = 0;
}

bool SyncTimer::beginStandalone(uint16_t divider, TimerCallback callback, float frequencyHz) {
    if (divider == 0) return false;
    if (frequencyHz <= 0 || frequencyHz > 1000) return false;

    _divider = divider;
    _callback = callback;
    _standaloneMode = true;
    _synchronized = true;
    _state = SYNC_IDLE;

    detachInterrupt(digitalPinToInterrupt(_inputPin));

    uint64_t tickPeriodUs = 1000000 / (frequencyHz * divider);
    if (tickPeriodUs == 0) return false;

    _tickCount = 0;
    _t_int = (uint32_t)tickPeriodUs;

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
    if (_instance->_standaloneMode) return;

    uint64_t now = micros();

    // IDLE: first pulse. Record and wait for second.
    if (_instance->_state == SYNC_IDLE) {
        _instance->_lastEdgeTime = now;
        _instance->_state = SYNC_ARMED;
        return;
    }

    uint64_t pulsePeriod = now - _instance->_lastEdgeTime;

    // Sanity bounds. Lower bound corresponds to 300 BPM at any PPQN.
    uint32_t minPeriod = 200000UL / _instance->_ppqn;
    if (pulsePeriod < minPeriod || pulsePeriod > 10000000UL) {
        // Out of range: re-arm. Next pulse becomes the new first.
        _instance->_lastEdgeTime = now;
        _instance->_state = SYNC_ARMED;
        _instance->_synchronized = false;
        timerStop(_instance->_timer);
        timerAlarmDisable(_instance->_timer);
        _instance->_tickCount = 0;
        _instance->_ticksSincePulse = 0;
        return;
    }

    // ARMED: second valid pulse. Initialize timer, transition to LOCKED.
    if (_instance->_state == SYNC_ARMED) {
        uint16_t N = _instance->_divider / _instance->_ppqn;
        uint32_t t_int = (uint32_t)(pulsePeriod / N);
        if (t_int == 0) t_int = 1;

        _instance->_t_int = t_int;
        _instance->_period = pulsePeriod;
        _instance->_lastEdgeTime = now;
        _instance->_lastTickTime = now;
        _instance->_tickCount = 0;
        _instance->_ticksSincePulse = 0;
        _instance->_synchronized = true;
        _instance->_state = SYNC_LOCKED;

        timerStop(_instance->_timer);
        timerAlarmDisable(_instance->_timer);
        timerWrite(_instance->_timer, 0);
        timerAlarmWrite(_instance->_timer, t_int, true);
        timerAlarmEnable(_instance->_timer);
        timerStart(_instance->_timer);
        return;
    }

    // LOCKED: PLL update.
    uint32_t t_int = _instance->_t_int;
    uint16_t ticks_since = _instance->_ticksSincePulse;
    uint32_t elapsed = (uint32_t)(now - _instance->_lastTickTime);

    // Phase at pulse, in ticks * 256 (fixed point, 1/256 tick resolution).
    uint64_t phase_256_u = (uint64_t)ticks_since * 256ULL
                         + ((uint64_t)elapsed * 256ULL) / t_int;
    uint32_t phase_256 = (uint32_t)phase_256_u;

    uint16_t N = _instance->_divider / _instance->_ppqn;
    uint32_t target_256 = (uint32_t)N * 256UL;

    // ticks_next_256 = 2 * target_256 - phase_256
    // (target - phase) is the error; ticks_next = N + error = N + target - phase
    //  = 2*target - phase
    int32_t ticks_next_256 = (int32_t)(2UL * target_256) - (int32_t)phase_256;

    // Clamp to reasonable range
    if (ticks_next_256 < (int32_t)(target_256 / 2)) ticks_next_256 = target_256 / 2;
    if (ticks_next_256 > (int32_t)(2UL * target_256)) ticks_next_256 = 2UL * target_256;

    // T_target = pulsePeriod * 256 / ticks_next_256
    uint32_t t_target = (uint32_t)(((uint64_t)pulsePeriod * 256ULL) / (uint32_t)ticks_next_256);

    // Damped update: T_new = (4 * T_int + T_target) / 5  (alpha = 0.2)
    uint32_t t_new = (4UL * t_int + t_target) / 5UL;
    if (t_new == 0) t_new = 1;

    _instance->_t_int = t_new;
    _instance->_period = pulsePeriod;
    _instance->_lastEdgeTime = now;
    _instance->_ticksSincePulse = 0;

    timerStop(_instance->_timer);
    timerAlarmDisable(_instance->_timer);
    timerWrite(_instance->_timer, 0);
    timerAlarmWrite(_instance->_timer, t_new, true);
    timerAlarmEnable(_instance->_timer);
    timerStart(_instance->_timer);
}

void IRAM_ATTR SyncTimer::onTimerTick() {
    if (!_instance) return;

    uint64_t now = micros();

    _instance->_tickPeriod = now - _instance->_lastTickTime;
    _instance->_lastTickTime = now;

    // Increment counter, wrapping at _divider
    _instance->_tickCount++;
    if (_instance->_tickCount >= _instance->_divider) {
        _instance->_tickCount = 0;
    }

    // Fire callback with new counter value
    if (_instance->_callback) {
        _instance->_callback(_instance->_tickCount);
    }

    if (_instance->_standaloneMode) return;

    // External LOCKED mode
    if (_instance->_state == SYNC_LOCKED) {
        _instance->_ticksSincePulse++;

        uint32_t elapsed = (uint32_t)(now - _instance->_lastEdgeTime);
        uint32_t timeout = (uint32_t)(2ULL * _instance->_period);
        if (timeout > 0 && elapsed > timeout) {
            timerStop(_instance->_timer);
            timerAlarmDisable(_instance->_timer);
            _instance->_state = SYNC_IDLE;
            _instance->_synchronized = false;
            _instance->_tickCount = 0;
            _instance->_ticksSincePulse = 0;
            return;
        }

        // --- Wrap-time phase correction ---
        // At each counter wrap (once per quarter note), measure how far the
        // wrap sits from the nearest external pulse. Nudge _t_int by a small,
        // proportional amount so the wrap converges toward the aligned pulse.
        // This is a slow secondary loop and does not touch the PLL math.
        if (_instance->_tickCount == 0 && _instance->_period > 0) {
            uint32_t dt_prev = (uint32_t)(now - _instance->_lastEdgeTime);
            uint32_t period  = (uint32_t)_instance->_period;

            if (dt_prev < period) {
                uint32_t dt_next = period - dt_prev;

                // Signed error in µs.
                //   positive = wrap is late (closer to previous pulse)
                //   negative = wrap is early (closer to next pulse)
                int32_t err_us = (dt_prev < dt_next)
                               ?  (int32_t)dt_prev
                               : -(int32_t)dt_next;

                // Proportional correction; converge over ~8 wraps.
                // Accumulator is in 1/256 µs units to avoid integer dead-band.
                int32_t desired_256 = (err_us * 256) / (192 * 8);
                _instance->_wrap_corr_accum += desired_256;

                int32_t int_part = _instance->_wrap_corr_accum / 256;
                _instance->_wrap_corr_accum -= int_part * 256;

                if (int_part != 0) {
                    int32_t t_signed = (int32_t)_instance->_t_int;
                    t_signed -= int_part;             // late → smaller T
                    if (t_signed < 1)      t_signed = 1;
                    if (t_signed > 100000) t_signed = 100000;
                    _instance->_t_int = (uint32_t)t_signed;
                    timerAlarmWrite(_instance->_timer, _instance->_t_int, true);
                }
            }
        }
    }
}

bool SyncTimer::isSynchronized() {
    if (_standaloneMode) return true;
    return _synchronized;
}

uint16_t SyncTimer::getDivider() {
    return _divider;
}

uint16_t SyncTimer::getTickCount() {
    return _tickCount;
}