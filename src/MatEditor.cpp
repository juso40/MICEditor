#include "stdafx.h"
#include "MatEditor.h"
#include <fstream>
#include <iostream>
#include "CHookManager.h"
#include "Exceptions.h"
#include "Logging.h"
#include "detours/detours.h"
#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


typedef HRESULT(__stdcall* f_EndScene)(IDirect3DDevice9* pDevice); // Our function prototype 
f_EndScene onEndScene; // Original Endscene
typedef HRESULT(__stdcall* f_Reset)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters); // Our function prototype 
f_Reset onReset; // Original Reset



typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
WNDPROC oWndProc;
static HWND window;


bool HookDisableInspectionInput(UObject* Caller, UFunction* Func, FStruct* params) {
	return false;
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) {
	case WM_KEYUP:
		if (wParam == VK_INSERT) {

			if (!MEditor::bWantsEditorOpen) {
				MEditor::MatHookManager->Register("WillowGame.ItemInspectionGFxMovie.HandleInputKey", "HookInspectionInput", HookDisableInspectionInput);
			}
			else {
				MEditor::MatHookManager->Remove("WillowGame.ItemInspectionGFxMovie.HandleInputKey", "HookInspectionInput");
			}
			MEditor::bWantsEditorOpen = !MEditor::bWantsEditorOpen;
		}
	}

	if (MEditor::bWantsEditorOpen) {
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}



BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
	DWORD wndProcId;
	GetWindowThreadProcessId(handle, &wndProcId);

	if (GetCurrentProcessId() != wndProcId)
		return TRUE; // skip to next window

	window = handle;
	return FALSE; // window found abort search
}

HWND GetProcessWindow() {
	window = NULL;
	EnumWindows(EnumWindowsCallback, NULL);
	return window;
}

bool GetD3D9Device(void** pTable, size_t Size) {
	if (!pTable)
		return false;

	IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	if (!pD3D)
		return false;

	IDirect3DDevice9* pDummyDevice = NULL;

	// options to create dummy device
	D3DPRESENT_PARAMETERS d3dpp = {};
	d3dpp.Windowed = false;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = GetProcessWindow();

	HRESULT dummyDeviceCreated = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);

	if (dummyDeviceCreated != S_OK) {
		// may fail in windowed fullscreen mode, trying again with windowed mode
		d3dpp.Windowed = !d3dpp.Windowed;

		dummyDeviceCreated = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);

		if (dummyDeviceCreated != S_OK) {
			pD3D->Release();
			return false;
		}
	}

	memcpy(pTable, *reinterpret_cast<void***>(pDummyDevice), Size);

	pDummyDevice->Release();
	pD3D->Release();
	return true;
}


size_t split(const std::string& txt, std::vector<std::string>& strs, char ch) {
	size_t pos = txt.find(ch);
	size_t initialPos = 0;
	strs.clear();

	// Decompose statement
	while (pos != std::string::npos) {
		strs.push_back(txt.substr(initialPos, pos - initialPos));
		initialPos = pos + 1;

		pos = txt.find(ch, initialPos);
	}

	// Add the last one
	strs.push_back(txt.substr(initialPos, min(pos, txt.size()) - initialPos + 1));

	return strs.size();
}

std::string GetCurrPath() {
	char path[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, path);
	return std::string(path);
}



namespace MEditor {
	// The current solution is probably suboptimal
	// At some point I should switch to a Class instead of abusing this namespace
	CHookManager* MatHookManager;
	char cFilterMats[64];
	char cFilterMatsParents[64];
	char cFilterTexture2D[64];
	char cExportedFileName[128];
	bool bWantsEditorOpen;
	bool bWantsTexture2DWindow;
	std::vector<UObject*> vMaterialInstanceConstants;
	std::vector<char*> vMaterialInstanceConstantPathNames;
	std::vector<char*> vMaterialsPathNameFiltered;
	std::vector<UObject*> vMaterialsFiltered;
	char* cSelectedMaterial = NULL;
	UMaterialInstanceConstant* SelectedMaterial = nullptr;
	UObject* SelectedMaterialParent = nullptr;
	char* cSelectedMaterialParent = NULL;
	std::vector<const char*> vTextureParameters;
	std::vector<UObject*> vTexture2D;
	std::vector<char*> vTexture2DPathNames;
	std::vector<UObject*> vTexture2DFiltered;
	std::vector<char*> vTexture2DPathNamesFiltered;
	std::unordered_map<const char*, std::tuple<const char*, UObject*>> mTextureParametersSelected;
	std::vector<const char*> vVectorParameters;
	std::unordered_map<const char*, ImVec4> mVectorParametersSelected;
	std::vector<const char*> vScalarParameters;
	std::unordered_map<const char*, float> mScalarParametersSelected;
	std::map<std::string, std::vector<UObject*>> mMaterialToExpressions;

