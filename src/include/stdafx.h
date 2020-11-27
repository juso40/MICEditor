#pragma once

#define VERSION_MAJOR	0
#define VERSION_MINOR	7
#define VERSION_PATCH	8

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN


#include <windows.h>
#include <cstdio>
#include <iostream>
#include <ShlObj.h>
#include <string>
#include <map>
#include <regex>
#include <typeinfo>

#include <Shlwapi.h>
#pragma comment (lib, "Shlwapi.lib")

namespace UnrealSDK
{
	extern void* pGObjects;
	extern void* pGNames;
	extern void* pGObjHash;
	extern void* pGCRCTable;
	extern void* pNameHash;
}

#include "gamedefines.h"

#include "logging.h"

#include "UnrealEngine/Core/Core_structs.h"
#include "UnrealEngine/Core/Core_f_structs.h"
#include "UnrealEngine/Core/Core_classes.h"

#include "UnrealEngine/Engine/Engine_structs.h"
#include "UnrealEngine/Engine/Engine_f_structs.h"
#include "UnrealEngine/Engine/Engine_classes.h"

#include "TypeMap.h"

#include "Games.h"
