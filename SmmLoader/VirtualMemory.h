#pragma once

bool ReadVirtualMemory(size_t VirtualAddress, size_t OutputAddress, size_t Size, size_t UserCr3);

bool MakeAddressExecutable(size_t Address);