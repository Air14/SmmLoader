#pragma once
#include <memory>
#include <string_view>
#include <array>
#include <optional>

extern "C"
{
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Protocol/LoadedImage.h>
}

class SmmModulesManager
{
public:
	void LoadAndStartModule(size_t ImageVirtualAddress, size_t ImageSize);

	void UnloadModule(size_t DriverNameAddress, size_t DriverNameSize);

	void PrintLoadedModulesInfo() const;
private:
	std::optional<size_t> ReadModule(size_t ImageVirtualAddress, size_t ImageSize) const;

	std::optional<size_t> LoadModule(size_t PhysicalAddress);

	bool AddAndStartModule(size_t BaseAddress, size_t ImageSize, size_t EntryPoint, std::string_view Name);

	bool StartModule(size_t EntryPointAddress, EFI_HANDLE MmImageHandle);

	std::unique_ptr<char[]> ReadModuleName(size_t DriverNameAddress, size_t DriverNameSize) const;

	std::string_view FindModuleName(size_t EndAddressOfModule) const;

	bool IsModuleAlreadyLoaded(std::string_view Name) const;

	struct SmmModule
	{
		bool Loaded;
		std::string_view Name;
		size_t BaseAddress;
		size_t ImageSize;
		EFI_HANDLE MmImageHandle;
		EFI_LOADED_IMAGE_PROTOCOL LoadedImageProtocol;
	};

	std::array<SmmModule, 0x10> m_SmmModules{};
};
