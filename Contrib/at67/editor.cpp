#include <stdio.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>


#if defined(_WIN32)
#include <direct.h>
#include "dirent/dirent.h"
#define getcwd _getcwd
#undef max
#undef min
#else
#include <unistd.h>
#include <dirent.h>
#endif


#include <SDL.h>
#include "memory.h"
#include "cpu.h"
#include "audio.h"
#include "editor.h"
#include "loader.h"
#include "timing.h"
#include "graphics.h"
#include "assembler.h"
#include "expression.h"
#include "inih/INIReader.h"


namespace Editor
{
    enum SingleStepMode {RunToBrk=0, StepVpc, StepWatch, NumSingleStepModes};

    struct KeyCodeMod
    {
        int scanCode;
        SDL_Keymod modifier;
    };

    struct FileEntry
    {
        FileType _fileType;
        std::string _name;
    };


    int _cursorX = 0;
    int _cursorY = 0;

    bool _hexEdit = false;
    bool _startMusic = false;
    bool _handlePS2Key = false;

    SDL_Keycode _sdlKeyScanCode = 0;
    uint16_t _sdlKeyModifier = 0;

    bool _pageUpButton = false;
    bool _pageDnButton = false;
    bool _delAllButton = false;


    bool _singleStep = false;
    bool _singleStepEnabled = false;
    SingleStepMode _singleStepMode = RunToBrk;
    uint32_t _singleStepTicks = 0;
    uint16_t _singleStepValue = 0x0000;
    uint16_t _singleStepAddress = VIDEO_Y_ADDRESS;

    std::string _cwdPath = "";
    std::string _filePath = "";

    MouseState _mouseState;
    MemoryMode _memoryMode = RAM;
    EditorMode _editorMode = Hex;
    EditorMode _editorModePrev = Hex;
    KeyboardMode _keyboardMode = Giga;
    OnVarType _onVarType = OnNone;

    uint8_t _memoryDigit = 0;
    uint8_t _addressDigit = 0;

    uint16_t _hexBaseAddress = HEX_BASE_ADDRESS;
    uint16_t _vpcBaseAddress = HEX_BASE_ADDRESS;
    uint16_t _loadBaseAddress = LOAD_BASE_ADDRESS;
    uint16_t _varsBaseAddress = VARS_BASE_ADDRESS;

    uint16_t _cpuUsageAddressA = HEX_BASE_ADDRESS;
    uint16_t _cpuUsageAddressB = HEX_BASE_ADDRESS + 0x0020;
    
    std::vector<uint16_t> _breakPoints;

    int _fileEntriesSize = 0;
    int _fileEntriesIndex = 0;
    std::vector<FileEntry> _fileEntries;

    int _romEntriesSize = 0;
    int _romEntriesIndex = 0;
    std::vector<RomEntry> _romEntries;

    INIReader _configIniReader;
    std::map<std::string, int> _sdlKeys;
    std::map<std::string, KeyCodeMod> _emulator;
    std::map<std::string, KeyCodeMod> _keyboard;
    std::map<std::string, KeyCodeMod> _hardware;
    std::map<std::string, KeyCodeMod> _debugger;

    int getCursorX(void) {return _cursorX;}
    int getCursorY(void) {return _cursorY;}
    bool getHexEdit(void) {return _hexEdit;}
    bool getSingleStepEnabled(void) {return _singleStepEnabled;}
    bool getStartMusic(void) {return _startMusic;}

    bool getPageUpButton(void) {return _pageUpButton;}
    bool getPageDnButton(void) {return _pageDnButton;}
    bool getDelAllButton(void) {return _delAllButton;}
 
    MemoryMode getMemoryMode(void) {return _memoryMode;}
    EditorMode getEditorMode(void) {return _editorMode;}
    KeyboardMode getKeyboardMode(void) {return _keyboardMode;}
    OnVarType getOnVarType(void) {return _onVarType;}

    uint8_t getMemoryDigit(void) {return _memoryDigit;}
    uint8_t getAddressDigit(void) {return _addressDigit;}
    uint16_t getHexBaseAddress(void) {return _hexBaseAddress;}
    uint16_t getVpcBaseAddress(void) {return _vpcBaseAddress;}
    uint16_t getLoadBaseAddress(void) {return _loadBaseAddress;}
    uint16_t getVarsBaseAddress(void) {return _varsBaseAddress;}
    uint16_t getSingleStepAddress(void) {return _singleStepAddress;}
    uint16_t getCpuUsageAddressA(void) {return _cpuUsageAddressA;}
    uint16_t getCpuUsageAddressB(void) {return _cpuUsageAddressB;}

    int getBreakPointsSize(void) {return int(_breakPoints.size());}
    uint16_t getBreakPointAddress(int index) {return _breakPoints.size() ? _breakPoints[index % _breakPoints.size()] : 0;}
    void addBreakPoint(uint16_t address) {_breakPoints.push_back(address);}
    void clearBreakPoints(void) {_breakPoints.clear();}

    int getFileEntriesIndex(void) {return _fileEntriesIndex;}
    int getFileEntriesSize(void) {return int(_fileEntries.size());}
    FileType getFileEntryType(int index) {return _fileEntries.size() ? _fileEntries[index % _fileEntries.size()]._fileType : File;}
    FileType getCurrentFileEntryType(void) {return _fileEntries.size() ? _fileEntries[(_cursorY + _fileEntriesIndex) % _fileEntries.size()]._fileType : File;}
    std::string* getFileEntryName(int index) {return _fileEntries.size() ? &_fileEntries[index % _fileEntries.size()]._name : nullptr;}
    std::string* getCurrentFileEntryName(void) {return _fileEntries.size() ? &_fileEntries[(_cursorY + _fileEntriesIndex) % _fileEntries.size()]._name : nullptr;}

    int getRomEntriesIndex(void) {return _romEntriesIndex;}
    int getRomEntriesSize(void) {return int(_romEntries.size());}
    uint8_t getRomEntryType(int index) {return _romEntries.size() ? _romEntries[index % _romEntries.size()]._type : 0;}
    uint8_t getCurrentRomEntryType(int& index) {if(_romEntries.size() == 0) return 0; index = (_cursorY + _romEntriesIndex) % _romEntries.size(); return _romEntries[index]._type;}
    std::string* getRomEntryName(int index) {return _romEntries.size() ? &_romEntries[index % _romEntries.size()]._name : nullptr;}
    std::string* getCurrentRomEntryName(int& index) {if(_romEntries.size() == 0) return nullptr; index = (_cursorY + _romEntriesIndex) % _romEntries.size(); return &_romEntries[index]._name;}
    int getCurrentRomEntryIndex(void) {return _romEntries.size() ? (_cursorY + _romEntriesIndex) % _romEntries.size() : 0;}
    void addRomEntry(uint8_t type, std::string& name) {Editor::RomEntry romEntry = {type, name}; _romEntries.push_back(romEntry); return;}

    void resetEditor(void) {_memoryDigit = 0; _addressDigit = 0;}
    void setEditorMode(EditorMode editorMode) {_editorModePrev = _editorMode; _editorMode = editorMode;}

    void setCursorX(int x) {_cursorX = x;}
    void setCursorY(int y) {_cursorY = y;}
    void setHexEdit(bool hexEdit) {_hexEdit = hexEdit; if(!hexEdit) resetEditor();}
    void setStartMusic(bool startMusic) {_startMusic = startMusic;}
    void setSingleStepAddress(uint16_t address) {_singleStepAddress = address;}
    void setLoadBaseAddress(uint16_t address) {_loadBaseAddress = address;}
    void setCpuUsageAddressA(uint16_t address) {_cpuUsageAddressA = address;}
    void setCpuUsageAddressB(uint16_t address) {_cpuUsageAddressB = address;}

    void getMouseState(MouseState& mouseState) {mouseState = _mouseState;}

