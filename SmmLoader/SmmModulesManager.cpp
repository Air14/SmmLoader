#include "SmmModulesManager.h"
#include "SmmCpu.h"
#include "VirtualMemory.h"

extern "C"
{
#include <Library/MmServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/PeCoffLib.h>
#include <Protocol/LoadedImage.h>
}

void SmmModulesManager::LoadAndStartModule(size_t ImageVirtualAddress, size_t ImageSize)
{
	const auto moduleBaseAddress = ReadModule(ImageVirtualAddress, ImageSize);
	if (!moduleBaseAddress.has_value())
		return;

	const auto moduleName = FindModuleName(*moduleBaseAddress + ImageSize);
	if (IsModuleAlreadyLoaded(moduleName))
	{
		DebugPrint(DEBUG_INFO, "Module %a is already loaded\n", moduleName.data());
		gMmst->MmFreePages(*moduleBaseAddress, EFI_SIZE_TO_PAGES(ImageSize));
		return;
	}

	DebugPrint(DEBUG_INFO, "Loading module %a\n", moduleName.data());
	const auto entryPoint = LoadModule(*moduleBaseAddress);
	if (!entryPoint.has_value())
	{
		gMmst->MmFreePages(*moduleBaseAddress, EFI_SIZE_TO_PAGES(ImageSize));
		return;
	}

	if (!AddAndStartModule(*moduleBaseAddress, ImageSize, *entryPoint, moduleName))
		gMmst->MmFreePages(*moduleBaseAddress, EFI_SIZE_TO_PAGES(ImageSize));
}

void SmmModulesManager::UnloadModule(size_t DriverNameAddress, size_t DriverNameSize)
{
	const auto moduleName = ReadModuleName(DriverNameAddress, DriverNameSize);
	if (!moduleName.get())
		return;

	for (auto& smmModule : m_SmmModules)
	{
		if (smmModule.Name != moduleName.get())
			continue;

		DebugPrint(DEBUG_INFO, "Unloading driver %a\n", smmModule.Name.data());
		smmModule.LoadedImageProtocol.Unload(smmModule.MmImageHandle);
		DebugPrint(DEBUG_INFO, "Driver unloaded\n");

		gMmst->MmUninstallProtocolInterface(smmModule.MmImageHandle, &gEfiLoadedImageProtocolGuid, &smmModule.LoadedImageProtocol);
		gMmst->MmFreePages(smmModule.BaseAddress, EFI_SIZE_TO_PAGES(smmModule.ImageSize));
		ZeroMem(&smmModule, sizeof(smmModule));
	}
}

void SmmModulesManager::PrintLoadedModulesInfo() const
{
	DebugPrint(DEBUG_INFO, "Module name\tModule base address\tModule size\n");
	for (const auto& loadedModule : m_SmmModules)
	{
		if (!loadedModule.Loaded)
			continue;

		DebugPrint(DEBUG_INFO, "%a\t0x%llx\t\t0x%llx\n", loadedModule.Name.data(), loadedModule.BaseAddress, loadedModule.ImageSize);
	}
}

std::optional<size_t> SmmModulesManager::ReadModule(size_t ImageVirtualAddress, size_t ImageSize) const
{
	size_t physicalAddress{};
	if (EFI_ERROR(gMmst->MmAllocatePages(EFI_ALLOCATE_TYPE::AllocateAnyPages, EFI_MEMORY_TYPE::EfiRuntimeServicesCode, EFI_SIZE_TO_PAGES(ImageSize), &physicalAddress)))
	{
		DebugPrint(DEBUG_INFO, "Could not allocate 0x%llx pages\n", EFI_SIZE_TO_PAGES(ImageSize));
		return {};
	}
	ZeroMem(reinterpret_cast<void*>(physicalAddress), EFI_SIZE_TO_PAGES(ImageSize) * EFI_PAGE_SIZE);

	const auto cr3 = SmmCpu::Get().ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_CR3);
	if (!cr3.has_value())
	{
		DebugPrint(DEBUG_INFO, "Could not get user cr3\n");
		gMmst->MmFreePages(physicalAddress, EFI_SIZE_TO_PAGES(ImageSize));
		return {};
	}

	if (!ReadVirtualMemory(ImageVirtualAddress, physicalAddress, ImageSize, *cr3))
	{
		DebugPrint(DEBUG_INFO, "Could not read VirtualAddress: 0x%llx\n", ImageVirtualAddress);
		gMmst->MmFreePages(physicalAddress, EFI_SIZE_TO_PAGES(ImageSize));
		return {};
	}

	return physicalAddress;
}

