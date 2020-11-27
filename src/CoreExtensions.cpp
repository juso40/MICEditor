#include "stdafx.h"
#include <stack>
#include <utility>


UObject* UObject::Load(UClass* ClassToLoad, const char* ObjectFullName)
{
	return UObject::GObjects()->Data[0]->DynamicLoadObject(FString(ObjectFullName), ClassToLoad, true);
}

UObject* UObject::Load(const char* ClassName, const char* ObjectFullName)
{
	UClass* classToLoad = FindClass(ClassName);
	if (classToLoad)
		return Load(classToLoad, ObjectFullName);
	return nullptr;
}

UObject* UObject::Find(UClass* Class, const char* ObjectFullName)
{
	const bool doInjectedNext = UnrealSDK::gInjectedCallNext;
	UnrealSDK::DoInjectedCallNext();
	UObject* ret = FindObject(FString(ObjectFullName), Class);
	if (doInjectedNext)
		UnrealSDK::DoInjectedCallNext();
	return ret;
}

UObject* UObject::Find(const char* ClassName, const char* ObjectFullName)
{
	UClass* classToFind = FindClass(ClassName, true);
	if (classToFind)
		return Find(classToFind, ObjectFullName);
	return nullptr;
}

std::vector<UObject*> UObject::FindObjectsRegex(const std::string& RegexString)
{
	const std::regex re = std::regex(RegexString);
	std::vector<UObject*> ret;
	while (!GObjects())
		Sleep(100);
	while (!FName::Names())
		Sleep(100);
	for (size_t i = 0; i < GObjects()->Count; ++i)
	{
		UObject* object = GObjects()->Data[i];
		if (object && std::regex_match(object->GetFullName(), re))
			ret.push_back(object);
	}
	return ret;
}

std::vector<UObject*> UObject::FindObjectsContaining(const std::string& StringLookup)
{
	std::vector<UObject*> ret;
	while (!GObjects())
		Sleep(100);
	while (!FName::Names())
		Sleep(100);
	for (size_t i = 0; i < GObjects()->Count; ++i)
	{
		UObject* object = GObjects()->Data[i];
		if (object && object->GetFullName().find(StringLookup) != std::string::npos)
			ret.push_back(object);
	}
	return ret;
}

class UPackage* UObject::GetPackageObject() const
{
	UObject* pkg = nullptr;
	UObject* outer = this->Outer;
	while (outer)
	{
		pkg = outer;
		outer = pkg->Outer;
	}
	return static_cast<UPackage*>(pkg);
};

UClass* UObject::StaticClass()
{
	static auto ptr = static_cast<UClass*>(GObjects()->Data[2]);
	return ptr;
};

std::vector<UObject*> UObject::FindAll(char* InStr)
{
	UClass* inClass = FindClass(InStr, true);
	if (!inClass)
		throw std::exception("Unable to find class");
	std::vector<UObject*> ret;
	for (size_t i = 0; i < GObjects()->Count; ++i)
	{
		UObject* object = GObjects()->Data[i];
		if (object && object->Class == inClass)
			ret.push_back(object);
	}
	return ret;
}

void* FHelper::GetPropertyAddress(UProperty* Prop)
{
	return reinterpret_cast<char*>(this) + Prop->Offset_Internal;
}


struct FStruct FHelper::GetStructProperty(UStructProperty* Prop)
{
	return FStruct{Prop->GetStruct(), GetPropertyAddress(Prop)};
}

struct FString* FHelper::GetStrProperty(UProperty* Prop)
{
	return reinterpret_cast<FString*>(GetPropertyAddress(Prop));
}

class UObject* FHelper::GetObjectProperty(UProperty* Prop)
{
	return reinterpret_cast<UObject **>(GetPropertyAddress(Prop))[0];
}

class UComponent* FHelper::GetComponentProperty(UProperty* Prop)
{
	return reinterpret_cast<UComponent **>(GetPropertyAddress(Prop))[0];
}

class UClass* FHelper::GetClassProperty(UProperty* Prop)
{
	return reinterpret_cast<UClass **>(GetPropertyAddress(Prop))[0];
}

struct FName* FHelper::GetNameProperty(UProperty* Prop)
{
	return reinterpret_cast<FName*>(GetPropertyAddress(Prop));
}

int FHelper::GetIntProperty(UProperty* Prop)
{
	return reinterpret_cast<int*>(GetPropertyAddress(Prop))[0];
}

struct FScriptInterface* FHelper::GetInterfaceProperty(UProperty* Prop)
{
	return reinterpret_cast<FScriptInterface*>(GetPropertyAddress(Prop));
}