    void getMouseMenuCursor(int& x, int& y, int& cy)
    {
        // Normalised mouse position
        float mx = float(_mouseState._x) / float(Graphics::getWidth());
        float my = float(_mouseState._y) / float(Graphics::getHeight());

        // PageUp, PageDn and DelAll buttons
        float pux0 = float(MENU_START_X+PAGEUP_START_X) / float(SCREEN_WIDTH);
        float puy0 = float(MENU_START_Y+PAGEUP_START_Y) / float(SCREEN_HEIGHT);
        float pux1 = float(MENU_START_X+PAGEUP_START_X+FONT_WIDTH) / float(SCREEN_WIDTH);
        float puy1 = float(MENU_START_Y+PAGEUP_START_Y+FONT_HEIGHT) / float(SCREEN_HEIGHT);
        float pdx0 = float(MENU_START_X+PAGEDN_START_X) / float(SCREEN_WIDTH);
        float pdy0 = float(MENU_START_Y+PAGEDN_START_Y) / float(SCREEN_HEIGHT);
        float pdx1 = float(MENU_START_X+PAGEDN_START_X+FONT_WIDTH) / float(SCREEN_WIDTH);
        float pdy1 = float(MENU_START_Y+PAGEDN_START_Y+FONT_HEIGHT) / float(SCREEN_HEIGHT);
        float dax0 = float(MENU_START_X+DELALL_START_X) / float(SCREEN_WIDTH);
        float day0 = float(MENU_START_Y+DELALL_START_Y) / float(SCREEN_HEIGHT);
        float dax1 = float(MENU_START_X+DELALL_START_X+FONT_WIDTH) / float(SCREEN_WIDTH);
        float day1 = float(MENU_START_Y+DELALL_START_Y+FONT_HEIGHT) / float(SCREEN_HEIGHT);

        _pageUpButton = (mx >= pux0  &&  mx < pux1  &&  my >= puy0  &&  my < puy1);
        _pageDnButton = (mx >= pdx0  &&  mx < pdx1  &&  my >= pdy0  &&  my < pdy1);
        _delAllButton = (mx >= dax0  &&  mx < dax1  &&  my >= day0  &&  my < day1);

        // Normalised cursor origin
        float ox = float(MENU_START_X) / float(SCREEN_WIDTH);
        float oy = float(MENU_START_Y) / float(SCREEN_HEIGHT);

        // Menu text cursor positions
        x = -1, y = -1, cy = -5;
        if(mx >= ox  &&  mx < 0.98f  &&  my >= oy  &&  my < 1.0f)
        {
            x = int((mx - ox) * 1.0f / (1.0f - ox) * (MENU_CHARS_X+0.65f));
            y = int((my - oy) * 1.0f / (1.0f - oy) * MENU_CHARS_Y);
            cy = y - 4;
        }

        //fprintf(stderr, "%d %d %d  %d %d %d  %08x\n", x, y, cy, _pageUpButton, _pageDnButton, _delAllButton, _mouseState._state);
    }

    std::string getBrowserPath(bool removeSlash)
    {
        std::string str = _filePath;
        if(removeSlash  &&  str.length()) str.erase(str.length()-1);
        return str;
    }

    bool scanCodeFromIniKey(const std::string& sectionString, const std::string& iniKey, const std::string& defaultKey, KeyCodeMod& keyCodeMod)
    {
        keyCodeMod.modifier = KMOD_NONE;

        std::string mod;
        std::string key = _configIniReader.Get(sectionString, iniKey, defaultKey);
        key = Expression::strToUpper(key);

        // Parse CTRL or ALT
        size_t keyPos = key.find("+");
        if(keyPos != std::string::npos  &&  keyPos != key.length()-1)
        {
            mod = key.substr(0, keyPos);
            key = key.substr(keyPos + 1);
            if(mod == "ALT")  keyCodeMod.modifier = KMOD_LALT;
            if(mod == "CTRL") keyCodeMod.modifier = KMOD_LCTRL;
        }

        if(_sdlKeys.find(key) == _sdlKeys.end())
        {
            fprintf(stderr, "Editor::scanCodeFromIniKey() : key %s not recognised in INI file '%s' : reverting to default key '%s'\n", key.c_str(), INPUT_CONFIG_INI, defaultKey.c_str());
            keyCodeMod.scanCode = _sdlKeys[defaultKey];
            return false;
        }

        keyCodeMod.scanCode = _sdlKeys[key];
        //fprintf(stderr, "Editor::scanCodeFromIniKey() : %s : %s : %s : %s : key=%d : mod=%d\n", sectionString.c_str(), iniKey.c_str(), key.c_str(), mod.c_str(), keyCodeMod.scanCode, keyCodeMod.modifier);
        return true;
    }


    // Attempt to solve Window's Non Client Area process starvation
    int eventFilter(void* data, SDL_Event *event)
    {
        if(event->type == SDL_WINDOWEVENT  ||  event->type == SDL_SYSWMEVENT)
        {
            if(event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED  ||  event->window.event == SDL_WINDOWEVENT_MOVED)
            {
                SDL_RenderSetViewport(Graphics::getRenderer(), NULL);
                //Graphics::refreshScreen();
                //Graphics::render();
                //fprintf(stderr, "%08x\n", event->window.event);
                for(int i=0; i<CLOCK_FREQ/600; i++) Cpu::process();
            }
        }

        return 1;
    }