std::optional<size_t> SmmModulesManager::LoadModule(size_t PhysicalAddress)
{
	PE_COFF_LOADER_IMAGE_CONTEXT imageContext{};
	imageContext.Handle = reinterpret_cast<void*>(PhysicalAddress);
	imageContext.ImageRead = PeCoffLoaderImageReadFromMemory;

	auto status = PeCoffLoaderGetImageInfo(&imageContext);
	if (EFI_ERROR(status))
	{
		DebugPrint(DEBUG_INFO, "Could not get image info status: 0x%llx\n", status);
		return {};
	}
	imageContext.ImageAddress = PhysicalAddress;

	status = PeCoffLoaderLoadImage(&imageContext);
	if (EFI_ERROR(status))
	{
		DebugPrint(DEBUG_INFO, "Could not load image, status: 0x%llx\timage status: 0x%llx\n", status, imageContext.ImageError);
		return {};
	}

	status = PeCoffLoaderRelocateImage(&imageContext);
	if (EFI_ERROR(status))
	{
		DebugPrint(DEBUG_INFO, "Could not relocate image status 0x%llx\n", status);
		return {};
	}

	for (auto address = imageContext.ImageAddress + 0x1000; address < imageContext.ImageAddress + imageContext.ImageSize; address += EFI_PAGE_SIZE)
		MakeAddressExecutable(address);

	InvalidateInstructionCacheRange(reinterpret_cast<void*>(imageContext.ImageAddress), imageContext.ImageSize);
	return imageContext.EntryPoint;
}

bool SmmModulesManager::AddAndStartModule(size_t BaseAddress, size_t ImageSize, size_t EntryPoint, std::string_view Name)
{
	for (auto& smmModule : m_SmmModules)
	{
		if (smmModule.Loaded)
			continue;

		smmModule.Name = Name;
		smmModule.BaseAddress = BaseAddress;
		smmModule.ImageSize = ImageSize;
		const auto status = gMmst->MmInstallProtocolInterface(&smmModule.MmImageHandle, &gEfiLoadedImageProtocolGuid, EFI_NATIVE_INTERFACE, &smmModule.LoadedImageProtocol);
		if (EFI_ERROR(status))
		{
			DebugPrint(DEBUG_INFO, "Could not install LoadedImageProtocol 0x%llx\n", status);
			return false;
		}

		if (!StartModule(EntryPoint, smmModule.MmImageHandle))
		{
			gMmst->MmUninstallProtocolInterface(smmModule.MmImageHandle, &gEfiLoadedImageProtocolGuid, &smmModule.LoadedImageProtocol);
			return false;
		}

		return smmModule.Loaded = true;
	}

	DebugPrint(DEBUG_INFO, "There are no more free modules\n");
	return false;
}

bool SmmModulesManager::StartModule(size_t EntryPointAddress, EFI_HANDLE MmImageHandle)
{
	// Hdr revision needs to be equal 0x10032 to load MmStandaloneModule
	const auto oldRevision = gMmst->Hdr.Revision;
	const auto newRevision = gMmst->Hdr.Revision < 0x10032 ? 0x10032 : gMmst->Hdr.Revision;
	gMmst->Hdr.Revision = newRevision;

	DebugPrint(DEBUG_INFO, "Calling module entry point\n");
	using MmEntryPoint_t = EFI_STATUS(EFIAPI*)(EFI_HANDLE ImageHandle, EFI_MM_SYSTEM_TABLE* MmSystemTable);
	const auto status = reinterpret_cast<MmEntryPoint_t>(EntryPointAddress)(MmImageHandle, gMmst);
	DebugPrint(DEBUG_INFO, "Module returned status: 0x%llx\n", status);

	gMmst->Hdr.Revision = oldRevision;

	return !EFI_ERROR(status);
}

std::unique_ptr<char[]> SmmModulesManager::ReadModuleName(size_t DriverNameAddress, size_t DriverNameSize) const
{
	auto nameBuffer = std::make_unique<char[]>(DriverNameSize);
	const auto cr3 = SmmCpu::Get().ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_CR3);
	if (!cr3.has_value())
	{
		DebugPrint(DEBUG_INFO, "Could not get user cr3\n");
		return nullptr;
	}

	if (!ReadVirtualMemory(DriverNameAddress, reinterpret_cast<size_t>(nameBuffer.get()), DriverNameSize, *cr3))
	{
		DebugPrint(DEBUG_INFO, "Could not read VirtualAddress: 0x%llx\n", DriverNameAddress);
		return nullptr;
	}

	return nameBuffer;
}

// Module name must be located at the end of image and be surronded by 0x00 bytes
std::string_view SmmModulesManager::FindModuleName(size_t EndAddressOfModule) const
{
	for (auto i = reinterpret_cast<const char*>(EndAddressOfModule) - 2;; --i)
		if (*i == 0)
			return std::string_view(++i);
}

bool SmmModulesManager::IsModuleAlreadyLoaded(std::string_view Name) const
{
	for (auto& module : m_SmmModules)
		if (module.Loaded && module.Name == Name)
			return true;

	return false;
}