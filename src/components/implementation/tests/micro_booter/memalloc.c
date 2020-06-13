/* The TLSF allocator
   The TLSF allocator's FLI will be decided upon the memory block size.
   The TLSF memory allocator is consisted of FLI, SLI and allocatable
   memory. The FLI is classified by 2^n, and the SLI segregates the
   FLI section by an power of 2, i.e. 8 or 16. Thus, when we need
   an memory block, we try to find it in the corresponding FLI, and
   then the SLI.(This is a two-dimensional matrix.) Then
   (1) If the SLI has no allocatable blocks, we will allocate some
       from the nearest bigger block.
   (2) If there is some block from the SLI block, allocate the memory
       size and put the residue memory into the corresponding FLI and
       SLI area.
   When freeing memory, the adjacent free memory blocks will automatically
   merge.
   In the system, the FLI is variable and the SLI is fixed to 8.
   The FLI has a miniumum block size of 64 Byte(If the allocated size
   is always smaller than 64 bits, then there's no need to use DSA.)
   To make sure that it is like this, we set the smallest allocatable
   size to 64B. In addition, we set the alignment to 8.
   [FLI]:
   .....    6       5      4       3         2        1         0
          8K-4K   4K-2K  2K-1K  1K-512B  511-256B  255-128B  127-64B
   For example, when a memory block is 720 byte, then it should be in
   FLI=3,SLI=3.
   When a lower FLI has no blocks for allocation, it will "borrow"
   a block from the nearest FLI block that is big enough. */

typedef unsigned char u8_t;
typedef char          s8_t;
typedef unsigned long ptr_t;
typedef long          ret_t;
typedef long          cnt_t;


#define TLSF_WORD_ORDER 5
#define TLSF_ALLBITS ((ptr_t)(-1))
#define TLSF_WORD_SIZE (1 << TLSF_WORD_ORDER)
#define TLSF_WORD_MASK (~(TLSF_ALLBITS << TLSF_WORD_ORDER))
#define TLSF_ALIGN_ORDER (TLSF_WORD_ORDER - 3)
#define TLSF_ALIGN_MASK (~(TLSF_ALLBITS << TLSF_ALIGN_ORDER))
#define TLSF_BITMAP_SIZE ((TLSF_MAX_PREEMPT_PRIO - 1) / TLSF_WORD_SIZE + 1)

#define TLSF_COVERAGE_MARKER()
#define TLSF_MEM_FREE (0)
#define TLSF_MEM_USED (1)
#define TLSF_ERR_MEM -1


#define TLSF_POW2(POW) (((ptr_t)1) << (POW))
#define TLSF_ROUND_DOWN(NUM, POW) (((NUM) >> (POW)) << (POW))
#define TLSF_ROUND_UP(NUM, POW) TLSF_ROUND_DOWN((NUM) + TLSF_POW2(POW) - 1, POW)
#define TLSF_MEM_POS(FLI, SLI) ((SLI) + ((FLI) << 3))

ptr_t
TLSF_MSB_Get(ptr_t Val)
{
	return 31 - __builtin_clz(Val);
}

struct TLSF_List {
	volatile struct TLSF_List *Prev;
	volatile struct TLSF_List *Next;
};

/* The head struct of a memory block */
struct TLSF_Mem_Head {
	/* This is what is used in TLSF LUT */
	struct TLSF_List Head;
	/* Is this block used at the moment? */
	ptr_t State;
	/* The pointer to the tail */
	volatile struct TLSF_Mem_Tail *Tail;
};

/* The tail struct of a memory block */
struct TLSF_Mem_Tail {
	/* This is for tailing the memory */
	volatile struct TLSF_Mem_Head *Head;
};

struct TLSF_Mem {
	/* The number of FLIs in the system */
	ptr_t FLI_Num;
	/* The start address of the actual memory pool */
	ptr_t Start;
	/* The size of this pool, including the header, bitmap and list table */
	ptr_t Size;
	/* The location of the list table itself */
	struct TLSF_List *Table;
	/* The bitmap - This is actually an array that have an indefinite length, and will
	 * be decided at runtime. Don't fuss if lint says that this can overflow; it is safe. */
	ptr_t Bitmap[1];
};

void
TLSF_List_Crt(volatile struct TLSF_List *Head)
{
	Head->Prev = (struct TLSF_List *)Head;
	Head->Next = (struct TLSF_List *)Head;
}

