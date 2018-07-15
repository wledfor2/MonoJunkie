#include <string>
#include "Utility.hpp"
#include "Exceptions.hpp"
#include "InjectionInternals.hpp"
#include "../Blackbone/src/BlackBone/Process/Process.h"
#include "../Blackbone/src/BlackBone/Process/RPC/RemoteFunction.hpp"
#include "../Blackbone/src/BlackBone/Process/Threads/Thread.h"
#include "../Blackbone/src/BlackBone/Asm/AsmVariant.hpp"

MonoInternals::MonoInternals(blackbone::Process& targetProcess, const std::wstring& targetDLLFilename) {

	//initialize member variables to default values
	cachedDomain = nullptr;
	process = &targetProcess;
	rpc_mono_get_root_domain = nullptr;
	rpc_mono_assembly_open = nullptr;
	rpc_mono_assembly_get_image = nullptr;
	rpc_mono_class_from_name = nullptr;
	rpc_mono_class_get_method_from_name = nullptr;
	rpc_mono_runtime_invoke = nullptr;

	//Acquire Mono HMODULE from remote process
	const blackbone::ModuleData* module = targetProcess.modules().GetModule(targetDLLFilename);

	//check if Mono.dll was found, GetModule returns nullptr on failure
	if (module != nullptr) {

		//array of procedures we are remotely retrieving from Mono.dll, std::strings because GetProcAddress doesn't allow unicode, also we need it for 
		std::vector<std::string> TARGET_PROCS = {
			"mono_get_root_domain",
			"mono_assembly_open",
			"mono_assembly_get_image",
			"mono_class_from_name",
			"mono_class_get_method_from_name",
			"mono_runtime_invoke"
		};

		//blackbone pointers to procedures, which we will cast and use below
		std::vector<blackbone::ptr_t> procs;

		//iterate over all target procs we are remotely getting
		for (std::vector<std::string>::size_type k = 0; k < TARGET_PROCS.size(); k++) {

			//get the current target function exported from mono.dll in the remote process
			blackbone::exportData targetExport = targetProcess.modules().GetExport(module, TARGET_PROCS[k].c_str());

			//check if the remote function was found (NOTE: The invalid/uninitialized value of blackbone::ptr_t is 0!)
			if (targetExport.procAddress != 0) {

				//Add current proc to list of procs for later assignment
				procs.push_back(targetExport.procAddress);

			} else {

				//Missing required function, unable to continue. (NOTE: std::string is converted by onError(const std::string&)
				throw MonoInternalsException("Unable to get required remote procedure \"" + TARGET_PROCS[k] + "\".");

			}

		}

		//define RPC function addresses
		remote_mono_get_root_domain = reinterpret_cast<mono_get_root_domain_t>(procs[0]);
		remote_mono_assembly_open = reinterpret_cast<mono_assembly_open_t>(procs[1]);
		remote_mono_assembly_get_image = reinterpret_cast<mono_assembly_get_image_t>(procs[2]);
		remote_mono_class_from_name = reinterpret_cast<mono_class_from_name_t>(procs[3]);
		remote_mono_class_get_method_from_name = reinterpret_cast<mono_class_get_method_from_name_t>(procs[4]);
		remote_mono_runtime_invoke = reinterpret_cast<mono_runtime_invoke_t>(procs[5]);

	} else {

		//Can't find Mono.dll, unable to continue!
		throw MonoInternalsException(_T("Unable to find target DLL \"") + targetDLLFilename + _T("\" in remote process."));

	}

}

MonoInternals::~MonoInternals() {

	//cleanup (NOTE: Defined in Utility.hpp, reduces visible boilerplate code)
	__DELETE_IF_NOT_NULL(rpc_mono_get_root_domain);
	__DELETE_IF_NOT_NULL(rpc_mono_assembly_open);
	__DELETE_IF_NOT_NULL(rpc_mono_assembly_get_image);
	__DELETE_IF_NOT_NULL(rpc_mono_class_from_name);
	__DELETE_IF_NOT_NULL(rpc_mono_class_get_method_from_name);
	__DELETE_IF_NOT_NULL(rpc_mono_runtime_invoke);

}

//retrieves a pointer to our target process
blackbone::Process& MonoInternals::getProcess() {

	//check if the process exists and is still running
	if (process == nullptr || !process->valid()) {

		//process does not exist, or is no longer running. Unrecoverable!
		throw MonoInternalsException(_T("Target process is no longer valid."));

	}

	return *process;
}

blackbone::Thread* MonoInternals::getMainThread() {

	//retrieve the main thread from our target process. Returns nullptr on failure.
	//getProcess will throw an exception upwards if the target process is invalid
	blackbone::Thread* main = getProcess().threads().getMain();

	//Check if the thread no longer exists or is no longer valid
	if (main == nullptr || !main->valid()) {

		//unrecoverable error, we can't really do anything now
		throw MonoInternalsException(_T("Unable to retrieve main thread."));

	}

	return main;

}

