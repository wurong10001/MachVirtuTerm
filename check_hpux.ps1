$clang = 'C:\Users\Administrator\Desktop\android\sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe'
$src   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\kernel\src\mach_kernel\hp_pa\HP700\hpux_label.c'
$inc1  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\android_compat'
$inc2  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\kernel\src\mach_kernel'
$log   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\hpux_check.txt'
$out = & $clang -target aarch64-linux-android28 -fsyntax-only -std=gnu89 -DMACH_KERNEL `
    -Wno-implicit-function-declaration -Wno-int-conversion -Wno-implicit-int `
    -Wno-pointer-to-int-cast -Wno-macro-redefined -Wno-incompatible-library-redeclaration `
    -Wno-typedef-redefinition -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter `
    -Wno-dangling-else -Wno-bool-conversion -Wno-comment `
    -I $inc1 -I $inc2 $src 2>&1
$out | Set-Content -Path $log -Encoding utf8
Write-Output ("EXIT=" + $LASTEXITCODE + "  LINES=" + $out.Count)