void
TLSF_List_Del(volatile struct TLSF_List *Prev, volatile struct TLSF_List *Next)
{
	Next->Prev = (struct TLSF_List *)Prev;
	Prev->Next = (struct TLSF_List *)Next;
}

void
TLSF_List_Ins(volatile struct TLSF_List *New, volatile struct TLSF_List *Prev, volatile struct TLSF_List *Next)
{
	Next->Prev = (struct TLSF_List *)New;
	New->Next  = (struct TLSF_List *)Next;
	New->Prev  = (struct TLSF_List *)Prev;
	Prev->Next = (struct TLSF_List *)New;
}

static const u8_t TLSF_RBIT_Table[256] =
  {0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48,
   0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4,
   0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C,
   0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2,
   0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A,
   0xFA, 0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E,
   0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1, 0x21,
   0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1, 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
   0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55,
   0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD,
   0x7D, 0xFD, 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 0x0B,
   0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
   0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F,
   0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

ptr_t
TLSF_RBIT_Get(ptr_t Val)
{
	ptr_t Ret;
	ptr_t Src;
	u8_t *To;
	u8_t *From;

	Src  = Val;
	To   = (u8_t *)(&Ret);
	From = (u8_t *)(&Src);

#if (TLSF_WORD_ORDER == 4)
	To[0] = TLSF_RBIT_Table[From[1]];
	To[1] = TLSF_RBIT_Table[From[0]];
#elif (TLSF_WORD_ORDER == 5)
	To[0] = TLSF_RBIT_Table[From[3]];
	To[1] = TLSF_RBIT_Table[From[2]];
	To[2] = TLSF_RBIT_Table[From[1]];
	To[3] = TLSF_RBIT_Table[From[0]];
#else
	To[0] = TLSF_RBIT_Table[From[7]];
	To[1] = TLSF_RBIT_Table[From[6]];
	To[2] = TLSF_RBIT_Table[From[5]];
	To[3] = TLSF_RBIT_Table[From[4]];
	To[4] = TLSF_RBIT_Table[From[3]];
	To[5] = TLSF_RBIT_Table[From[2]];
	To[6] = TLSF_RBIT_Table[From[1]];
	To[7] = TLSF_RBIT_Table[From[0]];
#endif

	return Ret;
}

ptr_t
TLSF_LSB_Get(ptr_t Val)
{
	return TLSF_WORD_SIZE - 1 - TLSF_MSB_Get(TLSF_RBIT_Get(Val));
}


void  _TLSF_Mem_Block(volatile struct TLSF_Mem_Head *Addr, ptr_t Size);
void  _TLSF_Mem_Ins(volatile void *Pool, volatile struct TLSF_Mem_Head *Mem_Head);
void  _TLSF_Mem_Del(volatile void *Pool, volatile struct TLSF_Mem_Head *Mem_Head);
ret_t _TLSF_Mem_Search(volatile void *Pool, ptr_t Size, cnt_t *FLI_Level, cnt_t *SLI_Level);


ret_t
TLSF_Mem_Init(volatile void *Pool, ptr_t Size)
{
	cnt_t                     FLI_Cnt;
	ptr_t                     Offset;
	ptr_t                     Bitmap_Size;
	volatile struct TLSF_Mem *Mem;

	/* See if the memory pool is large enough to enable dynamic allocation - at
	 * least 1024 machine words or pool initialization will be refused */
	if ((Pool == 0) || (Size < (1024 * sizeof(ptr_t))) || ((((ptr_t)Pool) + Size) < Size)) {
		TLSF_COVERAGE_MARKER();
		return TLSF_ERR_MEM;
	} else
		TLSF_COVERAGE_MARKER();

	/* See if the address and size is word-aligned */
	if (((((ptr_t)Pool) & TLSF_ALIGN_MASK) != 0) || ((Size & TLSF_ALIGN_MASK) != 0)) {
		TLSF_COVERAGE_MARKER();
		return TLSF_ERR_MEM;
	} else
		TLSF_COVERAGE_MARKER();

	Mem       = (volatile struct TLSF_Mem *)Pool;
	Mem->Size = Size;
	/* Calculate the FLI value needed for this - we always align to 64 byte */
	Mem->FLI_Num = TLSF_MSB_Get(Size - sizeof(struct TLSF_Mem)) - 6 + 1;

	/* Decide the location of the bitmap */
	Offset      = sizeof(struct TLSF_Mem);
	Bitmap_Size = TLSF_ROUND_UP(Mem->FLI_Num, TLSF_ALIGN_ORDER);
	/* Initialize the bitmap */
	for (FLI_Cnt = 0; FLI_Cnt < (cnt_t)(Bitmap_Size >> TLSF_ALIGN_ORDER); FLI_Cnt++) Mem->Bitmap[FLI_Cnt] = 0;

	/* Decide the location of the allocation table - "-sizeof(ptr_t)" is
	 * because we defined the length=1 in our struct already */
	Offset += Bitmap_Size - sizeof(ptr_t);
	Mem->Table = (struct TLSF_List *)(((ptr_t)Mem) + Offset);
	/* Initialize the allocation table */
	for (FLI_Cnt = 0; FLI_Cnt < (cnt_t)(Mem->FLI_Num); FLI_Cnt++) {
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 0)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 1)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 2)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 3)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 4)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 5)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 6)]));
		TLSF_List_Crt(&(Mem->Table[TLSF_MEM_POS(FLI_Cnt, 7)]));
	}

	/* Calculate the offset of the actual allocatable memory - each FLI have
	 * 8 SLIs, and each SLI has a corresponding table header */
	Offset += sizeof(struct TLSF_List) * 8 * Mem->FLI_Num;
	Mem->Start = ((ptr_t)Mem) + Offset;

	/* Initialize the first big block */
	_TLSF_Mem_Block((struct TLSF_Mem_Head *)(Mem->Start), Size - Offset);
	/* Insert the memory into the corresponding level */
	_TLSF_Mem_Ins(Pool, (struct TLSF_Mem_Head *)(Mem->Start));

	return 0;
}

