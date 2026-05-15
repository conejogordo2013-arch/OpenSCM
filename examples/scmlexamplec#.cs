using System;
using System.Runtime.InteropServices;

public static class ScmlHostExample {
    [DllImport("scml")] private static extern IntPtr scml_vm_create();
    [DllImport("scml")] private static extern void scml_vm_destroy(IntPtr vm);
    // In a shared-library build, expose scml_vm_load_file/scml_vm_run with the same signatures as vm.h.
    public static void Main() => Console.WriteLine("SCML C# host uses P/Invoke against the C API.");
}
