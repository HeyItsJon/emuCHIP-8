#include <iostream>
using std::cout;
using std::endl;
using std::ios;
using std::hex;

#include <fstream>
using std::ifstream;

#include <string>
using std::string;

#include <random>
using std::srand;
using std::rand;

#include <ctime>

#include "Cpu.h"
#include "SDL.h"

Cpu::Cpu()
{
}

Cpu::~Cpu()
{
}

// Description:		Executes the opcode the program counter is pointing to in memory
// Inputs:			None
// Outputs:			int		Status of the operation
//							0 = no errors, -1 = unsupported opcode
//							-2 = stack underflow, -3 = stack overflow
int Cpu::Execute()
{
	unsigned int opcode = ((unsigned int)(memory[pc]) << 8) | ((unsigned int)memory[pc + 1]);
	// Precalculate various helpful values
	unsigned char x = GET_NIBBLE(opcode, 2);
	unsigned char y = GET_NIBBLE(opcode, 1);
	unsigned char n = GET_NIBBLE(opcode, 0);
	unsigned char nn = GET_BYTE(opcode, 0);
	unsigned int nnn = GET_NNN(opcode);
	unsigned char Vx = V[x];
	unsigned char Vy = V[y];

	//cout << hex << opcode << endl;
	switch (opcode & 0xF000)
	{
	case(0x0000):
		switch (opcode)
		{
		case(0x00E0):	// 00E0 - Clears the screen
			memset(display, 0, sizeof(display)); // 0 - black, 1 - white
			pc += 2;
			break;

		case(0x00EE):	// 00EE - Returns from subroutine
			if (stack.empty())
				return -2; // stack underflow
			pc = stack.back();
			stack.pop_back();
			break;

		default:
			return -1; // unsupported opcode
		}
		break;

	case(0x1000):	// 1NNN - Jumps to address NNN
		pc = nnn;
		break;

	case(0x2000):	// 2NNN - Calls subroutine at NNN
		if (stack.size() >= 16)
			return -3; // stack overflow
		stack.push_back(pc + 2);
		pc = nnn;
		break;

	case(0x3000):	// 3XNN - Skips the next instruction if VX equals NN
		(Vx == nn) ? pc += 4 : pc += 2;
		break;

	case(0x4000):	// 4XNN - Skips the next instruction if VX doesn't equal NN
		(Vx != nn) ? pc += 4 : pc += 2;
		break;

	case(0x5000):	// 5XY0 - Skips the next instruction if VX equals VY
		if (GET_NIBBLE(opcode, 0) != 0x0000)
			return -1; // unsupported opcode
		(Vx == Vy) ? pc += 4 : pc += 2;
		break;

	case(0x6000):	// 6XNN - Sets VX to NN
		V[x] = nn;
		pc += 2;
		break;

	case(0x7000):	// 7XNN - Adds NN to VX. (Carry flag is not changed)
		V[x] += nn;
		pc += 2;
		break;

	case(0x8000):
		switch (GET_NIBBLE(opcode, 0))
		{
		case(0x0000):	// 8XY0 - Sets VX to the value of VY
			V[x] = Vy;
			pc += 2;
			break;

		case(0x0001):	// 8XY1 - Sets VX to VX or VY
			V[x] = Vx | Vy;
			pc += 2;
			break;

		case(0x0002):	// 8XY2 - Sets VX to VX and VY
			V[x] = Vx & Vy;
			pc += 2;
			break;

		case(0x0003):	// 8XY3 - Sets VX to VX xor VY
			V[x] = Vx ^ Vy;
			pc += 2;
			break;

		case(0x0004):	// 8XY4 - Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
			V[0xF] = (255 - Vx < Vy) ? 1 : 0;
			V[x] += Vy;
			pc += 2;
			break;

		case(0x0005):	// 8XY5 - VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't
			V[0xF] = (Vy > Vx) ? 0 : 1;
			V[x] -= Vy;
			pc += 2;
			break;

		case(0x0006):	// 8XY6 - Shifts VY right by one and stores the result to VX (VY remains unchanged). VF is set to the value of the least significant bit of VY before the shift
			V[0xF] = Vy & 0x01;
			V[x] = Vy >> 1;
			pc += 2;
			break;

		case(0x0007):	// 8XY7 - Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
			(Vy < Vx) ? V[0xF] = 0 : V[0xF] = 1;
			V[x] = Vy - Vx;
			pc += 2;
			break;

		case(0x000E):	// 8XYE - Shifts VY left by one and copies the result to VX. VF is set to the value of the most significant bit of VY before the shift
			((Vy & 0x80) == 0) ? V[0xF] = 0 : V[0xF] = 1;
			V[x] = Vy << 1;
			pc += 2;
			break;

		default:
			return -1; // unsupported opcode
		}
		break;

	case(0x9000):	// 9XY0 - Skips the next instruction if VX doesn't equal VY
		if (GET_NIBBLE(opcode, 0) != 0x0000)
			return -1;
		(Vx != Vy) ? pc += 4 : pc += 2;
		break;

	case(0xA000):	// ANNN - Sets I to the address NNN
		I = nnn;
		pc += 2;
		break;

	case(0xB000):	// BNNN - Jumps to the address NNN plus V0
		pc = V[0] + nnn;
		break;

	case(0xC000):	// CXNN -  Sets VX to the result of a bitwise and operation on a random number and NN
	{
		srand(time(NULL));
		unsigned char r = rand() & 0xFF;
		V[x] = r & nn;
		pc += 2;
	}
		break;

	case(0xD000):	// DXYN -  Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels 
	{
		unsigned char j = 0;

		// Bisqwit's draw function, with explanations

		// Bisqwit's lambda function - put
		// xor's the byte of display at a with b
		// takes the resulting byte and xor's it again with b - a nonzero result means a bit in display was flipped from 1 to 0
		// and with b (necessary?) and return the byte
		auto put = [this](int a, unsigned char b)
		{
			return ((display[a] ^= b) ^ b) & b;
		};

		// Sprites can span between two bytes in the display memory.
		// The arguments of the put function are used to select the byte in display (the a parameter)
		// and the section of the sprite at memory location I plus the height (the b parameter).
		// Vx and Vy are the origin points of the sprite.
		// They are mod'd with W and H respectively to allow for wrapping.  Vy is multiplied by W to get our correct index.
		// The result is divided by 8 to get us into the correct byte - the decimal value is truncated, so if
		// we want bit 5 in the display memory, we'll get the 0th byte (which contains the 5th bit), and so forth.
		// Vx is then mod'd with 8 and the memory value is right shifted that many bits to get the range of bits we want from the sprite.
		// The process is repeated, but 7 is added to xo to get us into the next byte in display memory.
		// The memory value is now left shifted to get us the remaining bits we need from the sprite.
		// This still works if the sprite lands completely within one byte of display memory.
		// In the first put call, the sprite is not shifted at all and the sprite values are xor'd in the put function.
		// In the second put call, the sprite byte is shifted 8 places, giving it the value 0x00.  When put executes,
		// nothing happens.
		// Everything is repeated until n (the height) is zero.
		// In the end, if j is nonzero, then a bit flipped from set to cleared, and V[F] is set to 1.
		// (the padding within the put calls just helps with readability, even if it's technically bad practice)
		for (j = 0; n--;)
			j |= put(((Vx + 0) % W + (Vy + n) % H * W) / 8, memory[(I + n) & 0xFFF] >> (Vx % 8)) | \
			put(((Vx + 7) % W + (Vy + n) % H * W) / 8, memory[(I + n) & 0xFFF] << (8 - Vx % 8));
		(j == 0) ? V[0xF] = 0 : V[0xF] = 1;
		pc += 2;
	}
		break;

	case(0xE000):
		switch (GET_BYTE(opcode, 0))
		{
		case(0x009E):	// EX9E - Skips the next instruction if the key stored in VX is pressed
						// Potential for errors if V[x] > 0xF
			(keys[Vx]) ? pc += 4 : pc += 2;
			break;

		case(0x00A1):	// EXA1 - Skips the next instruction if the key stored in VX isn't pressed
						// Potential for errors if V[x] > 0xF
			(!keys[Vx]) ? pc += 4 : pc += 2;
			break;

		default:
			return -1; // unsupported opcode
		}
		break;

	case(0xF000):
		switch (GET_BYTE(opcode, 0))
		{
		case(0x0007):	// FX07 - Sets VX to the value of the delay timer
			V[x] = delayTimer;
			pc += 2;
			break;

		case(0x000A):	// FX0A - A key press is awaited, and then stored in VX
			for (int i = 0; i < NUMBER_OF_KEYS; i++)
			{
				if (keys[i])
				{
					V[x] = i;
					pc += 2; // blocking operation - only increment pc if key is pressed
					break;
				}
			}
			break;

		case(0x0015):	// FX15 - Sets the delay timer to VX
			delayTimer = Vx;
			pc += 2;
			break;

		case(0x0018):	// FX18 - Sets the sound timer to VX
			soundTimer = Vx;
			pc += 2;
			break;

		case(0x001E):	// FX1E - Adds VX to I
			I += Vx;
			(I > 0xFFF) ? V[0xF] = 1 : V[0xF] = 0;
			pc += 2;
			break;

		case(0x0029):	// FX29 - Sets I to the location of the sprite for the character in VX
			I = FONT_BEGIN + Vx * 5;
			pc += 2;
			break;

		case(0x0033):	// FX33 - Stores the binary-coded decimal representation of VX
			memory[I] = Vx / 100;
			memory[I + 1] = (Vx % 100) / 10;
			memory[I + 2] = Vx % 10;
			pc += 2;
			break;

		case(0x0055):	// FX55 - Stores V0 to VX (including VX) in memory starting at address I
			for (int j = 0; j <= x; j++)
				memory[I + j] = V[j];
			pc += 2;
			break;

		case (0x0065):	// FX65 - Fills V0 to VX (including VX) with values from memory starting at address I
			for (int j = 0; j <= x; j++)
				V[j] = memory[I + j];
			pc += 2;
			break;

		default:
			return -1; // unsupported opcode
		}
		break;

	default:
		return -1; // unsupported opcode
	}

	return 0; // success
}