void
_TLSF_Mem_Block(volatile struct TLSF_Mem_Head *Addr, ptr_t Size)
{
	volatile struct TLSF_Mem_Head *Mem_Head;

	/* Get the big memory block's size and position */
	Mem_Head = (struct TLSF_Mem_Head *)Addr;

	/* Initialize the big memory block */
	Mem_Head->State      = TLSF_MEM_FREE;
	Mem_Head->Tail       = (struct TLSF_Mem_Tail *)(((ptr_t)Mem_Head) + Size - sizeof(struct TLSF_Mem_Tail));
	Mem_Head->Tail->Head = Mem_Head;
}

void
_TLSF_Mem_Ins(volatile void *Pool, volatile struct TLSF_Mem_Head *Mem_Head)
{
	ptr_t                      FLI_Level;
	ptr_t                      SLI_Level;
	ptr_t                      Level;
	ptr_t                      Size;
	volatile struct TLSF_Mem * Mem;
	volatile struct TLSF_List *Slot;

	/* Get the memory pool and block size */
	Mem  = (volatile struct TLSF_Mem *)Pool;
	Size = (ptr_t)(Mem_Head->Tail) - ((ptr_t)Mem_Head) - sizeof(struct TLSF_Mem_Head);

	/* Guarantee the Mem_Size is bigger than 64 or a failure will surely occur here */
	FLI_Level = TLSF_MSB_Get(Size) - 6;
	/* Decide the SLI level directly from the FLI level */
	SLI_Level = (Size >> (FLI_Level + 3)) & 0x07;
	/* Calculate the bit position */
	Level = TLSF_MEM_POS(FLI_Level, SLI_Level);
	/* Get the slot */
	Slot = &(Mem->Table[Level]);

	/* See if there are any blocks in the level, equal means no. So what we inserted is the first block */
	if (Slot == Slot->Next) {
		TLSF_COVERAGE_MARKER();
		/* Set the corresponding bit in the TLSF bitmap */
		Mem->Bitmap[Level >> TLSF_WORD_ORDER] |= TLSF_POW2(Level & TLSF_WORD_MASK);
	} else
		TLSF_COVERAGE_MARKER();

	/* Insert the node now */
	TLSF_List_Ins(&(Mem_Head->Head), Slot, Slot->Next);
}

