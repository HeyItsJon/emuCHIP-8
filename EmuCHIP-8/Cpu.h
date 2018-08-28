#pragma once

#include <vector>
using std::vector;

#include "SDL.h"

// Macros
#define GET_BYTE(x,y)	(x >> (y*8) & 0xFF)
#define GET_NIBBLE(x,y)	(x >> (y*4) & 0xF)
#define GET_NNN(x)		(x & 0xFFF)

// Useful constants
const unsigned int W = 64, H = 32;
#define MEMORY_BEGIN 0x200
#define FONT_BEGIN 0x50
#define SPRITE_WIDTH 8
#define NUMBER_OF_KEYS 16
#define DISPLAY_SIZE (W*H/8)

// Set random number range to 0-255
#ifdef RAND_MAX
#undef RAND_MAX
#endif
#define RAND_MAX 255

class Cpu
{
private:
	// 4K of memory
	unsigned char memory[4096];
	// 16 8-bit data registers
	unsigned char V[16];
	// Address register
	unsigned short I;
	// Program counter
	unsigned short pc;
	// Stack
	vector<short> stack;
	// Delay timer - count down at 60Hz
	unsigned char delayTimer;
	// 64*32 pixel monochrome display - each bit represents one pixel: 0 = black, 1 = white
	unsigned char display[DISPLAY_SIZE];

public:
	Cpu();
	~Cpu();

	// Hex keyboard input
	unsigned char keys[NUMBER_OF_KEYS];
	// Sound timer - count down at 60Hz
	unsigned char soundTimer;

	int Execute();
	bool LoadMemory(string);
	int Init();
	void UpdateTimers();
	void RenderTo(unsigned int[]);
};

