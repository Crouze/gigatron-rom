#include <algorithm>

#include "graphics.h"
#include "timing.h"
#include "editor.h"


namespace Graphics
{
    uint32_t _pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    uint32_t _colours[COLOUR_PALETTE];

    SDL_Window* _window = NULL;
    SDL_Renderer* _renderer = NULL;
    SDL_Texture* _texture = NULL;
    SDL_Surface* _surface = NULL;
    SDL_Surface* _fontSurface = NULL;


    uint32_t* getPixels(void) {return _pixels;}
    uint32_t* getColours(void) {return _colours;}

    SDL_Window* getWindow(void) {return _window;}
    SDL_Renderer* getRenderer(void) {return _renderer;}
    SDL_Texture* getTexture(void) {return _texture;}
    SDL_Surface* getSurface(void) {return _surface;}
    SDL_Surface* getFontSurface(void) {return _fontSurface;}


    void initialise(void)
    {
        for(int i=0; i<COLOUR_PALETTE; i++)
        {
            int r = (i>>0) & 3;
            int g = (i>>2) & 3;
            int b = (i>>4) & 3;

            r = r | (r << 2) | (r << 4) | (r << 6);
            g = g | (g << 2) | (g << 4) | (g << 6);
            b = b | (b << 2) | (b << 4) | (b << 6);

            uint32_t p = 0xFF000000;
            p |= r << 16;
            p |= g << 8;
            p |= b << 0;
            _colours[i] = p;
        }

        //SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
        if(SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP, &_window, &_renderer) < 0)
        {
            SDL_Quit();
            fprintf(stderr, "Error: failed to create SDL _window\n");
            exit(EXIT_FAILURE);
        }

