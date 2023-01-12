[Defines]
  PLATFORM_NAME                  = SmmLoader
  PLATFORM_GUID                  = 95EB41CA-42CD-43A3-AC28-319CB72C61E8
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/SmmLoader
  SUPPORTED_ARCHITECTURES        = X64|IA32
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = SmmLoader/SmmLoader.fdf

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLibRepStr/BaseMemoryLibRepStr.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf

  # Use if you want to get debug messages on real hardware
  DebugLib|SmmLoader/DebugLib/DebugLibNvVar.inf
  
  # Use if you want to get debug messages in qemu
  # DebugLib|OvmfPkg/Library/PlatformDebugLibIoPort/PlatformRomGDebugLibIoPortNocheck.inf

  # Use if you don't want get any debug messages
  # DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf

[Components]
  SmmLoader/SmmLoader/SmmLoader.inf {
    <LibraryClasses>
      UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
      MmServicesTableLib|MdePkg/Library/MmServicesTableLib/MmServicesTableLib.inf
  }

  SmmLoader/SampleSmmModule/SampleSmmModule.inf {
    <LibraryClasses>
      StandaloneMmDriverEntryPoint|MdePkg/Library/StandaloneMmDriverEntryPoint/StandaloneMmDriverEntryPoint.inf
      MmServicesTableLib|MdePkg/Library/StandaloneMmServicesTableLib/StandaloneMmServicesTableLib.inf
  }

[BuildOptions.Common]
  MSFT:*_*_*_CC_FLAGS = /Zc:wchar_t- /std:c++20  /D _HAS_EXCEPTIONS=0 /GL-
  MSFT:*_*_*_DLINK_FLAGS = /ALIGN:4096 /FILEALIGN:4096