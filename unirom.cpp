/*
    unirom.cpp: decode Universal product info tables in Old World Mac ROMs
    By R. Belmont
    license:BSD-3-Clause

    This is catastrophically gross code but it does what I need.

    To build: clang++ -o unirom -O3 unirom.cpp
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>

// box flag = gestalt ID - 6 for most of these machines
static const char *boxnames[128] =
{
	/* 0-15 */    "II", "IIx", "IIcx", "SE/30", "Portable", "IIci", "Four Square", "IIfx", "Aurora CX16", "Aurora SE25", "Aurora SE16", "Classic", "IIsi", "LC", "Quadra 900", "PowerBook 170",
	/* 16-31 */   "Quadra 700", "Classic II", "PowerBook 100", "PowerBook 140", "Quadra 950", "LC III", "Sun MAE or Carnation 33", "PowerBook Duo 210", "Centris 650", "Columbia", "PowerBook Duo 230", "PowerBook 180", "PowerBook 160", "Quadra 800", "Quadra 650", "LC II",
	/* 32-47 */   "PowerBook Duo 250", "DBLite 20", "Vail 16", "PowerMac 5200 (was Carnation 25)", "PowerMac 6200 (was Carnation 16)", "Cyclone 33", "Brazil 16L", "IIvx", "Brazil 16F", "Brazil 32F", "Brazil C", "Color Classic", "PowerBook 180c", "Wombat 40", "Centris 610", "Quadra 610",
	/* 48-63 */   "PowerBook 145", "Brazil 32cF", "LC 520", "Unused", "Wombat 20", "Wombat 40F", "Centris 660AV", "88100 or Quadra 700 w/RISC Card", "LC III+", "WLCD33", "PowerMac 8100", "PDM 80F", "PDM 100F", "PowerMac 7500", "PowerMac 7300", "PowerMac 7600/8600/9600",
	/* 64-79 */   "LC 930", "Hokusai", "PowerBook 5x0", "Pippin", "PDM Evt1", "PowerMac 6100", "Yeager FSTN", "PowerBook Duo 270c", "Quadra 840AV", "Tempest 33", "LC 550", "Color Classic", "Centris 650 w/RISC Card", "Color Classic II", "PowerBook 165", "PowerBook 190",
	/* 80-95 */   "LC475 20 MHz", "Optimus 20", "Mac TV", "LC 475", "LC 475 33 MHz", "Optimus 25", "LC 575", "Quadra 605 20 MHz", "Quadra 605", "Quadra 605 33 MHz", "Malcolm 25", "Malcolm 33", "Quadra 630/LC 630", "Tell", "PDM 66WLCD", "PDM 80WLCD",
	/* 96-111 */  "PowerBook Duo 280", "PowerBook Duo 280c", "Quadra 900 w/RISC Card", "Quadra 950 w/RISC Card", "Centris 610 w/RISC Card", "Quadra 800 w/RISC Card", "Quadra 610 w/RISC Card", "Quadra 650 w/RISC Card", "Tempest w/RISC Card", "PDM 50L", "PowerMac 7100", "PDM 80L", "Blackbird BFD", "PowerBook 150", "Quadra 700 w/STP", "Quadra 900 w/STP",
	/* 112-127 */ "Quadra 950 w/STP", "Centris 610 w/STP", "Centris 650 w/STP", "Quadra 610 w/STP", "Quadra 650 w/STP", "Quadra 800 w/STP", "PowerBook Duo 2300", "AJ 80", "PowerBook 5x0 PowerPC upgrade", "Malcolm BB80", "PowerBook 5300", "M2 80", "MAE on HP/UX", "MAE on AIX", "MAE on AUX", "Extended"
};

static const char *decoders[25] =
{
	"Unknown", "PALs", "BBU", "Normandy", "Mac II Glue", "MDU", "FMC", "V8/Eagle/Spice", "Orwell", "Jaws", "MSC", "Sonora/Ardbeg/Tinker Bell", "Niagra", "YMCA or djMEMC/MEMCjr/F108",
	"djMEMC/MEMCjr/F108", "HMC", "Pratt", "Hammerhead", "Tinker Bell", "19", "20", "21", "22", "23", "Grackle"
};

static const char *decoder_regs[45] =
{
	"ROM", "diag ROM", "VIA1", "SCC Read", "SCC Write", "IWM/SWIM", "PWM", "Sound", "SCSI", "SCSIDack", "SCSIHsk",
	"VIA2", "ASC", "RBV", "VDAC", "SCSIDMA", "SWIMIOP", "SCCIOP", "OSS", "FMC", "RPU", "Orwell", "JAWS", "SONIC", "SCSI96 1", "SCSI96 2",
	"DAFB or Civic", "PSC DMA", "ROMPhysAddr", "Patch ROM", "NewAge", "Unused31", "Singer", "DSP", "MACE", "MUNI", "AMIC DMA",
	"Pratt", "SWIM3", "AWACS", "Civic", "Sebastian", "BART", "Grand Central"
};