float FHelper::GetFloatProperty(UProperty* Prop)
{
	return reinterpret_cast<float*>(GetPropertyAddress(Prop))[0];
}

struct FScriptDelegate* FHelper::GetDelegateProperty(UProperty* Prop)
{
	return reinterpret_cast<FScriptDelegate*>(GetPropertyAddress(Prop));
}

unsigned char FHelper::GetByteProperty(UProperty* Prop)
{
	return reinterpret_cast<unsigned char*>(GetPropertyAddress(Prop))[0];
}

bool FHelper::GetBoolProperty(UBoolProperty* Prop)
{
	return !!(GetIntProperty(Prop) & Prop->GetMask());
}


TArray<UObject*>* UObject::GObjects()
{
	const auto objectArray = static_cast<TArray<UObject*>*>(UnrealSDK::pGObjects);
	return objectArray;
}

const char* UObject::GetName() const
{
	return this->Name.GetName();
}

std::string UObject::GetNameCpp() const
{
	std::string output = "U";

	if (this->IsA(FindClass("Actor")))
	{
		output = "A";
	}

	output += GetName();

	return output;
}


std::string UObject::GetObjectName()
{
	std::stack<UObject*> parents{};

	for (auto current = this; current; current = current->Outer)
		parents.push(current);

	std::string output{};

	while (!parents.empty())
	{
		output += parents.top()->GetName();
		parents.pop();
		if (!parents.empty())
			output += '.';
	}

	return output;
}

std::string UObject::GetFullName()
{
	if (!this->Class)
		return "(null)";

	std::string output = this->Class->GetName();
	output += " ";

	output += GetObjectName();

	return output;
}

UClass* UObject::FindClass(const char* ClassName, const bool Lookup)
{
	if (UnrealSDK::ClassMap.count(ClassName))
		return UnrealSDK::ClassMap[ClassName];

	if (!Lookup)
		return nullptr;

	for (size_t i = 0; i < GObjects()->Count; ++i)
	{
		const auto object = GObjects()->Data[i];

		if (!object || !object->Class)
			continue;

		// Might as well lookup all objects since we're going to be iterating over most objects regardless
		const char* c = object->Class->GetName();
		if (!strcmp(c, "Class"))
			UnrealSDK::ClassMap[object->GetName()] = static_cast<UClass*>(object);
	}
	if (UnrealSDK::ClassMap.count(ClassName))
		return UnrealSDK::ClassMap[ClassName];
	return nullptr;
}

bool UObject::IsA(UClass* PClass) const
{
	for (auto superClass = this->Class; superClass; superClass = static_cast<UClass*>(superClass->SuperField))
	{
		if (superClass == PClass)
			return true;
	}

	return false;
}






struct FFunction UObject::GetFunction(std::string& PropName)
{
	const auto obj = this->Class->FindChildByName(FName(PropName));
	const auto function = reinterpret_cast<UFunction*>(obj);
	return FFunction{this, function};
}


//class FScriptMap* UObject::GetMapProperty(std::string& PropName) {
//	class UProperty *prop = this->Class->FindPropeFindChildByNamertyByName(FName(PropName));
//	if (!prop || prop->Class->GetName() != "MapProperty")
//		return nullptr;
//	return (FScriptMap *)(((char *)this) + prop->Offset_Internal);
//}
//struct FScriptArray UObject::GetArrayProperty(std::string& PropName);





void FFrame::SkipFunction()
{
	// allocate temporary memory on the stack for evaluating parameters
	char params[1000] = "";
	memset(params, 0, 1000);
	for (UProperty* Property = (UProperty*)Node->Children; Code[0] != 0x16; Property = (UProperty*)Property->Next)
		UnrealSDK::pFrameStep(this, this->Object,
		                   (void*)((Property->PropertyFlags & 0x100) ? nullptr : params + Property->Offset_Internal));

	Code++;
	memset(params, 0, 1000);
}

FStruct::FStruct(UStruct* s, void* b)
{
	Logging::LogD("Creating FStruct of type '%s' from %p\n", s->GetObjectName().c_str(), b);
	structType = s;
	base = b;
};



FArray::FArray(TArray<char>* array, UProperty* s)
{
	Logging::LogD("Creating FArray from %p, count: %d, max: %d\n", array, array->Count, array->Max);
	arr = array;
	type = s;
	IterCounter = 0;
};


int FArray::GetAddress() const
{
	return (int)arr->Data;
}

FArray* FArray::Iter()
{
	IterCounter = 0;
	return this;
}

