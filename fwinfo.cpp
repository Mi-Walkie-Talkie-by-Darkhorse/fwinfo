#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#pragma pack(push, 1) 

typedef struct {
	unsigned short Signature;
	unsigned char  BlockType;
	unsigned char  Unknown3;
	unsigned int   Unknown4;	// Always 0
	unsigned int   DataLength;
	unsigned int   DataCRC32;
	unsigned int   FirmwareVersion;
} FirmwareBlockHeaderV1;

typedef struct {
	unsigned int   Signature;
	unsigned char  HeaderChecksum;
	unsigned char  BlockType;
	unsigned short HardwareRevision;
	unsigned int   FirmwareVersion;
	unsigned int   DataCRC32;
	unsigned int   DataLength;
	unsigned short Unknown0x14;
	unsigned short DeviceType;
} FirmwareBlockHeaderV2;

#pragma pack(pop)

enum {
	FirmwareBlockHeaderV1Signature = 0xAA55,
	FirmwareBlockHeaderV2Signature = 0x5A,
};


static unsigned CalculateHeaderChecksum(const FirmwareBlockHeaderV2& Header) {
	unsigned result = 0;

	for(const unsigned char *Src = &Header.HeaderChecksum + 1, *End = (const unsigned char*)(&Header + 1); Src < End; ++Src) {
		unsigned Data = *Src;

		for(int i = 8; i > 0; --i) {
			int bit0 = (result ^ Data) & 1;
			result >>= 1;
			if (bit0) result ^= 0x8C;
			Data >>= 1;
		}
	}
	return result;
}

static unsigned CRC32Tbl[256];
static void InitCRC32Table() {
	for(int i = 0; i < 256; ++i) {
		unsigned CRC = i;

		for(int j = 8; j > 0; --j) {
			if (CRC & 1) CRC = (CRC >> 1) ^ 0xEDB88320;
			else CRC >>= 1;
		}
		CRC32Tbl[i] = CRC;
	}
}

static unsigned CalculateBlockCRC32(FILE *file, unsigned& DataLength) {
	unsigned CRC32 = (unsigned)-1;

	for(unsigned i = 0, n = DataLength; i < n; ++i) {
		int nextByte = fgetc(file);
		if (nextByte < 0) {
			DataLength = i;
			break;
		}
		CRC32 = CRC32Tbl[(CRC32 ^ nextByte) & 0xFF] ^ (CRC32 >> 8);
	}
	return ~CRC32;
}

static void printBlockDataLength(unsigned DataLength, unsigned ActualLength) {
	printf("\n  Block data length: 0x%06X (%u)", DataLength, DataLength);

	if (DataLength != ActualLength) {
		printf(" <-- Error! Only 0x%06X (%u) data bytes available", ActualLength, ActualLength);
	}
}

static void printBlockDataCRC32(unsigned CRC32, unsigned ActualCRC32) {
	printf("\n  Block data CRC32: 0x%08X", CRC32);

	if (ActualCRC32 != CRC32) {
		printf(" <-- Error! Actual data CRC32: 0x%08X", ActualCRC32);
	}
}

static void printFirmwareVersion(unsigned FirmwareVersion) {
	printf("\n  Firmware version: %u.%u.%u", 
		(FirmwareVersion >> 24) & 0xFF, (FirmwareVersion >> 16) & 0xFF,
		FirmwareVersion & 0xFFFF);
}

static void RewriteFirmwareBlockHeader(FILE *fwFile, const void *const FirmwareBlockHeader, long HeaderOffset) {
	long CurrentPos = ftell(fwFile);
	if (fseek(fwFile, HeaderOffset, SEEK_SET)
	|| (fwrite(FirmwareBlockHeader, sizeof(FirmwareBlockHeaderV1), 1, fwFile) != 1)
	||  fseek(fwFile, CurrentPos, SEEK_SET)) {
		printf("Error writing firmware block header.\nProcess terminated.\n\n");
		exit(1);
	}

	printf("Block header errors fixed\n");
}