static const unsigned int rom_offsets[] =
	{
		// chksum   table 1     table 2     box via ID
		0x368cadfe, 0x000032c0, 0x00000000, 18, 32, 60, /* IIci */
		0x36b7fb6c, 0x000032c4, 0x00000000, 18, 32, 60, /* IIsi */
		0x4147dd77, 0x000032c0, 0x00000000, 18, 32, 60, /* IIfx */
		0x4957eb49, 0x000032cc, 0x00000000, 18, 32, 60, /* IIvx */
		0x350eacf0, 0x000032cc, 0x00000000, 18, 32, 60, /* LC */
		0x35c28f5f, 0x000032b4, 0x00000000, 18, 32, 60, /* LC II */
		0xecd99dc0, 0x0000322c, 0x00000000, 18, 32, 60, /* Color Classic */
		0xecfa989b, 0x00003200, 0x000d1540, 18, 32, 60, /* PowerBook Duo 210/230/250 */
		0xecbbc41c, 0x00003220, 0x000d2780, 18, 32, 60, /* LC III */
		0xede66cbd, 0x00003224, 0x000d1e70, 18, 32, 60, /* LC 520/550 and friends */
		0xfda22562, 0x0000325c, 0x000a79c0, 18, 32, 60, /* PowerBook 150 */
		0xeaf1678d, 0x00003230, 0x000d0670, 18, 32, 60, /* Macintosh TV / LC 550 */
		0x420dbff3, 0x000031c8, 0x00000000, 18, 32, 60, /* Quadra 700/900, PowerBook 140/170 */
		0xe33b2724, 0x00003218, 0x00000000, 18, 32, 60, /* PowerBook 160/165/165c/180/180c */
		0xf1a6f343, 0x00003230, 0x000d2800, 18, 32, 60, /* Quadra 800 and friends (earlier) */
		0xf1acad13, 0x00003230, 0x000d2800, 18, 32, 60, /* Quadra 800 and friends (later) */
		0x0024d346, 0x0000325c, 0x000a79c0, 18, 32, 68, /* PowerBook Duo 270 */
		0x015621d7, 0x0000325c, 0x000a79c0, 18, 32, 68, /* PowerBook Duo 280 */
		0xff7439ee, 0x0000325c, 0x000a79c0, 18, 32, 68, /* LC 475/575/580/Quadra 605 */
		0xb6909089, 0x0000325c, 0x000a79c0, 18, 32, 68, /* PowerBook 520/540/550 */
		0x06684214, 0x00003260, 0x000a79c0, 18, 32, 68, /* LC 630/Performa 630/Quadra 630 */
		0x064dc91d, 0x00003260, 0x000a79c0, 18, 32, 68, /* LC 580 / Performa 580/588 / Pioneer MPC-LX100 */
		0x5bf10fd1, 0x00013b8c, 0x00013b20, 18, 48, 84, /* Quadra 660AV */
		0x87d3c814, 0x00013a76, 0x00013ae2, 18, 48, 84, /* Quadra 840AV */
		0x9feb69b3, 0x000148a0, 0x000148f8, 18, 48, 84, /* PowerMac 6100/7100/8100 */
		0x9a5dc01f, 0x00014e60, 0x00000000, 18, 48, 84, /* STP PowerPC 601 Upgrade Card */
		0x63abfd3f, 0x000203e0, 0x0002046c, 18, 48, 88, /* PowerMac 5200/5300/6200/6300 */
		0x4d27039c, 0x0003eb50, 0x0003eb6c, 18, 48, 88, /* PowerBook 190 */
		0x83c54f75, 0x0003eb50, 0x0003eb6c, 18, 48, 88, /* PowerBook 520 PPC upgrade */
		0x83a21950, 0x0003f080, 0x0003f0a4, 18, 48, 88, /* PowerBook 1400 */
		0x852cfbdf, 0x0003ea70, 0x0003ea8c, 18, 48, 88, /* PowerBook 5300 */
		0x2bef21b7, 0x00013b20, 0x00013b28, 18, 48, 88, /* Pippin v1.0 */
		0x575be6bb, 0x00012b30, 0x00012b58, 18, 48, 88, /* Motorola StarMax 3000/4000/5500, Umax C500/600 */
		0x960e4be9, 0x00018840, 0x00018858, 18, 48, 88, /* PowerMac 7300/7600/8600/9600 */
		0x276ec1f1, 0x00013b50, 0x00013b5c, 18, 48, 88, /* PowerBook 2400/3400 */
		0x79d68d63, 0x00012d00, 0x00012d08, 18, 48, 88, /* PowerMac G3 "Beige" */
		0x78f57389, 0x00012d10, 0x00000000, 18, 48, 88, /* PowerMac G3 "Beige" */
		0xb46ffb63, 0x00014390, 0x00014398, 18, 48, 88, /* PowerBook G3 "WallStreet" */

		0xffffffff, 0xffffffff, 0xffffffff, 0, /* terminate list */
};

static FILE *file;

