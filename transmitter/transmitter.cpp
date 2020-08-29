#define IR_IMPLEMENT
#include <ir_container/ir_ring.h>
#include <ir_math/ir_fft.h>
#include <SDL.h>
#include <stdlib.h>

static const unsigned int nsamples = 128;
static const unsigned int nfrequences = 1;
static const unsigned int frequences[] =
{
	2
};

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

void transmit(const unsigned char data[nfrequences])
{
	std::complex<float> cbuffer[nsamples];
	int ibuffer[nsamples];

	//Start beep
	for (unsigned int j = 0; j < nsamples; j++) cbuffer[j] = 0.0f; 
	for (unsigned int j = 0; j < nfrequences; j++)
	{

		cbuffer[1 + frequences[j]] = 1.0f;
		cbuffer[nsamples - frequences[j]] = 1.0f;
	}
	ir::ifft(cbuffer, nsamples);
	float maxreal = 0.0f;
	for (unsigned int j = 0; j < nfrequences; j++)
	{
		if (maxreal < abs(cbuffer[j])) maxreal = abs(cbuffer[j]);
	};
	for (unsigned int j = 0; j < nsamples; j++) ibuffer[j] = (int)(0x7FFFFFFF * cbuffer[j].real() / maxreal);
	transmitted_stream.write((Uint8*)ibuffer, nsamples * 4);

	//Transmitting
	//For all 8 bits
	for (unsigned int i = 0; i < 8; i++)
	{
		unsigned char mask = 1 << i;
		for (unsigned int j = 0; j < nsamples; j++) cbuffer[j] = 0.0f;
		for (unsigned int j = 0; j < nfrequences; j++)
		{
			if ((data[j] & mask) != 0)
			{
				cbuffer[1 + frequences[j]] = 1.0f;
				cbuffer[nsamples - frequences[j]] = 1.0f;
			}
		}
		ir::ifft(cbuffer, nsamples);
		for (unsigned int j = 0; j < nsamples; j++) ibuffer[j] = (int)(0x7FFFFFFF * cbuffer[j].real() / maxreal);
		transmitted_stream.write((Uint8*)ibuffer, nsamples * 4);
	}

	//End silence
	memset(ibuffer, 0, nsamples * 4);
	transmitted_stream.write((Uint8*)ibuffer, nsamples * 4);
};

int main(int argc, char **argv)
{
	//Initing SDL
	if (SDL_Init(SDL_INIT_AUDIO) != 0) return 1;
	
	//Opening audio device
	SDL_AudioSpec desired, obtained;
	SDL_zero(desired);
	desired.freq = 48000;
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
		else for (unsigned int i = 0; i < strlen(s); i++) transmit((unsigned char*)&s[i]);
	}

	//Exiting
	SDL_CloseAudioDevice(device);
	SDL_Quit();
	return 0;
};