//gets the root mono domain; that is, the domain from which all other AppDomains derive
MonoDomain* MonoInternals::mono_get_root_domain() {

	//used cached domain, if available
	MonoDomain* domain = this->cachedDomain;

	//check if cached domain is available
	if (domain == nullptr) {

		//check if we need to create a cached RemoteFunction for for mono_get_root_domain
		if (rpc_mono_get_root_domain == nullptr) {

			//create the RPC and cache it for future calls
			rpc_mono_get_root_domain = new blackbone::RemoteFunction<mono_get_root_domain_t>(getProcess(), remote_mono_get_root_domain);

		}

		//domain is not cached, attempt to get it
		//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
		NTSTATUS error = rpc_mono_get_root_domain->Call(domain, getMainThread());
		
		//check if we got the domain (NOTE: second check is just for debugging purposes, as if error is success, domain should be non-null)
		if (NT_SUCCESS(error) && domain != nullptr) {

			//cache the domain
			this->cachedDomain = domain;

		} else {

			//unable to get root mono domain, tell the user why
			throw MonoInternalsException(_T("Unable to acquire root mono domain: ") + GetNTErrorString(error));

		}

	}

	//return mono's main AppDomain
	return domain;

}

MonoAssembly* MonoInternals::mono_assembly_open(const std::string& fileName) {

	//loaded assembly
	MonoAssembly* loadedAssembly = nullptr;

	//status code passed back out by mono_assembly_open
	MonoImageOpenStatus status = MONO_IMAGE_OK;

	//check if we need to create a cached RemoteFunction for mono_assembly_open
	if (rpc_mono_assembly_open == nullptr) {

		//create the RPC and cache it for future calls
		rpc_mono_assembly_open = new blackbone::RemoteFunction<mono_assembly_open_t>(getProcess(), remote_mono_assembly_open, fileName.c_str(), &status);

	} else {

		//we already have a RemoteFunction created, but we need to update the current arguments
		rpc_mono_assembly_open->setArg(0, blackbone::AsmVariant(fileName.c_str()));
		rpc_mono_assembly_open->setArg(1, blackbone::AsmVariant(&status));

	}

	//call mono_assembly_open remotely -- this is more or less the same as mono_assembly_open(narrowFileName.c_str(), &status)
	//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
	NTSTATUS error = rpc_mono_assembly_open->Call(loadedAssembly, getMainThread());

	//check if the call worked
	if (NT_SUCCESS(error)) {

		//check if the assembly failed to be loaded, otherwise we're done!
		if (status != MONO_IMAGE_OK || loadedAssembly == nullptr) {

			//assembly failed to be loaded, convert the error code to a string and throw it up
			throw MonoInternalsException(_T("Unable to retrieve assembly: " + toString(status)));

		}

	} else {

		//unable to get root mono domain, tell the user why
		throw MonoInternalsException(_T("Unable to load assembly: ") + GetNTErrorString(error));

	}

	return loadedAssembly;

}

MonoImage* MonoInternals::mono_assembly_get_image(MonoAssembly* assembly) {

	//Image loaded from Assembly
	MonoImage* image = nullptr;

	//check if we need to create a cached RemoteFunction for mono_assembly_open
	if (rpc_mono_assembly_get_image == nullptr) {

		//create the RPC and cache it for future calls
		rpc_mono_assembly_get_image = new blackbone::RemoteFunction<mono_assembly_get_image_t>(getProcess(), remote_mono_assembly_get_image, assembly);

	} else {

		//we already have a RemoteFunction created, but we need to update the current arguments
		rpc_mono_assembly_get_image->setArg(0, blackbone::AsmVariant(assembly));

	}

	//call mono_assembly_get_image remotely -- this is more or less the same as mono_assembly_get_image(assembly)
	//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
	NTSTATUS error = rpc_mono_assembly_get_image->Call(image, getMainThread());

	//check if the call worked
	if (NT_SUCCESS(error)) {

		//check if the assembly failed to be loaded, otherwise we're done!
		if (image == nullptr) {

			//could not generate image from loaded assembly
			throw MonoInternalsException(_T("Unable to generate image from assembly: Returned image is null."));

		}

	} else {

		//unable to get root mono domain, tell the user why
		throw MonoInternalsException(_T("Unable to generate image from assembly: ") + GetNTErrorString(error));

	}

	return image;
}

//Retreives a MonoClass for a given namespace and class name from the given Mono image snapshot.
MonoClass* MonoInternals::mono_class_from_name(MonoImage* image, const std::string& nameSpace, const std::string& className) {

	//C# class loaded from Image snapshot of the assembly
	MonoClass* foundClass = nullptr;

	//check if we need to create a cached RemoteFunction for mono_assembly_open
	if (rpc_mono_class_from_name == nullptr) {

		//create the RPC and cache it for future calls
		rpc_mono_class_from_name = new blackbone::RemoteFunction<mono_class_from_name_t>(getProcess(), remote_mono_class_from_name, image, nameSpace.c_str(), className.c_str());

	} else {

		//we already have a RemoteFunction created, but we need to update the current arguments
		rpc_mono_class_from_name->setArg(0, blackbone::AsmVariant(image));
		rpc_mono_class_from_name->setArg(1, blackbone::AsmVariant(nameSpace.c_str()));
		rpc_mono_class_from_name->setArg(2, blackbone::AsmVariant(className.c_str()));

	}

	//call mono_assembly_get_image remotely -- this is more or less the same as mono_class_from_name(image, nameSpace, className)
	//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
	NTSTATUS error = rpc_mono_class_from_name->Call(foundClass, getMainThread());

	//check if the call worked
	if (NT_SUCCESS(error)) {

		//check if the class name failed to be found, otherwise we're done!
		if (foundClass == nullptr) {

			//could not find class by name
			throw MonoInternalsException("Unable to retrieve class \"" + nameSpace + "." + className + "\": Class could not be found.");

		}

	} else {

		//unable to get root mono domain, tell the user why
		throw MonoInternalsException("Unable to retrieve class \"" + nameSpace + "." + className + "\": " + GetNTErrorStringA(error));

	}

	return foundClass;

}