unsigned int read8()
{
	unsigned char temp;

	fread(&temp, 1, 1, file);

	return temp;
}

unsigned int read16()
{
	unsigned char data[2];

	fread(&data, 2, 1, file);

	return data[0]<<8 | data[1];
}

unsigned int read32()
{
	unsigned char data[4];

	fread(&data, 4, 1, file);

	return data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
}

void seek32(unsigned int offset)
{
	fseek(file, offset, SEEK_SET);
}

void parse_table(unsigned int tblOffs, unsigned int boxOffs, unsigned int viaOffs, unsigned int idOffs)
{
	unsigned int entryPtr, infoPtr, tblPos;

	tblPos = tblOffs;

	seek32(tblPos);
	tblPos += 4;
	entryPtr = read32();
	while (entryPtr != 0)
	{
		unsigned int box, decoder;
		unsigned int ID, via1, via2;

		infoPtr = entryPtr + (tblPos - 4);

		seek32(infoPtr);
		const unsigned int decoderOffs = read32();

		seek32(infoPtr + 8);
		const int videoOffs = (int)read32();
		unsigned int videoAddr = infoPtr + videoOffs;

		seek32(infoPtr+boxOffs);
		box = read8();
		decoder = read8();

		seek32(infoPtr+viaOffs);
		via1 = read32();
		via2 = read32();

		seek32(infoPtr+idOffs);
		ID = read16();

		bool bShowedSomething = false;
		if (box <= 127)
		{
			bShowedSomething = true;
			if (decoder <= 24)
			{
				printf("[info %x] Box %s decoder %d (%s) VIA mask %08x VIA match %08x ID %04x\n", infoPtr, boxnames[box], decoder, decoders[decoder], via1, via2, ID);
			}
			else
			{
				printf("[info %x] Box %s decoder %d VIA mask %08x VIA match %08x ID %04x\n", infoPtr, boxnames[box], decoder, via1, via2, ID);
			}
		}
		else if (box == 0xfe)
		{
			bShowedSomething = true;
			printf("[info %x] Box Plus decoder %d VIA mask %08x VIA match %08x\n", infoPtr, decoder, via1, via2);
		}
		else if (box == 0xff)
		{
			bShowedSomething = true;
			printf("[info %x] Box SE decoder %d VIA mask %08x VIA match %08x\n", infoPtr, decoder, via1, via2);
		}

		int itemCount = 0;
		if (bShowedSomething)
		{
			seek32(videoAddr);
			const unsigned int screenPhysBase = read32();
			const unsigned int screen32Base = read32();
			const unsigned int screen24Base = read32();
			printf("\tScreen physical %08x logical 32-bit %08x logical 24-bit %08x\n", screenPhysBase, screen32Base, screen24Base);

			seek32(infoPtr + decoderOffs - 4);
			const unsigned int ROMLoc = read32();
			int numItems = 45;
			if (decoder < 15)
			{
				numItems = 30;
			}
			printf("\t");
			for (int i = 0; i < numItems; i++)
			{
				unsigned int value = read32();
				if (value > 0)
				{
					itemCount++;
					if (itemCount >= 5)
					{
						printf("\n\t");
						itemCount = 0;
					}
					printf("%s @ %08x ", decoder_regs[i], value);
				}
			}

			printf("\n");
		}

		if (itemCount > 0)
		{
			printf("\n");
		}

		seek32(tblPos);
		tblPos += 4;
		entryPtr = read32();
	}
}

int main(int argc, char *argv[])
{
	int i;
	unsigned int romID;
	unsigned int tableOffset = 0, tableOffset2 = 0, boxOffs = 0, viaOffs = 0, idOffs = 0;

	if (argc < 2)
	{
		fprintf(stderr, "UniROM: Old World Mac Universal ROM explorer v1.0\n");
		fprintf(stderr, "By R. Belmont 2009-2023.  License: BSD-3-clause.\n");
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		return -1;
	}

	file = fopen(argv[1], "rb");
	if (!file)
	{
		fprintf(stderr, "ERROR: Unable to open %s\n", argv[1]);
		return -1;
	}

	romID = read32();
	printf("ROM ID is %08x\n", romID);

	i = 0;
	while (rom_offsets[i] != 0xffffffff)
	{
		if (rom_offsets[i] == romID)
		{
			tableOffset = rom_offsets[i+1];
			tableOffset2 = rom_offsets[i+2];
			boxOffs = rom_offsets[i+3];
			viaOffs = rom_offsets[i+4];
			idOffs = rom_offsets[i+5];
			break;
		}
		else
		{
			i += 6;
		}
	}

	if ((tableOffset | tableOffset2) == 0)
	{
		fprintf(stderr, "Don't know table offset for this ROM\n");
		return -1;
	}

	if (tableOffset != 0)
	{
		parse_table(tableOffset, boxOffs, viaOffs, idOffs);
	}

	if (tableOffset2 != 0)
	{
		parse_table(tableOffset2, boxOffs, viaOffs, idOffs);
	}

	fclose(file);
	return 0;
}
