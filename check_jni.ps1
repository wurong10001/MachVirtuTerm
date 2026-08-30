$clang = 'C:\Users\Administrator\Desktop\android\sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe'
$files = @(
  'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\jni_bridge.cpp',
  'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\native-lib.cpp'
)
$inc1  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\android_compat'
$sysroot = 'C:\Users\Administrator\Desktop\android\sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64\sysroot'
foreach ($f in $files) {
    $out = & $clang -target aarch64-linux-android28 -fsyntax-only -std=c++17 `
        --sysroot=$sysroot -I $inc1 $f 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Output ("PASS  : " + $f)
    } else {
        Write-Output ("FAIL  : " + $f + "  (" + $out.Count + " lines)")
        $out | Select-Object -First 12 | ForEach-Object { Write-Output ("       " + $_) }
    }
}
