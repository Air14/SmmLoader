/*
* Based on:
* https://github.com/tianocore/edk2/blob/master/OvmfPkg/Library/PlatformDebugLibIoPort/DebugLib.c
* https://github.com/tianocore/edk2/blob/master/OvmfPkg/Library/PlatformDebugLibIoPort/DebugLibDetect.c
*/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MmServicesTableLib.h>
#include <Protocol/SmmVariable.h>

#define MAX_DEBUG_MESSAGE_LENGTH  0xffe

STATIC VA_LIST gVaListNull;
STATIC BOOLEAN  gDebugNvVarChecked = FALSE;
STATIC BOOLEAN  gDebugNvVarFound = FALSE;
STATIC EFI_SMM_VARIABLE_PROTOCOL* gSmmVariableProtocol = NULL;
STATIC CHAR16* gDebugBufferVarName = L"Debug";
STATIC EFI_GUID gDebugBufferGuid = { 0x1f681694, 0xda81, 0x49c6, { 0xbd, 0xc2, 0x39, 0x42, 0xca, 0x6a, 0x29, 0x14 } };

typedef struct _DEBUG_BUFFER
{
    UINT16 Offset;
    UINT8 Data[MAX_DEBUG_MESSAGE_LENGTH];
}DEBUG_BUFFER;

VOID WriteToNvVar(VOID* Buffer, UINTN BufferSize)
{
    DEBUG_BUFFER debugBuffer;
    UINT32 attributes = 0;
    UINTN size = sizeof(DEBUG_BUFFER);
    EFI_STATUS status = gSmmVariableProtocol->SmmGetVariable(gDebugBufferVarName, &gDebugBufferGuid, &attributes, &size, &debugBuffer);
    if (EFI_ERROR(status))
        return;

    if (debugBuffer.Offset + BufferSize > MAX_DEBUG_MESSAGE_LENGTH)
        ZeroMem(&debugBuffer, sizeof(debugBuffer));

    CopyMem(debugBuffer.Data + debugBuffer.Offset, Buffer, BufferSize);
    debugBuffer.Offset += (UINT16)BufferSize + 1; // + 1 for zero terminator

    gSmmVariableProtocol->SmmSetVariable(gDebugBufferVarName, &gDebugBufferGuid,
        EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
        sizeof(debugBuffer), &debugBuffer);
}

BOOLEAN EFIAPI PlatformDebugLibNvVarDetect()
{
    EFI_STATUS status = gMmst->MmLocateProtocol(&gEfiSmmVariableProtocolGuid, NULL, (void**)&gSmmVariableProtocol);
    if (EFI_ERROR(status))
        return FALSE;

    DEBUG_BUFFER debugBuffer;
    UINT32 attributes = 0;
    UINTN size = sizeof(DEBUG_BUFFER);
    status = gSmmVariableProtocol->SmmGetVariable(gDebugBufferVarName, &gDebugBufferGuid, &attributes, &size, &debugBuffer);

    if (EFI_ERROR(status))
    {
        ZeroMem(&debugBuffer, sizeof(debugBuffer));

        status = gSmmVariableProtocol->SmmSetVariable(gDebugBufferVarName, &gDebugBufferGuid,
            EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
            sizeof(debugBuffer), &debugBuffer);

        if (EFI_ERROR(status))
            return FALSE;
    }

    return TRUE;
}

BOOLEAN EFIAPI PlatformDebugLibNvVarFound()
{
    if (!gDebugNvVarChecked) 
    {
        gDebugNvVarFound = PlatformDebugLibNvVarDetect();
        gDebugNvVarChecked = TRUE;
    }

    return gDebugNvVarFound;
}

VOID EFIAPI DebugPrint(IN UINTN ErrorLevel, IN CONST CHAR8* Format, ...)
{
    VA_LIST Marker;

    VA_START(Marker, Format);
    DebugVPrint(ErrorLevel, Format, Marker);
    VA_END(Marker);
}

VOID DebugPrintMarker(IN UINTN ErrorLevel, IN CONST CHAR8* Format, IN VA_LIST VaListMarker, IN BASE_LIST BaseListMarker)
{
    CHAR8 Buffer[MAX_DEBUG_MESSAGE_LENGTH];
    UINTN Length;

    if (!PlatformDebugLibNvVarFound())
        return;

    if (BaseListMarker == NULL) 
        Length = AsciiVSPrint(Buffer, sizeof(Buffer), Format, VaListMarker);
    else 
        Length = AsciiBSPrint(Buffer, sizeof(Buffer), Format, BaseListMarker);

    WriteToNvVar(Buffer, Length);
}

VOID EFIAPI DebugVPrint(IN UINTN ErrorLevel, IN CONST CHAR8* Format, IN VA_LIST VaListMarker)
{
    DebugPrintMarker(ErrorLevel, Format, VaListMarker, NULL);
}

VOID EFIAPI DebugBPrint(IN UINTN ErrorLevel, IN CONST CHAR8* Format, IN BASE_LIST BaseListMarker)
{
    DebugPrintMarker(ErrorLevel, Format, gVaListNull, BaseListMarker);
}

BOOLEAN EFIAPI DebugPrintLevelEnabled(IN CONST UINTN ErrorLevel)
{
    return TRUE;
}

BOOLEAN EFIAPI DebugAssertEnabled()
{
    return FALSE;
}

BOOLEAN EFIAPI DebugPrintEnabled()
{
    return TRUE;
}

VOID EFIAPI DebugAssert(IN CONST CHAR8* FileName, IN UINTN LineNumber, IN CONST CHAR8* Description)
{

}