    void initialise(void)
    {
        //SDL_SetEventFilter(eventFilter, NULL);

        SDL_StartTextInput();

        // Current working directory
        char cwdPath[FILENAME_MAX];
        if(!getcwd(cwdPath, FILENAME_MAX)) sprintf(cwdPath, ".");
        _cwdPath = std::string(cwdPath);
        _filePath = _cwdPath + "/";

        // Keyboard to SDL key mapping
        _sdlKeys["ENTER"]       = SDLK_RETURN;
        _sdlKeys["CR"]          = SDLK_RETURN;
        _sdlKeys["A"]           = SDLK_a;
        _sdlKeys["B"]           = SDLK_b;
        _sdlKeys["C"]           = SDLK_c;
        _sdlKeys["D"]           = SDLK_d;
        _sdlKeys["E"]           = SDLK_e;
        _sdlKeys["F"]           = SDLK_f;
        _sdlKeys["G"]           = SDLK_g;
        _sdlKeys["H"]           = SDLK_h;
        _sdlKeys["I"]           = SDLK_i;
        _sdlKeys["J"]           = SDLK_j;
        _sdlKeys["K"]           = SDLK_k;
        _sdlKeys["L"]           = SDLK_l;
        _sdlKeys["M"]           = SDLK_m;
        _sdlKeys["N"]           = SDLK_n;
        _sdlKeys["O"]           = SDLK_o;
        _sdlKeys["P"]           = SDLK_p;
        _sdlKeys["Q"]           = SDLK_q;
        _sdlKeys["R"]           = SDLK_r;
        _sdlKeys["S"]           = SDLK_s;
        _sdlKeys["T"]           = SDLK_t;
        _sdlKeys["U"]           = SDLK_u;
        _sdlKeys["V"]           = SDLK_v;
        _sdlKeys["W"]           = SDLK_w;
        _sdlKeys["X"]           = SDLK_x;
        _sdlKeys["Y"]           = SDLK_y;
        _sdlKeys["Z"]           = SDLK_z;
        _sdlKeys["1"]           = SDLK_1;
        _sdlKeys["!"]           = SDLK_1;
        _sdlKeys["2"]           = SDLK_2;
        _sdlKeys["@"]           = SDLK_2;
        _sdlKeys["3"]           = SDLK_3;
        _sdlKeys["#"]           = SDLK_3;
        _sdlKeys["4"]           = SDLK_4;
        _sdlKeys["$"]           = SDLK_4;
        _sdlKeys["5"]           = SDLK_5;
        _sdlKeys["%"]           = SDLK_5;
        _sdlKeys["6"]           = SDLK_6;
        _sdlKeys["^"]           = SDLK_6;
        _sdlKeys["7"]           = SDLK_7;
        _sdlKeys["&"]           = SDLK_7;
        _sdlKeys["8"]           = SDLK_8;
        _sdlKeys["*"]           = SDLK_8;
        _sdlKeys["9"]           = SDLK_9;
        _sdlKeys["("]           = SDLK_9;
        _sdlKeys["0"]           = SDLK_0;
        _sdlKeys[")"]           = SDLK_0;
        _sdlKeys["F1"]          = SDLK_F1;
        _sdlKeys["F2"]          = SDLK_F2;
        _sdlKeys["F3"]          = SDLK_F3;
        _sdlKeys["F4"]          = SDLK_F4;
        _sdlKeys["F5"]          = SDLK_F5;
        _sdlKeys["F6"]          = SDLK_F6;
        _sdlKeys["F7"]          = SDLK_F7;
        _sdlKeys["F8"]          = SDLK_F8;
        _sdlKeys["F9"]          = SDLK_F9;
        _sdlKeys["F10"]         = SDLK_F10;
        _sdlKeys["F11"]         = SDLK_F11;
        _sdlKeys["F12"]         = SDLK_F12;
        _sdlKeys["SPACE"]       = SDLK_SPACE;
        _sdlKeys["BACKSPACE"]   = SDLK_BACKSPACE;
        _sdlKeys["TAB"]         = SDLK_TAB;
        _sdlKeys["_"]           = SDLK_MINUS;
        _sdlKeys["-"]           = SDLK_MINUS;
        _sdlKeys["+"]           = SDLK_EQUALS;
        _sdlKeys["="]           = SDLK_EQUALS;
        _sdlKeys["`"]           = SDLK_BACKQUOTE;
        _sdlKeys["~"]           = SDLK_BACKQUOTE;
        _sdlKeys["<"]           = SDLK_COMMA;
        _sdlKeys[","]           = SDLK_COMMA;
        _sdlKeys[">"]           = SDLK_PERIOD;
        _sdlKeys["."]           = SDLK_PERIOD;
        _sdlKeys["["]           = SDLK_LEFTBRACKET;
        _sdlKeys["{"]           = SDLK_LEFTBRACKET;
        _sdlKeys["]"]           = SDLK_RIGHTBRACKET;
        _sdlKeys["}"]           = SDLK_RIGHTBRACKET;
        _sdlKeys[";"]           = SDLK_SEMICOLON;
        _sdlKeys[":"]           = SDLK_SEMICOLON;
        _sdlKeys["'"]           = SDLK_QUOTE;
        _sdlKeys["\""]          = SDLK_QUOTE;
        _sdlKeys["\\"]          = SDLK_BACKSLASH;
        _sdlKeys["|"]           = SDLK_BACKSLASH;
        _sdlKeys["/"]           = SDLK_SLASH;
        _sdlKeys["?"]           = SDLK_SLASH;
        _sdlKeys["LEFT"]        = SDLK_LEFT;
        _sdlKeys["RIGHT"]       = SDLK_RIGHT;
        _sdlKeys["UP"]          = SDLK_UP;
        _sdlKeys["DOWN"]        = SDLK_DOWN;
        _sdlKeys["PAGEUP"]      = SDLK_PAGEUP;
        _sdlKeys["PAGEDOWN"]    = SDLK_PAGEDOWN;
        _sdlKeys["CAPSLOCK"]    = SDLK_CAPSLOCK;
        _sdlKeys["PRINTSCREEN"] = SDLK_PRINTSCREEN;
        _sdlKeys["SCROLLLOCK"]  = SDLK_SCROLLLOCK;
        _sdlKeys["ESC"]         = SDLK_ESCAPE;
        _sdlKeys["PAUSE"]       = SDLK_PAUSE;
        _sdlKeys["INSERT"]      = SDLK_INSERT;
        _sdlKeys["HOME"]        = SDLK_HOME;
        _sdlKeys["DELETE"]      = SDLK_DELETE;
        _sdlKeys["END"]         = SDLK_END;
        _sdlKeys["NUMLOCK"]     = SDLK_NUMLOCKCLEAR;
        _sdlKeys["KP_DIVIDE"]   = SDLK_KP_DIVIDE;
        _sdlKeys["KP_MULTIPLY"] = SDLK_KP_MULTIPLY;
        _sdlKeys["KP_MINUS"]    = SDLK_KP_MINUS; 
        _sdlKeys["KP_PLUS"]     = SDLK_KP_PLUS;
        _sdlKeys["KP_ENTER"]    = SDLK_KP_ENTER;
        _sdlKeys["KP_1"]        = SDLK_KP_1;
        _sdlKeys["KP_2"]        = SDLK_KP_2;
        _sdlKeys["KP_3"]        = SDLK_KP_3;
        _sdlKeys["KP_4"]        = SDLK_KP_4;
        _sdlKeys["KP_5"]        = SDLK_KP_5;
        _sdlKeys["KP_6"]        = SDLK_KP_6;
        _sdlKeys["KP_7"]        = SDLK_KP_7;
        _sdlKeys["KP_8"]        = SDLK_KP_8;
        _sdlKeys["KP_9"]        = SDLK_KP_9;
        _sdlKeys["KP_0"]        = SDLK_KP_0;
        _sdlKeys["KP_PERIOD"]   = SDLK_KP_PERIOD;
        _sdlKeys["LCTRL"]       = SDLK_LCTRL;
        _sdlKeys["LSHIFT"]      = SDLK_LSHIFT;
        _sdlKeys["LALT"]        = SDLK_LALT;
        _sdlKeys["LGUI"]        = SDLK_LGUI;
        _sdlKeys["RCTRL"]       = SDLK_RCTRL;
        _sdlKeys["RSHIFT"]      = SDLK_RSHIFT;
        _sdlKeys["RALT"]        = SDLK_RALT;
        _sdlKeys["RGUI"]        = SDLK_RGUI;


        // Emulator INI key to SDL key mapping
        _emulator["MemoryMode"]   = {SDLK_m, KMOD_LCTRL};
        _emulator["MemorySize"]   = {SDLK_z, KMOD_LCTRL};
        _emulator["Browse"]       = {SDLK_b, KMOD_LCTRL};
        _emulator["RomType"]      = {SDLK_r, KMOD_LCTRL};
        _emulator["HexMonitor"]   = {SDLK_x, KMOD_LCTRL};
        _emulator["Disassembler"] = {SDLK_d, KMOD_LCTRL};
        _emulator["ScanlineMode"] = {SDLK_s, KMOD_LCTRL};
        _emulator["Reset"]        = {SDLK_F1, KMOD_LCTRL};
        _emulator["Execute"]      = {SDLK_F5, KMOD_LCTRL};
        _emulator["Help"]         = {SDLK_h, KMOD_LCTRL};
        _emulator["Quit"]         = {SDLK_q, KMOD_LCTRL};
        _emulator["Speed+"]       = {SDLK_EQUALS, KMOD_NONE};
        _emulator["Speed-"]       = {SDLK_MINUS, KMOD_NONE};

        // Keyboard INI key to SDL key mapping
        _keyboard["Mode"]   = {SDLK_k, KMOD_LCTRL};
        _keyboard["Left"]   = {SDLK_a, KMOD_NONE};
        _keyboard["Right"]  = {SDLK_d, KMOD_NONE};
        _keyboard["Up"]     = {SDLK_w, KMOD_NONE};
        _keyboard["Down"]   = {SDLK_s, KMOD_NONE};
        _keyboard["Start"]  = {SDLK_SPACE, KMOD_NONE};
        _keyboard["Select"] = {SDLK_z, KMOD_NONE};
        _keyboard["A"]      = {SDLK_GREATER, KMOD_NONE};
        _keyboard["B"]      = {SDLK_SLASH, KMOD_NONE};

        // Hardware INI key to SDL key mapping
        _hardware["Reset"]   = {SDLK_F1, KMOD_LALT};
        _hardware["Execute"] = {SDLK_F5, KMOD_LALT};

        // Debugger INI key to SDL key mapping
        _debugger["Debug"]     = {SDLK_F6, KMOD_LCTRL};
        _debugger["RunToBrk"]  = {SDLK_F7, KMOD_LCTRL};
        _debugger["StepVpc"]   = {SDLK_F8, KMOD_LCTRL};
        _debugger["StepWatch"] = {SDLK_F9, KMOD_LCTRL};

        // Input configuration
        INIReader iniReader(INPUT_CONFIG_INI);
        _configIniReader = iniReader;
        if(_configIniReader.ParseError() < 0)
        {
            fprintf(stderr, "Editor::initialise() : couldn't load INI file '%s' : reverting to default keys.\n", INPUT_CONFIG_INI);
            return;
        }

        // Parse input keys INI file
        enum Section {Emulator, Keyboard, Hardware, Debugger};
        std::map<std::string, Section> section;
        section["Emulator"] = Emulator;
        section["Keyboard"] = Keyboard;
        section["Hardware"] = Hardware;
        section["Debugger"] = Debugger;
        for(auto sectionString : _configIniReader.Sections())
        {
            if(section.find(sectionString) == section.end())
            {
                fprintf(stderr, "Editor::initialise() : INI file '%s' has bad Sections : reverting to default keys.\n", INPUT_CONFIG_INI);
                break;
            }

            switch(section[sectionString])
            {
                case Emulator:
                {
                    scanCodeFromIniKey(sectionString, "MemoryMode",   "CTRL+M",   _emulator["MemoryMode"]);
                    scanCodeFromIniKey(sectionString, "MemorySize",   "CTRL+Z",   _emulator["MemorySize"]);
                    scanCodeFromIniKey(sectionString, "Browse",       "CTRL+B",   _emulator["Browse"]);
                    scanCodeFromIniKey(sectionString, "RomType",      "CTRL+R",   _emulator["RomType"]);
                    scanCodeFromIniKey(sectionString, "HexMonitor",   "CTRL+X",   _emulator["HexMonitor"]);
                    scanCodeFromIniKey(sectionString, "Disassembler", "CTRL+D",   _emulator["Disassembler"]);
                    scanCodeFromIniKey(sectionString, "ScanlineMode", "CTRL+S",   _emulator["ScanlineMode"]);
                    scanCodeFromIniKey(sectionString, "Reset",        "CTRL+F1",  _emulator["Reset"]);
                    scanCodeFromIniKey(sectionString, "Execute",      "CTRL+F5",  _emulator["Execute"]);
                    scanCodeFromIniKey(sectionString, "Help",         "CTRL+H",   _emulator["Help"]);
                    scanCodeFromIniKey(sectionString, "Quit",         "CTRL+Q",   _emulator["Quit"]);
                    scanCodeFromIniKey(sectionString, "Speed+",       "+",        _emulator["Speed+"]);
                    scanCodeFromIniKey(sectionString, "Speed-",       "-",        _emulator["Speed-"]);
                }
                break;

                case Keyboard:
                {
                    scanCodeFromIniKey(sectionString, "Mode",   "CTRL+K", _keyboard["Mode"]);
                    scanCodeFromIniKey(sectionString, "Left",   "A",      _keyboard["Left"]);
                    scanCodeFromIniKey(sectionString, "Right",  "D",      _keyboard["Right"]);
                    scanCodeFromIniKey(sectionString, "Up",     "W",      _keyboard["Up"]);
                    scanCodeFromIniKey(sectionString, "Down",   "S",      _keyboard["Down"]);
                    scanCodeFromIniKey(sectionString, "Start",  "SPACE",  _keyboard["Start"]);
                    scanCodeFromIniKey(sectionString, "Select", "Z",      _keyboard["Select"]);
                    scanCodeFromIniKey(sectionString, "A",      ".",      _keyboard["A"]);
                    scanCodeFromIniKey(sectionString, "B",      "/",      _keyboard["B"]);
                }
                break;

                case Hardware:
                {
                    scanCodeFromIniKey(sectionString, "Reset",   "ALT+F1", _hardware["Reset"]);
                    scanCodeFromIniKey(sectionString, "Execute", "ALT+F5", _hardware["Execute"]);
                }
                break;

                case Debugger:
                {
                    scanCodeFromIniKey(sectionString, "Debug",     "CTRL+F6", _debugger["Debug"]);
                    scanCodeFromIniKey(sectionString, "RunToBrk",  "CTRL+F7", _debugger["RunToBrk"]);
                    scanCodeFromIniKey(sectionString, "StepVpc",   "CTRL+F8", _debugger["StepVpc"]);
                    scanCodeFromIniKey(sectionString, "StepWatch", "CTRL+F9", _debugger["StepWatch"]);
                }
                break;
            }
        }
    }

