#include <map>
#include <string>
#include <ctime>
#include <climits>
#ifdef WIN
#define _WIN32_WINNT 0x0501	//Necessary for some macros and functions, tells windows.h to include functions only available in Windows XP or later
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

#include "environment.h"

using namespace std;

/*#define INCLUDE_SYSWM
#include "SDLCompat.h"
#if defined(USE_SDL) && defined(LIN) && defined(SDL_VIDEO_DRIVER_X11)
SDL_SysWMinfo sdl_wminfo;
Atom XA_CLIPBOARD, XA_TARGETS, XA_UTF8_STRING;
#endif

std::string clipboardText = "";*/

int scale = 1;
bool fullscreen = false;

int mousex = 0, mousey = 0;

void DrawPixel(pixel * vid, pixel color, int x, int y)
{
	if (x >= 0 && x < WINDOWW && y >= 0 && y < WINDOWH)
		vid[x+y*WINDOWW] = color;
}
// draws a custom cursor, used to make 3D mode work properly (normal cursor ruins the effect)
void DrawCursor(pixel * vid)
{
	for (int j = 0; j <= 9; j++)
	{
		for (int i = 0; i <= j; i++)
		{
			if (i == 0 || i == j)
				DrawPixel(vid, 0xFFFFFFFF, mousex+i, mousey+j);
			else
				DrawPixel(vid, 0xFF000000, mousex+i, mousey+j);
		}
	}
	DrawPixel(vid, 0xFFFFFFFF, mousex, mousey+10);
	for (int i = 0; i < 5; i++)
	{
		DrawPixel(vid, 0xFF000000, mousex+1+i, mousey+10);
		DrawPixel(vid, 0xFFFFFFFF, mousex+6+i, mousey+10);
	}
	DrawPixel(vid, 0xFFFFFFFF, mousex, mousey+11);
	DrawPixel(vid, 0xFF000000, mousex+1, mousey+11);
	DrawPixel(vid, 0xFF000000, mousex+2, mousey+11);
	DrawPixel(vid, 0xFFFFFFFF, mousex+3, mousey+11);
	DrawPixel(vid, 0xFF000000, mousex+4, mousey+11);
	DrawPixel(vid, 0xFF000000, mousex+5, mousey+11);
	DrawPixel(vid, 0xFFFFFFFF, mousex+6, mousey+11);

	DrawPixel(vid, 0xFFFFFFFF, mousex, mousey+12);
	DrawPixel(vid, 0xFF000000, mousex+1, mousey+12);
	DrawPixel(vid, 0xFFFFFFFF, mousex+2, mousey+12);
	DrawPixel(vid, 0xFFFFFFFF, mousex+4, mousey+12);
	DrawPixel(vid, 0xFF000000, mousex+5, mousey+12);
	DrawPixel(vid, 0xFF000000, mousex+6, mousey+12);
	DrawPixel(vid, 0xFFFFFFFF, mousex+7, mousey+12);

	DrawPixel(vid, 0xFFFFFFFF, mousex, mousey+13);
	DrawPixel(vid, 0xFFFFFFFF, mousex+1, mousey+13);
	DrawPixel(vid, 0xFFFFFFFF, mousex+4, mousey+13);
	DrawPixel(vid, 0xFF000000, mousex+5, mousey+13);
	DrawPixel(vid, 0xFF000000, mousex+6, mousey+13);
	DrawPixel(vid, 0xFFFFFFFF, mousex+7, mousey+13);

	DrawPixel(vid, 0xFFFFFFFF, mousex, mousey+14);
	for (int i = 0; i < 2; i++)
	{
		DrawPixel(vid, 0xFFFFFFFF, mousex+5, mousey+14+i);
		DrawPixel(vid, 0xFF000000, mousex+6, mousey+14+i);
		DrawPixel(vid, 0xFF000000, mousex+7, mousey+14+i);
		DrawPixel(vid, 0xFFFFFFFF, mousex+8, mousey+14+i);

		DrawPixel(vid, 0xFFFFFFFF, mousex+6, mousey+16+i);
		DrawPixel(vid, 0xFF000000, mousex+7, mousey+16+i);
		DrawPixel(vid, 0xFF000000, mousex+8, mousey+16+i);
		DrawPixel(vid, 0xFFFFFFFF, mousex+9, mousey+16+i);
	}

	DrawPixel(vid, 0xFFFFFFFF, mousex+7, mousey+18);
	DrawPixel(vid, 0xFFFFFFFF, mousex+8, mousey+18);
}

int elapsedTime = 0, currentTime = 0, lastTime = 0, currentFrame = 0;
unsigned int lastTick = 0;
float fps = 0, delta = 1.0f, inputScale = 1.0f;
ui::Engine * engine = NULL;
bool showDoubleScreenDialog = false;
float currentWidth, currentHeight;
bool crashed = false;
GameController * gameController = NULL;
struct retro_log_callback logger_holder;
bool hasLeftHeld;
bool hasRightHeld;
uint8_t* framebuffer;
int16_t* audio_data;

