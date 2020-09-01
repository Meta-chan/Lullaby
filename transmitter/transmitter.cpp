#include "../lullaby.h"
#include <SDL.h>
#ifdef M_PI
	#undef M_PI
#endif
#define IR_IMPLEMENT
#include <ir_container/ir_ring.h>
#include <ir_math/ir_fft.h>
#include <ir_md5.h>

class Transmitter
{
private:
	ir::Ring<Uint8> _stream;
	bool _sdl_inited = false;
	int _device = 0;

	static void _callback(void* userdata, Uint8* stream, signed int len);
	void _transmit_multibit(const bool multibit[nfrequencies]);
	void _transmit_multibyte(const unsigned char multibyte[nfrequencies]);
	void _transmit_silence();

public:
	Transmitter();
	bool ok();
	void transmit(const void *data, unsigned char size);
	~Transmitter();
};

void Transmitter::_callback(void* userdata, Uint8* stream, signed int len)
{
	ir::Ring<Uint8> *transstream = &((Transmitter*)userdata)->_stream;
	if (transstream->count() < (unsigned int)len)
	{
		unsigned int count = transstream->count();
		transstream->read(stream, count);
		memset(stream + count, 0, len - count);
	}
	else transstream->read(stream, len);
};

void Transmitter::_transmit_multibit(const bool multibit[nfrequencies])
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
	_stream.write((Uint8*)ibuffer, nsamples * sizeof(int));
};

void Transmitter::_transmit_multibyte(const unsigned char multibyte[nfrequencies])
{
	for (unsigned int i = 0; i < 8; i++)
	{
		bool multibit[nfrequencies];
		for (unsigned int j = 0; j < nfrequencies; j++)
		{
			multibit[j] = multibyte[j] & (1 << i);
		}
		_transmit_multibit(multibit);
	}
};

void Transmitter::_transmit_silence()
{
	int ibuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++) ibuffer[i] = 0;
	_stream.write((Uint8*)ibuffer, nsamples * sizeof(int));
};

void Transmitter::transmit(const void *data, unsigned char size)
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
		_transmit_multibyte(buffer + i * nfrequencies);
	}

	//Silence
	_transmit_silence();
};

bool Transmitter::ok()
{
	return _sdl_inited && _device != 0;
};

Transmitter::Transmitter() : _stream(1024 * 1024)
{
	//Initing SDL
	if (SDL_Init(SDL_INIT_AUDIO) != 0) return;
	_sdl_inited = true;

	//Opening audio device
	SDL_AudioSpec desired, obtained;
	SDL_zero(desired);
	desired.freq = frequency;
	desired.format = AUDIO_S32SYS;
	desired.channels = 1;
	desired.samples = 4096;
	desired.callback = _callback;
	desired.userdata = this;
	_device = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &desired, &obtained, 0);
	if (_device == 0) return;
	SDL_PauseAudioDevice(_device, SDL_FALSE);
};

Transmitter::~Transmitter()
{
	if (_device != 0) SDL_CloseAudioDevice(_device);
	if (_sdl_inited) SDL_Quit();
};

int main(int argc, char **argv)
{
	Transmitter t;
	if (!t.ok()) return 1;

	while (true)
	{
		char s[16];
		scanf_s("%15s", s, _countof(s));
		t.transmit(s, (unsigned char)strlen(s));
	}

	return 0;
};