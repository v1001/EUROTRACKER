#include "UserInput.h"

// Static instance for callbacks
UserInput* UserInput::instance = nullptr;

UserInput::UserInput(uint8_t deadzone_percent) : 
    last_enc_a_pos(0),
    last_enc_b_pos(0),
    last_enc_a_change(0),
    last_enc_b_change(0),
    btn_joystick(nullptr),
    btn_enc_a(nullptr),
    btn_enc_b(nullptr),
    btn_save(nullptr),
    joystick_x_center(2048),
    joystick_y_center(2048),
    joystick_deadzone(deadzone_percent)
{
    instance = this;
    
    // Initialize joystick state
    joystick = {0};
    encoder_a = {0, 0, false, 0};
    encoder_b = {0, 0, false, 0};
    
    // Initialize button states with pins
    joystick_button = {JOYSTICK_BUTTON_PIN, false, false, false, false, false, false, 0, 0, 0};
    encoder_a_button = {ENC_A_BUTTON_PIN, false, false, false, false, false, false, 0, 0, 0};
    encoder_b_button = {ENC_B_BUTTON_PIN, false, false, false, false, false, false, 0, 0, 0};
    save_button = {SAVE_BUTTON_PIN, false, false, false, false, false, false, 0, 0, 0};
}

UserInput::~UserInput() {
    // Clean up OneButton instances
    delete btn_joystick;
    delete btn_enc_a;
    delete btn_enc_b;
    delete btn_save;
    instance = nullptr;
}

void UserInput::begin() {
    // Initialize pins
    pinMode(JOYSTICK_X_PIN, INPUT);
    pinMode(JOYSTICK_Y_PIN, INPUT);
    
    pinMode(ENC_A_PIN_A, INPUT_PULLUP);
    pinMode(ENC_A_PIN_B, INPUT_PULLUP);
    pinMode(ENC_B_PIN_A, INPUT_PULLUP);
    pinMode(ENC_B_PIN_B, INPUT_PULLUP);
    
    // Initialize hardware encoders - Use SINGLE EDGE mode for 1 count per detent
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    
    // Encoder A
    enc_a.attachSingleEdge(ENC_A_PIN_A, ENC_A_PIN_B);
    enc_a.setFilter(1023);
    enc_a.clearCount();
    
    // Encoder B
    enc_b.attachSingleEdge(ENC_B_PIN_A, ENC_B_PIN_B);  // Swapped order
    enc_b.setFilter(1023);
    enc_b.clearCount();
    
    // Initialize OneButton instances
    initButton(btn_joystick, JOYSTICK_BUTTON_PIN,
               onJoystickClick, onJoystickDoubleClick, onJoystickLongPress);
    
    initButton(btn_enc_a, ENC_A_BUTTON_PIN,
               onEncoderAClick, onEncoderADoubleClick, onEncoderALongPress);
    
    initButton(btn_enc_b, ENC_B_BUTTON_PIN,
               onEncoderBClick, onEncoderBDoubleClick, onEncoderBLongPress);
    
    initButton(btn_save, SAVE_BUTTON_PIN,
               onSaveClick, onSaveDoubleClick, onSaveLongPress);
    
    // Calibrate joystick
    calibrateJoystick();
}

void UserInput::initButton(OneButton* &btn, int pin, 
                           void (*click)(), 
                           void (*doubleClick)(), 
                           void (*longPress)()) {
    btn = new OneButton(pin, true, true);
    btn->setClickMs(button_click_time);
    btn->setPressMs(button_press_time);
    btn->setDebounceMs(button_debounce_time);
    
    if (click) btn->attachClick(click);
    if (doubleClick) btn->attachDoubleClick(doubleClick);
    if (longPress) btn->attachLongPressStart(longPress);
}

