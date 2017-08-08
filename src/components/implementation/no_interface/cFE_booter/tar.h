#define TAR_BLOCKSIZE 512
#define INT32_MAX 0x7FFFFFF //2^31 - 1

uint32 round_to_blocksize(uint32 offset);

uint32 oct_to_dec(char *oct);

uint32 tar_load();

uint32 tar_parse();

uint32 tar_cphdr(uint32 tar_offset, struct fsobj *file);