static bool FixErrors;
static int ProcessFirmwareBlockV1(FILE *fwFile) {
	long HeaderOffset = ftell(fwFile);

	FirmwareBlockHeaderV1 FirmwareBlockHeader;
	size_t HeaderLength = fread(&FirmwareBlockHeader, 1, sizeof(FirmwareBlockHeader), fwFile);
	if (!HeaderLength) return 0;

	if (HeaderLength != sizeof(FirmwareBlockHeader)) {
		printf("Bad firmware block at 0x%06lX\n"
			"  Block header incomplete\n\n", HeaderOffset);
		return 0;
	}

	if (FirmwareBlockHeader.Signature != FirmwareBlockHeaderV1Signature) {
		printf("Bad firmware block at 0x%06lX:\n"
			"  Block header signature: 0x%04X\n\n",
			HeaderOffset, FirmwareBlockHeader.Signature);
		return 0;
	}

	const char *BlockType;
	switch(FirmwareBlockHeader.BlockType) {
		case 1:  BlockType = " (CPU)"; break;
		case 2:  BlockType = " (BLE)"; break;
		default: BlockType = ""; break;
	}

	printf("Firmware block at 0x%06lX:\n"
		"  Block header signature: 0x%04X (MJDJJ01FY firmware)\n"
		"  Block type: %u%s\n"
		"  Unknown [3]: 0x%02X",
		HeaderOffset, FirmwareBlockHeader.Signature, 
		FirmwareBlockHeader.BlockType, BlockType,
		FirmwareBlockHeader.Unknown3);

	unsigned OriginalDataLength = FirmwareBlockHeader.DataLength;
	unsigned ActualDataCRC32 = CalculateBlockCRC32(fwFile, FirmwareBlockHeader.DataLength);
	printBlockDataLength(OriginalDataLength, FirmwareBlockHeader.DataLength);
	printBlockDataCRC32(FirmwareBlockHeader.DataCRC32, ActualDataCRC32);
	printFirmwareVersion(FirmwareBlockHeader.FirmwareVersion);

	printf("\n  Block data starts at 0x%06lX\n", HeaderOffset + sizeof(FirmwareBlockHeader));

	if (FixErrors 
	&& ((OriginalDataLength != FirmwareBlockHeader.DataLength) 
	|| (ActualDataCRC32 != FirmwareBlockHeader.DataCRC32))) {
		FirmwareBlockHeader.DataCRC32 = ActualDataCRC32;
		RewriteFirmwareBlockHeader(fwFile, &FirmwareBlockHeader, HeaderOffset);
	}

	putchar('\n');
	return OriginalDataLength == FirmwareBlockHeader.DataLength;
}