void UserInput::readInputs() {
    uint64_t current_time = millis();
    
    // Read joystick
    joystick.x_raw = analogRead(JOYSTICK_X_PIN);
    joystick.y_raw = analogRead(JOYSTICK_Y_PIN);
    
    // Convert to -100 to 100 range using calibrated center
    joystick.x_position = map(joystick.x_raw - joystick_x_center, -2048, 2048, -100, 100);
    joystick.y_position = map(joystick.y_raw - joystick_y_center, -2048, 2048, -100, 100);
    joystick.y_position = -joystick.y_position;  // Inverted Y-axis
    
    // Apply deadzone
    if (abs(joystick.x_position) < (int)joystick_deadzone) joystick.x_position = 0;
    if (abs(joystick.y_position) < (int)joystick_deadzone) joystick.y_position = 0;
    
    joystick.is_centered = (joystick.x_position == 0 && joystick.y_position == 0);
    
    // Update encoder A
    long current_pos_a = enc_a.getCount();
    encoder_a.position = current_pos_a;  // INVERTED
    
    if (current_pos_a != last_enc_a_pos) {
        int new_dir = (current_pos_a > last_enc_a_pos) ? 1 : -1;
        
        if (new_dir != encoder_a.rotation_dir) {
            if ((current_time - last_enc_a_change) >= ENCODER_DEBOUNCE_MS) {
                encoder_a.rotation_dir = new_dir;
                last_enc_a_change = current_time;
            }
        } else {
            encoder_a.rotation_dir = new_dir;
        }
        
        encoder_a.rotating = true;
        encoder_a.last_update = current_time;
        last_enc_a_pos = current_pos_a;
    } else {
        if (encoder_a.rotating && (current_time - encoder_a.last_update) > 100) {
            encoder_a.rotating = false;
            encoder_a.rotation_dir = 0;
        }
    }
    
    // Update encoder B
    long current_pos_b = enc_b.getCount();
    encoder_b.position = current_pos_b;  // INVERTED
    
    if (current_pos_b != last_enc_b_pos) {
        int new_dir = (current_pos_b > last_enc_b_pos) ? 1 : -1;
        
        if (new_dir != encoder_b.rotation_dir) {
            if ((current_time - last_enc_b_change) >= ENCODER_DEBOUNCE_MS) {
                encoder_b.rotation_dir = new_dir;
                last_enc_b_change = current_time;
            }
        } else {
            encoder_b.rotation_dir = new_dir;
        }
        
        encoder_b.rotating = true;
        encoder_b.last_update = current_time;
        last_enc_b_pos = current_pos_b;
    } else {
        if (encoder_b.rotating && (current_time - encoder_b.last_update) > 100) {
            encoder_b.rotating = false;
            encoder_b.rotation_dir = 0;
        }
    }
    
    // Update OneButton instances (must be called frequently)
    if (btn_joystick) btn_joystick->tick();
    if (btn_enc_a) btn_enc_a->tick();
    if (btn_enc_b) btn_enc_b->tick();
    if (btn_save) btn_save->tick();
    
    // Update button states from OneButton
    if (btn_joystick) updateButtonState(joystick_button, *btn_joystick, current_time);
    if (btn_enc_a) updateButtonState(encoder_a_button, *btn_enc_a, current_time);
    if (btn_enc_b) updateButtonState(encoder_b_button, *btn_enc_b, current_time);
    if (btn_save) updateButtonState(save_button, *btn_save, current_time);
}

void UserInput::updateButtonState(ButtonState &state, OneButton &button, uint64_t current_time) {
    // Clear one-shot events from previous cycle
    clearButtonEvents(state);
    
    // Read pin directly from state
    bool is_pressed = !digitalRead(state.pin);
    
    if (is_pressed != state.pressed) {
        state.pressed = is_pressed;
        if (is_pressed) {
            state.just_pressed = true;
            state.last_event_time = current_time;
        } else {
            state.just_released = true;
            state.last_event_time = current_time;
        }
    }
    
    // Update press duration
    if (state.pressed) {
        state.press_duration = current_time - state.last_event_time;
    } else {
        state.press_duration = 0;
    }
}

