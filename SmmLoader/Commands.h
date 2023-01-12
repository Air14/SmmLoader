#pragma once

enum class Commands : size_t
{
	LoadAndStartModule = 0xbadc0ded1e,
	UnloadModule,
	LoadedModulesInfo,
};