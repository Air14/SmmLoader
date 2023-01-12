#include <utility>

extern "C"
{
#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
}

#include <intrin.h>
#include "ia32_compat.h"
#include "VirtualMemory.h"
#include "CpuUtils.h"

namespace
{

#define PAGE_SHIFT 12
#define PAGE_SIZE 0x1000

#define PFN_TO_PAGE(_val_) ((_val_) << PAGE_SHIFT)
#define PAGE_TO_PFN(_val_) ((_val_) >> PAGE_SHIFT)
#define PML4_ADDRESS(_val_) ((_val_) & 0xfffffffffffff000)
#define PML4_INDEX(_addr_) (((_addr_) >> 39) & 0x1ff)
#define PDPT_INDEX(_addr_) (((_addr_) >> 30) & 0x1ff)
#define PDE_INDEX(_addr_) (((_addr_) >> 21) & 0x1ff)
#define PTE_INDEX(_addr_) (((_addr_) >> 12) & 0x1ff)

#define PAGE_OFFSET_4K(_addr_) ((_addr_) & 0xfff)

#pragma section(".virt",read,write)
__declspec(align(4096)) char TempPage[0x1000]{};

bool RemapLargePageToSmaller(pde_64* PdEntry)
{
    pte_64* pteArray{};
    if (EFI_ERROR(gMmst->MmAllocatePages(EFI_ALLOCATE_TYPE::AllocateAnyPages, EFI_MEMORY_TYPE::EfiRuntimeServicesCode, 1,
        reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pteArray))))
    {
        DebugPrint(DEBUG_INFO, "Could not allocate page for array of pte\n");
        return false;
    }

    for (size_t i{}; i < 512; ++i)
    {
        pteArray[i].flags = PdEntry->flags;
        pteArray[i].page_frame_number += i;
    }

    ScopedWpOff wpOff{};
    PdEntry->page_frame_number = PAGE_TO_PFN(reinterpret_cast<size_t>(pteArray));
    PdEntry->large_page = 0;

    __writecr3(__readcr3());

    return true;
}

pte_64* GetTempPagePte()
{
    const auto cr3 = __readcr3();
    const auto address = reinterpret_cast<size_t>(TempPage);

    auto pml4Entry = reinterpret_cast<pml4e_64*>(PFN_TO_PAGE(PAGE_TO_PFN(cr3)) + PML4_INDEX(address) * sizeof(size_t));
    if (!pml4Entry->present)
        return nullptr;

    auto pdptEntry = reinterpret_cast<pdpte_64*>(PFN_TO_PAGE(pml4Entry->page_frame_number) + PDPT_INDEX(address) * sizeof(size_t));
    if (!pdptEntry->present || pdptEntry->large_page)
        return nullptr;

    auto pdEntry = reinterpret_cast<pde_64*>(PFN_TO_PAGE(pdptEntry->page_frame_number) + PDE_INDEX(address) * sizeof(size_t));
    if (!pdEntry->present)
        return nullptr;

    if (pdEntry->large_page && !RemapLargePageToSmaller(pdEntry))
        return nullptr;

    return reinterpret_cast<pte_64*>(PFN_TO_PAGE(pdEntry->page_frame_number) + PTE_INDEX(address) * sizeof(size_t));
}

void ChangePtePfn(pte_64* Pte, size_t NewPfn)
{
    ScopedWpOff wpOff{};
    Pte->page_frame_number = NewPfn;
    __writecr3(__readcr3());
}

bool ReadVirtualPage(size_t VirtualAddress, size_t UserCr3)
{
    auto tempPagePte = GetTempPagePte();
    if (!tempPagePte)
        return false;

    ChangePtePfn(tempPagePte, PAGE_TO_PFN(UserCr3));
    auto pml4Entry = reinterpret_cast<pml4e_64*>(reinterpret_cast<size_t>(TempPage) + PML4_INDEX(VirtualAddress) * sizeof(size_t));
    if (!pml4Entry->present)
        return false;

    ChangePtePfn(tempPagePte, pml4Entry->page_frame_number);
    auto pdptEntry = reinterpret_cast<pdpte_64*>(reinterpret_cast<size_t>(TempPage) + PDPT_INDEX(VirtualAddress) * sizeof(size_t));
    if (!pdptEntry->present || pdptEntry->large_page)
        return false;

    ChangePtePfn(tempPagePte, pdptEntry->page_frame_number);
    auto pdEntry = reinterpret_cast<pde_64*>(reinterpret_cast<size_t>(TempPage) + PDE_INDEX(VirtualAddress) * sizeof(size_t));
    if (!pdEntry->present)
        return false;

    if (pdEntry->large_page)
    {
        ChangePtePfn(tempPagePte, pdEntry->page_frame_number + PTE_INDEX(VirtualAddress));
        return true;
    }
    else
    {
        ChangePtePfn(tempPagePte, pdEntry->page_frame_number);
        auto ptEntry = reinterpret_cast<pte_64*>(reinterpret_cast<size_t>(TempPage) + PTE_INDEX(VirtualAddress) * sizeof(size_t));
        if (!ptEntry->present)
            return false;

        ChangePtePfn(tempPagePte, ptEntry->page_frame_number);

        return true;
    }
}

}

bool ReadVirtualMemory(size_t VirtualAddress, size_t OutputAddress, size_t Size, size_t UserCr3)
{
    size_t readBytes{};
    while (Size > 0)
    {
        if (!ReadVirtualPage(PML4_ADDRESS(VirtualAddress), UserCr3))
        {
            DebugPrint(DEBUG_INFO, "Could not read page\n");
            return false;
        }

        const auto offset = VirtualAddress % PAGE_SIZE;
        const auto bytesToCopy = std::min(Size, PAGE_SIZE - offset);
        CopyMem(reinterpret_cast<void*>(OutputAddress), TempPage + offset, bytesToCopy);
        readBytes += bytesToCopy;
        VirtualAddress += bytesToCopy;
        OutputAddress += bytesToCopy;
        Size -= bytesToCopy;
    }
    return true;
}

bool MakeAddressExecutable(size_t Address)
{
    const auto cr3 = __readcr3();

    auto pml4Entry = reinterpret_cast<pml4e_64*>(PFN_TO_PAGE(PAGE_TO_PFN(cr3)) + PML4_INDEX(Address) * sizeof(size_t));
    if (!pml4Entry->present)
        return false;

    auto pdptEntry = reinterpret_cast<pdpte_64*>(PFN_TO_PAGE(pml4Entry->page_frame_number) + PDPT_INDEX(Address) * sizeof(size_t));
    if (!pdptEntry->present)
        return false;

    if (pdptEntry->large_page)
    {
        ScopedWpOff wpOff{};
        pdptEntry->execute_disable = false;
        return true;
    }

    auto pdEntry = reinterpret_cast<pde_64*>(PFN_TO_PAGE(pdptEntry->page_frame_number) + PDE_INDEX(Address) * sizeof(size_t));
    if (!pdEntry->present)
        return false;

    if (pdEntry->large_page)
    {
        ScopedWpOff wpOff{};
        pdEntry->execute_disable = false;
        return true;
    }

    auto ptEntry = reinterpret_cast<pte_64*>(PFN_TO_PAGE(pdEntry->page_frame_number) + PTE_INDEX(Address) * sizeof(size_t));

    ScopedWpOff wpOff{};
    ptEntry->execute_disable = false;
    return true;
}