// Description:		Loads the memeory array with the font set and ROM data from file
// Inputs:			string	inFile		The path to the ROM file
// Outputs:			bool				Status of the operation
//										true = no errors, false = ROM file failed to open
bool Cpu::LoadMemory(string inFile)
{
	ifstream inROM(inFile, ios::binary);
	const unsigned char fontset[80] =
	{
		0xF0, 0x90, 0x90, 0x90, 0xF0,	// 0
		0x20, 0x60, 0x20, 0x20, 0x70,	// 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0,	// 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0,	// 3
		0x90, 0x90, 0xF0, 0x10, 0x10,	// 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0,	// 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0,	// 6
		0xF0, 0x10, 0x20, 0x40, 0x40,	// 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0,	// 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0,	// 9
		0xF0, 0x90, 0xF0, 0x90, 0x90,	// A
		0xE0, 0x90, 0xE0, 0x90, 0xE0,	// B
		0xF0, 0x80, 0x80, 0x80, 0xF0,	// C
		0xE0, 0x90, 0x90, 0x90, 0xE0,	// D
		0xF0, 0x80, 0xF0, 0x80, 0xF0,	// E
		0xF0, 0x80, 0xF0, 0x80, 0x80	// F
	};

	// Load fontset into memory
	memcpy(&memory[FONT_BEGIN], fontset, sizeof(fontset));

	// Load ROM into memory
	if (!inROM.is_open())
		return false;

	// Borrowing some Bisqwit code, with some modifications
	for (unsigned int i = 0; inROM.good(); i++)
		memory[MEMORY_BEGIN + i] = inROM.get();

	inROM.close();

	return true;
}

