#include <iostream>
#include <vector>
#include <cstdlib>
#include <Windows.h>
#include <tchar.h>
#include "MonoJunkie.hpp"
#include "Utility.hpp"
#include "Exceptions.hpp"
#include "Utility.hpp"
#include "InjectionInternals.hpp"
#include "../Blackbone/src/BlackBone/Process/Process.h"

//Find and prepare the target process for assembly injection
void InjectAssembly(Configuration& configuration) {

	//output injection configuration
	std::wcout << _T("Attempting to inject ") << configuration.assemblyFileName << _T(" into ") << configuration.targetProcessEXE << _T("...") << std::endl;

	//vector of found process IDs matching the given exeName
	std::vector<DWORD> foundPIDs;

	//Find all processes with the given exe name and add the PIDs to our vector
	blackbone::Process::EnumByName(configuration.targetProcessEXE, foundPIDs);

	//check if any (unique) process matching the exe name was found
	if (foundPIDs.size() == 1) {

		//Get the found PID of the target EXE
		DWORD targetProcessID = foundPIDs[0];

		//output PID
		std::wcout << _T("Found process with PID ") << targetProcessID << std::endl;

		//Process object that acts as a handle for our target process
		blackbone::Process targetProcess;

		//Attach to the found target process
		NTSTATUS attached = targetProcess.Attach(targetProcessID);

		//Check if we successfully attached to the found process
		if (NT_SUCCESS(attached)) {

			//Get process barrier type, we can use this to determine which mode the target process is executing under.
			blackbone::WoW64Type processBarrierType = targetProcess.core().native()->GetWow64Barrier().type;

			//check if process is pure 32-bit or pure 64-bit (NOTE: Cleaner than GetWow64Barrier().sourceWow64 && GetWow64Barrier().targetWow64)
			if (processBarrierType != blackbone::wow_32_64 && processBarrierType != blackbone::wow_64_32) {

				//Create mono internals class which handles acqui ring all RPCs
				MonoInternals internals(targetProcess);

				//Retrieve root app domain
				MonoDomain* domain = internals.mono_get_root_domain();

				//output address for debugging purposes
				__LOG_ADDRESS(_T("Mono Domain"), domain);

				//Load our target assembly
				MonoAssembly* assembly = internals.mono_assembly_open(configuration.assemblyPath);

				//output address for debugging purposes
				__LOG_ADDRESS(_T("Assembly"), assembly);

				//Generate image from loaded assembly
				MonoImage* image = internals.mono_assembly_get_image(assembly);

				//output address for debugging purposes
				__LOG_ADDRESS(_T("Image"), image);

				//Retrieve target class
				MonoClass* targetClass = internals.mono_class_from_name(image, configuration.targetNamespace, configuration.targetClass);

				//output address for debugging purposes
				__LOG_ADDRESS(_T("Class"), targetClass);

				//retrieve target method
				MonoMethod* targetMethod = internals.mono_class_get_method_from_name(targetClass, configuration.targetMethod, 0);

				//output address for debugging purposes
				__LOG_ADDRESS(_T("Method"), targetMethod);

				//call target method
				internals.mono_runtime_invoke(targetMethod, nullptr, nullptr, nullptr);

				//Done! output injection success
				std::wcout << _T("Injection complete. Called ") << configuration.targetNamespace << _T("::") << configuration.targetClass << _T(".") << configuration.targetMethod << _T("().");

			} else {

				//Unfortunately, we cannot perform an injection if our target and this program are not both 32-bit or 64-bit.
				throw InjectionException(_T("Process mode mismatch. Both processes must be either 32-bit or both 64-bit."));

			}

		} else {

			//Failed to attach to the found process ID
			throw InjectionException(_T("Failed to attach to found process: ") + GetNTErrorString(attached));

		}

	} else {

		//no/duplicate processes found with the given exe name
		throw InjectionException(_T("Unable to find unique process with the executable name \"") + configuration.targetProcessEXE + _T("\"."));

	}

}

//returns true if the given string starts with "-", "--", or "/" which denote the beginning of a command line option name
bool IsOption(const std::wstring& s) {

	//get string size (to check if there is an option name after the symbol)
	std::wstring::size_type length = s.length();

	//get first character (to check which symbol the option starts with) -- we do this because there is no need to call substr repeatedly
	wchar_t firstCharacter = s[0];

	//check for the given string to start with "--", "-", or "/" (in that order)
	return (length > 1 && (firstCharacter == _T('-') || firstCharacter == _T('/')) && s[1] != _T('-')) || (length > 2 && firstCharacter == '-' && s[1] == '-');

}

//trims "-", "--" or "/" from the start of a string (NOTE: only intended to be called after IsOption, which validates the string)
std::wstring GetOptionName(const std::wstring& s) {

	//get a copy of the string to return
	std::wstring trimmed = s;

	//check if the second character is not a symbol (IE: check for --)
	if (trimmed.at(1) != '-') {

		//only remove first character
		trimmed = trimmed.substr(1);
		
	} else {

		//remove first 2 characters
		trimmed = trimmed.substr(2);

	}

	//return trimmed string
	return trimmed;

}