	FLinearColor fCol;
	bool imguiInit = true;



	void Initialize() {
		bWantsEditorOpen = false;
		bWantsTexture2DWindow = false;

		void* d3d9Device[119];
		if (GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
			//hook stuff using the dumped addresses

			Logging::Log(Util::Format("Got the d3d9Device vTable at: 0x%x", d3d9Device).c_str());
			onEndScene = reinterpret_cast<f_EndScene>(d3d9Device[42]);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(reinterpret_cast<PVOID*>(&onEndScene), Hooked_EndScene);
			DetourTransactionCommit();


			onReset = reinterpret_cast<f_Reset>(d3d9Device[16]);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(reinterpret_cast<PVOID*>(&onReset), Hooked_Reset);
			DetourTransactionCommit();

			return;
		}
		Logging::Log("Did not find any d3d9Device vTable! Will try again in 50ms.");
		Sleep(50);
		Initialize();
	}

	void RefreshMatInstConsts() {
		vMaterialInstanceConstants = UObject::FindAll(const_cast<char*>("MaterialInstanceConstant"));
		vTexture2D = UObject::FindAll(const_cast<char*>("Texture2D"));

		vMaterialInstanceConstantPathNames.clear();
		for (auto v : vMaterialInstanceConstants) {
			vMaterialInstanceConstantPathNames.push_back(UObject::PathName(v).AsString());
		}

		vTexture2DPathNames.clear();
		for (auto v : vTexture2D) {
			vTexture2DPathNames.push_back(UObject::PathName(v).AsString());
		}
	}


	void FilterPathObjVecs(const char* toFind, const std::vector<char*>* vPathNames, const std::vector<UObject*>* vObjects, std::vector<char*>* vFilterdPathNames, std::vector<UObject*>* vFilteredObjects) {
		vFilterdPathNames->clear();
		vFilteredObjects->clear();
		for (int i = 0; i < vPathNames->size(); ++i) {
			if (StrStrIA(vPathNames->at(i), toFind) != NULL) {
				vFilterdPathNames->push_back(vPathNames->at(i));
				vFilteredObjects->push_back(vObjects->at(i));
			}
		}
	}


	UMaterial* GetParentMat() {
		UObject* parent = SelectedMaterial;
		while (!parent->IsA(UObject::FindClass("Material"))) {
			parent = reinterpret_cast<UMaterialInstanceConstant*>(parent)->GetMaterial();
		}
		return reinterpret_cast<UMaterial*>(parent);
	}



	void GetTextureParameters() {
		if (SelectedMaterial == nullptr) return;
		UMaterial* parentMat = GetParentMat();

		vTextureParameters.clear();
		std::string parentName = parentMat->PathName(parentMat).AsString();
		// cache our parents Expressions 
		if (mMaterialToExpressions.find(parentName) == mMaterialToExpressions.end()) {
			mMaterialToExpressions[parentName] = parentMat->FindObjectsContaining(parentName);
			auto vec = mMaterialToExpressions[parentName];
			vec.erase(vec.begin() + 0); // remove the ParentObject itself
		}


		for (const auto& expression : mMaterialToExpressions[parentName]) {
			if (expression->IsA(UObject::FindClass("MaterialExpressionTextureSampleParameter"))) {
				UMaterialExpressionTextureSampleParameter* currExp = reinterpret_cast<UMaterialExpressionTextureSampleParameter*>(expression);
				if (currExp == nullptr) continue;
				// only add new parameternames if its not already present
				if (std::find(vTextureParameters.begin(), vTextureParameters.end(), currExp->ParameterName.GetName()) == vTextureParameters.end())
					vTextureParameters.push_back(currExp->ParameterName.GetName());
			}
		}
	}