// Description:		Initializes the member variables of CPU
// Inputs:			None
// Outputs:			int		status
//							0 = success, else return SDL_Init error code
int Cpu::Init()
{
	// Initialize array values to 0 in memory

	// Memory
	memset(memory, 0, sizeof(memory));
	// Data registers
	memset(V, 0, sizeof(V));
	// Hex keyboard input
	memset(keys, 0, sizeof(keys));
	// Display (all black)
	memset(display, 0, sizeof(display)); // 0 - black, 1 - white

	// Initalize registers and timers

	// CHIP-8 does not access memory below 0x200
	pc = MEMORY_BEGIN;
	I = 0;
	delayTimer = 0;
	soundTimer = 0;

	return 0;
}

// Description:		Decrements the timers if they are nonzero
// Inputs:			None
// Outputs:			None
void Cpu::UpdateTimers()
{
	if (delayTimer)
		delayTimer--;
	if (soundTimer)
		soundTimer--;
}

// Description:		Render the screen in a RGB buffer
//					(Code borrowed from Bisqwit, with explanations)
// Inputs:			unsigned int pixels[]	Screen pixel data in ARGB format (32 bit per pixel)
// Outputs:			None
void Cpu::RenderTo(unsigned int pixels[])
{
	// loop over whole W*H of display
	// i / 8 truncates decimal so use display[0] for i = (0-7), etc.
	// shift bits over from MSB to LSB (7 - i % 8) - i = 0, shift 7; i = 1, shift 6; ... ; i = 7, shift 0; repeat
	// & 1 to get the actual bit and set everything to 0
	// multiply by 0xFFFFFF - if bit = 0, get black (r=g=b=0); if bit = 1, get white (r=g=b=255=FF)
	for(unsigned int i = 0; i < W*H; i++)
		pixels[i] = 0xFFFFFF * ((display[i / 8] >> (7 - i % 8)) & 1);
}