static int ProcessFirmwareBlockV2(FILE *fwFile) {
	long HeaderOffset = ftell(fwFile);

	FirmwareBlockHeaderV2 FirmwareBlockHeader;
	size_t HeaderLength = fread(&FirmwareBlockHeader, 1, sizeof(FirmwareBlockHeader), fwFile);
	if (!HeaderLength) return 0;

	if (HeaderLength != sizeof(FirmwareBlockHeader)) {
		printf("Bad firmware block at 0x%06lX\n"
			"  Block header incomplete\n\n", HeaderOffset);
		return 0;
	}

	if (FirmwareBlockHeader.Signature != FirmwareBlockHeaderV2Signature) {
		printf("Bad firmware block at 0x%06lX:\n"
			"  Block header signature: 0x%08X\n\n",
			HeaderOffset, FirmwareBlockHeader.Signature);
		return 0;
	}

	printf("Firmware block at 0x%06lX:\n"
		"  Block header signature: 0x%08X\n"
		"  Block header checksum: 0x%02X",
		HeaderOffset, FirmwareBlockHeader.Signature, 
		FirmwareBlockHeader.HeaderChecksum);

	unsigned HeaderChecksum = CalculateHeaderChecksum(FirmwareBlockHeader);
	if (HeaderChecksum != FirmwareBlockHeader.HeaderChecksum) {
		printf(" <-- Error! Actual header checksum: 0x%02X", HeaderChecksum);
	}

	const char *BlockType;
	switch(FirmwareBlockHeader.BlockType) {
		case 0:  BlockType = " (CPU)"; break;
		case 1:  BlockType = " (BLE)"; break;
		case 2:  BlockType = " (Ext. ROM)"; break;
		default: BlockType = ""; break;
	}

	printf("\n  Block type: %u%s\n  Device hardware revision: %u",
		FirmwareBlockHeader.BlockType, BlockType,
		FirmwareBlockHeader.HardwareRevision);
	printFirmwareVersion(FirmwareBlockHeader.FirmwareVersion);

	unsigned OriginalDataLength = FirmwareBlockHeader.DataLength;
	unsigned ActualDataCRC32 = CalculateBlockCRC32(fwFile, FirmwareBlockHeader.DataLength);
	printBlockDataCRC32(FirmwareBlockHeader.DataCRC32, ActualDataCRC32);
	printBlockDataLength(OriginalDataLength, FirmwareBlockHeader.DataLength);

	const char *DeviceType;
	switch(FirmwareBlockHeader.DeviceType) {
		case 4:  DeviceType = " (MJDJJ02FY)"; break;
		case 5:  DeviceType = " (MJDJJ03FY)"; break;
		default: DeviceType = ""; break;
	}

	printf("\n  Unknown [0x14]: 0x%02X\n  Device type: %u%s\n"
		"  Block data starts at 0x%06lX\n",
		FirmwareBlockHeader.Unknown0x14, FirmwareBlockHeader.DeviceType,
		DeviceType, HeaderOffset + sizeof(FirmwareBlockHeader));

	if (FixErrors 
	&& ((OriginalDataLength != FirmwareBlockHeader.DataLength) 
	|| (ActualDataCRC32 != FirmwareBlockHeader.DataCRC32)
	|| (HeaderChecksum != FirmwareBlockHeader.HeaderChecksum))) {
		FirmwareBlockHeader.DataCRC32 = ActualDataCRC32;
		FirmwareBlockHeader.HeaderChecksum = CalculateHeaderChecksum(FirmwareBlockHeader);
		RewriteFirmwareBlockHeader(fwFile, &FirmwareBlockHeader, HeaderOffset);
	}

	putchar('\n');
	return OriginalDataLength == FirmwareBlockHeader.DataLength;
}

int main(int argc, char *const arg[]) {
	const char *mode = "rb";
	switch(argc) {
	case 2: break;
	case 3: {
		char *const Flag = arg[2];
		*((short*)Flag) &= 0xDFFD;
		if (!strcmp(Flag, "-F")) {
			FixErrors = true;
			mode = "r+b";
			break;
		}
	}
	default: {
		const char *const FullName = arg[0];
		const char *Name = strrchr(FullName, '/');
		if (!Name) Name = strrchr(FullName, '\\');
		Name = Name ? Name + 1 : FullName;

		if (argc > 1) printf("\n%s: bad command line.", Name);
		printf("\nUsage: %s <firmware file> [-f]\n\n", Name);
		return 1;
	}
	}

	putchar('\n');
	const char *const fwFileName = arg[1];
	FILE *fwFile = fopen(fwFileName, mode);
	if (!fwFile) {
		printf("Error opening file \"%s\"\n\n", fwFileName);
		return 1;
	}

	union {
		FirmwareBlockHeaderV1 V1;
		FirmwareBlockHeaderV2 V2;
	} FirstBlockHeader;

	if (fread(&FirstBlockHeader, sizeof(FirstBlockHeader), 1, fwFile) != 1) {
		printf("Error reading firmware block header");
		printf(".\n\"%s\" does not seems to be a Mi Walkie-talkie firmware file.\n\n", fwFileName);
		return 1;
	}

	rewind(fwFile);
	InitCRC32Table();

	if (FirstBlockHeader.V2.Signature == FirmwareBlockHeaderV2Signature) {
		while(ProcessFirmwareBlockV2(fwFile)) {};
	} else if (FirstBlockHeader.V1.Signature == FirmwareBlockHeaderV1Signature) {
		while(ProcessFirmwareBlockV1(fwFile)) {};
	} else {
		printf("Unknown file signature 0x%08X", FirstBlockHeader.V2.Signature);
		printf(".\n\"%s\" does not seems to be a Mi Walkie-talkie firmware file.\n\n", fwFileName);
		return 1;
	}

	return 0;
}
