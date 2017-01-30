#pragma once

#include <string>
#include "../Blackbone/src/BlackBone/Process/Process.h"
#include "../Blackbone/src/BlackBone/Process/RPC/RemoteFunction.hpp"

//dummy definition for a mono domain struct (AppDomain?)
typedef void MonoDomain;

//dummy definition for a mono assembly struct
typedef void MonoAssembly;

//dummy definition for a mono image
typedef void MonoImage;

//dummy definition for the mono native representation for a C# class
typedef void MonoClass;

//dummy definition for a mono object
typedef void MonoObject;

//dummy definition for the mono native representation of a C# method
typedef void MonoMethod;

//definition for MonoImageOpenStatus (defined as an enum in mono, but we really don't care) used to denote status when opening .net image files
typedef int MonoImageOpenStatus;

//values for MonoImageOpenStatus
#define MONO_IMAGE_OK 0
#define MONO_IMAGE_ERROR_ERRNO 1
#define MONO_IMAGE_MISSING_ASSEMBLYREF 2
#define MONO_IMAGE_IMAGE_INVALID 3

//Mono is a pure C DLL, and we need to inform blackbone which calling convention to use.
#define MONO_FUNCTION __cdecl

//MonoDomain* mono_get_root_domain()
typedef MonoDomain* (MONO_FUNCTION *mono_get_root_domain_t)();

//MonoAssembly* mono_assembly_open(const char* filename, MonoImageOpenStatus* status)
typedef MonoAssembly* (MONO_FUNCTION *mono_assembly_open_t)(const char*, MonoImageOpenStatus*);

//MonoImage* mono_assembly_get_image (MonoAssembly* assembly) 
typedef MonoImage* (MONO_FUNCTION* mono_assembly_get_image_t)(MonoAssembly*);

//MonoClass* mono_class_from_name (MonoImage* image, const char* name_space, const char* name)
typedef MonoClass* (MONO_FUNCTION *mono_class_from_name_t)(MonoImage*, const char*, const char*);

//MonoMethod* mono_class_get_method_from_name (MonoClass* klass, const char* name, int param_count);
typedef MonoMethod* (MONO_FUNCTION *mono_class_get_method_from_name_t)(MonoClass*, const char*, int);

//MonoObject* mono_runtime_invoke (MonoMethod* method, void* obj, void** params, MonoObject** exc)
typedef MonoObject* (MONO_FUNCTION *mono_runtime_invoke_t)(MonoMethod*, void*, void**, MonoObject**);

//Class that wraps all the low level details of any RPC calls.
class MonoInternals {

private:

	//members for mono RPCs
	MonoDomain* cachedDomain;
	blackbone::Process* process;

	//rpc internals
	blackbone::RemoteFunction<mono_get_root_domain_t>* rpc_mono_get_root_domain;
	blackbone::RemoteFunction<mono_assembly_open_t>* rpc_mono_assembly_open;
	blackbone::RemoteFunction<mono_assembly_get_image_t>* rpc_mono_assembly_get_image;
	blackbone::RemoteFunction<mono_class_from_name_t>* rpc_mono_class_from_name;
	blackbone::RemoteFunction<mono_class_get_method_from_name_t>* rpc_mono_class_get_method_from_name;
	blackbone::RemoteFunction<mono_runtime_invoke_t>* rpc_mono_runtime_invoke;
	mono_get_root_domain_t remote_mono_get_root_domain;
	mono_assembly_open_t remote_mono_assembly_open;
	mono_assembly_get_image_t remote_mono_assembly_get_image;
	mono_class_from_name_t remote_mono_class_from_name;
	mono_class_get_method_from_name_t remote_mono_class_get_method_from_name;
	mono_runtime_invoke_t remote_mono_runtime_invoke;

	//internal methods
	blackbone::Thread* getMainThread();
	blackbone::Process& getProcess();

	//converts a MonoImageOpenStatus to a string
	std::wstring toString(MonoImageOpenStatus);

public:

	//Initialize and cleanup RPCs for the given process.
	MonoInternals(blackbone::Process& targetProcess);
	~MonoInternals();

	//Returns the root MonoDomain (the AppDomain the process starts with).
	MonoDomain* mono_get_root_domain();

	//Loads an assembly with a given filename, and passes out a status code
	MonoAssembly* mono_assembly_open(const std::string&);

	//Generates a MonoImage snapshot from the given MonoAssembly
	MonoImage* mono_assembly_get_image(MonoAssembly*);

	//Retreives a MonoClass for a given namespace and class name from the given Mono image snapshot.
	MonoClass* mono_class_from_name(MonoImage*, const std::string&, const std::string&);

	//Retrieves a MonoMethod for the given name, and argument count
	MonoMethod* mono_class_get_method_from_name(MonoClass*, const std::string&, int);

	//Invokes the given MonoMethod on the given opaque "this" object, with the given parameters, and accepts a MonoObject to retrieve an exception if it is thrown.
	//If any exception is thrown, the resulting MonoObject will be null.
	MonoObject* mono_runtime_invoke(MonoMethod*, void*, void**, MonoObject**);

};