MonoMethod* MonoInternals::mono_class_get_method_from_name(MonoClass* taretClass, const std::string& methodName, int argCount) {

	//C# Method to be retrieved from the target class
	MonoMethod* method = nullptr;

	//check if we need to create a cached RemoteFunction for mono_assembly_open
	if (rpc_mono_class_get_method_from_name == nullptr) {

		//create the RPC and cache it for future calls
		rpc_mono_class_get_method_from_name = new blackbone::RemoteFunction<mono_class_get_method_from_name_t>(getProcess(), remote_mono_class_get_method_from_name, taretClass, methodName.c_str(), argCount);

	} else {

		//we already have a RemoteFunction created, but we need to update the current arguments
		rpc_mono_class_get_method_from_name->setArg(0, blackbone::AsmVariant(taretClass));
		rpc_mono_class_get_method_from_name->setArg(1, blackbone::AsmVariant(methodName.c_str()));
		rpc_mono_class_get_method_from_name->setArg(2, blackbone::AsmVariant(argCount));

	}

	//call mono_assembly_get_image remotely -- this is more or less the same as mono_class_get_method_from_name(taretClass, methodName, argCount)
	//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
	NTSTATUS error = rpc_mono_class_get_method_from_name->Call(method, getMainThread());

	//check if the call worked
	if (NT_SUCCESS(error)) {

		//check if the method failed to be found, otherwise we're done!
		if (method == nullptr) {

			//could not locate the method name in the given class
			throw MonoInternalsException("Unable to retrieve the method \"" + methodName + "\": Method could not be found.");

		}

	} else {

		//unable to get root mono domain, tell the user why
		throw MonoInternalsException("Unable to retrieve the method \"" + methodName + "\": " + GetNTErrorStringA(error));

	}

	return method;

}

MonoObject* MonoInternals::mono_runtime_invoke(MonoMethod* method, void* object, void** arguments, MonoObject** exeception) {

	//result of the invocation boxed as an object
	MonoObject* result = nullptr;

	//check if we need to create a cached RemoteFunction for mono_assembly_open
	if (rpc_mono_runtime_invoke == nullptr) {

		//create the RPC and cache it for future calls
		rpc_mono_runtime_invoke = new blackbone::RemoteFunction<mono_runtime_invoke_t>(getProcess(), remote_mono_runtime_invoke, method, object, arguments, exeception);

	} else {

		//we already have a RemoteFunction created, but we need to update the current arguments
		rpc_mono_runtime_invoke->setArg(0, blackbone::AsmVariant(method));
		rpc_mono_runtime_invoke->setArg(1, blackbone::AsmVariant(object));
		rpc_mono_runtime_invoke->setArg(2, blackbone::AsmVariant(&arguments));
		rpc_mono_runtime_invoke->setArg(3, blackbone::AsmVariant(&exeception));

	}

	//call mono_runtime_invoke remotely -- this is more or less the same as mono_runtime_invoke(method, object, arguments, exeception)
	//NOTE: We don't need to call updateArgs afterwards, that is only for if the process changes the pointers we pass in later after the call.
	NTSTATUS error = rpc_mono_runtime_invoke->Call(result, getMainThread());

	//check if the call worked
	if (!NT_SUCCESS(error)) {

		//unable to get root mono domain, tell the user why
		throw MonoInternalsException("Unable to invoke the given method: " + GetNTErrorStringA(error));

	}

	return result;
}

std::wstring MonoInternals::toString(MonoImageOpenStatus code) {

	std::wstring s = _T("Unknown");

	switch (code) {

		//not an error
		case MONO_IMAGE_OK: {
			s = _T("Success");
			break;
		}

		//Mono is telling us to check errno for an error
		case MONO_IMAGE_ERROR_ERRNO: {
			s = GetLastErrorString();
			break;
		}

		//The assembly we are loading is missing a dependency
		case MONO_IMAGE_MISSING_ASSEMBLYREF: {
			s = _T("Assembly has a dependency that is not yet loaded");
			break;
		}

		//user tried to inject something that isn't a .NET DLL?
		case MONO_IMAGE_IMAGE_INVALID: {
			s = _T("Invalid .NET assembly");
			break;
		}

		//Unknown
		default: {
			break;
		}

	}

	return s;

}