	// Definitely will need a more dynamic way of retrieving the fitting parameterNames
	void GetVectorParameters() {
		fCol = UObject::MakeLinearColor(1.0, 1.0, 1.0, 1.0);
		if (SelectedMaterial == nullptr) return;
		UMaterial* parentMat = GetParentMat();

		vVectorParameters.clear();
		std::string parentName = parentMat->PathName(parentMat).AsString();
		// cache our parents Expressions 
		if (mMaterialToExpressions.find(parentName) == mMaterialToExpressions.end()) {
			mMaterialToExpressions[parentName] = parentMat->FindObjectsContaining(parentName);
			auto vec = mMaterialToExpressions[parentName];
			vec.erase(vec.begin() + 0); // remove the ParentObject itself
		}


		for (const auto& expression : mMaterialToExpressions[parentName]) {
			if (expression->IsA(UObject::FindClass("MaterialExpressionVectorParameter"))) {
				UMaterialExpressionVectorParameter* currExp = reinterpret_cast<UMaterialExpressionVectorParameter*>(expression);
				if (currExp == nullptr) continue;
				// only add new parameternames if its not already present
				if (std::find(vVectorParameters.begin(), vVectorParameters.end(), currExp->ParameterName.GetName()) == vVectorParameters.end())
					vVectorParameters.push_back(currExp->ParameterName.GetName());
			}
		}

		// Fill our VecotrParam map with 1.0f initialized floats each time the user changes the MatInstConst or it's Parent
		mVectorParametersSelected.clear();
		for (const char* param : vVectorParameters) {
			SelectedMaterial->GetVectorParameterValue(FName(param), &fCol);
			mVectorParametersSelected[param] = ImVec4(fCol.R, fCol.G, fCol.B, fCol.A);
		}
	}

	void GetScalarParameters() {
		if (SelectedMaterial == nullptr) return;
		UMaterial* parentMat = GetParentMat();

		vScalarParameters.clear();
		std::string parentName = parentMat->PathName(parentMat).AsString();
		// cache our parents Expressions 
		if (mMaterialToExpressions.find(parentName) == mMaterialToExpressions.end()) {
			mMaterialToExpressions[parentName] = parentMat->FindObjectsContaining(parentName);
			auto vec = mMaterialToExpressions[parentName];
			vec.erase(vec.begin() + 0); // remove the ParentObject itself
		}


		for (const auto& expression : mMaterialToExpressions[parentName]) {
			if (expression->IsA(UObject::FindClass("MaterialExpressionScalarParameter"))) {
				UMaterialExpressionScalarParameter* currExp = reinterpret_cast<UMaterialExpressionScalarParameter*>(expression);
				if (currExp == nullptr) continue;
				// only add new parameternames if its not already present
				if (std::find(vScalarParameters.begin(), vScalarParameters.end(), currExp->ParameterName.GetName()) == vScalarParameters.end())
					vScalarParameters.push_back(currExp->ParameterName.GetName());
			}
		}

		mScalarParametersSelected.clear();
		float val;
		for (const char* param : vScalarParameters) {
			SelectedMaterial->GetScalarParameterValue(FName(param), &val);
			mScalarParametersSelected[param] = val;
		}
	}

