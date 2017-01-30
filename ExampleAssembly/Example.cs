using System.IO;

/// <summary>
/// Example assembly, inject with the arguments:
/// 
/// MonoJunkie.exe -dll "ExampleAssembly.dll" -namespace ExampleAssembly -class Example -method OnLoad -exe targetexe.exe
/// 
/// </summary>
namespace ExampleAssembly {

    public class Example {

		/// <summary>
		/// Only static methods (that take no arguments) can be called from MonoJunkie.
		/// 
		/// Classes that the method lives in will not be instantiated (there is nothing to stop you from instantiating it yourself in the static method, though)!
		/// </summary>
		public static void OnLoad() {

			//In the real world, there is no guarantee we will be able to use standard input/output in the process. I prefer using log files.
			using (StreamWriter @out = new StreamWriter(new FileStream("ExampleAssembly.log", FileMode.OpenOrCreate))) {

				//Let us know everything is okay and we're up and running.
				@out.WriteLine("Injection worked! Bye!");
				@out.Flush();
				@out.Close();

			}

		}

		/// <summary>
		/// Unload methods can also be called remotely by MonoJunkie.
		/// </summary>
		public static void OnUnload() {

			//...

		}

    }

}
