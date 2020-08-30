#ifndef LULLABY
#define LULLABY

static const unsigned int frequency = 44100;
static const unsigned int nsamples = 128;
static const unsigned int nfrequencies = 1;
static const unsigned int frequencies[] =
{
	2
};

struct Header
{
	unsigned char signature[nfrequencies];
	unsigned char size;
	unsigned short hash;
};

#endif