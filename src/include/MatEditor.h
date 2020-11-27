#pragma once
#ifndef MAT_EDITOR_H
#define MAT_EDITOR_H

#include <Windows.h>
#include <vector>
#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "imgui.h"


namespace MEditor {
	void Initialize();
	void RefreshMatInstConsts();
	void FilterPathObjVecs(const char* toFind, const std::vector<char*>* vPathNames, const std::vector<UObject*>* vObjects, std::vector<char*>* vFilterdPathNames, std::vector<UObject*>* vFilteredObjects);
	void GetTextureParameters();
	void GetVectorParameters();
	void GetScalarParameters();
	int ExportToFile(const std::string* path);
	bool TPSSkinFix(UObject* Caller, UFunction* Func, FStruct* params);
	void HookInits();
	HRESULT __stdcall  Hooked_EndScene(IDirect3DDevice9* pDevice);
	HRESULT __stdcall  Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

	extern CHookManager* MatHookManager;
	extern char cFilterMats[64];
	extern char cFilterMatsParents[64];
	extern char cFilterTexture2D[64];
	extern char cExportedFileName[128];
	extern bool bWantsEditorOpen;
	extern bool bWantsTexture2DWindow;
	extern std::vector<UObject*> vMaterialInstanceConstants;
	extern std::vector<char*> vMaterialInstanceConstantPathNames;
	extern std::vector<char*> vMaterialsPathNameFiltered;
	extern std::vector<UObject*> vMaterialsFiltered;
	extern char* cSelectedMaterial;
	extern UMaterialInstanceConstant* SelectedMaterial;
	extern UObject* SelectedMaterialParent;
	extern char* cSelectedMaterialParent;
	extern std::vector<const char*> vTextureParameters;
	extern std::vector<UObject*> vTexture2D;
	extern std::vector<char*> vTexture2DPathNames;
	extern std::vector<UObject*> vTexture2DFiltered;
	extern std::vector<char*> vTexture2DPathNamesFiltered;
	extern std::unordered_map<const char*, std::tuple<const char*, UObject*>> mTextureParametersSelected;
	extern std::vector<const char*> vVectorParameters;
	extern std::unordered_map<const char*, ImVec4> mVectorParametersSelected;
	extern std::vector<const char*> vScalarParameters;



}


#endif