void
_TLSF_Mem_Del(volatile void *Pool, volatile struct TLSF_Mem_Head *Mem_Head)
{
	ptr_t                      FLI_Level;
	ptr_t                      SLI_Level;
	ptr_t                      Level;
	ptr_t                      Size;
	volatile struct TLSF_Mem * Mem;
	volatile struct TLSF_List *Slot;

	/* Get the memory pool and block size */
	Mem  = (volatile struct TLSF_Mem *)Pool;
	Size = (ptr_t)(Mem_Head->Tail) - ((ptr_t)Mem_Head) - sizeof(struct TLSF_Mem_Head);

	/* Guarantee the Mem_Size is bigger than 64 or a failure will surely occur here */
	FLI_Level = TLSF_MSB_Get(Size) - 6;
	/* Decide the SLI level directly from the FLI level */
	SLI_Level = (Size >> (FLI_Level + 3)) & 0x07;
	/* Calculate the bit position */
	Level = TLSF_MEM_POS(FLI_Level, SLI_Level);
	/* Get the slot */
	Slot = &(Mem->Table[Level]);

	/* Delete the node now */
	TLSF_List_Del(Mem_Head->Head.Prev, Mem_Head->Head.Next);

	/* See if there are any blocks in the level, equal means no. So
	 * what we deleted is the last blockm need to clear the flag */
	if (Slot == Slot->Next) {
		TLSF_COVERAGE_MARKER();
		/* Clear the corresponding bit in the TLSF bitmap */
		Mem->Bitmap[Level >> TLSF_WORD_ORDER] &= ~TLSF_POW2(Level & TLSF_WORD_MASK);
	} else
		TLSF_COVERAGE_MARKER();
}

ret_t
_TLSF_Mem_Search(volatile void *Pool, ptr_t Size, cnt_t *FLI_Level, cnt_t *SLI_Level)
{
	ptr_t                     FLI_Level_Temp;
	ptr_t                     SLI_Level_Temp;
	cnt_t                     Level;
	cnt_t                     Word;
	cnt_t                     Limit;
	ptr_t                     LSB;
	volatile struct TLSF_Mem *Mem;

	/* Make sure that it is bigger than 64. 64=2^6 */
	FLI_Level_Temp = TLSF_MSB_Get(Size) - 6;

	/* Decide the SLI level directly from the FLI level. We plus the number by one here
	 * so that we can avoid the list search. However, when the allocated memory is just
	 * one of the levels, then we don't need to jump to the next level and can fit directly */
	SLI_Level_Temp = (Size >> (FLI_Level_Temp + 3)) & 0x07;
	if (Size != (TLSF_POW2(FLI_Level_Temp + 3) * (SLI_Level_Temp + 8))) {
		TLSF_COVERAGE_MARKER();
		SLI_Level_Temp++;

		/* If the SLI level is the largest of the SLI level, then jump to the next FLI level */
		if (SLI_Level_Temp == 8) {
			TLSF_COVERAGE_MARKER();
			FLI_Level_Temp += 1;
			SLI_Level_Temp = 0;
		} else
			TLSF_COVERAGE_MARKER();
	} else
		TLSF_COVERAGE_MARKER();

	/* Check if the FLI level is over the boundary */
	Mem = (volatile struct TLSF_Mem *)Pool;
	if (FLI_Level_Temp >= Mem->FLI_Num) {
		TLSF_COVERAGE_MARKER();
		return -1;
	} else
		TLSF_COVERAGE_MARKER();

	/* Try to find one position on this processor word level */
	Level = TLSF_MEM_POS(FLI_Level_Temp, SLI_Level_Temp);
	LSB   = TLSF_LSB_Get(Mem->Bitmap[Level >> TLSF_WORD_ORDER] >> (Level & TLSF_WORD_MASK));
	/* If there's at least one block that matches the query, return the level */
	if (LSB < TLSF_WORD_SIZE) {
		TLSF_COVERAGE_MARKER();
		Level      = (Level & (~TLSF_WORD_MASK)) + LSB + (Level & TLSF_WORD_MASK);
		*FLI_Level = Level >> 3;
		*SLI_Level = Level & 0x07;
		return 0;
	}
	/* No one exactly fits */
	else {
		TLSF_COVERAGE_MARKER();
		Limit = TLSF_ROUND_UP(Mem->FLI_Num, TLSF_ALIGN_ORDER) >> TLSF_ALIGN_ORDER;
		/* From the next word, query one by one */
		for (Word = (Level >> TLSF_WORD_ORDER) + 1; Word < Limit; Word++) {
			/* If the level has blocks of one FLI level */
			if (Mem->Bitmap[Word] != 0) {
				TLSF_COVERAGE_MARKER();
				/* Find the actual level */
				LSB        = TLSF_LSB_Get(Mem->Bitmap[Word]);
				*FLI_Level = ((Word << TLSF_WORD_ORDER) + LSB) >> 3;
				*SLI_Level = LSB & 0x07;
				return 0;
			} else
				TLSF_COVERAGE_MARKER();
		}
	}

	/* Search failed */
	return -1;
}

