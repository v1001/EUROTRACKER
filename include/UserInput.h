#ifndef USER_INPUT_H
#define USER_INPUT_H

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <OneButton.h>

class UserInput {
public:
    struct JoystickState {
        int x_raw;
        int y_raw;
        int x_position;
        int y_position;
        bool is_centered;
    } joystick;

    struct EncoderState {
        long position;
        int rotation_dir;
        bool rotating;
        uint64_t last_update;
    } encoder_a, encoder_b;

    // Professional button state with event types
    struct ButtonState {
        int pin;                    // GPIO pin number for this button
        bool pressed;               // Current button state
        bool just_pressed;          // True for one cycle when first pressed
        bool just_released;         // True for one cycle when first released
        bool single_click;          // True for one cycle when single click detected
        bool double_click;          // True for one cycle when double click detected
        bool long_press;            // True for one cycle when long press detected
        uint64_t press_duration;   // Duration button has been pressed (ms)
        uint64_t press_count;      // Number of press events (including all types)
        uint64_t last_event_time;  // Timestamp of last button event
    };
    
    ButtonState joystick_button;
    ButtonState encoder_a_button;
    ButtonState encoder_b_button;
    ButtonState save_button;

    // Constructor with deadzone percentage (0-100)
    UserInput(uint8_t deadzone_percent = 10);
    ~UserInput();
    
    void begin();
    void readInputs();  // Must be called periodically (recommended every 10ms or less)
    void calibrateJoystick();
    void getCenterCoordinates(int &x_center, int &y_center);
    bool getCenterCoordinates(int *x_center, int *y_center);
    
    // Button configuration
    void setButtonClickTime(uint64_t ms) { button_click_time = ms; }
    void setButtonPressTime(uint64_t ms) { button_press_time = ms; }
    void setButtonDebounceTime(uint64_t ms) { button_debounce_time = ms; }

private:
    static const int JOYSTICK_X_PIN = 33;
    static const int JOYSTICK_Y_PIN = 32;
    static const int JOYSTICK_BUTTON_PIN = 23;
    
    static const int ENC_A_PIN_A = 36;
    static const int ENC_A_PIN_B = 39;
    static const int ENC_A_BUTTON_PIN = 18;
    
    static const int ENC_B_PIN_A = 35;
    static const int ENC_B_PIN_B = 34;
    static const int ENC_B_BUTTON_PIN = 19;
    
    static const int SAVE_BUTTON_PIN = 16;
    
    // Encoder instances
    ESP32Encoder enc_a;
    ESP32Encoder enc_b;
    
    // Tracking for direction detection
    long last_enc_a_pos;
    long last_enc_b_pos;
    uint64_t last_enc_a_change;
    uint64_t last_enc_b_change;
    static const uint64_t ENCODER_DEBOUNCE_MS = 150;
    
    // OneButton instances
    OneButton* btn_joystick;
    OneButton* btn_enc_a;
    OneButton* btn_enc_b;
    OneButton* btn_save;
    
    // Button timing configuration
    uint64_t button_click_time = 300;      // Max time for click detection (ms)
    uint64_t button_press_time = 800;      // Time for long press detection (ms)
    uint64_t button_debounce_time = 50;    // Debounce time (ms)
    
    // Joystick calibration values
    int joystick_x_center;
    int joystick_y_center;
    const uint8_t joystick_deadzone;
    
    // Static instance for callbacks
    static UserInput* instance;
    
    // Callback handlers
    static void onJoystickClick();
    static void onJoystickDoubleClick();
    static void onJoystickLongPress();
    
    static void onEncoderAClick();
    static void onEncoderADoubleClick();
    static void onEncoderALongPress();
    
    static void onEncoderBClick();
    static void onEncoderBDoubleClick();
    static void onEncoderBLongPress();
    
    static void onSaveClick();
    static void onSaveDoubleClick();
    static void onSaveLongPress();
    
    void updateButtonState(ButtonState &state, OneButton &button, uint64_t current_time);
    void clearButtonEvents(ButtonState &state);
    void initButton(OneButton* &btn, int pin, 
                    void (*click)(), 
                    void (*doubleClick)(), 
                    void (*longPress)());
};

#endif