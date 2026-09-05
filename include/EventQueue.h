#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <Arduino.h>
#include <queue>
#include <vector>

// ============================================================================
// CV Transition Mode
// ============================================================================



// ============================================================================
// Gate Event
// ============================================================================

struct GateEvent {
    enum Type : uint8_t { GATE_ON, GATE_OFF };
    
    uint64_t timestamp;
    Type type;
    
    GateEvent() : timestamp(0), type(GATE_ON){}
    GateEvent(uint64_t ts, Type t) 
        : timestamp(ts), type(t){}
    
    bool operator>(const GateEvent& other) const {
        return timestamp > other.timestamp;
    }
};

// ============================================================================
// CV Event
// ============================================================================

struct CVEvent {
    enum CVTransitionMode : uint8_t {
        CV_STEP,      // Jump instantly to target value when event executes
        CV_SMOOTH     // Glide/ramp from current to target over time
    };
    uint64_t timestamp;
    uint16_t value;         // 0-4095
    CVTransitionMode transitionMode;
    
    CVEvent() : timestamp(0), value(0), transitionMode(CV_STEP) {}
    CVEvent(uint64_t ts, uint16_t val, CVTransitionMode(t))
        : timestamp(ts), value(val), transitionMode(t){}
    
    bool operator>(const CVEvent& other) const {
        return timestamp > other.timestamp;
    }
};

// ============================================================================
// Gate Queue (single queue for all 6 digital outputs)
// ============================================================================

class GateQueue {
public:
    void push(const GateEvent& e) { _queue.push(e); }
    void push(uint64_t ts, GateEvent::Type t) { 
        _queue.push(GateEvent(ts, t)); 
    }
    void pop() { if (!_queue.empty()) _queue.pop(); }
    const GateEvent& peek() const { return _queue.top(); }
    bool isEmpty() const { return _queue.empty(); }
    size_t size() const { return _queue.size(); }
    void clear() { while (!_queue.empty()) _queue.pop(); }
    
private:
    std::priority_queue<GateEvent, std::vector<GateEvent>, std::greater<GateEvent>> _queue;
};

// ============================================================================
// CV Queue (one per melodic track)
// ============================================================================

class CVQueue {
public:
    void push(const CVEvent& e) { _queue.push(e); }
    void push(uint64_t ts, uint16_t val, CVEvent::CVTransitionMode t) { 
        _queue.push(CVEvent(ts, val, t)); 
    }
    void pop() { if (!_queue.empty()) _queue.pop(); }
    const CVEvent& peek() const { return _queue.top(); }
    bool isEmpty() const { return _queue.empty(); }
    size_t size() const { return _queue.size(); }
    void clear() { while (!_queue.empty()) _queue.pop(); }
    
    // Last executed event timestamp and value (for dynamic glide calculation)
    uint64_t getLastEventTimestamp() const { return _lastEventTimestamp; }
    uint16_t getLastEventValue() const { return _lastEventValue; }
    void setLastEvent(uint64_t timestamp, uint16_t value) {
        _lastEventTimestamp = timestamp;
        _lastEventValue = value;
    }
    
private:
    std::priority_queue<CVEvent, std::vector<CVEvent>, std::greater<CVEvent>> _queue;
    uint64_t _lastEventTimestamp = 0;
    uint16_t _lastEventValue = 0;
};

#endif // EVENT_QUEUE_H