void *
TLSF_Malloc(volatile void *Pool, ptr_t Size)
{
	cnt_t                          FLI_Level;
	cnt_t                          SLI_Level;
	volatile struct TLSF_Mem *     Mem;
	ptr_t                          Old_Size;
	volatile struct TLSF_Mem_Head *Mem_Head;
	ptr_t                          Rounded_Size;
	volatile struct TLSF_Mem_Head *New_Mem;
	ptr_t                          New_Size;

	if ((Pool == 0) || (Size == 0)) {
		TLSF_COVERAGE_MARKER();
		return (void *)(0);
	} else
		TLSF_COVERAGE_MARKER();

	/* Round up the size:a multiple of 8 and bigger than 64B */
	Rounded_Size = TLSF_ROUND_UP(Size, 3);
	/* See if it is smaller than the smallest block */
	Rounded_Size = (Rounded_Size > 64) ? Rounded_Size : 64;

	/* See if such block exists, if not, abort */
	if (_TLSF_Mem_Search(Pool, Rounded_Size, &FLI_Level, &SLI_Level) != 0) {
		TLSF_COVERAGE_MARKER();
		return (void *)(0);
	} else
		TLSF_COVERAGE_MARKER();

	Mem = (volatile struct TLSF_Mem *)Pool;

	/* There is such block. Get it and delete it from the TLSF list. */
	Mem_Head = (struct TLSF_Mem_Head *)(Mem->Table[TLSF_MEM_POS(FLI_Level, SLI_Level)].Next);
	_TLSF_Mem_Del(Pool, Mem_Head);

	/* Allocate and calculate if the space left could be big enough to be a new
	 * block. If so, we will put the block back into the TLSF table */
	New_Size = ((ptr_t)(Mem_Head->Tail)) - ((ptr_t)Mem_Head) - sizeof(struct TLSF_Mem_Head) - Rounded_Size;
	if (New_Size >= (sizeof(struct TLSF_Mem_Head) + 64 + sizeof(struct TLSF_Mem_Tail))) {
		TLSF_COVERAGE_MARKER();
		Old_Size = sizeof(struct TLSF_Mem_Head) + Rounded_Size + sizeof(struct TLSF_Mem_Tail);
		New_Mem  = (volatile struct TLSF_Mem_Head *)(((ptr_t)Mem_Head) + Old_Size);

		_TLSF_Mem_Block(Mem_Head, Old_Size);
		_TLSF_Mem_Block(New_Mem, New_Size);

		/* Put the extra block back */
		_TLSF_Mem_Ins(Pool, New_Mem);
	} else
		TLSF_COVERAGE_MARKER();

	/* Mark the block as in use */
	Mem_Head->State = TLSF_MEM_USED;

	/* Finally, return the start address */
	return (void *)(((ptr_t)Mem_Head) + sizeof(struct TLSF_Mem_Head));
}

