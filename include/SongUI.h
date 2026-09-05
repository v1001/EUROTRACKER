#ifndef SONG_UI_H
#define SONG_UI_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongData.h"

class SongUI {
public:
    SongUI(DisplayManager& display, UserInput& userInput, SongData& songData);
    ~SongUI();
    
    // Initialization
    void begin();
    
    // UI update
    void update();
    
    // Navigation
    void handleNavigation();
    
    // Getters for cursor position
    int getSelectedTrack() const { return _cursorCol - 1; }
    int getSelectedStep() const { return _scrollOffset + _cursorRow; }
    
    // Drawing
    void draw();

    // Autosave logic
    bool needsSave() const;
    void clearSaveFlag();
    void setSaveFlag();

    void resetEncoderTracking();
    void resetCursor() { _cursorRow = 0; _cursorCol = 1; _scrollOffset = 0; }

    // Playback progress tracking
    void setCurrentPlayingStep(int step) { _currentPlayingStep = step; }
    void setPlaybackProgress(int percent) { _playbackProgress = percent; }

private:
    // Constants
    static const int ROWS_PER_PAGE = 4;
    static const int NUM_TRACKS = 6;
    static const int PATTERN_STEPS = 32;
    static const int MOVE_DELAY = 200;
    static const unsigned long DISPLAY_TIMEOUT = 2000;
    
    // References
    DisplayManager& _display;
    UserInput& _userInput;
    SongData& _songData;
    
    // Navigation
    int _cursorRow;
    int _cursorCol;
    int _scrollOffset;
    
    // Display mode
    bool _showPattern;
    unsigned long _lastEncoderATime;
    unsigned long _lastEncoderBTime;
    
    // Navigation timing
    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;
    const unsigned long NAV_DELAY_MS = 100;
    const unsigned long JOYSTICK_CONTINUOUS_DELAY = 50;
    
    // Track X positions
    const int _trackXPositions[6];
    
    // Drawing helpers
    void drawPatternPreview(StepPattern& pattern, int x, int y);
    void drawStatusBar();

    // Encoder tracking
    long _lastEncoderAPos;
    long _lastEncoderBPos;
    
    void handleEncoders();
    void handleButtons();

    bool _needsSave;

    // Playback progress
    int _currentPlayingStep;
    int _playbackProgress;
    
    void drawProgressBar(int x, int y, int width, int height, int percent);
};

#endif