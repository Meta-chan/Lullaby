#include "../lullaby.h"
#include <SDL.h>
#ifdef M_PI
	#undef M_PI
#endif
#define IR_IMPLEMENT
#include <ir_container/ir_ring.h>
#include <ir_math/ir_fft.h>
#include <ir_md5.h>

ir::Ring<Uint8> transmitted_stream(1024 * 1024);

void play_callback(void* userdata, Uint8* stream, signed int len)
{
	if (transmitted_stream.count() < (unsigned int)len)
	{
		unsigned int count = transmitted_stream.count();
		transmitted_stream.read(stream, count);
		memset(stream + count, 0, len - count);
	}
	else transmitted_stream.read(stream, len);
};

void transmit_multibit(const bool multibit[nfrequencies])
{
	std::complex<float> cbuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++)
	{
		cbuffer[i] = 0.0f;
	}

	for (unsigned int i = 0; i < nfrequencies; i++)
	{
		if (multibit[i])
		{
			cbuffer[1 + frequencies[i]] = 1.0f;
			cbuffer[nsamples - frequencies[i]] = 1.0f;
		}
	}

	ir::ifft_nodivide(cbuffer, nsamples);
	int ibuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++)
	{
		ibuffer[i] = (int)(0x7FFFFF00 * 0.5f * cbuffer[i].real());
	}
	transmitted_stream.write((Uint8*)ibuffer, nsamples * sizeof(int));
};

void transmit_multibyte(const unsigned char multibyte[nfrequencies])
{
	bool multibit[nfrequencies];
	for (unsigned int i = 0; i < 8; i++)
	{
		for (unsigned int j = 0; j < nfrequencies; j++)
		{
			multibit[j] = multibyte[j] & (1 << i);
		}
		transmit_multibit(multibit);
	}
};

void transmit_silence()
{
	int ibuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++) ibuffer[i] = 0;
	transmitted_stream.write((Uint8*)ibuffer, nsamples * sizeof(int));
};

void transmit(const void *data, unsigned char size)
{
	//Filling buffer with header
	unsigned char buffer[(255 + sizeof(Header) + nfrequencies - 1) / nfrequencies * nfrequencies];
	for (unsigned int i = 0; i < nfrequencies; i++)
	{
		((Header*)buffer)->signature[i] = 0xFF;
	}
	((Header*)buffer)->size = size;
	unsigned short hash[8];
	ir::md5(data, size, hash);
	((Header*)buffer)->hash = hash[0];

	//Filling buffer with data
	memcpy(buffer + sizeof(Header), data, size);

	//Setting zero tail
	unsigned int nmultibytes = (sizeof(Header) + size + nfrequencies - 1) / nfrequencies;
	memset(buffer + sizeof(Header) + size, 0, nmultibytes * nfrequencies - size - sizeof(Header));

	//Transmitting
	for (unsigned int i = 0; i < nmultibytes; i++)
	{
		transmit_multibyte(buffer + i * nfrequencies);
	}

	//Silence
	transmit_silence();
};

int main(int argc, char **argv)
{
	//Initing SDL
	if (SDL_Init(SDL_INIT_AUDIO) != 0) return 1;
	
	//Opening audio device
	SDL_AudioSpec desired, obtained;
	SDL_zero(desired);
	desired.freq = frequency;
	desired.format = AUDIO_S32SYS;
	desired.channels = 1;
	desired.samples = 4096;
	desired.callback = play_callback;
	SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &desired, &obtained, 0);
	if (device == 0) { SDL_Quit(); return 2; }
	SDL_PauseAudioDevice(device, SDL_FALSE);

	//Transmitting
	while (true)
	{
		char s[16];
		scanf_s("%15s", s, _countof(s));
		if (strlen(s) == 0) break;
		else transmit(s, (unsigned char)strlen(s));
	}

	//Exiting
	SDL_CloseAudioDevice(device);
	SDL_Quit();
	return 0;
};