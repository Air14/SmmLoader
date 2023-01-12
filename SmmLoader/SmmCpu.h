#pragma once

#include <optional>
#undef _ASSERT

extern "C"
{
#include <Library/MmServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SmmCpu.h>
}

class SmmCpu
{
public:
    static SmmCpu& Get()
    {
        static SmmCpu smmCpu{};
        return smmCpu;
    }

    bool Initialize()
    {
        auto status = gMmst->MmLocateProtocol(&gEfiSmmCpuProtocolGuid, NULL, reinterpret_cast<void**>(&m_MmCpu));
        if (EFI_ERROR(status) || !m_MmCpu)
        {
            DebugPrint(DEBUG_INFO, "Could not locate SmmCpuProtocol\n");
            return false;
        }

        return true;
    }

    bool FindCurrentCpu()
    {
        for (size_t i{}; i < gMmst->NumberOfCpus; ++i)
        {
            if (const auto rax = ReadRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RAX, i);
                rax.has_value() && rax == m_BackdoorRaxValue && 
                WriteRegister(EFI_MM_SAVE_STATE_REGISTER::EFI_MM_SAVE_STATE_REGISTER_RAX, m_BackdoorRaxValue ^ 0xffffffffffffffffULL, i))
            {
                m_CurrentCpu = i;
                return true;
            }
        }

        return false;
    }

    template <typename T = size_t>
    std::optional<T> ReadRegister(EFI_MM_SAVE_STATE_REGISTER Register, size_t CurrentCpu = SmmCpu::Get().m_CurrentCpu) const
    {
        T value{};
        if (EFI_ERROR(m_MmCpu->ReadSaveState(m_MmCpu, sizeof(T), Register, CurrentCpu, &value)))
            return {};

        return value;
    }

    template <typename T = size_t>
    bool WriteRegister(EFI_MM_SAVE_STATE_REGISTER Register, T Value, size_t CurrentCpu = SmmCpu::Get().m_CurrentCpu) const
    {
        return !EFI_ERROR(m_MmCpu->WriteSaveState(m_MmCpu, sizeof(T), static_cast<EFI_MM_SAVE_STATE_REGISTER>(Register), CurrentCpu, &Value));
    }

private:
    SmmCpu() = default;

    EFI_MM_CPU_PROTOCOL* m_MmCpu;
    size_t m_CurrentCpu;
    const size_t m_BackdoorRaxValue = 0xC8E10B9DF93100A5ULL;
};