void UserInput::clearButtonEvents(ButtonState &state) {
    state.just_pressed = false;
    state.just_released = false;
    state.single_click = false;
    state.double_click = false;
    state.long_press = false;
}

// Joystick Button Callbacks
void UserInput::onJoystickClick() {
    if (instance) {
        instance->joystick_button.single_click = true;
        instance->joystick_button.press_count++;
        instance->joystick_button.last_event_time = millis();
    }
}

void UserInput::onJoystickDoubleClick() {
    if (instance) {
        instance->joystick_button.double_click = true;
        instance->joystick_button.press_count++;
        instance->joystick_button.last_event_time = millis();
    }
}

void UserInput::onJoystickLongPress() {
    if (instance) {
        instance->joystick_button.long_press = true;
        instance->joystick_button.press_count++;
        instance->joystick_button.last_event_time = millis();
    }
}

// Encoder A Button Callbacks
void UserInput::onEncoderAClick() {
    if (instance) {
        instance->encoder_a_button.single_click = true;
        instance->encoder_a_button.press_count++;
        instance->encoder_a_button.last_event_time = millis();
    }
}

void UserInput::onEncoderADoubleClick() {
    if (instance) {
        instance->encoder_a_button.double_click = true;
        instance->encoder_a_button.press_count++;
        instance->encoder_a_button.last_event_time = millis();
    }
}

void UserInput::onEncoderALongPress() {
    if (instance) {
        instance->encoder_a_button.long_press = true;
        instance->encoder_a_button.press_count++;
        instance->encoder_a_button.last_event_time = millis();
    }
}

// Encoder B Button Callbacks
void UserInput::onEncoderBClick() {
    if (instance) {
        instance->encoder_b_button.single_click = true;
        instance->encoder_b_button.press_count++;
        instance->encoder_b_button.last_event_time = millis();
    }
}

void UserInput::onEncoderBDoubleClick() {
    if (instance) {
        instance->encoder_b_button.double_click = true;
        instance->encoder_b_button.press_count++;
        instance->encoder_b_button.last_event_time = millis();
    }
}

void UserInput::onEncoderBLongPress() {
    if (instance) {
        instance->encoder_b_button.long_press = true;
        instance->encoder_b_button.press_count++;
        instance->encoder_b_button.last_event_time = millis();
    }
}

// Save Button Callbacks
void UserInput::onSaveClick() {
    if (instance) {
        instance->save_button.single_click = true;
        instance->save_button.press_count++;
        instance->save_button.last_event_time = millis();
    }
}

void UserInput::onSaveDoubleClick() {
    if (instance) {
        instance->save_button.double_click = true;
        instance->save_button.press_count++;
        instance->save_button.last_event_time = millis();
    }
}

void UserInput::onSaveLongPress() {
    if (instance) {
        instance->save_button.long_press = true;
        instance->save_button.press_count++;
        instance->save_button.last_event_time = millis();
    }
}

void UserInput::calibrateJoystick() {
    // Average multiple readings for better calibration
    const int SAMPLES = 10;
    long sum_x = 0, sum_y = 0;
    
    for (int i = 0; i < SAMPLES; i++) {
        sum_x += analogRead(JOYSTICK_X_PIN);
        sum_y += analogRead(JOYSTICK_Y_PIN);
        delay(5);
    }
    
    joystick_x_center = sum_x / SAMPLES;
    joystick_y_center = sum_y / SAMPLES;
}

void UserInput::getCenterCoordinates(int &x_center, int &y_center) {
    x_center = joystick_x_center;
    y_center = joystick_y_center;
}

bool UserInput::getCenterCoordinates(int *x_center, int *y_center) {
    if (x_center && y_center) {
        *x_center = joystick_x_center;
        *y_center = joystick_y_center;
        return true;
    }
    return false;
}