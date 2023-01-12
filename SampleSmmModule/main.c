#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>

EFI_MM_SYSTEM_TABLE* gMmst;
EFI_MM_CPU_IO_PROTOCOL *mMmCpuIo;
EFI_HANDLE gDispatchHandle;

EFI_STATUS EFIAPI SmmUnload(EFI_HANDLE ImageHandle)
{
    DebugPrint(DEBUG_INFO, "Unloading SampleSmmModule\n");
    EFI_STATUS status = gMmst->MmiHandlerUnRegister(gDispatchHandle);
    if(EFI_ERROR(status))
	    DebugPrint(DEBUG_INFO, "Could not unregister handler 0x%llx\n", status);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI SmiHandler(IN EFI_HANDLE DispatchHandle, IN CONST VOID* RegisterContext, IN OUT VOID* CommBuffer, IN OUT UINTN* CommBufferSize)
{
    UINT8 commandNumber;
    mMmCpuIo->Io.Read(mMmCpuIo, MM_IO_UINT8, 0xb2, 1, &commandNumber);

    if(commandNumber == 0xde)
    {
	  DebugPrint(DEBUG_INFO, "SMI 0x%02x\n", commandNumber);
        commandNumber = 0xed;
        mMmCpuIo->Io.Write(mMmCpuIo, MM_IO_UINT8, 0xb2, 1, &commandNumber);
    }

    return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
}

EFI_STATUS EFIAPI SmmInitialize (IN EFI_HANDLE ImageHandle, IN EFI_MM_SYSTEM_TABLE* MmSystemTable)
{
    DebugPrint(DEBUG_INFO, "SampleSmmModule initializing\n");

    gMmst = MmSystemTable;
    EFI_STATUS status = gMmst->MmLocateProtocol(&gEfiMmCpuIoProtocolGuid, NULL, (VOID **)&mMmCpuIo);
    status = gMmst->MmiHandlerRegister(SmiHandler, NULL, &gDispatchHandle);
    return status;
}