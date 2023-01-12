extern "C"
{
#include <Library/MmServicesTableLib.h>
}

void* __cdecl operator new(size_t Size) noexcept(false)
{
    if (!Size)
        return nullptr;

    void* buff{};
    if (EFI_ERROR(gMmst->MmAllocatePool(EfiRuntimeServicesCode, Size, &buff)))
        return nullptr;
    else
        return buff;
}

void __cdecl operator delete(void* Memory) noexcept
{
    if (Memory)
        gMmst->MmFreePool(Memory);
}

void __cdecl operator delete(void* Memory, size_t)
{
    if (Memory)
        gMmst->MmFreePool(Memory);
}

void* __cdecl operator new[](size_t Size) noexcept(false)
{
    return operator new(Size);
}

void __cdecl operator delete[](void* Memory) noexcept
{
    return operator delete(Memory);
}

void __cdecl operator delete[](void* Memory, size_t Size)
{
    return operator delete(Memory, Size);
}