void
TLSF_Free(volatile void *Pool, void *Mem_Ptr)
{
	volatile struct TLSF_Mem *     Mem;
	volatile struct TLSF_Mem_Head *Mem_Head;
	volatile struct TLSF_Mem_Head *Left_Head;
	volatile struct TLSF_Mem_Head *Right_Head;
	cnt_t                          Merge_Left;

	/* Check if pointer is null */
	if ((Pool == 0) || (Mem_Ptr == 0)) {
		TLSF_COVERAGE_MARKER();
		return;
	} else
		TLSF_COVERAGE_MARKER();

	/* See if the address is within the allocatable address range. If not, abort directly. */
	Mem = (volatile struct TLSF_Mem *)Pool;
	if ((((ptr_t)Mem_Ptr) <= ((ptr_t)Mem)) || (((ptr_t)Mem_Ptr) >= (((ptr_t)Mem) + Mem->Size))) {
		TLSF_COVERAGE_MARKER();
		return;
	} else
		TLSF_COVERAGE_MARKER();

	Mem_Head = (struct TLSF_Mem_Head *)(((ptr_t)Mem_Ptr) - sizeof(struct TLSF_Mem_Head));
	/* See if the block can really be freed */
	if (Mem_Head->State == TLSF_MEM_FREE) {
		TLSF_COVERAGE_MARKER();
		return;
	} else
		TLSF_COVERAGE_MARKER();

	/* Mark it as free */
	Mem_Head->State = TLSF_MEM_FREE;

	/* Now check if we can merge it with the higher blocks */
	Right_Head = (struct TLSF_Mem_Head *)(((ptr_t)(Mem_Head->Tail)) + sizeof(struct TLSF_Mem_Tail));
	if (((ptr_t)Right_Head) != (((ptr_t)Mem) + Mem->Size)) {
		TLSF_COVERAGE_MARKER();
		/* If this one is unoccupied */
		if ((Right_Head->State) == TLSF_MEM_FREE) {
			TLSF_COVERAGE_MARKER();
			/* Delete, merge */
			_TLSF_Mem_Del(Pool, Right_Head);
			_TLSF_Mem_Block(Mem_Head,
			                ((ptr_t)(Right_Head->Tail)) + sizeof(struct TLSF_Mem_Tail) - (ptr_t)Mem_Head);
		} else
			TLSF_COVERAGE_MARKER();
	} else
		TLSF_COVERAGE_MARKER();

	/* Now check if we can merge it with the lower blocks */
	Merge_Left = 0;
	if ((ptr_t)Mem_Head != Mem->Start) {
		TLSF_COVERAGE_MARKER();
		Left_Head = ((struct TLSF_Mem_Tail *)(((ptr_t)Mem_Head) - sizeof(struct TLSF_Mem_Tail)))->Head;

		/* If this one is unoccupied */
		if (Left_Head->State == TLSF_MEM_FREE) {
			TLSF_COVERAGE_MARKER();
			/* Delete, merge */
			_TLSF_Mem_Del(Pool, Left_Head);
			_TLSF_Mem_Block(Left_Head, (ptr_t)((ptr_t)(Mem_Head->Tail) + sizeof(struct TLSF_Mem_Tail)
			                                   - (ptr_t)Left_Head));

			/* We have completed the merge here and the original block has destroyed.
			 * Thus there's no need to insert it into the list again */
			Merge_Left = 1;
		} else
			TLSF_COVERAGE_MARKER();
	} else
		TLSF_COVERAGE_MARKER();

	/* If we did not merge it with the left-side blocks, insert the original pointer's block
	 * into the TLSF table(Merging with the right-side one won't disturb this) */
	if (Merge_Left == 0) {
		TLSF_COVERAGE_MARKER();
		_TLSF_Mem_Ins(Pool, Mem_Head);
	} else {
		TLSF_COVERAGE_MARKER();
		_TLSF_Mem_Ins(Pool, Left_Head);
	}
}

