#include <map>
#include <string>
#include <ctime>
#include <climits>
#ifdef WIN
#define _WIN32_WINNT 0x0501    //Necessary for some macros and functions, tells windows.h to include functions only available in Windows XP or later
#include <direct.h>
#endif
#include "libretro.h"

#ifdef X86_SSE
#include <xmmintrin.h>
#endif
#ifdef X86_SSE3
#include <pmmintrin.h>
#endif

#include <iostream>
#include <sstream>
#include <string>
#include "Config.h"
#include "graphics/Graphics.h"
#if defined(LIN)
#include "icon.h"
#endif
#include <signal.h>
#include <stdexcept>

#ifndef WIN
#include <unistd.h>
#endif
/*#ifdef MACOSX
#include <ApplicationServices/ApplicationServices.h>
extern "C" {
    char * readClipboard();
    void writeClipboard(const char * clipboardData);
}
#endif*/

#include "Format.h"

#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "simulation/SaveRenderer.h"
#include "client/Client.h"
#include "Misc.h"

#include "gui/game/GameController.h"
#include "gui/game/GameView.h"

#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/interface/Keys.h"
#include "gui/Style.h"

#include "client/HTTP.h"

#include "Environment.h"

using namespace std;

/*#define INCLUDE_SYSWM
#include "SDLCompat.h"
#if defined(USE_SDL) && defined(LIN) && defined(SDL_VIDEO_DRIVER_X11)
SDL_SysWMinfo sdl_wminfo;
Atom XA_CLIPBOARD, XA_TARGETS, XA_UTF8_STRING;
#endif

std::string clipboardText = "";*/

int elapsedTime = 0, currentTime = 0, lastTime = 0, currentFrame = 0;
unsigned int lastTick = 0;
float fps = 0, delta = 1.0f, inputScale = 1.0f;
ui::Engine * engine = NULL;
bool showDoubleScreenDialog = false;
float currentWidth, currentHeight;
bool crashed = false;
GameController * gameController = NULL;
bool hasLeftHeld;
bool hasRightHeld;
bool hasMiddleHeld;
uint8_t* framebuffer;
int mouseX, resultX;
int mouseY, resultY;

void EngineProcess() {
    if (!engine->Running()) {
        LibRetro::Shutdown();
        return;
    }

    if (!crashed) {
        if (engine->Broken()) {
            engine->UnBreak();
            return;
        }

        engine->Tick();
        engine->Draw();
    }

    auto buffer = engine->g->DumpFrame();

    // We want to copy the pixels into the framebuffer with the longer lifetime, otherwise
    // the frontend may read garbage pixels.
    memcpy(framebuffer, buffer.Buffer, WINDOWW * WINDOWH * 4);

    LibRetro::UploadVideoFrame(framebuffer, buffer.Width, buffer.Height, 4 * buffer.Width);
}

void BlueScreen(const char * detailMessage){
    ui::Engine * engine = &ui::Engine::Ref();
    engine->g->fillrect(0, 0, engine->GetWidth(), engine->GetHeight(), 17, 114, 169, 210);

    std::string errorTitle = "ERROR";
    std::string errorDetails = "Details: " + std::string(detailMessage);
    std::string errorHelp = "An unrecoverable fault has occurred, please report the error by visiting the website below\n"
        "https://github.com/libretro/ThePowderToy/issues";
    int currentY = 0, width, height;
    int errorWidth = 0;
    Graphics::textsize(errorHelp.c_str(), errorWidth, height);

    engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorTitle.c_str(), 255, 255, 255, 255);
    Graphics::textsize(errorTitle.c_str(), width, height);
    currentY += height + 4;

    engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorDetails.c_str(), 255, 255, 255, 255);
    Graphics::textsize(errorTitle.c_str(), width, height);
    currentY += height + 4;

    engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorHelp.c_str(), 255, 255, 255, 255);
    Graphics::textsize(errorTitle.c_str(), width, height);
    currentY += height + 4;

    crashed = true;
}

