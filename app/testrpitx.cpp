#include <unistd.h>
#include "../src/librpitx.h"
#include <unistd.h>
#include "stdio.h"
#include <cstring>
#include <signal.h>

bool running = true;
/*int 
gcd ( int a, int b )
{
  int c;
  while ( a != 0 ) {
     c = a; a = b%a;  b = c;
  }
  return b;
}*/

uint64_t gcd(uint64_t x, uint64_t y)
{
	return y == 0 ? x : gcd(y, x % y);
}

uint64_t lcm(uint64_t x, uint64_t y)
{
	return x * y / gcd(x, y);
}

void SimpleTest(uint64_t Freq)
{
	generalgpio genpio;
	fprintf(stderr, "GPIOPULL =%x\n", genpio.gpioreg[GPPUDCLK0]);

#define PULL_OFF 0
#define PULL_DOWN 1
#define PULL_UP 2
	genpio.gpioreg[GPPUD] = 1; //PULL_DOWN;
	usleep(150);
	genpio.gpioreg[GPPUDCLK0] = (1 << 4); //GPIO CLK is GPIO 4
	usleep(150);
	genpio.gpioreg[GPPUDCLK0] = (0); //GPIO CLK is GPIO 4

	//genpio.setpulloff(4);

	padgpio pad;
	pad.setlevel(7);

	clkgpio clk;
	clk.print_clock_tree();
	clk.SetPllNumber(clk_pllc, 0);

	//clk.SetAdvancedPllMode(true);
	//clk.SetPLLMasterLoop(0,4,0);
	//clk.Setppm(+7.7);
	clk.SetCenterFrequency(Freq, 1000);
	double freqresolution = clk.GetFrequencyResolution();
	double RealFreq = clk.GetRealFrequency(0);
	fprintf(stderr, "Frequency resolution=%f Error freq=%f\n", freqresolution, RealFreq);
	int Deviation = 0;
	clk.SetFrequency(000);

	clk.enableclk(4);
	usleep(100);
	//clk.SetClkDivFrac(100,0); // If mash!=0 update doesnt seem to work
	int count = 0;
	while (running)
	{
		//clk.SetMasterMultFrac(44,(1<<count));
		//uint32_t N=(1<<18);
		uint32_t N = (1 << 13) * count;

		//clk.SetMasterMultFrac(34,N);
		printf("count =%d gcd%d spurious%f N=%x %f\n", count, lcm(N, 1 << 20), (double)gcd(1 << 20, N) * 19.2e6 / (double)(1 << 20), N, N / (float)(1 << 20));
		count = (count + 1) % 128;
		//usleep(10000000);
		int a = getc(stdin);
		static int Ki = 4, Kp = 0, Ka = 0;
		Kp = Kp + 1;
		if (Kp > 15)
		{
			Kp = 0;
			Ki = Ki + 1;
		}
		//Ki=Ki+1;
		if (Ki > 11)
		{
			Ki = 4;
			Ka++;
		}
		Ki = Kp;

		clk.SetClkDivFrac(count, count);

		//clk.SetPLLMasterLoop(Ki,4,Ka);
		//clk.SetPLLMasterLoop(2,4,0);
		//clk.SetPLLMasterLoop(3,4,0); //best one

		//printf("Ki=%d :Kp %d Ka %d\n ",Ki,Kp,Ka);
		/*clk.SetFrequency(000);
		sleep(5);
		clk.SetFrequency(freqresolution);
		sleep(5);*/
	}
	/*
	for(int i=0;i<100000;i+=1)
	{
		clk.SetFrequency(0);
		usleep(1000);
	}*/
	clk.disableclk(4);
}

void SimpleTestDMA(uint64_t Freq)
{

	int SR = 200000;
	int FifoSize = 4096;
	ngfmdmasync ngfmtest(Freq, SR, 14, FifoSize);
	for (int i = 0; running;)
	{
		//usleep(10);
		usleep(FifoSize * 1000000.0 * 3.0 / (4.0 * SR));
		int Available = ngfmtest.GetBufferAvailable();
		if (Available > FifoSize / 2)
		{
			int Index = ngfmtest.GetUserMemIndex();
			//printf("GetIndex=%d\n",Index);
			for (int j = 0; j < Available; j++)
			{
				//ngfmtest.SetFrequencySample(Index,((i%10000)>5000)?1000:0);
				ngfmtest.SetFrequencySample(Index + j, (i % SR) / 10.0);
				i++;
			}
		}
	}
	fprintf(stderr, "End\n");

	ngfmtest.stop();
}