void *
TLSF_Realloc(volatile void *Pool, void *Mem_Ptr, ptr_t Size)
{
	/* The size of the original memory block */
	ptr_t Mem_Size;
	/* The rounded size of the new memory request */
	ptr_t Rounded_Size;
	ptr_t Count;
	/* The pointer to the pool */
	volatile struct TLSF_Mem *Mem;
	/* The head of the old memory */
	volatile struct TLSF_Mem_Head *Mem_Head;
	/* The right-side block head */
	volatile struct TLSF_Mem_Head *Right_Head;
	/* The pointer to the residue memory head */
	volatile struct TLSF_Mem_Head *Res_Mem;
	/* The new memory block */
	void *New_Mem;
	/* The size of the memory block including the header sizes */
	ptr_t Old_Size;
	/* The size of the residue memory block including the header sizes */
	ptr_t Res_Size;

	/* Check if no pool present */
	if (Pool == 0) {
		TLSF_COVERAGE_MARKER();
		return 0;
	} else
		TLSF_COVERAGE_MARKER();

	/* Are we passing in a NULL pointer? */
	if (Mem_Ptr == 0) {
		TLSF_COVERAGE_MARKER();
		return TLSF_Malloc(Pool, Size);
	} else
		TLSF_COVERAGE_MARKER();

	/* Is the size passed in zero? If yes, we free directly */
	if (Size == 0) {
		TLSF_COVERAGE_MARKER();
		TLSF_Free(Pool, Mem_Ptr);
		return 0;
	} else
		TLSF_COVERAGE_MARKER();

	/* See if the address is within the allocatable address range. If not, abort directly. */
	Mem = (volatile struct TLSF_Mem *)Pool;
	if ((((ptr_t)Mem_Ptr) <= ((ptr_t)Mem)) || (((ptr_t)Mem_Ptr) >= (((ptr_t)Mem) + Mem->Size))) {
		TLSF_COVERAGE_MARKER();
		return 0;
	} else
		TLSF_COVERAGE_MARKER();

	/* Yes, get the location of the header of the memory */
	Mem_Head = (struct TLSF_Mem_Head *)(((ptr_t)Mem_Ptr) - sizeof(struct TLSF_Mem_Head));
	/* See if the block can really be realloced */
	if (Mem_Head->State == TLSF_MEM_FREE) {
		TLSF_COVERAGE_MARKER();
		return 0;
	} else
		TLSF_COVERAGE_MARKER();

	/* Round up the size:a multiple of 8 and bigger than 64B */
	Rounded_Size = TLSF_ROUND_UP(Size, 3);
	/* See if it is smaller than the smallest block */
	Rounded_Size = (Rounded_Size > 64) ? Rounded_Size : 64;

	Mem_Size = ((ptr_t)Mem_Head->Tail) - ((ptr_t)Mem_Ptr);
	/* Does the right-side head exist at all? */
	Right_Head = (struct TLSF_Mem_Head *)(((ptr_t)(Mem_Head->Tail)) + sizeof(struct TLSF_Mem_Tail));
	if (((ptr_t)Right_Head) == (((ptr_t)Mem) + Mem->Size)) {
		TLSF_COVERAGE_MARKER();
		Right_Head = 0;
	} else
		TLSF_COVERAGE_MARKER();

	/* Are we gonna expand it? */
	if (Mem_Size < Rounded_Size) {
		/* Expanding */
		TLSF_COVERAGE_MARKER();
		/* Does the right side exist at all? */
		if (Right_Head != 0) {
			TLSF_COVERAGE_MARKER();
			/* Is it allocated? */
			if (Right_Head->State == TLSF_MEM_FREE) {
				TLSF_COVERAGE_MARKER();
				/* Right-side exists and is free. How big is its usable size? Is it sufficient for our
				 * realloc? */
				if ((((ptr_t)Right_Head->Tail) - ((ptr_t)Mem_Ptr)) >= Rounded_Size) {
					TLSF_COVERAGE_MARKER();
					/* Remove the right-side from the free list so we can operate on it */
					_TLSF_Mem_Del(Pool, Right_Head);
					/* Allocate and calculate if the space left could be big enough to be a new
					 * block. If so, we will put the block back into the TLSF table */
					Res_Size = ((ptr_t)(Right_Head->Tail)) - ((ptr_t)Mem_Ptr) - Rounded_Size;
					/* Is the residue big enough to be a block? */
					if (Res_Size
					    >= (sizeof(struct TLSF_Mem_Head) + 64 + sizeof(struct TLSF_Mem_Tail))) {
						TLSF_COVERAGE_MARKER();
						Old_Size = sizeof(struct TLSF_Mem_Head) + Rounded_Size
						           + sizeof(struct TLSF_Mem_Tail);
						Res_Mem = (volatile struct TLSF_Mem_Head *)(((ptr_t)Mem_Head)
						                                            + Old_Size);

						_TLSF_Mem_Block(Mem_Head, Old_Size);
						_TLSF_Mem_Block(Res_Mem, Res_Size);

						/* Put the extra block back */
						_TLSF_Mem_Ins(Pool, Res_Mem);
					} else {
						/* Residue too small. Merging the whole thing in is the only option */
						TLSF_COVERAGE_MARKER();
						Old_Size = ((ptr_t)(Right_Head->Tail)) - ((ptr_t)Mem_Head)
						           + sizeof(struct TLSF_Mem_Tail);
						_TLSF_Mem_Block(Mem_Head, Old_Size);
					}

					/* Mark the block as in use (making new block clears this flag) */
					Mem_Head->State = TLSF_MEM_USED;
					/* Return the old pointer because we expanded it */
					return Mem_Ptr;
				}
				/* Right-side not large enough, have to go malloc then memcpy */
				else
					TLSF_COVERAGE_MARKER();
			}
			/* It is allocated, have to go malloc then memcpy */
			else
				TLSF_COVERAGE_MARKER();
		}
		/* Right-side doesn't exist, have to go malloc then memcpy */
		else
			TLSF_COVERAGE_MARKER();

		New_Mem = TLSF_Malloc(Pool, Rounded_Size);
		/* See if we can allocate this much, if we can't at all, exit */
		if (New_Mem == 0) {
			TLSF_COVERAGE_MARKER();
			return 0;
		} else
			TLSF_COVERAGE_MARKER();

		/* Copy old memory to new memory - we know that this is always aligned, so this is fine */
		for (Count = 0; Count < (Mem_Size >> TLSF_ALIGN_ORDER); Count++)
			((ptr_t *)New_Mem)[Count] = ((ptr_t *)Mem_Ptr)[Count];

		/* Free old memory then return */
		TLSF_Free(Pool, Mem_Ptr);
		return New_Mem;
	}
	/* Shrinking or keeping */
	else
		TLSF_COVERAGE_MARKER();

	/* Are we keeping the size? */
	if (Mem_Size == Rounded_Size) {
		TLSF_COVERAGE_MARKER();
		return Mem_Ptr;
	} else
		TLSF_COVERAGE_MARKER();

	/* Does the right side exist at all? */
	if (Right_Head != 0) {
		TLSF_COVERAGE_MARKER();
		/* Is it allocated? */
		if (Right_Head->State == TLSF_MEM_FREE) {
			/* Right-side not allocated. Need to merge the block */
			TLSF_COVERAGE_MARKER();
			/* Remove the right-side from the allocation list so we can operate on it */
			_TLSF_Mem_Del(Pool, Right_Head);
			Res_Size = ((ptr_t)(Right_Head->Tail)) - ((ptr_t)Mem_Ptr) - Rounded_Size;
			Old_Size = sizeof(struct TLSF_Mem_Head) + Rounded_Size + sizeof(struct TLSF_Mem_Tail);
			Res_Mem  = (volatile struct TLSF_Mem_Head *)(((ptr_t)Mem_Head) + Old_Size);

			_TLSF_Mem_Block(Mem_Head, Old_Size);
			_TLSF_Mem_Block(Res_Mem, Res_Size);

			/* Put the extra block back */
			_TLSF_Mem_Ins(Pool, Res_Mem);

			/* Mark the block as in use (making new block clears this flag) */
			Mem_Head->State = TLSF_MEM_USED;
			/* Return the old pointer because we shrinked it */
			return Mem_Ptr;
		}
		/* Allocated. Need to see if the residue block itself is large enough to be inserted back */
		else
			TLSF_COVERAGE_MARKER();
	} else
		TLSF_COVERAGE_MARKER();

	/* The right-side head either does not exist or is allocated. Calculate the resulting residue size */
	Res_Size = Mem_Size - Rounded_Size;
	if (Res_Size < (sizeof(struct TLSF_Mem_Head) + 64 + sizeof(struct TLSF_Mem_Tail))) {
		TLSF_COVERAGE_MARKER();
		/* The residue block wouldn't even count as a small one. Do nothing and quit */
		return Mem_Ptr;
	} else
		TLSF_COVERAGE_MARKER();

	/* The residue will be big enough to become a standalone block. We need to place it back */
	Old_Size = sizeof(struct TLSF_Mem_Head) + Rounded_Size + sizeof(struct TLSF_Mem_Tail);
	Res_Mem  = (volatile struct TLSF_Mem_Head *)(((ptr_t)Mem_Head) + Old_Size);

	_TLSF_Mem_Block(Mem_Head, Old_Size);
	_TLSF_Mem_Block(Res_Mem, Res_Size);

	/* Put the extra block back */
	_TLSF_Mem_Ins(Pool, Res_Mem);

	/* Mark the block as in use (making new block clears this flag) */
	Mem_Head->State = TLSF_MEM_USED;
	/* Return the old pointer because we shrinked it */
	return Mem_Ptr;
}