void SigHandler(int signal)
{
    switch(signal) {
    case SIGSEGV:
        BlueScreen("Memory read/write error");
        break;
    case SIGFPE:
        BlueScreen("Floating point exception");
        break;
    case SIGILL:
        BlueScreen("Program execution exception");
        break;
    case SIGABRT:
        BlueScreen("Unexpected program abort");
        break;
    }
}

void keyboard_callback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers) {
    bool shift = (key_modifiers & RETROKMOD_SHIFT) != 0;
    bool ctrl = (key_modifiers & RETROKMOD_CTRL) != 0;
    bool alt = (key_modifiers & RETROKMOD_ALT) != 0;

    if (down) {
        engine->onKeyPress(keycode, character, shift, ctrl, alt);
    } else {
        engine->onKeyRelease(keycode, character, shift, ctrl, alt);
    }
}

void retro_init() {
    printf("Core init\n");

    retro_keyboard_callback callback{};
    callback.callback = keyboard_callback;
    if (!LibRetro::SetKeyboardCallback(&callback)) {
        printf("Unable to set keyboard callback\n");
    }

    framebuffer = static_cast<uint8_t *>(malloc(WINDOWW * WINDOWH * 4));

    LibRetro::SetPixelFormat(RETRO_PIXEL_FORMAT_XRGB8888);

    int tempScale = Client::Ref().GetPrefInteger("Scale", 1);

    int scale = tempScale;

    Client::Ref().Initialise("");

    ui::Engine::Ref().g = new Graphics();
    ui::Engine::Ref().Scale = scale;
    inputScale = 1.0f/float(scale);
    ui::Engine::Ref().Fullscreen = false;

    engine = &ui::Engine::Ref();
    engine->Begin(WINDOWW, WINDOWH);
    engine->SetFastQuit(Client::Ref().GetPrefBool("FastQuit", true));

#if !defined(DEBUG) && !defined(_DEBUG)
    //Get ready to catch any dodgy errors
    signal(SIGSEGV, SigHandler);
    signal(SIGFPE, SigHandler);
    signal(SIGILL, SigHandler);
    signal(SIGABRT, SigHandler);
#endif

#ifdef X86_SSE
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef X86_SSE3
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

    retro_reset();
}

void retro_deinit() {
    free(framebuffer);

    Client::Ref().SetPref("Scale", ui::Engine::Ref().GetScale());
    ui::Engine::Ref().CloseWindow();
    delete gameController;
    delete ui::Engine::Ref().g;
    Client::Ref().Shutdown();
}

unsigned retro_api_version() {
    return RETRO_API_VERSION;
}

