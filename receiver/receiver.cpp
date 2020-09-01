#include "../lullaby.h"
#include <SDL.h>
#ifdef M_PI
#undef M_PI
#endif
#define IR_IMPLEMENT
#include <ir_container/ir_ring.h>
#include <ir_math/ir_fft.h>
#include <ir_md5.h>

class Receiver
{
private:
	ir::Ring<Uint8> _stream;
	int _treshold = 0;
	float _max_calibration;
	float _calibration[nfrequencies];
	bool _sdl_inited = false;
	int _device = 0;

	static void _callback(void* userdata, Uint8* stream, int len);
	bool _receive_signature_multibit();
	bool _receive_signature_multibyte();
	bool _receive_multibit(bool multibit[nfrequencies]);
	bool _receive_multibyte(unsigned char multibyte[nfrequencies]);

public:
	Receiver();
	bool ok();
	int receive( void *data, unsigned char *size);
	~Receiver();
};

void Receiver::_callback(void* userdata, Uint8* stream, int len)
{
	ir::Ring<Uint8> *recstream = &((Receiver*)userdata)->_stream;
	if (recstream->size() - recstream->count() < (unsigned int)len)
	{
		recstream->read_direct((len + sizeof(int) - 1) & (sizeof(int) - 1));
	}
	recstream->write(stream, len);
};

bool Receiver::_receive_signature_multibit()
{
	//Waiting for buffer to be filled
	while (_stream.count() < nsamples * sizeof(int))
	{
		SDL_Delay(5);
	}

	//Copying buffer
	int ibuffer[nsamples];
	_stream.read((Uint8*)ibuffer, nsamples * sizeof(int));
	std::complex<float> cbuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++)
	{
		cbuffer[i] = (float)ibuffer[i];
	}

	//Conducting FFT
	ir::fft(cbuffer, nsamples);

	//Conditions for signature multibit to be accepted:
	//1) Maximum is on some carrying frequency
	//2) All other carrying frequencies are >0.5 of this frequency
	//3) All other frequencies (except 0) are < 0.1 of this frequency

	float _max_calibration = 0.0f;
	for (unsigned int i = 1; i < nsamples / 2 + 1; i++)
	{
		if (_max_calibration < abs(cbuffer[i])) _max_calibration = abs(cbuffer[i]);
	}
	//1 and 2
	for (unsigned int i = 0; i < nfrequencies; i++)
	{
		if (abs(cbuffer[frequencies[i] + 1]) < 0.5f * _max_calibration) return false;
		_calibration[i] = abs(cbuffer[frequencies[i] + 1]);
	}
	//3
	for (unsigned int i = 0; i < nfrequencies + 1; i++)
	{
		for (unsigned int j = ((i == 0) ? 1  : (frequencies[i-1] + 2));
			j < ((i == nfrequencies) ? (nsamples / 2 + 1) : (frequencies[i] + 1));
			j++)
		{
			if (abs(cbuffer[j]) > 0.1f * _max_calibration) return false;
		}
	}
	return true;
};

bool Receiver::_receive_signature_multibyte()
{
	for (unsigned int i = 0; i < 8; i++)
	{
		if (!_receive_signature_multibit()) return false;
	}
};

bool Receiver::_receive_multibit(bool multibit[nfrequencies])
{
	//Waiting for buffer to be filled
	while (_stream.count() < nsamples * sizeof(int))
	{
		SDL_Delay(5);
	}

	//Copying buffer
	int ibuffer[nsamples];
	_stream.read((Uint8*)ibuffer, nsamples * sizeof(int));
	std::complex<float> cbuffer[nsamples];
	for (unsigned int i = 0; i < nsamples; i++)
	{
		cbuffer[i] = (float)ibuffer[i];
	}

	//Conducting FFT
	ir::fft(cbuffer, nsamples);

	//Checking for noise
	for (unsigned int i = 0; i < nfrequencies + 1; i++)
	{
		for (unsigned int j = ((i == 0) ? 1 : (frequencies[i - 1] + 2));
			j < ((i == nfrequencies) ? (nsamples / 2 + 1) : (frequencies[i] + 1));
			j++)
		{
			if (abs(cbuffer[j]) > 0.1f * _max_calibration) return false;
		}
	}

	//Finding data
	for (unsigned int i = 0; i < nfrequencies; i++)
	{
		multibit[i] = abs(cbuffer[1 + frequencies[i]]) > _calibration[i];
	}

	return true;
};

bool Receiver::_receive_multibyte(unsigned char multibyte[nfrequencies])
{
	for (unsigned int i = 0; i < 8; i++)
	{
		bool multibit[nfrequencies];
		if (!_receive_multibit(multibit)) return false;
		for (unsigned int j = 0; j < nfrequencies; j++)
		{
			if (multibit[j]) multibyte[j] |= (1 << i);
			else multibyte[j] &= ~(1 << i);
		}
	}
};

bool Receiver::ok()
{
	return _sdl_inited && _device != 0;
};

int Receiver::receive(void *data, unsigned char *size)
{
	while (true)
	{
		if (_stream.count() < sizeof(int)) return 1;
		
		int value;
		_stream.read((Uint8*)&value, sizeof(int));
		if (abs(value) >= _treshold)
		{
			if (_receive_signature_multibyte()) break;
			else _treshold = 2 * abs(value);
		}
	}

	unsigned int received = nfrequencies;	//received by _receive_signature_multibyte
	unsigned char buffer[(255 + sizeof(Header) + nfrequencies - 1) / nfrequencies * nfrequencies];

	//Reading size
	if (!_receive_multibyte(buffer + received)) return false;
	received += nfrequencies;

	//Reading data and hash
	while (received < ((Header*)buffer)->size)
	{
		if (!_receive_multibyte(buffer + received)) return false;
		received += nfrequencies;
	}

	//Checking hash
	unsigned short hash[8];
	ir::md5(buffer + sizeof(Header), ((Header*)buffer)->size, hash);
	if (hash[0] != ((Header*)buffer)->hash) return -1;

	*size = ((Header*)buffer)->size;
	memcpy(data, buffer + sizeof(Header), ((Header*)buffer)->size);
	return 0;
};

Receiver::Receiver() : _stream(1024 * 1024)
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
	SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &desired, &obtained, 0);
	if (device == 0) return;
	SDL_PauseAudioDevice(device, SDL_FALSE);
};

Receiver::~Receiver()
{
	if (_device != 0) SDL_CloseAudioDevice(_device);
	if (_sdl_inited) SDL_Quit();
};

int main(int argc, char **argv)
{
	Receiver r;
	if (!r.ok()) return 1;
	
	char buffer[256];
	unsigned char size;
	while (true)
	{
		int res = r.receive(buffer, &size);
		if (res == 0)
		{
			buffer[size] = '\0';
			printf("%s", buffer);
		}
		else if (res < 0) printf("ERROR\n");
	}

	return 0;
};