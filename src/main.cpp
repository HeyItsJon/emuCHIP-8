#include <iostream>
using std::ios;
using std::cerr;
using std::cout;
using std::cin;
using std::endl;

#include <string>
using std::string;

#include <ctime>

#include <unordered_map>
using std::unordered_map;

#include <cmath>
using std::round;

#include <Windows.h>

#include "Cpu.h"
#include "SDL.h"

int main(int argc, char *argv[])
{
	Cpu chip8;
	char filePathINI[50];
	string filePath;
	time_t lastUpdate;
	time_t now;
	const double updateRate = (1000.0/60.0); // 60Hz
	int status;
	unsigned int pixels[W*H]; // each pixel in ARGB form - 8 bits each, 32 bits total per pixel
	bool running = true;

	// Get values from ini file
	GetPrivateProfileString("SETTINGS", "ROM_PATH", "C:/EmuCHIP-8/c8games/", filePathINI, sizeof(filePathINI) / sizeof(filePathINI[0]), "C:/EmuCHIP-8.ini");
	int cycles = GetPrivateProfileInt("SETTINGS", "CYCLES", 10, "C:/EmuCHIP-8/EmuCHIP-8.ini");

	filePath = string(filePathINI);
	if (argc > 1)
		filePath += argv[1];
	else
		filePath += "PONG2";

	if (chip8.Init() < 0)
	{
		cerr << "Error during initialization!" << endl;
		cin.get();
		exit(1);
	}
	if (!chip8.LoadMemory(filePath))
	{
		cerr << "ROM not found!" << endl;
		cin.get();
		exit(1);
	}

	// Welcome message
	cout << "\nEmuCHIP-8 - A CHIP-8 Emulator" << endl;
	cout << "v0.2 7-24-2018" << endl;
	cout << "Written by: Jon Altenburger" << endl;

	// UI code borrowed from Bisqwit
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *window = SDL_CreateWindow("EmuCHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * 10, H * 10, SDL_WINDOW_SHOWN);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, W, H);
	SDL_Surface *icon = SDL_LoadBMP("C:/EmuCHIP-8/C8.bmp");
	SDL_SetWindowIcon(window, icon);
	SDL_FreeSurface(icon);

	// Initialize audio
	SDL_AudioSpec spec, obtained;
	SDL_AudioDeviceID dev;
	spec.freq = 44100;
	spec.format = AUDIO_S8;
	spec.channels = 1;
	// ~20ms, 440Hz wave requires ~100 samples per period, we want an intergral number of periods to keep the sound smooth
	// freq/50 gives the number of samples in ~20ms - divide by 100, round, and multiple by 100 rounds to the nearest 100
	spec.samples = round((spec.freq/50.0) / 100.0) * 100;
	spec.callback = NULL;
	dev = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, 0);
	SDL_PauseAudioDevice(dev, 0); // unpause
	// Create square wave - array size is the number of samples per frame in the audiospec
	char *squareWave = (char *)malloc(obtained.samples);
	for (int i = 0; i < obtained.samples; i++)
		squareWave[i] = ((i % 100 < 50) ? 25 : -25);

	// map of keyboard input to emulated hex keyboard
	// Physical:     Hex:
	// 1 2 3 4  -->  1 2 3 C
	// Q W E R  -->  4 5 6 D
	// A S D F  -->  7 8 9 E
	// Z X C V  -->  A 0 B F
	unordered_map<int, int> keymap
	{
		{ SDLK_1, 0x1 },{ SDLK_2, 0x2 },{ SDLK_3, 0x3 },{ SDLK_4, 0xC },
		{ SDLK_q, 0x4 },{ SDLK_w, 0x5 },{ SDLK_e, 0x6 },{ SDLK_r, 0xD },
		{ SDLK_a, 0x7 },{ SDLK_s, 0x8 },{ SDLK_d, 0x9 },{ SDLK_f, 0xE },
		{ SDLK_z, 0xA },{ SDLK_x, 0x0 },{ SDLK_c, 0xB },{ SDLK_v, 0xF },
		{ SDLK_ESCAPE, -1 }
	};

	lastUpdate = clock();
	while (running)
	{
		// Run a specified number of opcodes to slow down the emulator
		for (int i = 0; i < cycles && running; i++)
		{
			status = chip8.Execute();
			if (status == 0)
			{
			}
			else if (status == -1)
			{
				cerr << "Unsupported opcode!" << endl;
				cin.get();
				running = false;
			}
			else if (status == -2)
			{
				cerr << "Stack underflow!" << endl;
				cin.get();
				running = false;
			}
			else if (status == -3)
			{
				cerr << "Stack overflow!" << endl;
				cin.get();
				running = false;
			}

			for (SDL_Event ev; SDL_PollEvent(&ev) && running;)
			{
				switch (ev.type)
				{
				case SDL_QUIT:
					running = false;
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					auto i = keymap.find(ev.key.keysym.sym);
					if (i == keymap.end())
						break;
					if (i->second == -1)
					{
						// User hit ESC
						running = false;
						break;
					}
					chip8.keys[i->second] = ((ev.type == SDL_KEYDOWN) ? 1 : 0);
					break;
				}
			}
		}

		// Wait until it's time to refresh the display
		do
		{
			now = clock();
		} while ((now - lastUpdate) < updateRate && running);

		// Update display
		chip8.RenderTo(pixels);
		SDL_UpdateTexture(texture, nullptr, pixels, 4 * W);
		SDL_RenderCopy(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);

		// Play sound
		if (chip8.soundTimer)
		{
			// It's possible to build up extra samples in the queue, so if we already
			// have >= obtained.samples in the queue already, just skip it so things don't get out of control
			if (SDL_GetQueuedAudioSize(dev) >= obtained.samples)
			{
				if (SDL_QueueAudio(dev, squareWave, obtained.samples) < 0)
				{
					cerr << "Error playing sound!" << endl;
					cin.get();
					running = false;
				}
			}
		}
		else if (SDL_GetQueuedAudioSize(dev) > 0)
		{
			// If the sound timer is zero and we still have samples in the queue, flush them out
			SDL_ClearQueuedAudio(dev);
		}	

		// Update the sound and delay timers
		chip8.UpdateTimers();
		// Update the clock
		lastUpdate = clock();
	}

	SDL_Quit();
	return 0;
}