//Parses command line arguments
Configuration ParseCommandLine(int argc, wchar_t** argv) {

	//The injection configuration parsed from the command line options. See class definition for more information.
	Configuration parsedConfiguration;

	//iterate over all command line arguments
	for (int k = 1; !parsedConfiguration.hadError && k < argc; k += 2) {

		//get the current command line argument as a C++ wide string (NOTE: Because of the argument length check in main, we know every command line option will have an argument, so this is safe!)
		std::wstring arg = argv[k];

		//check if current argument is the start of an option
		if (IsOption(arg)) {

			//get option name (without leading /, -. or --)
			std::wstring optionName = GetOptionName(arg);

			//get argument to current option (NOTE: Because of the argument length check in main, we know every command line option will have an argument, so this is safe!)
			std::wstring argument = argv[k + 1];

			//validate argument to option -- an argument cannot be empty (only possible when user passes in argument in quotes, IE: "")
			if (!argument.empty()) {

				//check which option we're currently parsing
				if (optionName == _T("dll")) {

					//attempt to get absolute path of the given argument
					std::wstring absolutePath = GetAbsolutePath(argument);

					//check if the absolute path was retrieved
					if (!absolutePath.empty()) {

						//Attempt to get the filename of the assembly from the absolute path
						std::wstring fileName = GetFileName(absolutePath);

						//check if the filename was retrieved from the path
						if (!fileName.empty()) {

							//Done! Store both the path and filename
							parsedConfiguration.assemblyPath = absolutePath;
							parsedConfiguration.assemblyFileName = fileName;

						} else {

							//underlying windows api failed, get the error message and return it
							parsedConfiguration.onError(_T("Unable to get file name of the DLL \"") + absolutePath + _T("\": ") + GetLastErrorString());

						}

					} else {

						//underlying windows api failed, get the error message and return it
						parsedConfiguration.onError(_T("Unable to get absolute path of the DLL \"") + argument + _T("\": ") + GetLastErrorString());

					}

				} else if (optionName == _T("namespace")) {

					//Namespace of our DLL "Main" class
					parsedConfiguration.targetNamespace = argument;

				} else if (optionName == _T("class")) {

					//Class name of our DLL "Main" function
					parsedConfiguration.targetClass = argument;

				} else if (optionName == _T("method")) {

					//"Main" method name
					parsedConfiguration.targetMethod = argument;

				} else if (optionName == _T("exe")) {

					//blackbone only expects the EXE filename of our target process, so we need to ignore everything but the filename/extension
					std::wstring exeName = GetFileName(argument);

					//check if we could retrieve the EXE filename from the given argument (GetFileName returns an empty string on error).
					if (!exeName.empty()) {

						//EXE of the process we are injecting our assembly into.
						parsedConfiguration.targetProcessEXE = exeName;

					} else {

						//Unable to get the exe filename. Tell the user why.
						parsedConfiguration.onError(_T("Unable to get EXE filename for the given EXE \"") + argument + _T("\":") + GetLastErrorString());

					}

				} else {

					//invalid option, flag an error
					parsedConfiguration.onError(_T("Unknown command line option \"") + optionName + _T("\"."));

				}

			} else {

				//invalid argument, flag an error
				parsedConfiguration.onError(_T("Invalid argument to option \"") + optionName + _T("\". Arguments must be non-empty strings."));

			}

		} else {

			//we got an argument to a command line option when we didn't expect one, or malformed command line option (IE: Missing symbol -- doesn't matter which)
			parsedConfiguration.onError(_T("Unexpected command line argument \"") + arg + _T("\"."));

		}

	}

	//Compilers HATE this one wierd trick
	std::pair<std::wstring, ConfigurationString*> strings[] = {
		{ _T("namespace"), &parsedConfiguration.targetNamespace },
		{ _T("class"), &parsedConfiguration.targetClass },
		{ _T("method"), &parsedConfiguration.targetMethod },
		{ _T("exe"), &parsedConfiguration.targetProcessEXE },
		{ _T("dll"), &parsedConfiguration.assemblyPath }
	};

	//Check and make sure we have no missing required arguments
	for (const std::pair<std::wstring, ConfigurationString*> arg : strings) {

		//check if this argument was never set
		if (arg.second->empty()) {

			//missing required argument
			parsedConfiguration.onError(_T("Required argument \"") + arg.first + _T("\" is missing."));

			//no reason to continue
			break;

		}

	}

	return parsedConfiguration;

}

int wmain(int argc, wchar_t** argv, wchar_t** envp) {

	//value returned to Windows
	int exitCode = EXIT_FAILURE;

	//check if we have enough command line arguments; that is, more than 0 arguments (excluding exe name) and every option has an argument
	if (argc > 1 && (argc - 1) % 2 == 0) {

		//Parse the configiuration from the command line options
		Configuration injectionConfiguration = ParseCommandLine(argc, argv);

		//check if 
		if (!injectionConfiguration.hadError) {

			//make sure the DLL that contains our payload exists
			if (FileExists(injectionConfiguration.assemblyPath)) {

				try {

					//hand configuration off to injection code
					InjectAssembly(injectionConfiguration);

					//Done. Exit successfully.
					exitCode = EXIT_SUCCESS;

				} catch(const BaseMonoJunkieException& e) {

					//output why injection failed
					std::wcerr << _T("ERROR: ") << e.what() << std::endl;

				}

			} else {

				//the given DLL doesn't exist.
				std::wcerr << _T("ERROR: The DLL path \"") << injectionConfiguration.assemblyFileName << _T("\" does not exist!") << std::endl;

			}

		} else {

			//user is not expecting help output, output usage and error message
			std::wcerr << _T("USAGE: ") << COMMAND_LINE_USAGE << std::endl;
			std::wcerr << _T("ERROR: ") << injectionConfiguration.errorString << std::endl;

		}

	} else {

		//Display usage (NOTE: Not an error, we only return an error code when arguments are invalid, not when there aren't enough).
		std::wcout << _T("USAGE: ") << COMMAND_LINE_USAGE << std::endl;
		exitCode = EXIT_SUCCESS;

	}

	return exitCode;

}