void EngineProcess()
{
    if (!engine->Running()) {
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

	for (int i = 0; i < buffer.Width * buffer.Height; i++) {
		auto pixel = buffer.Buffer[i];
		framebuffer[(i * 4)] = static_cast<uint8_t>((pixel) & 0xFF);
		framebuffer[(i * 4) + 1] = static_cast<uint8_t>((pixel >> 8) & 0xFF);
		framebuffer[(i * 4) + 2] = static_cast<uint8_t>((pixel >> 16) & 0xFF);
		framebuffer[(i * 4) + 3] = 0;
	}
    LibRetro::UploadVideoFrame(framebuffer, buffer.Width, buffer.Height, 4 * buffer.Width);
	LibRetro::UploadAudioFrame(audio_data, 32000 / 60);
}

void BlueScreen(const char * detailMessage){
	ui::Engine * engine = &ui::Engine::Ref();
	engine->g->fillrect(0, 0, engine->GetWidth(), engine->GetHeight(), 17, 114, 169, 210);

	std::string errorTitle = "ERROR";
	std::string errorDetails = "Details: " + std::string(detailMessage);
	std::string errorHelp = "An unrecoverable fault has occurred, please report the error by visiting the website below\n"
		"https://github.com/j-selby/ThePowderToy-LibRetro/issues";
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
	switch(signal){
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

	if (keycode == RETROK_TAB) {
		BlueScreen("Test crash");
	}

	if (down) {
		engine->onKeyPress(keycode, character, shift, ctrl, alt);
	} else {
		engine->onKeyRelease(keycode, character, shift, ctrl, alt);
	}
}

void retro_init() {
	if (!LibRetro::GetLogger(&logger_holder)) {
		printf("No frontend logger found.\n");
		return;
	}

	logger_holder.log(RETRO_LOG_INFO, "Core init\n");

	retro_keyboard_callback callback{};
	callback.callback = keyboard_callback;
	if (!LibRetro::SetKeyboardCallback(&callback)) {
		logger_holder.log(RETRO_LOG_ERROR, "Unable to set keyboard callback\n");
	}

	framebuffer = static_cast<uint8_t *>(malloc(WINDOWW * WINDOWH * 4));
	auto buffer_size = (32000 / 60) * 2;
	audio_data = static_cast<int16_t *>(malloc(static_cast<size_t>(buffer_size)));

	for (int i = 0; i < buffer_size / 2; i++) {
		audio_data[i] = 0;
	}

	LibRetro::SetPixelFormat(RETRO_PIXEL_FORMAT_XRGB8888);

    int tempScale = Client::Ref().GetPrefInteger("Scale", 1);

    int scale = tempScale;

    Client::Ref().Initialise("");

    ui::Engine::Ref().g = new Graphics();
    ui::Engine::Ref().Scale = scale;
    inputScale = 1.0f/float(scale);
    ui::Engine::Ref().Fullscreen = fullscreen;

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
	free(audio_data);

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
        currentFrame++;
        if (currentFrame > 60) {
            Client::Ref().Tick();
        }
        currentFrame %= 60;

		LibRetro::PollInput();
		bool left =
				(bool)(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)) ||
				(bool)(LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3));
        bool right =
                (bool)(LibRetro::CheckInput(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT));
		auto pointerX = (float) LibRetro::CheckInput(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
		auto pointerY = (float) LibRetro::CheckInput(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
        pointerX /= 0x7fff * 2;
        pointerY /= 0x7fff * 2;
        pointerX += 0.5;
        pointerY += 0.5;
        pointerX *= WINDOWW;
        pointerY *= WINDOWH;

        auto absX = (unsigned) pointerX;
        auto absY = (unsigned) pointerY;

        bool hasSentEvent = false;
		if (left && !hasLeftHeld) {
            engine->onMouseClick(absX, absY, 1);
            hasLeftHeld = true;
            hasSentEvent = true;
		} else if (!left && hasLeftHeld) {
            engine->onMouseUnclick(absX, absY, 1);
            hasLeftHeld = false;
            hasSentEvent = true;
        }

        if (right && !hasRightHeld) {
            engine->onMouseClick(absX, absY, 3);
            hasRightHeld = true;
            hasSentEvent = true;
        } else if (!right && hasRightHeld) {
            engine->onMouseUnclick(absX, absY, 3);
            hasRightHeld = false;
            hasSentEvent = true;
        }

        if (!hasSentEvent) {
            engine->onMouseMove(absX, absY);
        }

        EngineProcess();
    }
    catch(exception& e)
    {
        BlueScreen(e.what());
    }
}

void retro_reset() {
	logger_holder.log(RETRO_LOG_INFO, "Core reset\n");
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

    if (info->data == nullptr) {
        std::vector<unsigned char> gameSaveData = Client::Ref().ReadFile(info->path);
        SaveFile* newFile = new SaveFile(std::string("LibRetro Content File"));
        GameSave* newSave = new GameSave(gameSaveData);
        newFile->SetGameSave(newSave);
        gameController->LoadSaveFile(newFile);
        return true;
    } else {
        return retro_unserialize(info->data, info->size);
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
    for (size_t i = 0; i < size; i++) {
        exported[i] = data[i];
    }
    return true;
}

bool retro_unserialize(const void* data_, size_t size) {
    auto imported = static_cast<const char*>(data_);
    std::vector<char> data (size, 0);
    for (size_t i = 0; i < size; i++) {
        data[i] = imported[i];
    }

    SaveFile* newFile = new SaveFile(std::string("LibRetro Savestate"));
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
