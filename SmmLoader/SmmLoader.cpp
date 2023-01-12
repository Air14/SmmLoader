#include "SmmCpu.h"
#include "SmmModulesManager.h"
#include "Commands.h"

extern "C"
{
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
}

EFI_STATUS EFIAPI SmiHandler(IN EFI_HANDLE DispatchHandle, IN CONST VOID* RegisterContext, IN OUT VOID* CommBuffer, IN OUT UINTN* CommBufferSize)
{
    if (!SmmCpu::Get().FindCurrentCpu())
        return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;

    const auto command = SmmCpu::Get().ReadRegister<Commands>(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RCX);
    const auto arg1 = SmmCpu::Get().ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RDX);
    const auto arg2 = SmmCpu::Get().ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RSI);
    const auto arg3 = SmmCpu::Get().ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RDI);
    if (!command.has_value() || !arg1.has_value() || !arg2.has_value() || !arg3.has_value())
        return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;

    DebugPrint(DEBUG_INFO, "command: 0x%llx\targ1: 0x%llx\targ2: 0x%llx\targ3: 0x%llx\n", static_cast<size_t>(*command), *arg1, *arg2, *arg3);

    static SmmModulesManager moduleManager{};
	switch (*command)
	{
	case Commands::LoadAndStartModule:
        moduleManager.LoadAndStartModule(*arg1, *arg2);
        break;
	case Commands::UnloadModule:
        moduleManager.UnloadModule(*arg1, *arg2);
        break;
    case Commands::LoadedModulesInfo:
        moduleManager.PrintLoadedModulesInfo();
        break;
	}

    return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
}

extern "C" EFI_STATUS UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE * SystemTable)
{
    if (!SmmCpu::Get().Initialize())
    {
        DebugPrint(DEBUG_INFO, "Couldn't initialize smm cpu\n");
        return EFI_LOAD_ERROR;
    }

    if (EFI_HANDLE dispatchHandle{}; EFI_ERROR(gMmst->MmiHandlerRegister(SmiHandler, NULL, &dispatchHandle)))
    {
        DebugPrint(DEBUG_INFO, "Couldn't register mmi handler\n");
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}