    void backOneDirectory(void)
    {
        size_t slash = _filePath.find_last_of("\\/", _filePath.size()-2);
        if(slash != std::string::npos)
        {
            _filePath = _filePath.substr(0, slash + 1);
        }
    }

    void browseDirectory(void)
    {
        std::string path = _filePath  + ".";
        Assembler::setIncludePath(_filePath);

        _fileEntries.clear();

        DIR *dir;
        struct dirent *ent;
        std::vector<std::string> dirnames;
        dirnames.push_back("..");
        std::vector<std::string> filenames;
        if((dir = opendir(path.c_str())) != NULL)
        {
            while((ent = readdir(dir)) != NULL)
            {
                std::string name = std::string(ent->d_name);
                size_t nonWhiteSpace = name.find_first_not_of("  \n\r\f\t\v");
                if(ent->d_type == DT_DIR  &&  name[0] != '.'  &&  name.find("$RECYCLE") == std::string::npos  &&  nonWhiteSpace != std::string::npos)
                {
                    dirnames.push_back(name);
                }
                else if(ent->d_type == DT_REG  &&  (name.find(".gbas") != std::string::npos  ||  name.find(".gtb") != std::string::npos  ||  name.find(".gcl") != std::string::npos  ||
                                                    name.find(".gasm") != std::string::npos  ||  name.find(".vasm") != std::string::npos  ||  name.find(".gt1") != std::string::npos))
                {
                    filenames.push_back(name);
                }
            }
            closedir (dir);
        }

        std::sort(dirnames.begin(), dirnames.end());
        for(int i=0; i<dirnames.size(); i++)
        {
            FileEntry fileEntry = {Dir, dirnames[i]};
            _fileEntries.push_back(fileEntry);
        }

        std::sort(filenames.begin(), filenames.end());
        for(int i=0; i<filenames.size(); i++)
        {
            FileEntry fileEntry = {File, filenames[i]};
            _fileEntries.push_back(fileEntry);
        }

        // Only reset cursor and file index if file list size has changed
        if(int(_fileEntriesSize != _fileEntries.size()))
        {
            _cursorX = 0;
            _cursorY = 0;
            _fileEntriesIndex = 0;
            _fileEntriesSize = int(_fileEntries.size());
        }
    }

    void changeBrowseDirectory(void)
    {
        std::string entry = *getCurrentFileEntryName();

        if(entry != "..")
        {
            _filePath += entry + "/";
        }
        else
        {
            backOneDirectory();
        }

        browseDirectory();
    }

    void handlePageUp(uint16_t numRows)
    {
        switch(_editorMode)
        {
            case Hex:  _hexBaseAddress = (_hexBaseAddress - HEX_CHARS_X*numRows) & (Memory::getSizeRAM()-1); break;
            case Load: if(--_fileEntriesIndex < 0) _fileEntriesIndex = 0;                                    break;
            case Rom:  if(--_romEntriesIndex < 0) _romEntriesIndex = 0;                                      break;
            case Dasm: 
            {
                if(numRows == 1)
                {
                    _hexBaseAddress = (_hexBaseAddress - Assembler::getPrevDasmByteCount()) & (Memory::getSizeRAM()-1);
                }
                else
                {
                    _hexBaseAddress = (_hexBaseAddress - Assembler::getPrevDasmPageByteCount()) & (Memory::getSizeRAM()-1);
                }
            }
            break;
        }
    }

    void handlePageDown(uint16_t numRows)
    {
        switch(_editorMode)
        {
            case Hex:  _hexBaseAddress = (_hexBaseAddress + HEX_CHARS_X*numRows) & (Memory::getSizeRAM()-1); break;
            case Load:
            {
                if(_fileEntries.size() > HEX_CHARS_Y)
                {
                    _fileEntriesIndex++;
                    if(_fileEntries.size() - _fileEntriesIndex < HEX_CHARS_Y) _fileEntriesIndex--;
                }
            }
            break;
            case Rom:
            {
                if(_romEntries.size() > HEX_CHARS_Y)
                {
                    _romEntriesIndex++;
                    if(_romEntries.size() - _romEntriesIndex < HEX_CHARS_Y) _romEntriesIndex--;
                }
            }
            break;
            case Dasm:
            {
                if(numRows == 1)
                {
                    _hexBaseAddress = (_hexBaseAddress + Assembler::getCurrDasmByteCount()) & (Memory::getSizeRAM()-1);
                }
                else
                {
                    _hexBaseAddress = (_hexBaseAddress + Assembler::getCurrDasmPageByteCount()) & (Memory::getSizeRAM()-1);
                }
            }
            break;
        }
    }

    void handleMouseWheel(const SDL_Event& event)
    {
        if(event.wheel.y > 0) handlePageUp(1);
        if(event.wheel.y < 0) handlePageDown(1);
    }

