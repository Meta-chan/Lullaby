#ifndef LULLABY
#define LULLABY

static const unsigned int frequency = 48000;
static const unsigned int nsamples = 4096;
static const unsigned int nfrequencies = 1;
static const unsigned int frequencies[] =
{
	500
};

struct Header
{
	unsigned char signature[nfrequencies];
	unsigned char size;
	unsigned short hash;
};

#endif