        _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, SCREEN_WIDTH, SCREEN_HEIGHT);
        if(_texture == NULL)
        {
            SDL_Quit();
            fprintf(stderr, "Error: failed to create SDL _texture\n");
            exit(EXIT_FAILURE);
        }

        _surface = SDL_GetWindowSurface(_window);
        if(_surface == NULL)
        {
            SDL_Quit();
            fprintf(stderr, "Error: failed to create SDL _surface\n");
            exit(EXIT_FAILURE);
        }

        // Text
        SDL_Surface* fontSurface = SDL_LoadBMP("EmuFont-96x48.bmp"); 
        if(fontSurface == NULL)
        {
            SDL_Quit();
            fprintf(stderr, "Error: failed to create SDL font _surface\n");
            exit(EXIT_FAILURE);
        }
        _fontSurface = SDL_ConvertSurfaceFormat(fontSurface, _surface->format->format, NULL);
        SDL_FreeSurface(fontSurface);
        if(_fontSurface == NULL)
        {
            SDL_Quit();
            fprintf(stderr, "Error: failed to create SDL converted font _surface\n");
            exit(EXIT_FAILURE);
        }
    }

    void refreshPixel(const Cpu::State& S, int vgaX, int vgaY)
    {
        uint32_t colour = _colours[S._OUT & (COLOUR_PALETTE-1)];
        uint32_t address = vgaX*3 + vgaY*SCREEN_WIDTH;
        _pixels[address + 0] = colour;
        _pixels[address + 1] = colour;
        _pixels[address + 2] = colour;
    }

    void refreshScreen(void)
    {
        for(int y=0; y<GIGA_HEIGHT; y++)
        {
            for(int x=0; x<GIGA_WIDTH; x++)
            {
                uint16_t address = GIGA_VRAM + x + (y <<8);
                uint32_t screen = x*3 + y*4*SCREEN_WIDTH;
                uint32_t colour = _colours[Cpu::getRAM(address) & (COLOUR_PALETTE-1)];

                _pixels[screen + 0 + 0*SCREEN_WIDTH] = colour; _pixels[screen + 1 + 0*SCREEN_WIDTH] = colour; _pixels[screen + 2 + 0*SCREEN_WIDTH] = colour;
                _pixels[screen + 0 + 1*SCREEN_WIDTH] = colour; _pixels[screen + 1 + 1*SCREEN_WIDTH] = colour; _pixels[screen + 2 + 1*SCREEN_WIDTH] = colour;
                _pixels[screen + 0 + 2*SCREEN_WIDTH] = colour; _pixels[screen + 1 + 2*SCREEN_WIDTH] = colour; _pixels[screen + 2 + 2*SCREEN_WIDTH] = colour;
                _pixels[screen + 0 + 3*SCREEN_WIDTH] = 0x00;   _pixels[screen + 1 + 3*SCREEN_WIDTH] = 0x00;   _pixels[screen + 2 + 3*SCREEN_WIDTH] = 0x00;
            }
        }
    }

    void drawLeds(void)
    {
        // Update 60 times per second no matter what the FPS is
        if(Timing::getFrameTime()  &&  Timing::getFrameUpdate())
        {
            for(int i=0; i<NUM_LEDS; i++)
            {
                int mask = 1 << (NUM_LEDS-1 - i);
                int state = (Cpu::getXOUT() & mask) != 0;
                uint32_t colour = state ? 0xFF00FF00 : 0xFF770000;

                int address = int(float(SCREEN_WIDTH) * 0.866f) + i*NUM_LEDS + 3*SCREEN_WIDTH;
                _pixels[address + 0] = colour;
                _pixels[address + 1] = colour;
                _pixels[address + 2] = colour;
                address += SCREEN_WIDTH;
                _pixels[address + 0] = colour;
                _pixels[address + 1] = colour;
                _pixels[address + 2] = colour;
            }
        }
    }

    // Simple text routine, font is a non proportional 6*8 font loaded from a 96*48 BMP file
    bool drawText(const std::string& text, int x, int y, uint32_t colour, bool invert, int invertSize)
    {
        if(x<0 || x>=SCREEN_WIDTH || y<0 || y>=SCREEN_HEIGHT) return false;

        uint32_t* fontPixels = (uint32_t*)_fontSurface->pixels;
        for(int i=0; i<text.size(); i++)
        {
            uint8_t chr = text.c_str()[i] - 32;
            uint8_t row = chr % CHARS_PER_ROW;
            uint8_t col = chr / CHARS_PER_ROW;

            int srcx = row*FONT_WIDTH, srcy = col*FONT_HEIGHT;
            if(srcx+FONT_WIDTH-1>=FONT_BMP_WIDTH || srcy+FONT_HEIGHT-1>=FONT_BMP_HEIGHT) return false;

            int dstx = x + i*FONT_WIDTH, dsty = y;
            if(dstx+FONT_WIDTH-1>=SCREEN_WIDTH || dsty+FONT_HEIGHT-1>=SCREEN_HEIGHT) return false;

            for(int j=0; j<FONT_WIDTH; j++)
            {
                for(int k=0; k<FONT_HEIGHT; k++)
                {
                    int fontAddress = (srcx + j)  +  (srcy + k)*FONT_BMP_WIDTH;
                    int pixelAddress = (dstx + j)  +  (dsty + k)*SCREEN_WIDTH;
                    if((invert  &&  i<invertSize) ? !fontPixels[fontAddress] : fontPixels[fontAddress])
                    {
                        _pixels[pixelAddress] = 0xFF000000 | colour;
                    }
                    else
                    {
                        _pixels[pixelAddress] = 0xFF000000;
                    }
                }
            }
        }

        return true;
    }

    void drawDigitBox(uint8_t digit, int x, int y, uint32_t colour)
    {
        uint32_t pixelAddress = x + digit*FONT_WIDTH + y*SCREEN_WIDTH;

        pixelAddress += (FONT_HEIGHT-1)*SCREEN_WIDTH;
        for(int i=0; i<FONT_WIDTH; i++) _pixels[pixelAddress+i] = colour;

        //pixelAddress += (FONT_HEIGHT-4)*SCREEN_WIDTH;
        //for(int i=0; i<FONT_WIDTH; i++) _pixels[pixelAddress+i] = colour;
        //pixelAddress += SCREEN_WIDTH;
        //for(int i=0; i<FONT_WIDTH; i++) _pixels[pixelAddress+i] = colour;
        //pixelAddress += SCREEN_WIDTH;
        //for(int i=0; i<FONT_WIDTH; i++) _pixels[pixelAddress+i] = colour;
        //pixelAddress += SCREEN_WIDTH;
        //for(int i=0; i<FONT_WIDTH; i++) _pixels[pixelAddress+i] = colour;

        //for(int i=0; i<FONT_WIDTH-1; i++) _pixels[pixelAddress+i] = colour;
        //pixelAddress += (FONT_HEIGHT-1)*SCREEN_WIDTH;
        //for(int i=0; i<FONT_WIDTH-1; i++) _pixels[pixelAddress+i] = colour;
        //for(int i=0; i<FONT_HEIGHT; i++) _pixels[pixelAddress-i*SCREEN_WIDTH] = colour;
        //pixelAddress += FONT_WIDTH-1;
        //for(int i=0; i<FONT_HEIGHT; i++) _pixels[pixelAddress-i*SCREEN_WIDTH] = colour;
    }

    void renderText(void)
    {
        // Update 60 times per second no matter what the FPS is
        if(Timing::getFrameTime()  &&  Timing::getFrameUpdate())
        {
            char str[32];
            sprintf(str, "%3.2f    ", 1.0f / Timing::getFrameTime());

            drawText(std::string("LEDS:"), 485, 0, 0xFFFFFFFF, false, 0);
            drawText(std::string("FPS:"), 485, FONT_CELL_Y, 0xFFFFFFFF, false, 0);
            drawText(std::string(str), 547, FONT_CELL_Y, 0xFFFFFF00, false, 0);
            drawText(std::string(VERSION_STR), 531, 472, 0xFFFFFF00, false, 0);
            sprintf(str, "XOUT: %02X  IN: %02X", Cpu::getXOUT(), Cpu::getIN());
            drawText(std::string(str), 485, FONT_CELL_Y*2, 0xFFFFFFFF, false, 0);
            drawText("Mode:       ", 485, 472 - FONT_CELL_Y, 0xFFFFFFFF, false, 0);
            sprintf(str, "Hex ");
            if(Editor::getHexEdit() == true) sprintf(str, "Edit");
            if(Editor::getLoadFile() == true) sprintf(str, "Load");
            drawText(std::string(str), 553, 472 - FONT_CELL_Y, 0xFF00FF00, false, 0);
        }
    }

    void renderTextWindow(void)
    {
        // Update 60 times per second no matter what the FPS is
        if(Timing::getFrameTime()  &&  Timing::getFrameUpdate())
        {
            char str[32] = "";

            // File load
            if(Editor::getLoadFile() == true)
            {
                // File list
                drawText("Load:       Vars:", 485, FONT_CELL_Y*3, 0xFFFFFFFF, false, 0);
                uint16_t loadAddress = Editor::getLoadBaseAddress();
                uint16_t varsAddress = Editor::getVarsBaseAddress();
                bool onCursor0 = Editor::getCursorY() == -1  &&  (Editor::getCursorX() & 0x01) == 0;
                bool onCursor1 = Editor::getCursorY() == -1  &&  (Editor::getCursorX() & 0x01) == 1;
                sprintf(str, "%04X", loadAddress);
                drawText(std::string(str), 521, FONT_CELL_Y*3, (Editor::getHexEdit() && onCursor0) ? 0xFF00FF00 : 0xFFFFFFFF, onCursor0, 4);
                sprintf(str, "%04X", varsAddress);
                drawText(std::string(str), 593, FONT_CELL_Y*3, (Editor::getHexEdit() && onCursor1) ? 0xFF00FF00 : 0xFFFFFFFF, onCursor1, 4);
                for(int i=0; i<HEX_CHARS_Y; i++)
                {
                    drawText("                       ", 493, FONT_CELL_Y*4 + i*FONT_CELL_Y, 0xFFFFFFFF, false, 0);
                }
                for(int i=0; i<HEX_CHARS_Y; i++)
                {
                    int index = Editor::getFileNamesIndex() + i;
                    if(index >= int(Editor::getFileNamesSize())) break;
                    drawText(*Editor::getFileName(Editor::getFileNamesIndex() + i), 493, FONT_CELL_Y*4 + i*FONT_CELL_Y, 0xFFFFFFFF, i == Editor::getCursorY(), 18);
                }

                // 8 * 2 hex display of vCPU program variables
                for(int j=0; j<2; j++)
                {
                    for(int i=0; i<HEX_CHARS_X; i++)
                    {
                        sprintf(str, "%02X ", Cpu::getRAM(varsAddress++));
                        drawText(std::string(str), 493 + i*HEX_CHAR_WIDE, int(FONT_CELL_Y*4.25) + FONT_CELL_Y*HEX_CHARS_Y + j*(FONT_HEIGHT+FONT_GAP_Y), 0xFF00FFFF, false, 0);
                    }
                }

                // Edit digit select
                if(Editor::getHexEdit() == true)
                {
                    // Draw address digit selection box
                    if(onCursor0 == true) drawDigitBox(Editor::getAddressDigit(), 521, FONT_CELL_Y*3, 0xFFFF00FF);
                    if(onCursor1 == true) drawDigitBox(Editor::getAddressDigit(), 593, FONT_CELL_Y*3, 0xFFFF00FF);
                }
            }
            // Hex monitor
            else
            {
                switch(Editor::getHexRomMode())
                {
                    case 0: drawText("RAM:        Vars:", 485, FONT_CELL_Y*3, 0xFFFFFFFF, false, 0); break;
                    case 1: drawText("ROM0:       Vars:", 485, FONT_CELL_Y*3, 0xFFFFFFFF, false, 0); break;
                    case 2: drawText("ROM1:       Vars:", 485, FONT_CELL_Y*3, 0xFFFFFFFF, false, 0); break;
                }

                // 8 * 32 hex display of memory
                uint16_t hexddress = Editor::getHexBaseAddress();
                uint16_t varsAddress = Editor::getVarsBaseAddress();
                bool onCursor = Editor::getCursorY() == -1;
                bool onCursor0 = Editor::getCursorY() == -1  &&  (Editor::getCursorX() & 0x01) == 0;
                bool onCursor1 = Editor::getCursorY() == -1  &&  (Editor::getCursorX() & 0x01) == 1;
                sprintf(str, "%04X", hexddress);
                drawText(std::string(str), 521, FONT_CELL_Y*3, (Editor::getHexEdit() && onCursor0) ? 0xFF00FF00 : 0xFFFFFFFF, onCursor0, 4);
                sprintf(str, "%04X", varsAddress);
                drawText(std::string(str), 593, FONT_CELL_Y*3, (Editor::getHexEdit() && onCursor1) ? 0xFF00FF00 : 0xFFFFFFFF, onCursor1, 4);
                for(int j=0; j<HEX_CHARS_Y; j++)
                {
                    for(int i=0; i<HEX_CHARS_X; i++)
                    {
                        uint8_t value = 0;
                        switch(Editor::getHexRomMode())
                        {
                            case 0: value = Cpu::getRAM(hexddress++);    break;
                            case 1: value = Cpu::getROM(hexddress++, 0); break;
                            case 2: value = Cpu::getROM(hexddress++, 1); break;
                        }
                        sprintf(str, "%02X ", value);
                        bool onCursor = (i == Editor::getCursorX()  &&  j == Editor::getCursorY());
                        drawText(std::string(str), 493 + i*HEX_CHAR_WIDE, FONT_CELL_Y*4 + j*(FONT_HEIGHT+FONT_GAP_Y), (Editor::getHexEdit() && Editor::getHexRomMode() ==0 && onCursor) ? 0xFF00FF00 : 0xFFFFFFFF, onCursor, 2);
                    }
                }

                // 8 * 2 hex display of vCPU program variables
                for(int j=0; j<2; j++)
                {
                    for(int i=0; i<HEX_CHARS_X; i++)
                    {
                        sprintf(str, "%02X ", Cpu::getRAM(varsAddress++));
                        drawText(std::string(str), 493 + i*HEX_CHAR_WIDE, int(FONT_CELL_Y*4.25) + FONT_CELL_Y*HEX_CHARS_Y + j*(FONT_HEIGHT+FONT_GAP_Y), 0xFF00FFFF, false, 0);
                    }
                }

                // Edit digit select
                if(Editor::getHexEdit() == true)
                {
                    // Draw address digit selection box
                    if(onCursor0 == true) drawDigitBox(Editor::getAddressDigit(), 521, FONT_CELL_Y*3, 0xFFFF00FF);
                    if(onCursor1 == true) drawDigitBox(Editor::getAddressDigit(), 593, FONT_CELL_Y*3, 0xFFFF00FF);

                    // Draw memory digit selection box                
                    if(Editor::getCursorY() >= 0  &&  Editor::getHexRomMode() == 0) drawDigitBox(Editor::getMemoryDigit(), 493 + Editor::getCursorX()*HEX_CHAR_WIDE, FONT_CELL_Y*4 + Editor::getCursorY()*FONT_CELL_Y, 0xFFFF00FF);
                }
            }
        }
    }

    void render(bool synchronise)
    {
        drawLeds();
        renderText();
        renderTextWindow();

        SDL_UpdateTexture(_texture, NULL, _pixels, SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderCopy(_renderer, _texture, NULL, NULL);
        SDL_RenderPresent(_renderer);

        if(synchronise == true) Timing::synchronise();
    }
}