    void handleMouseLeftClick(void)
    {
        if(_editorMode == Hex  ||  (_cursorY < 0  &&  _editorMode != Rom))
        {
            _hexEdit = !_hexEdit;
            if(!_hexEdit) resetEditor();
        }
        else if(_editorMode == Load)
        {
            // No loading/browsing if cursor is out of bounds
            if(_cursorY >= getFileEntriesSize()) return;

            FileType fileType = getCurrentFileEntryType();
            switch(fileType)
            {
                case File: Loader::setUploadTarget(Loader::Emulator); break;
                case Dir: changeBrowseDirectory(); break;
            }
        }
        else if(_editorMode == Rom)
        {
            if(_cursorY < 0  ||  _cursorY >= getRomEntriesSize()) return;

            Cpu::loadRom(getCurrentRomEntryIndex());
        }
        else if(_editorMode == Dasm  &&  _singleStepEnabled)
        {
            // Breakpoints only work in vCPU code
            if(_cursorY < 0  ||  _cursorY >= Assembler::getDisassembledCodeSize() ||  _memoryMode != RAM) return;

            // If breakpoint already exists, delete it, otherwise save it
            auto it = std::find(_breakPoints.begin(), _breakPoints.end(), Assembler::getDisassembledCode(_cursorY)->_address);
            (it != _breakPoints.end()) ? _breakPoints.erase(it) : _breakPoints.push_back(Assembler::getDisassembledCode(_cursorY)->_address);
        }
    }

    void handleMouseRightClick(void)
    {
        if(_editorMode == Load)
        {
            // No loading/browsing if cursor is out of bounds
            if(_cursorY >= getFileEntriesSize()) return;

            FileType fileType = getCurrentFileEntryType();
            switch(fileType)
            {
                case File: Loader::setUploadTarget(Loader::Hardware); break;
            }
        }
    }

    OnVarType updateOnVarType(void)
    {
        if(_singleStepEnabled)
        {
            if(getCursorY() == -2  &&  getCursorX() > 5  &&  getCursorX() < 7) return OnWatch;
        }
        else
        {
            if(getCursorY() == -2  &&  getCursorX() > 3  &&  getCursorX() < 6) return OnCpuA;
            if(getCursorY() == -2  &&  getCursorX() > 5  &&  getCursorX() < 8) return OnCpuB;
        }
        if(getCursorY() == -1  &&  getCursorX() > 0  &&  getCursorX() < 3) return OnHex;
        if(getCursorY() == -1  &&  getCursorX() > 4  &&  getCursorX() < 7) return OnVars;
    
        return OnNone;
    }

    void updateEditor(void)
    {
        if(!_hexEdit)
        {
            resetEditor();
            return;
        }

        // Don't allow emulator commands to accidently modify fields
        if(_sdlKeyModifier == KMOD_LALT  ||  _sdlKeyModifier == KMOD_LCTRL) return;

        int range = 0;
        if(_sdlKeyScanCode >= SDLK_0  &&  _sdlKeyScanCode <= SDLK_9) range = 1;
        if(_sdlKeyScanCode >= SDLK_a  &&  _sdlKeyScanCode <= SDLK_f) range = 2;
        if(range == 1  ||  range == 2)
        {
            uint16_t value = 0;    
            switch(range)
            {
                case 1: value = _sdlKeyScanCode - SDLK_0;      break;
                case 2: value = _sdlKeyScanCode - SDLK_a + 10; break;
            }

            // Edit memory
            if(_memoryMode == RAM  &&  _cursorY >= 0)
            {
                uint16_t address = _hexBaseAddress + _cursorX + _cursorY*HEX_CHARS_X;
                switch(_memoryDigit)
                {
                    case 0: value = (value << 4) & 0x00F0; Cpu::setRAM(address, Cpu::getRAM(address) & 0x000F | value); break;
                    case 1: value = (value << 0) & 0x000F; Cpu::setRAM(address, Cpu::getRAM(address) & 0x00F0 | value); break;
                }
                _memoryDigit = (++_memoryDigit) & 0x01;
                return;
            }

            // Edit variables
            switch(_onVarType)
            {
                case OnCpuA:
                {
                    switch(_addressDigit)
                    {
                        case 0: value = (value << 12) & 0xF000; _cpuUsageAddressA = _cpuUsageAddressA & 0x0FFF | value; break;
                        case 1: value = (value << 8)  & 0x0F00; _cpuUsageAddressA = _cpuUsageAddressA & 0xF0FF | value; break;
                        case 2: value = (value << 4)  & 0x00F0; _cpuUsageAddressA = _cpuUsageAddressA & 0xFF0F | value; break;
                        case 3: value = (value << 0)  & 0x000F; _cpuUsageAddressA = _cpuUsageAddressA & 0xFFF0 | value; break;
                    }
                }
                break;

                case OnCpuB:
                {
                    switch(_addressDigit)
                    {
                        case 0: value = (value << 12) & 0xF000; _cpuUsageAddressB = _cpuUsageAddressB & 0x0FFF | value; break;
                        case 1: value = (value << 8)  & 0x0F00; _cpuUsageAddressB = _cpuUsageAddressB & 0xF0FF | value; break;
                        case 2: value = (value << 4)  & 0x00F0; _cpuUsageAddressB = _cpuUsageAddressB & 0xFF0F | value; break;
                        case 3: value = (value << 0)  & 0x000F; _cpuUsageAddressB = _cpuUsageAddressB & 0xFFF0 | value; break;
                    }
                }
                break;

                case OnHex:
                {
                    if(_editorMode == Load)
                    {
                        switch(_addressDigit)
                        {
                            case 0: value = (value << 12) & 0xF000; _loadBaseAddress = _loadBaseAddress & 0x0FFF | value; break;
                            case 1: value = (value << 8)  & 0x0F00; _loadBaseAddress = _loadBaseAddress & 0xF0FF | value; break;
                            case 2: value = (value << 4)  & 0x00F0; _loadBaseAddress = _loadBaseAddress & 0xFF0F | value; break;
                            case 3: value = (value << 0)  & 0x000F; _loadBaseAddress = _loadBaseAddress & 0xFFF0 | value; break;
                        }

                        if(_loadBaseAddress < LOAD_BASE_ADDRESS) _loadBaseAddress = LOAD_BASE_ADDRESS;
                    }
                    else
                    {
                        switch(_addressDigit)
                        {
                            case 0: value = (value << 12) & 0xF000; _hexBaseAddress = _hexBaseAddress & 0x0FFF | value; break;
                            case 1: value = (value << 8)  & 0x0F00; _hexBaseAddress = _hexBaseAddress & 0xF0FF | value; break;
                            case 2: value = (value << 4)  & 0x00F0; _hexBaseAddress = _hexBaseAddress & 0xFF0F | value; break;
                            case 3: value = (value << 0)  & 0x000F; _hexBaseAddress = _hexBaseAddress & 0xFFF0 | value; break;
                        }
                    }
                }
                break;

                case OnVars:
                {
                    switch(_addressDigit)
                    {
                        case 0: value = (value << 12) & 0xF000; _varsBaseAddress = _varsBaseAddress & 0x0FFF | value; break;
                        case 1: value = (value << 8)  & 0x0F00; _varsBaseAddress = _varsBaseAddress & 0xF0FF | value; break;
                        case 2: value = (value << 4)  & 0x00F0; _varsBaseAddress = _varsBaseAddress & 0xFF0F | value; break;
                        case 3: value = (value << 0)  & 0x000F; _varsBaseAddress = _varsBaseAddress & 0xFFF0 | value; break;
                    }
                }
                break;

                case OnWatch:
                {
                    switch(_addressDigit)
                    {
                        case 0: value = (value << 12) & 0xF000; _singleStepAddress = _singleStepAddress & 0x0FFF | value; break;
                        case 1: value = (value << 8)  & 0x0F00; _singleStepAddress = _singleStepAddress & 0xF0FF | value; break;
                        case 2: value = (value << 4)  & 0x00F0; _singleStepAddress = _singleStepAddress & 0xFF0F | value; break;
                        case 3: value = (value << 0)  & 0x000F; _singleStepAddress = _singleStepAddress & 0xFFF0 | value; break;
                    }
                }
                break;

                default: return;
            }

            _addressDigit = (++_addressDigit) & 0x03;
        }
    }

