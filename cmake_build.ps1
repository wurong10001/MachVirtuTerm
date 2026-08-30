$cmake = 'C:\Users\Administrator\Desktop\android\sdk\cmake\3.22.1\bin\cmake.exe'
$cpp   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp'
$out   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\build\hp700_cmake'
$tc    = 'C:\Users\Administrator\Desktop\android\sdk\ndk\28.2.13676358\build\cmake\android.toolchain.cmake'
$log   = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\cmake_config.log'

$ninja = 'C:\Users\Administrator\Desktop\android\sdk\cmake\3.22.1\bin\ninja.exe'
$o = & $cmake -S $cpp -B $out -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$tc" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 2>&1
$o | Set-Content -Path $log -Encoding utf8
Write-Output ("CONFIGURE EXIT=" + $LASTEXITCODE)

if ($LASTEXITCODE -eq 0) {
    $b = & $cmake --build $out 2>&1
    $b | Set-Content -Path ($log + '.build') -Encoding utf8
    Write-Output ("BUILD EXIT=" + $LASTEXITCODE)
}
