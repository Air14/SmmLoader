extern "C"
{
#include <PiMm.h>
#include <Library/BaseMemoryLib.h>

int memcmp(const void* ptr1, const void* ptr2, size_t num)
{
	return static_cast<int>(CompareMem(ptr1, ptr2, num));
}

void* memset(void* ptr, int value, size_t num)
{
	return SetMem(ptr, num, static_cast<UINT8>(value));
}

}