void retro_run() {
    try {
        // TODO: currentFrame isn't used for anything AFAIK
        currentFrame++;
        if (currentFrame > 60) {
            Client::Ref().Tick();
            currentFrame = 0;
        }

        LibRetro::PollInput();
        bool left =
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED)) ||
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)) ||
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2));
        bool middle =
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE)) ||
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y));
        bool right =
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT)) ||
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2));

        auto pointerX = (float) LibRetro::CheckInput(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        auto pointerY = (float) LibRetro::CheckInput(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

        // Map pointer coordinates to screen
        pointerX /= INT16_MAX * 2;
        pointerY /= INT16_MAX * 2;
        pointerX += 0.5;
        pointerY += 0.5;
        pointerX *= WINDOWW;
        pointerY *= WINDOWH;

        auto mouseScroll =
                -(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP)
                  || LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L)) +
                (LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
                  || LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R));

        auto absX = (unsigned) pointerX;
        auto absY = (unsigned) pointerY;

        if (absX != mouseX || absY != mouseY) {
            mouseX = resultX = absX;
            mouseY = resultY = absY;
        } else {
            float controllerX =
                    ((float)LibRetro::CheckInput(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                                 RETRO_DEVICE_ID_ANALOG_X) /
                     INT16_MAX);
            float controllerY =
                    ((float)LibRetro::CheckInput(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                                 RETRO_DEVICE_ID_ANALOG_Y) /
                     INT16_MAX);

            // Deadzone the controller inputs
            float smoothedX = std::abs(controllerX);
            float smoothedY = std::abs(controllerY);

            // TODO: Make deadzone configurable
            if (smoothedX < 0.1) {
                controllerX = 0;
            }
            if (smoothedY < 0.1) {
                controllerY = 0;
            }

            // TODO: Make speed configurable
            resultX += static_cast<int>(controllerX * 3);
            resultY += static_cast<int>(controllerY * 3);
        }

        bool hasSentEvent = false;
        if (left && !hasLeftHeld) {
            engine->onMouseClick(resultX, resultY, 1);
            hasLeftHeld = true;
            hasSentEvent = true;
        } else if (!left && hasLeftHeld) {
            engine->onMouseUnclick(resultX, resultY, 1);
            hasLeftHeld = false;
            hasSentEvent = true;
        }

        if (middle && !hasMiddleHeld) {
            engine->onMouseClick(resultX, resultY, 2);
            hasMiddleHeld = true;
            hasSentEvent = true;
        } else if (!middle && hasMiddleHeld) {
            engine->onMouseUnclick(resultX, resultY, 2);
            hasMiddleHeld = false;
            hasSentEvent = true;
        }

        if (right && !hasRightHeld) {
            engine->onMouseClick(resultX, resultY, 3);
            hasRightHeld = true;
            hasSentEvent = true;
        } else if (!right && hasRightHeld) {
            engine->onMouseUnclick(resultX, resultY, 3);
            hasRightHeld = false;
            hasSentEvent = true;
        }

        if (mouseScroll != 0) {
            engine->onMouseWheel(resultX, resultY, mouseScroll);
        }

        if (!hasSentEvent) {
            engine->onMouseMove(resultX, resultY);
        }

        EngineProcess();
    }
    catch(exception& e)
    {
        BlueScreen(e.what());
    }
}

void retro_reset() {
    printf("Core reset\n");
    try {
        if (gameController != nullptr) {
            delete gameController;
        }
        gameController = new GameController();
        engine->ShowWindow(gameController->GetView());
    }
    catch(exception& e)
    {
        BlueScreen(e.what());
    }
}

bool retro_load_game(const struct retro_game_info* info) {
    if (info == nullptr) {
        return true;
    }

    if (info->data) {
        // TODO: should this say "LibRetro Content"?
        return retro_unserialize(info->data, info->size);
    } else {
        std::vector<unsigned char> gameSaveData = Client::Ref().ReadFile(info->path);
        SaveFile* newFile = new SaveFile(std::string("LibRetro Content File"));
        GameSave* newSave = new GameSave(gameSaveData);
        newFile->SetGameSave(newSave);
        gameController->LoadSaveFile(newFile);
        return true;
    }
}

void retro_unload_game() {
    retro_reset();
}

unsigned retro_get_region() {
    return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
                             size_t num_info) {
    return retro_load_game(info);
}

// TODO: can this function be more efficient?
size_t retro_serialize_size() {
    auto data = gameController->GetSimulation()->Save(true);
    if (data == nullptr) {
        printf("No save data?\n");
        return 0;
    }

    auto serialised = data->Serialise();
    return serialised.size();
}

bool retro_serialize(void* data_, size_t size) {
    auto exported = static_cast<char*>(data_);
    auto save = gameController->GetSimulation()->Save(true);
    if (save == nullptr) {
        return false;
    }

    auto data = save->Serialise();
    memcpy(exported, data.data(), size);

    return true;
}

bool retro_unserialize(const void* data_, size_t size) {
    auto imported = static_cast<const char*>(data_);
    std::vector<char> data (size, 0);
    memcpy(data.data(), imported, size);

    SaveFile* newFile = new SaveFile(std::string("LibRetro Save State"));
    GameSave* newSave = new GameSave(data);
    newFile->SetGameSave(newSave);
    gameController->LoadSaveFile(newFile);
    delete newFile;

    return true;
}

void* retro_get_memory_data(unsigned id) {
    (void)id;

    return NULL;
}

size_t retro_get_memory_size(unsigned id) {
    (void)id;
    return 0;
}

void retro_cheat_reset() {}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {}
