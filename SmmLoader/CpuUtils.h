#pragma once
#include <intrin.h>
#include "ia32_compat.h"

class ScopedWpOff
{
public:
	ScopedWpOff() 
	{
		cr0 cr0{}; cr0.flags = __readcr0();
		m_WriteProtectEnabled = cr0.write_protect;
		cr0.write_protect = 0;

		__writecr0(cr0.flags);
	}

	~ScopedWpOff()
	{
		cr0 cr0{}; cr0.flags = __readcr0();
		cr0.write_protect = m_WriteProtectEnabled;

		__writecr0(cr0.flags);
	}

private:
	bool m_WriteProtectEnabled{};
};