    void startDebugger(void)
    {
        _singleStep = false;
        _singleStepEnabled = true;
        _singleStepMode = RunToBrk;

        _hexBaseAddress = (_memoryMode == RAM) ? Cpu::getVPC() : Cpu::getStateS()._PC;
        _vpcBaseAddress = _hexBaseAddress;
    }

    void resetDebugger(void)
    {
        _singleStep = false;
        _singleStepEnabled = false;
        _singleStepMode = RunToBrk;
    }

    // PS2 Keyboard emulation mode
    bool handlePs2KeyDown(void)
    {
        if(_keyboardMode == PS2  ||  _keyboardMode == HwPS2)
        {
            switch(_sdlKeyScanCode)
            {
                case SDLK_LEFT:     (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_LEFT,   true) : Cpu::setIN(Cpu::getIN() & ~INPUT_LEFT  ); return true;
                case SDLK_RIGHT:    (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_RIGHT,  true) : Cpu::setIN(Cpu::getIN() & ~INPUT_RIGHT ); return true;
                case SDLK_UP:       (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_UP,     true) : Cpu::setIN(Cpu::getIN() & ~INPUT_UP    ); return true;
                case SDLK_DOWN:     (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_DOWN,   true) : Cpu::setIN(Cpu::getIN() & ~INPUT_DOWN  ); return true;
                case SDLK_PAGEUP:   (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_START,  true) : Cpu::setIN(Cpu::getIN() & ~INPUT_START ); return true;
                case SDLK_PAGEDOWN: (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_SELECT, true) : Cpu::setIN(Cpu::getIN() & ~INPUT_SELECT); return true;
            }

            if((_sdlKeyScanCode >= 0  &&  _sdlKeyScanCode <= 31) ||  _sdlKeyScanCode == 127  ||  _sdlKeyScanCode == 'c')
            {
                switch(_sdlKeyScanCode)
                {
                    case SDLK_TAB:       (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_INPUT_A, true) : Cpu::setIN(Cpu::getIN() & ~INPUT_A); return true;
                    case SDLK_ESCAPE:    (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_INPUT_B, true) : Cpu::setIN(Cpu::getIN() & ~INPUT_B); return true;
                    case SDLK_RETURN:    (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_CR,      true) : Cpu::setIN('\n');                    return true;
                    case SDLK_DELETE:    (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_DEL,     true) : Cpu::setIN(127);                     return true;

                    case 'c':
                    {
                        if(_sdlKeyModifier == KMOD_LCTRL)
                        {
                            (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_CTLR_C, true) : Cpu::setIN(HW_PS2_CTLR_C);
                            return true;
                        }
                    }
                }
            }

            // Handle normal keys
            if(_sdlKeyScanCode >= 32  &&  _sdlKeyScanCode <= 126)
            {
                _handlePS2Key = true;
                return true;
            }
        }

        return false;
    }

