# MonoJunkie
**MonoJunkie** is a tool to inject .NET assemblies into Mono processes (though designed for use with Unity games). Any Mono process can be targeted, regardless of version. This project was written in an afternoon (to replace [mono-assembly-injector](https://github.com/gamebooster/mono-assembly-injector) for my personal use, which has some issues), so there are bound to bugs. I do plan to keep improving it, though!

**MonoJunkie** uses [Blackbone](https://github.com/DarthTon/Blackbone). We embed it directly due to some quirks with Blackbone's project files.

Finally (if it wasn't obvious), **MonoJunkie** is for Windows only. I may eventually port it to Linux, but there are better ways to do this on Linux than using this tool.

# Disclaimer
**DO NOT** use MonoJunkie with online games. Don't blame me if you get banned!

# Instructions
1. Follow the [caveats](#caveats) to ensure everything will work correctly in your assembly.
2. Start the process. Wait until the correct injection time (again, see [caveats](#caveats)).
3. Inject your assembly via the command line. For instance, for our example assembly: 

>MonoJunkie.exe -dll "ExampleAssembly.dll" -namespace ExampleAssembly -class Example -method OnLoad -exe targetexe.exe

# Caveats
1. MonoJunkie must be the same architecture as the target process. If the process is 64-bit, we must also be 64-bit. This is due to some issue with Blackbone crossing the WOW64 barrier.
2. The Assembly you are injecting must match the architecture of the target process (or Any CPU).
3. The Method MonoJunkie is calling must be static and publicly visible. MonoJunkie does not instantiate the class the method is a member of. See [the example](ExampleAssembly/Example.cs).
4. The correct time to inject depends on what you are injecting into. Injecting too early may cause issues and crashes. In unity games; for instance, you would usually want to wait until the Main menu has completely loaded before injecting.

# TODO
1. Generalize all calls in MonoInternals using a template, as it is messy boilerplate.
2. Use mono_assembly_loaded to check if the assembly is already loaded to avoid potential crashes in the target process.
3. Add unloading support using mono_assembly_close, mono_image_close. Once this and 2 are implemented, we can reload Assemblies.
4. Automatic dependency loading.
5. Add more command line options:
 - Allow PIDs
 - Allow targeting a specific thread to inject with.

# License
MIT - See [LICENSE](LICENSE)