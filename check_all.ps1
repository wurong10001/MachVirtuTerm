$clang = 'C:\Users\Administrator\Desktop\android\sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe'
$hpd   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\kernel\src\mach_kernel\hp_pa\HP700'
$inc1  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\android_compat'
$inc2  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\kernel\src\mach_kernel'
$inc3  = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\kernel\src'
$files = @(
  'autoconf.c','busconf.c','cons_conf.c','glue.c','grf.c','grf_conf.c',
  'grf_machdep.c','grf_sti.c','ite.c','ite_sti.c','dca.c','gkd.c','hil.c',
  'eisa_common.c','if_i596.c','hpux_label.c','kgdb_support.c','gkd_keymaps.c',
  'hil_keymaps.c'
)
foreach ($f in $files) {
    $src = Join-Path $hpd $f
    $out = & $clang -target aarch64-linux-android28 -fsyntax-only -std=gnu89 -DMACH_KERNEL `
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-implicit-int `
        -Wno-pointer-to-int-cast -Wno-macro-redefined -Wno-incompatible-library-redeclaration `
        -Wno-typedef-redefinition -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter `
        -Wno-dangling-else -Wno-bool-conversion -Wno-comment -Wno-asm-operand-widths `
        -Wno-unused-but-set-variable -Wno-unknown-pragmas -Wno-deprecated-declarations `
        -I $inc1 -I $inc2 -I $inc3 $src 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Output ("PASS  : " + $f)
    } else {
        $first = $out | Select-Object -First 6
        Write-Output ("FAIL  : " + $f + "  (" + $out.Count + " lines)")
        $first | ForEach-Object { Write-Output ("       " + $_) }
    }
}