void SimpleTestFileIQ(uint64_t Freq)
{
	FILE *iqfile = NULL;
	iqfile = fopen("../ssbtest.iq", "rb");
	if (iqfile == NULL)
		printf("input file issue\n");

#define IQBURST 1280
	bool stereo = true;
	int SR = 48000;
	int FifoSize = 512;
	iqdmasync iqtest(Freq, SR, 14, FifoSize, MODE_IQ);
	short IQBuffer[IQBURST * 2];
	std::complex<float> CIQBuffer[IQBURST];
	while (running)
	{
		int nbread = fread(IQBuffer, sizeof(short), IQBURST * 2, iqfile);
		if (nbread > 0)
		{
			for (int i = 0; i < nbread / 2; i++)
			{

				CIQBuffer[i] = std::complex<float>(IQBuffer[i * 2] / 32768.0, IQBuffer[i * 2 + 1] / 32768.0);
			}
			iqtest.SetIQSamples(CIQBuffer, nbread / 2, 1);
		}
		else
		{
			printf("End of file\n");
			fseek(iqfile, 0, SEEK_SET);
		}
	}

	iqtest.stop();
}

void SimpleTestbpsk(uint64_t Freq)
{

	clkgpio clk;
	clk.print_clock_tree();

	int SR = 250000;
	int FifoSize = 10000;
	int NumberofPhase = 2;
	phasedmasync biphase(Freq, SR, NumberofPhase, 14, FifoSize);
	padgpio pad;
	pad.setlevel(7);
	int lastphase = 0;
#define BURST_SIZE 100
	int PhaseBuffer[BURST_SIZE];
	while (running)
	{
		for (int i = 0; i < BURST_SIZE; i++)
		{
			int phase = (rand() % NumberofPhase);
			PhaseBuffer[i] = phase;
		}
		biphase.SetPhaseSamples(PhaseBuffer, BURST_SIZE);
	}
	biphase.stop();
}

void SimpleTestSerial()
{

	bool stereo = true;
	int SR = 10000;
	int FifoSize = 1024;
	bool dualoutput = true;
	serialdmasync testserial(SR, 14, FifoSize, dualoutput);

	while (running)
	{

		usleep(10);
		int Available = testserial.GetBufferAvailable();
		if (Available > 256)
		{
			int Index = testserial.GetUserMemIndex();

			for (int i = 0; i < Available; i++)
			{

				testserial.SetSample(Index + i, i);
			}
		}
	}
	testserial.stop();
}

void SimpleTestAm(uint64_t Freq)
{
	FILE *audiofile = NULL;
	audiofile = fopen("../ssbaudio48.wav", "rb");
	if (audiofile == NULL)
		printf("input file issue\n");

	bool Stereo = true;
	int SR = 48000;
	int FifoSize = 512;
	amdmasync amtest(Freq, SR, 14, FifoSize);

	short AudioBuffer[128 * 2];

	while (running)
	{
		//usleep(FifoSize*1000000.0*1.0/(8.0*SR));
		usleep(100);
		int Available = amtest.GetBufferAvailable();
		if (Available > 256)
		{
			int Index = amtest.GetUserMemIndex();
			int nbread = fread(AudioBuffer, sizeof(short), 128 * 2, audiofile);
			if (nbread > 0)
			{

				for (int i = 0; i < nbread / 2; i++)
				{
					if (!Stereo)
					{
						float x = ((AudioBuffer[i * 2] / 32768.0) + (AudioBuffer[i * 2 + 1] / 32768.0)) / 4.0;
						amtest.SetAmSample(Index + i, x);
					}
					else
					{
						float x = ((AudioBuffer[i] / 32768.0) / 2.0) * 8.0;
						amtest.SetAmSample(Index + i, x);
					}
				}
			}
			else
			{
				printf("End of file\n");
				fseek(audiofile, 0, SEEK_SET);
				//break;
			}
		}
	}
	amtest.stop();
}