    // Gigatron Keyboard emulation mode
    bool handleGigaKeyDown(void)
    {
        if(_keyboardMode == Giga)
        {
            if(_sdlKeyScanCode == _keyboard["Left"].scanCode  &&  _sdlKeyModifier == _keyboard["Left"].modifier)          {Cpu::setIN(Cpu::getIN() & ~INPUT_LEFT);   return true;}
            else if(_sdlKeyScanCode == _keyboard["Right"].scanCode  &&  _sdlKeyModifier == _keyboard["Right"].modifier)   {Cpu::setIN(Cpu::getIN() & ~INPUT_RIGHT);  return true;}
            else if(_sdlKeyScanCode == _keyboard["Up"].scanCode  &&  _sdlKeyModifier == _keyboard["Up"].modifier)         {Cpu::setIN(Cpu::getIN() & ~INPUT_UP);     return true;}
            else if(_sdlKeyScanCode == _keyboard["Down"].scanCode  &&  _sdlKeyModifier == _keyboard["Down"].modifier)     {Cpu::setIN(Cpu::getIN() & ~INPUT_DOWN);   return true;}
            else if(_sdlKeyScanCode == _keyboard["Start"].scanCode  &&  _sdlKeyModifier == _keyboard["Start"].modifier)   {Cpu::setIN(Cpu::getIN() & ~INPUT_START);  return true;}
            else if(_sdlKeyScanCode == _keyboard["Select"].scanCode  &&  _sdlKeyModifier == _keyboard["Select"].modifier) {Cpu::setIN(Cpu::getIN() & ~INPUT_SELECT); return true;}
            else if(_sdlKeyScanCode == _keyboard["A"].scanCode  &&  _sdlKeyModifier == _keyboard["A"].modifier)           {Cpu::setIN(Cpu::getIN() & ~INPUT_A);      return true;}
            else if(_sdlKeyScanCode == _keyboard["B"].scanCode  &&  _sdlKeyModifier == _keyboard["B"].modifier)           {Cpu::setIN(Cpu::getIN() & ~INPUT_B);      return true;}
        }
        else if(_keyboardMode == HwGiga)
        {
            if(_sdlKeyScanCode == _keyboard["Left"].scanCode  &&  _sdlKeyModifier == _keyboard["Left"].modifier)          {Loader::sendCommandToGiga('A', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["Right"].scanCode  &&  _sdlKeyModifier == _keyboard["Right"].modifier)   {Loader::sendCommandToGiga('D', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["Up"].scanCode  &&  _sdlKeyModifier == _keyboard["Up"].modifier)         {Loader::sendCommandToGiga('W', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["Down"].scanCode  &&  _sdlKeyModifier == _keyboard["Down"].modifier)     {Loader::sendCommandToGiga('S', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["Start"].scanCode  &&  _sdlKeyModifier == _keyboard["Start"].modifier)   {Loader::sendCommandToGiga('E', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["Select"].scanCode  &&  _sdlKeyModifier == _keyboard["Select"].modifier) {Loader::sendCommandToGiga('Q', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["A"].scanCode  &&  _sdlKeyModifier == _keyboard["A"].modifier)           {Loader::sendCommandToGiga('Z', true); return true;}
            else if(_sdlKeyScanCode == _keyboard["B"].scanCode  &&  _sdlKeyModifier == _keyboard["B"].modifier)           {Loader::sendCommandToGiga('X', true); return true;}
        }

        return false;
    }

    void handleKeyDown(void)
    {
        //fprintf(stderr, "Editor::handleKeyDown() : key=%d : mod=%04x\n", _sdlKeyScanCode, _sdlKeyModifier);

        if(_sdlKeyScanCode == _emulator["Quit"].scanCode  &&  _sdlKeyModifier == _emulator["Quit"].modifier)
        {
            Cpu::shutdown();
            exit(0);
        }

        // Emulator reset
        else if(_sdlKeyScanCode == _emulator["Reset"].scanCode  &&  _sdlKeyModifier == _emulator["Reset"].modifier) {resetDebugger(); Cpu::reset(); return;}

        // Hardware reset
        else if(_sdlKeyScanCode == _hardware["Reset"].scanCode  &&  _sdlKeyModifier == _hardware["Reset"].modifier) {Loader::sendCommandToGiga('R', false); return;}

        // Scanline handler
        else if(_sdlKeyScanCode == _emulator["ScanlineMode"].scanCode  &&  _sdlKeyModifier == _emulator["ScanlineMode"].modifier)
        {
            // ROMS after v1 have their own inbuilt scanline handlers
            if(Cpu::getRomType() != Cpu::ROMv1) {Cpu::setIN(Cpu::getIN() | INPUT_SELECT); return;}
        }

        // PS2 Keyboard emulation mode
        else if(handlePs2KeyDown()) return;

        // Gigatron Keyboard emulation mode
        else if(handleGigaKeyDown()) return;

        // Buffered audio locks the emulator to 60Hz
        else if(Audio::getRealTimeAudio()  &&  _sdlKeyScanCode == _emulator["Speed+"].scanCode  &&  _sdlKeyModifier == _emulator["Speed+"].modifier)
        {
            double timingHack = Timing::getTimingHack() - VSYNC_TIMING_60*0.05;
            if(timingHack >= 0.0) {Timing::setTimingHack(timingHack); return;}
        }

        else if(Audio::getRealTimeAudio()  &&  _sdlKeyScanCode == _emulator["Speed-"].scanCode  &&  _sdlKeyModifier == _emulator["Speed-"].modifier)
        {
            double timingHack = Timing::getTimingHack() + VSYNC_TIMING_60*0.05;
            if(timingHack <= VSYNC_TIMING_60) {Timing::setTimingHack(timingHack); return;}
        }
    }

    // PS2 Keyboard emulation mode
    bool handlePs2KeyUp(void)
    {
        _handlePS2Key = false;

        if(_keyboardMode == PS2)
        {
            switch(_sdlKeyScanCode)
            {
                case SDLK_LEFT:     Cpu::setIN(0xFF); return true;
                case SDLK_RIGHT:    Cpu::setIN(0xFF); return true;
                case SDLK_UP:       Cpu::setIN(0xFF); return true;
                case SDLK_DOWN:     Cpu::setIN(0xFF); return true;
                case SDLK_PAGEUP:   Cpu::setIN(0xFF); return true;
                case SDLK_PAGEDOWN: Cpu::setIN(0xFF); return true;
            }

            if((_sdlKeyScanCode >= 0  &&  _sdlKeyScanCode <= 31) ||  _sdlKeyScanCode == 127  ||  _sdlKeyScanCode == 'c')
            {
                switch(_sdlKeyScanCode)
                {
                    case SDLK_TAB:       Cpu::setIN(0xFF); return true;
                    case SDLK_ESCAPE:    Cpu::setIN(0xFF); return true;
                    case SDLK_RETURN:    Cpu::setIN(0xFF); return true;
                    case SDLK_DELETE:    Cpu::setIN(0xFF); return true;

                    case 'c': if(_sdlKeyModifier == KMOD_LCTRL) Cpu::setIN(0xFF); return true;
                }
            }

            if(_sdlKeyScanCode >= 32  &&  _sdlKeyScanCode <= 126)
            {
                Cpu::setIN(0xFF);
                return true;
            }
        }
        else if(_keyboardMode == HwPS2)
        {
            if(_sdlKeyScanCode >= 0  &&  _sdlKeyScanCode <= 127) return true;
        }

        return false;
    }

    // Gigatron Keyboard emulation mode
    bool handleGigaKeyUp(void)
    {
        if(_keyboardMode == Giga)
        {
            if(_sdlKeyScanCode == _keyboard["Left"].scanCode  &&  _sdlKeyModifier == _keyboard["Left"].modifier)          {Cpu::setIN(Cpu::getIN() | INPUT_LEFT);   return true;}
            else if(_sdlKeyScanCode == _keyboard["Right"].scanCode  &&  _sdlKeyModifier == _keyboard["Right"].modifier)   {Cpu::setIN(Cpu::getIN() | INPUT_RIGHT);  return true;}
            else if(_sdlKeyScanCode == _keyboard["Up"].scanCode  &&  _sdlKeyModifier == _keyboard["Up"].modifier)         {Cpu::setIN(Cpu::getIN() | INPUT_UP);     return true;}
            else if(_sdlKeyScanCode == _keyboard["Down"].scanCode  &&  _sdlKeyModifier == _keyboard["Down"].modifier)     {Cpu::setIN(Cpu::getIN() | INPUT_DOWN);   return true;}
            else if(_sdlKeyScanCode == _keyboard["Start"].scanCode  &&  _sdlKeyModifier == _keyboard["Start"].modifier)   {Cpu::setIN(Cpu::getIN() | INPUT_START);  return true;}
            else if(_sdlKeyScanCode == _keyboard["Select"].scanCode  &&  _sdlKeyModifier == _keyboard["Select"].modifier) {Cpu::setIN(Cpu::getIN() | INPUT_SELECT); return true;}
            else if(_sdlKeyScanCode == _keyboard["A"].scanCode  &&  _sdlKeyModifier == _keyboard["A"].modifier)           {Cpu::setIN(Cpu::getIN() | INPUT_A);      return true;}
            else if(_sdlKeyScanCode == _keyboard["B"].scanCode  &&  _sdlKeyModifier == _keyboard["B"].modifier)           {Cpu::setIN(Cpu::getIN() | INPUT_B);      return true;}
        }

        return false;
    }

    void handleKeyUp(void)
    {
        // Toggle help screen
        if(_sdlKeyScanCode == _emulator["Help"].scanCode  &&  _sdlKeyModifier == _emulator["Help"].modifier)
        {
            static bool helpScreen = false;
            helpScreen = !helpScreen;
            Graphics::setDisplayHelpScreen(helpScreen);
        }

        // Disassembler
        else if(_sdlKeyScanCode == _emulator["Disassembler"].scanCode  &&  _sdlKeyModifier == _emulator["Disassembler"].modifier)
        {
            if(_editorMode == Dasm)
            {
                _editorMode = _editorModePrev;
                _editorModePrev = Dasm;
            }
            else
            {
                _editorModePrev = _editorMode;
                _editorMode = Dasm;
            }
        }

        // ROMS after v1 have their own inbuilt scanline handlers
        else if(_sdlKeyScanCode == _emulator["ScanlineMode"].scanCode  &&  _sdlKeyModifier == _emulator["ScanlineMode"].modifier)
        {
            (Cpu::getRomType() != Cpu::ROMv1) ? Cpu::setIN(Cpu::getIN() & ~INPUT_SELECT) : Cpu::swapScanlineMode();
        }

        // Browse vCPU directory
        else if(_sdlKeyScanCode == _emulator["Browse"].scanCode  &&  _sdlKeyModifier == _emulator["Browse"].modifier)
        {
            if(!_singleStepEnabled)
            {
                if(_editorMode == Load)
                {
                    _editorMode = _editorModePrev;
                    _editorModePrev = Load;
                }
                else
                {
                    _editorModePrev = _editorMode;
                    _editorMode = Load;
                    browseDirectory();
                }
            }
        }

        // ROM type
        else if(_sdlKeyScanCode == _emulator["RomType"].scanCode  &&  _sdlKeyModifier == _emulator["RomType"].modifier)
        {
            if(!_singleStepEnabled)
            {
                if(_editorMode == Rom)
                {
                    _editorMode = _editorModePrev;
                    _editorModePrev = Rom;
                }
                else
                {
                    _editorModePrev = _editorMode;
                    _editorMode = Rom;
                    _hexEdit = false;
                    resetEditor();
                }
            }
        }

        // ROM type
        else if(_sdlKeyScanCode == _emulator["HexMonitor"].scanCode  &&  _sdlKeyModifier == _emulator["HexMonitor"].modifier)
        {
            if(_editorMode == Hex)
            {
                _editorMode = _editorModePrev;
                _editorModePrev = Hex;
            }
            else
            {
                _editorModePrev = _editorMode;
                _editorMode = Hex;
            }
        }

        // Debug mode
        else if(_sdlKeyScanCode == _debugger["Debug"].scanCode  &&  _sdlKeyModifier == _debugger["Debug"].modifier)
        {
            startDebugger();
        }

        // Keyboard mode
        else if(_sdlKeyScanCode ==  _keyboard["Mode"].scanCode  &&  _sdlKeyModifier == _keyboard["Mode"].modifier)
        {
            int keyboardMode = _keyboardMode;
            keyboardMode = (++keyboardMode) % NumKeyboardModes;
            _keyboardMode = (KeyboardMode)keyboardMode;

            // Enable/disable Arduino emulated PS2 keyboard
            (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(HW_PS2_ENABLE,  true) : Loader::sendCommandToGiga(HW_PS2_DISABLE, true);
        }

        // RAM/ROM mode
        else if(_sdlKeyScanCode ==  _emulator["MemoryMode"].scanCode  &&  _sdlKeyModifier == _emulator["MemoryMode"].modifier)
        {
            int memoryMode = _memoryMode;
            memoryMode = (_editorMode == Dasm) ? (++memoryMode) % (NumMemoryModes-1) : (++memoryMode) % NumMemoryModes;
            _memoryMode = (MemoryMode)memoryMode;
            
            // Update hex address with PC or vPC
            if(_singleStepEnabled)
            {
                _hexBaseAddress = (_memoryMode == RAM) ? Cpu::getVPC() : Cpu::getStateS()._PC;
                _vpcBaseAddress = _hexBaseAddress;
            }
        }

        // RAM Size
        else if(_sdlKeyScanCode ==  _emulator["MemorySize"].scanCode  &&  _sdlKeyModifier == _emulator["MemorySize"].modifier)
        {
            Cpu::swapMemoryModel();
        }

        // Hardware upload and execute
        else if(_sdlKeyScanCode == _hardware["Execute"].scanCode  &&  _sdlKeyModifier == _hardware["Execute"].modifier)
        {
            if(!_singleStepEnabled) Loader::setUploadTarget(Loader::Hardware);
        }

        updateEditor();

        // PS2 Keyboard emulation mode
        if(handlePs2KeyUp()) return;

        // Gigatron Keyboard emulation mode
        if(handleGigaKeyUp()) return;
    }


    void handleGuiEvents(SDL_Event& event)
    {
        switch(event.type)
        {
            case SDL_WINDOWEVENT:
            {
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    {
                        Graphics::setWidthHeight(event.window.data1, event.window.data2);
                    }
                    break;
                }
            }
            break;

            case SDL_QUIT: 
            {
                Cpu::shutdown();
                exit(0);
            }
        }
    }


    void singleStep(uint16_t address)
    {
        _singleStep = false;
        _singleStepEnabled = true;
        _hexBaseAddress = address;
        _vpcBaseAddress = address;
    }

    // Debug mode, handles it's own input and rendering
    bool handleDebugger(void)
    {
        // Gprintfs
        Assembler::printGprintfStrings();

        // Debug
        static uint16_t vPC = Cpu::getVPC();
        if(_singleStep)
        {
            static int cycles = 0;

            // Native debugging
            if(_memoryMode != RAM)
            {
                singleStep(Cpu::getStateS()._PC);
            }
            // vCPU debugging, (this code can potentially run for every Native instruction, for efficiency we check vPC so this code only runs for each vCPU instruction)
            else if(vPC != Cpu::getVPC()  ||  cycles >= MAX_SINGLE_STEP_CYCLES)
            {
                vPC = Cpu::getVPC();

                // Timeout on change of variable
                if(SDL_GetTicks() - _singleStepTicks > SINGLE_STEP_STALL_TIME)
                {
                    resetDebugger();
                    fprintf(stderr, "Editor::handleDebugger() : Single step stall for %d milliseconds : exiting debugger.\n", SDL_GetTicks() - _singleStepTicks);
                }
                // Single step on vPC or on watch value
                switch(_singleStepMode)
                {
                    case RunToBrk:
                    {
                        auto it = std::find(_breakPoints.begin(), _breakPoints.end(), vPC);
                        if(it != _breakPoints.end()) singleStep(vPC);
                    }
                    break;
                
                    case StepVpc:
                    {
                        // Step whenever program counter changes or when MAX_SINGLE_STEP_CYCLES cycles have occured, (to avoid deadlocks)
                        if(vPC != _singleStepValue  ||  cycles >= MAX_SINGLE_STEP_CYCLES) singleStep(vPC);
                    }
                    break;

                    case StepWatch:
                    {
                        if(Cpu::getRAM(_singleStepAddress) != _singleStepValue) singleStep(vPC);
                    }
                    break;
                }

                cycles = 0;
            }

            cycles++;
        }

        // Pause simulation and handle debugging keys
        while(_singleStepEnabled)
        {
            // Update graphics but only once every 16.66667ms
            static uint64_t prevFrameCounter = 0;
            double frameTime = double(SDL_GetPerformanceCounter() - prevFrameCounter) / double(SDL_GetPerformanceFrequency());

            Timing::setFrameUpdate(false);
            if(frameTime > VSYNC_TIMING_60)
            {
                _onVarType = updateOnVarType();

                prevFrameCounter = SDL_GetPerformanceCounter();
                Timing::setFrameUpdate(true);
                Graphics::refreshScreen();
                Graphics::render(false);
            }

            SDL_Event event;
            while(SDL_PollEvent(&event))
            {
                _sdlKeyScanCode = event.key.keysym.sym;
                _sdlKeyModifier = event.key.keysym.mod & (KMOD_LCTRL | KMOD_LALT);
                _mouseState._state = SDL_GetMouseState(&_mouseState._x, &_mouseState._y);

                handleGuiEvents(event);

                switch(event.type)
                {
                    case SDL_MOUSEBUTTONDOWN:
                    {
                        if(_pageUpButton  &&  event.button.button == SDL_BUTTON_LEFT) handlePageUp(HEX_CHARS_Y);
                        else if(_pageDnButton  &&  event.button.button == SDL_BUTTON_LEFT) handlePageDown(HEX_CHARS_Y);
                        else if(_memoryMode == RAM  &&  _delAllButton  &&  event.button.button == SDL_BUTTON_LEFT) _breakPoints.clear();
                        else if(event.button.button == SDL_BUTTON_LEFT) handleMouseLeftClick();
                    }
                    break;

                    case SDL_MOUSEWHEEL:
                    {
                        handleMouseWheel(event);
                    }
                    break;

                    case SDL_KEYUP:
                    {
                        // Leave debug mode
                        if((_sdlKeyScanCode == _debugger["Debug"].scanCode  &&  _sdlKeyModifier == _debugger["Debug"].modifier))
                        {
                            resetDebugger();
                        }
                        else
                        {
                            handleKeyUp();
                        }
                    }
                    break;

                    case SDL_KEYDOWN:
                    {
                        // Run to breakpoint, (vCPU only)
                        if(_memoryMode == RAM  &&  _sdlKeyScanCode == _debugger["RunToBrk"].scanCode  &&  _sdlKeyModifier == _debugger["RunToBrk"].modifier)
                        {
                            _singleStep = true;
                            _singleStepEnabled = false;
                            _singleStepMode = RunToBrk;
                            _singleStepTicks = SDL_GetTicks();
                        }
                        // Single step on vPC or on PC, (vCPU or native)
                        else if(_sdlKeyScanCode == _debugger["StepVpc"].scanCode  &&  _sdlKeyModifier == _debugger["StepVpc"].modifier)
                        {
                            _singleStep = true;
                            _singleStepEnabled = false;
                            _singleStepMode = StepVpc;
                            _singleStepTicks = SDL_GetTicks();
                            _singleStepValue = (_memoryMode == RAM) ? Cpu::getVPC() : Cpu::getStateS()._PC;
                        }
                        // Single step on watch value, (vCPU only)
                        else if(_memoryMode == RAM  &&  _sdlKeyScanCode == _debugger["StepWatch"].scanCode  &&  _sdlKeyModifier == _debugger["StepWatch"].modifier)
                        {
                            _singleStep = true;
                            _singleStepEnabled = false;
                            _singleStepMode = StepWatch;
                            _singleStepTicks = SDL_GetTicks();
                            _singleStepValue = Cpu::getRAM(_singleStepAddress);
                        }
                        else
                        {
                            handleKeyDown();
                        }
                    }
                    break;
                }
            }
        }

        return _singleStep;
    }

    void handlePS2key(SDL_Event& event)
    {
        if(_handlePS2Key) (_keyboardMode == HwPS2) ? Loader::sendCommandToGiga(event.text.text[0], true) : Cpu::setIN(event.text.text[0]);
    }

    void handleInput(void)
    {
        _onVarType = updateOnVarType();

        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            _sdlKeyScanCode = event.key.keysym.sym;
            _sdlKeyModifier = event.key.keysym.mod & (KMOD_LCTRL | KMOD_LALT);
            _mouseState._state = SDL_GetMouseState(&_mouseState._x, &_mouseState._y);

            handleGuiEvents(event);

            switch(event.type)
            {
                case SDL_MOUSEBUTTONDOWN:
                {
                    if(_pageUpButton  &&  event.button.button == SDL_BUTTON_LEFT) handlePageUp(HEX_CHARS_Y);
                    else if(_pageDnButton  &&  event.button.button == SDL_BUTTON_LEFT) handlePageDown(HEX_CHARS_Y);
                    else if(_memoryMode == RAM  &&  _delAllButton  &&  event.button.button == SDL_BUTTON_LEFT) _breakPoints.clear();
                    else if(event.button.button == SDL_BUTTON_LEFT) handleMouseLeftClick();
                    else if(event.button.button == SDL_BUTTON_RIGHT) handleMouseRightClick();
                }
                break;

                case SDL_MOUSEWHEEL: handleMouseWheel(event); break;
                case SDL_TEXTINPUT:  handlePS2key(event);     break;
                case SDL_KEYDOWN:    handleKeyDown();         break;
                case SDL_KEYUP:      handleKeyUp();           break;
            }
        }

        // Updates current game's high score once per second, (assuming handleInput is called in vertical blank)
        Loader::updateHighScore();
    }
}