	int ExportToFile(const std::string* path) {
		if (SelectedMaterial == nullptr) return 1;  // If no MaterialInstanceConstant was chosen we dont have anything to export :/
		std::ofstream file;
		try {
			file.open(*path);
			if (SelectedMaterialParent != nullptr) {
				file << "set " << UObject::PathName(SelectedMaterial).AsString() << " Parent MaterialInstanceConstant'" << UObject::PathName(SelectedMaterialParent).AsString() << "'\n";
			}

			if (!mTextureParametersSelected.empty()) {
				file << "set " << UObject::PathName(SelectedMaterial).AsString() << " TextureParameterValues (";

				for (const auto& [param, vals] : mTextureParametersSelected) {
					file << "(ParameterName=\"" << param << "\",ParameterValue=Texture2D'" << std::get<0>(vals) << "',ExpressionGUID=(A=0,B=0,C=0,D=0)),";
				}
				file.seekp((long)file.tellp() - 1);
				file << ")\n";
			}

			// This if should not really be needed, the map will always have at least the vanilla values :shrug:
			if (!mVectorParametersSelected.empty()) {
				file << "set " << UObject::PathName(SelectedMaterial).AsString() << " VectorParameterValues (";
				for (const auto& [param, vals] : mVectorParametersSelected) {
					file << "(ParameterName=\"" << param << "\",ParameterValue=(R=" << vals.x << ",G=" << vals.y << ",B=" << vals.z << ",A=" << vals.w << "),ExpressionGUID=(A=0,B=0,C=0,D=0)),";
				}
				file.seekp((long)file.tellp() - 1);
				file << ")\n";
			}

			if (!mScalarParametersSelected.empty()) {
				file << "set " << UObject::PathName(SelectedMaterial).AsString() << " ScalarParameterValues (";
				for (const auto& [param, vals] : mScalarParametersSelected) {
					file << "(ParameterName=\"" << param << "\",ParameterValue=" << vals << ",ExpressionGUID=(A=0,B=0,C=0,D=0)),";
				}
				file.seekp((long)file.tellp() - 1);
				file << ")\n";
			}
			file.close();
		}
		catch (const std::exception& e) {
			Logging::Log(Util::Format("[Error] An unexpected exception occured while trying to write a file:\n%s", e.what()).c_str());
		}
		return 0;
	}

	HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice) // Our hooked endscene
	{

		if (imguiInit) {
			imguiInit = false;
			ImGui::CreateContext();
			//ImGuiIO& io = ImGui::GetIO();
			ImGui_ImplWin32_Init(GetProcessWindow());
			ImGui_ImplDX9_Init(pDevice);
			ImGui::SetNextWindowSize(ImVec2(640, 440), ImGuiCond_FirstUseEver);
			oWndProc = (WNDPROC)SetWindowLongPtr(GetProcessWindow(), GWL_WNDPROC, (LONG_PTR)WndProc);
		}

		if (bWantsEditorOpen) {
			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Material Editor");

			// This list will be used multiple times, so it can stay on top
			if (ImGui::Button("Update MaterialInstanceConstant List")) RefreshMatInstConsts();


			// Used to choose what MatInstConst the user wants to edit
			if (ImGui::CollapsingHeader(Util::Format("MaterialInstanceConstant: %s", cSelectedMaterial).c_str())) {
				if (ImGui::InputText("Obj Filter", cFilterMats, IM_ARRAYSIZE(cFilterMats))) {
					FilterPathObjVecs(cFilterMats, &vMaterialInstanceConstantPathNames, &vMaterialInstanceConstants, &vMaterialsPathNameFiltered, &vMaterialsFiltered);
				}

				ImGui::Text(Util::Format("Selected: %s", cSelectedMaterial).c_str());
				if (ImGui::ListBoxHeader("MaterialInstanceConstant")) {
					for (int i = 0; i < vMaterialsPathNameFiltered.size(); ++i) {
						if (ImGui::Selectable(vMaterialsPathNameFiltered[i], false)) {
							cSelectedMaterial = vMaterialsPathNameFiltered[i];
							SelectedMaterial = reinterpret_cast<UMaterialInstanceConstant*>(vMaterialsFiltered[i]);
							SelectedMaterialParent = nullptr;
							cSelectedMaterialParent = nullptr;
							// Now that we have chosen a MaterialInstanceConstant we want to go back to its Material Parent and lookup all its allowed TexutreParameters
							GetTextureParameters();
							GetVectorParameters();
							GetScalarParameters();
						}
					}
					ImGui::ListBoxFooter();
				}
			}

			// This will be used to change the .Parent obj
			if (ImGui::CollapsingHeader(Util::Format("Parent: %s", cSelectedMaterialParent).c_str())) {
				if (SelectedMaterial == nullptr) {
					ImGui::Text("Please chose a MaterialInstanceConstant before looking for a new parent");
				}
				else {
					if (ImGui::InputText("Parent Filter", cFilterMatsParents, IM_ARRAYSIZE(cFilterMatsParents))) {
						FilterPathObjVecs(cFilterMatsParents, &vMaterialInstanceConstantPathNames, &vMaterialInstanceConstants, &vMaterialsPathNameFiltered, &vMaterialsFiltered);
					}

					ImGui::Text(Util::Format("Selected: %s", cSelectedMaterial).c_str());
					if (ImGui::ListBoxHeader("MaterialInstanceConstant Parent")) {
						for (int i = 0; i < vMaterialsPathNameFiltered.size(); ++i) {
							if (ImGui::Selectable(vMaterialsPathNameFiltered[i], false)) {
								if (vMaterialsFiltered[i] != SelectedMaterial) {
									cSelectedMaterialParent = vMaterialsPathNameFiltered[i];
									SelectedMaterialParent = vMaterialsFiltered[i];
									SelectedMaterial->SetParent(reinterpret_cast<UMaterialInterface*>(vMaterialsFiltered[i]));
									GetTextureParameters();
									GetVectorParameters();
									GetScalarParameters();
								}
							}
						}
						ImGui::ListBoxFooter();
					}
				}
			}

			// This will open a new ImGui Window to select a new Texture2D, only one instance of this window can be open at a time.
			static const char* cTexture2DWindowFor;
			if (bWantsTexture2DWindow) {
				ImGui::SetNextWindowSize(ImVec2(640, 440), ImGuiCond_FirstUseEver);
				ImGui::Begin(cTexture2DWindowFor, &bWantsTexture2DWindow);
				if (ImGui::InputText("Texture2D Filter", cFilterTexture2D, IM_ARRAYSIZE(cFilterTexture2D))) {
					FilterPathObjVecs(cFilterTexture2D, &vTexture2DPathNames, &vTexture2D, &vTexture2DPathNamesFiltered, &vTexture2DFiltered);
				}
				ImGui::ListBoxHeader("Texture2D", ImVec2(-1, -20));
				for (int i = 0; i < vTexture2DPathNamesFiltered.size(); ++i) {
					if (ImGui::Selectable(vTexture2DPathNamesFiltered[i], false)) {
						mTextureParametersSelected[cTexture2DWindowFor] = std::make_tuple(vTexture2DPathNamesFiltered[i], vTexture2DFiltered[i]);
						SelectedMaterial->SetTextureParameterValue(FName(cTexture2DWindowFor), reinterpret_cast<UTexture2D*>(vTexture2DFiltered[i]));
					}
				}
				ImGui::ListBoxFooter();
				ImGui::End();
			}

			// Here we want to set the different TextureParameterValues
			if (ImGui::CollapsingHeader("TextureParameterValues")) {
				if (SelectedMaterial == nullptr) {
					ImGui::Text("Please start by selecting a MaterialInstanceConstant");
				}
				else {
					for (const char* paramName : vTextureParameters) {
						if (ImGui::Button(paramName)) {
							if (!bWantsTexture2DWindow) {
								cTexture2DWindowFor = paramName;
								bWantsTexture2DWindow = true;
								std::fill(std::begin(cFilterTexture2D), std::end(cFilterTexture2D), '\0');
							}
						}
						// If paramName is in map
						if (mTextureParametersSelected.count(paramName) == 1) {
							ImGui::SameLine();
							ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), UObject::PathName(std::get<1>(mTextureParametersSelected[paramName])).AsString());
						}
					}
				}
			}

			// Here we want to set the different VectorParameterValues
			if (ImGui::CollapsingHeader("VectorParameterValues")) {
				for (const char* paramName : vVectorParameters) {
					if (ImGui::DragFloat4(paramName, &mVectorParametersSelected[paramName].x, 0.005F)) {
						fCol.R = mVectorParametersSelected[paramName].x;
						fCol.G = mVectorParametersSelected[paramName].y;
						fCol.B = mVectorParametersSelected[paramName].z;
						fCol.A = mVectorParametersSelected[paramName].w;

						SelectedMaterial->SetVectorParameterValue(FName(paramName), &fCol);
					}
					ImGui::Spacing();
				}
			}

			// ScalarParameterValue DragFloat Sliders
			if (ImGui::CollapsingHeader("ScalarParameterValue")) {
				for (const char* paramName : vScalarParameters) {
					if (ImGui::DragFloat(paramName, &mScalarParametersSelected[paramName], 0.005F)) {
						SelectedMaterial->SetScalarParameterValue(FName(paramName), mScalarParametersSelected[paramName]);
					}
				}
			}


			// Export to external file
			if (ImGui::CollapsingHeader("Export To File")) {
				ImGui::InputText("File Name", cExportedFileName, IM_ARRAYSIZE(cExportedFileName));
				if (ImGui::Button("Export")) {
					std::string pathBinaries = GetCurrPath();
					pathBinaries = pathBinaries.substr(0, pathBinaries.find_last_of("\\") + 1);
					pathBinaries.append(cExportedFileName);
					ExportToFile(&pathBinaries);
				}
			}

			ImGui::End();
			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		}


		return onEndScene(pDevice); // Call original ensdcene so the game can draw
	}

	HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
		if (imguiInit)
			return onReset(pDevice, pPresentationParameters);


		ImGui_ImplDX9_InvalidateDeviceObjects();
		ImGui::DestroyContext();
		HRESULT hr = onReset(pDevice, pPresentationParameters);
		ImGui::CreateContext();
		ImGui_ImplDX9_CreateDeviceObjects();
		return hr;
	}

	void HookInits() {
		MatHookManager->Register("Engine.PlayerController.ConsoleCommand", "ExecSkinFixer", TPSSkinFix);
	}

	bool TPSSkinFix(UObject* Caller, UFunction* Func, FStruct* params) {
		return true;

		// fix the rest, it crashes sometimes
		class UObject* obj = params->structType->FindChildByName(FName("Command"));
		if (obj == nullptr) return true;
		const auto prop = reinterpret_cast<UProperty*>(obj);
		std::string command = (((FHelper*)((char*)(params->base)))->GetStrProperty(prop))->AsString();


		if (command.rfind("exec", 0) == 0) {
			std::string fileName = command.substr(command.find_last_of(" ") + 1, command.size());
			if (fileName.empty()) return true; // No filename given after exec
			try {
				std::string pathBinaries = GetCurrPath();
				pathBinaries = pathBinaries.substr(0, pathBinaries.find_last_of("\\") + 1);
				pathBinaries.append(fileName);
				std::ifstream infile(pathBinaries);

				size_t pos;
				std::string strObject;
				UObject* Object;
				std::vector<std::string> splitLine;
				std::vector<size_t> positionsParams;
				std::vector<size_t> positionsValues;
				static std::string sParameterName = "ParameterName=\"";
				static std::string sParameterValue = "ParameterValue=";

				for (std::string line; std::getline(infile, line); ) { // read our file line by line
					if (line.rfind("set", 0) > 0) continue;  // only the lines that directly start with an set command should be read in
					if (split(line, splitLine, ' ') > 5) continue;

					strObject = splitLine[1];
					// Set Parent Object
					if (StrStrIA(splitLine[2].c_str(), "parent")) {
						Object = UObject::Find("Object", strObject.c_str());
						if (Object == nullptr) continue;
						reinterpret_cast<UMaterialInstanceConstant*>(Object)->SetParent(reinterpret_cast<UMaterialInterface*>(UObject::Find("Object", splitLine[3].c_str())));
					}
					// Set TextureParameterValues
					else if (StrStrIA(splitLine[2].c_str(), "textureparametervalues")) {
						Object = UObject::Find("Object", strObject.c_str());
						if (Object == nullptr) continue;
						positionsParams.clear();
						positionsValues.clear();

						size_t pos = line.find(sParameterName, 0);
						while (pos != std::string::npos) {
							positionsParams.push_back(pos);
							pos = line.find(sParameterName, pos + 1);
						}
						pos = line.find(sParameterValue, 0);
						while (pos != std::string::npos) {
							positionsValues.push_back(pos);
							pos = line.find(sParameterValue, pos + 1);
						}
						if (positionsParams.size() != positionsValues.size()) continue;

						for (int i = 0; i < positionsParams.size(); i++) {
							reinterpret_cast<UMaterialInstanceConstant*>(Object)->SetTextureParameterValue(FName(line.substr(positionsParams[i] + sParameterName.size(), line.find("\"", positionsParams[i] + sParameterName.size()) - (positionsParams[i] + sParameterName.size()))),
																										   reinterpret_cast<UTexture2D*>(UObject::Find("Object", line.substr(positionsValues[i] + sParameterValue.size(),
																																											 line.find(",", positionsValues[i] + sParameterValue.size())
																																											 - (positionsValues[i] + sParameterValue.size())).c_str())));
						}
					}
					// Set VectorParameterValues
					else if (StrStrIA(splitLine[2].c_str(), "vectorparametervalues")) {
						Object = UObject::Find("Object", strObject.c_str());
						if (Object == nullptr) continue;
						positionsParams.clear();
						positionsValues.clear();

						size_t pos = line.find(sParameterName, 0);
						while (pos != std::string::npos) {
							positionsParams.push_back(pos);
							pos = line.find(sParameterName, pos + 1);
						}
						pos = line.find(sParameterValue, 0);
						while (pos != std::string::npos) {
							positionsValues.push_back(pos);
							pos = line.find(sParameterValue, pos + 1);
						}
						if (positionsParams.size() != positionsValues.size()) continue;

						for (int i = 0; i < positionsParams.size(); i++) {
							fCol.R = std::stof(line.substr(line.find("R=", positionsValues[i]) + 2, line.find(",", positionsValues[i]) - (line.find("R=", positionsValues[i]))));
							fCol.G = std::stof(line.substr(line.find("G=", positionsValues[i]) + 2, line.find(",", positionsValues[i]) - (line.find("G=", positionsValues[i]))));
							fCol.B = std::stof(line.substr(line.find("B=", positionsValues[i]) + 2, line.find(",", positionsValues[i]) - (line.find("B=", positionsValues[i]))));
							fCol.A = std::stof(line.substr(line.find("A=", positionsValues[i]) + 2, line.find(")", positionsValues[i]) - (line.find("A=", positionsValues[i]))));

							reinterpret_cast<UMaterialInstanceConstant*>(Object)->SetVectorParameterValue(FName(line.substr(positionsParams[i] + sParameterName.size(), line.find("\"", positionsParams[i] + sParameterName.size())
																															- (positionsParams[i] + sParameterName.size()))), &fCol);
						}
					}
					// Set ScalarPararameterValues
					else if (StrStrIA(splitLine[2].c_str(), "scalarparametervalues")) {
						Object = UObject::Find("Object", strObject.c_str());
						if (Object == nullptr) continue;
						positionsParams.clear();
						positionsValues.clear();

						size_t pos = line.find(sParameterName, 0);
						while (pos != std::string::npos) {
							positionsParams.push_back(pos);
							pos = line.find(sParameterName, pos + 1);
						}
						pos = line.find(sParameterValue, 0);
						while (pos != std::string::npos) {
							positionsValues.push_back(pos);
							pos = line.find(sParameterValue, pos + 1);
						}
						if (positionsParams.size() != positionsValues.size()) continue;

						for (int i = 0; i < positionsParams.size(); i++) {
							reinterpret_cast<UMaterialInstanceConstant*>(Object)->SetScalarParameterValue(FName(line.substr(positionsParams[i] + sParameterName.size(), line.find("\"", positionsParams[i] + sParameterName.size())
																															- (positionsParams[i] + sParameterName.size()))),
																										  std::stof(line.substr(positionsValues[i] + sParameterValue.size(), line.find(",", positionsValues[i]) - (positionsValues[i] + sParameterValue.size()))));
						}
					}
				}
			}
			catch (const std::exception& e) {
				Logging::Log(e.what());
			}

		}
		return true;
	}

}