void SimpleTestOOK(uint64_t Freq)
{

	int SR = 1000;
	int FifoSize = 21; //24
	ookburst ook(Freq, SR, 14, FifoSize);

	unsigned char TabSymbol[FifoSize] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0};

	ook.SetSymbols(TabSymbol, FifoSize);
}

void SimpleTestOOKTiming(uint64_t Freq)
{

	// 30ms max message
	ookbursttiming ooksender(Freq, 300000);
	ookbursttiming::SampleOOKTiming Message[100];

	for (size_t i = 0; i < 100; i += 2)
	{
		Message[i].value = 1;
		Message[i].duration = 50;
		Message[i + 1].value = 0;
		Message[i + 1].duration = 2000;
	}
	ooksender.SendMessage(Message, 100);
}

uint8_t SetCRC8(uint8_t *addr)
{
	char nibble[8];
	for (size_t i = 0; i < 4; i++)
	{

		nibble[i * 2] = 0;
		nibble[i * 2 + 1] = 0;

		for (int j = 0; j < 4; j++)
		{
			nibble[i * 2] |= addr[i * 8 + j] << (j);
			nibble[i * 2 + 1] |= addr[i * 8 + j + 4] << (j);
		}
		fprintf(stderr, "nibble[%d]=%x\nnibble[%d]=%x\n", i * 2, nibble[i * 2], i * 2 + 1, nibble[i * 2 + 1]);
	}
	int8_t checksumcalc = 0;
	if ((nibble[2] & 0x6) != 6)
	{ // temperature packet
		fprintf(stderr, "Temp\n");
		checksumcalc = (0xf - nibble[0] - nibble[1] - nibble[2] - nibble[3] - nibble[4] - nibble[5] - nibble[6] - nibble[7]) & 0xf;
	}
	else
	{
		if ((nibble[3] & 0x7) == 3)
		{ // Rain packet
			fprintf(stderr, "Rain\n");
			checksumcalc = (0x7 + nibble[0] + nibble[1] + nibble[2] + nibble[3] + nibble[4] + nibble[5] + nibble[6] + nibble[7]) & 0xf;
		}
		else
		{ // Wind packet
			fprintf(stderr, "wind\n");
			checksumcalc = (0xf - nibble[0] - nibble[1] - nibble[2] - nibble[3] - nibble[4] - nibble[5] - nibble[6] - nibble[7]) & 0xf;
		}
	}
	for (size_t i = 0; i < 4; i++)
	{
		addr[32 + i] = (checksumcalc >> (i)) & 1;
	}
	fprintf(stderr, "CRC=%x", checksumcalc);
	return checksumcalc;
}

void AlectoOOK(uint64_t Freq)
{

	// 30ms max message
	ookbursttiming ooksender(Freq, 1200000);
	ookbursttiming::SampleOOKTiming Message[50 * 6 * 2]; // 46 exactly * 6 times

	ookbursttiming::SampleOOKTiming pulse;
	pulse.value = 1;
	pulse.duration = 450;//468

	ookbursttiming::SampleOOKTiming Sync;
	Sync.value = 0;
	Sync.duration = 9000;//8956

	ookbursttiming::SampleOOKTiming Zero;
	Zero.value = 0;
	Zero.duration = 2000;//2016

	ookbursttiming::SampleOOKTiming One;
	One.value = 0;
	One.duration = 4000;

	size_t n = 0;

	//Widn average
	unsigned char AlectoProtocol[] = {0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0};
	// Wind gust direction
	unsigned char AlectoProtocol1[] = {0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	// Temperature
	unsigned char AlectoProtocolT[] = {0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0};
	//ID

	/*AlectoProtocol[0]=1;
	unsigned char LowBattery=0;
	AlectoProtocol[8]=LowBattery;
	AlectoProtocol[9]=1;//Wind
	AlectoProtocol[10]=1;//Wind
	*/
	//AlectoProtocol[11] = 1;  //Button generated
	//AlectoProtocol[28] = 1;  //Wind
	//AlectoProtocol1[28] = 1; //Wind

	

	//CheckSum n8 = ( 0xf - n0 - n1 - n2 - n3 - n4 - n5 - n6 - n7 ) & 0xf
	

	for (size_t h = 0; h < 6; h++)
	{


	int direction = h*45;
	for (size_t i = 0; i < 8; i++)
	{
		AlectoProtocol1[15 + i] = (direction >> (i)) & 0x1;
	}
	int Speed = h*10;
	Speed=Speed/0.2;
	for (size_t i = 0; i < 8; i++)
	{
		AlectoProtocol[24 + i] = (direction >> (i)) & 0x1;
		AlectoProtocol1[24 + i] = (direction >> (i)) & 0x1;
	}
	SetCRC8(AlectoProtocol1);
	SetCRC8(AlectoProtocol);
	n=0;
	for (size_t i = 0; i < 8; i++)
	{
		Message[n++] = pulse;
		Message[n++] = One;
	}

	Message[n++] = pulse;
	Message[n++] = Sync;
	for (size_t k = 0; k < 6; k++)
	{
		for (size_t i = 0; i < 36; i++)
		{
			Message[n++] = pulse;
			if (k % 2 == 0)
			{
				if (AlectoProtocol[i] == 0)
					Message[n++] = Zero;
				else
					Message[n++] = One;
			}
			else
			{
				if (AlectoProtocol1[i] == 0)
					Message[n++] = Zero;
				else
					Message[n++] = One;
			}
			//Message[n++]=(AlectoProtocol[i]==0)?Zero:One;
		}

		if (k < 5) // Not last one
		{
			for (size_t i = 0; i < 4; i++)
			{
				Message[n++] = pulse;
				Message[n++] = Sync;
			}
		}
	}
	Message[n++] = pulse;
	Message[n++] = Sync;
	Message[n++] = pulse;

	fprintf(stderr, "N=%d\n", n);
	ooksender.SendMessage(Message, n);
	sleep(10);
	
		int Temperature = h * 100+200;
		for (size_t i = 0; i < 12; i++)
		{
			AlectoProtocolT[12 + i] = (Temperature >> (i)) & 0x1;
		}
		//AlectoProtocolT[11] = 1;
		//AlectoProtocolT[8] = 1;
		SetCRC8(AlectoProtocolT);
		fprintf(stderr, "Temperature sending\n");
		n = 0;
		for (size_t i = 0; i < 8; i++)
		{
			Message[n++] = pulse;
			Message[n++] = One;
		}

		Message[n++] = pulse;
		Message[n++] = Sync;
		for (size_t k = 0; k < 6; k++)
		{
			for (size_t i = 0; i < 36; i++)
			{
				Message[n++] = pulse;

				if (AlectoProtocolT[i] == 0)
					Message[n++] = Zero;
				else
					Message[n++] = One;
			}
			if (k < 5) // Not last one
			{
				//for (size_t i = 0; i < 4; i++)
				{
					Message[n++] = pulse;
					Message[n++] = Sync;
				}
			}
		}
		Message[n++] = pulse;
		Message[n++] = Sync;
		Message[n++] = pulse;

		ooksender.SendMessage(Message, n);
		//sleep(5);
		sleep(20);
	}
}

void SimpleTestBurstFsk(uint64_t Freq)
{

	//int SR=40625;
	int SR = 1000;
	float Deviation = 2000;
	int FifoSize = 21;
	fskburst fsktest(Freq, SR, Deviation, 14, FifoSize);

	unsigned char TabSymbol[FifoSize] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0};

	//while(running)
	{

		fsktest.SetSymbols(TabSymbol, FifoSize);
	}
	fsktest.stop();
}
static void
terminate(int num)
{
	running = false;
	fprintf(stderr, "Caught signal - Terminating\n");
}

int main(int argc, char *argv[])
{

	uint64_t Freq = 144200000;
	if (argc > 1)
		Freq = atof(argv[1]);

	for (int i = 0; i < 64; i++)
	{
		struct sigaction sa;

		std::memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
	}

	//SimpleTest(Freq);
	//SimpleTestbpsk(Freq);
	//SimpleTestFileIQ(Freq);
	//SimpleTestDMA(Freq);
	//SimpleTestAm(Freq);
	//SimpleTestOOK(Freq);
	//SimpleTestBurstFsk(Freq);
	//SimpleTestOOKTiming(Freq);
	AlectoOOK(Freq);
}
