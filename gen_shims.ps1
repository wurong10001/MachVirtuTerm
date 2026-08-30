# Generate android_compat/machine/*.h forwarding headers -> hp_pa/*.h
# and android_compat/mach/machine/*.h -> mach/hp_pa/*.h
$ErrorActionPreference = 'Stop'
$base = 'C:\Users\Administrator\Desktop\Projects\AndroidStudioProjects\Mos\app\src\main\cpp\android_compat'
$utf8 = New-Object System.Text.UTF8Encoding($false)

# machine/ -> hp_pa/  (from include/hp_pa/Makefile DATAFILES + HP700_DATAFILES)
$machineFiles = @(
  'arch_types.h','asm.h','asm_em.h','asp.h','ast.h','break.h','clock.h','cpu.h',
  'cpu_data.h','db_machdep.h','debug.h','endian.h','iodc.h','iomod.h','iplhdr.h',
  'kkt.h','kkt_map.h','lock.h','locore.h','mach_param.h','machine_routines.h',
  'machine_rpc.h','machparam.h','opcode_is_something.h','pdc.h','pim.h','pmap.h',
  'psw.h','regs.h','rpb.h','rtclock_entries.h','setjmp.h','spl.h','syscall_subr.h',
  'task.h','thread.h','thread_act.h','trap.h','viper.h','vm_tuning.h','xpr.h',
  'disk.h','hilioctl.h','grfioctl.h'
)
$mdir = Join-Path $base 'machine'
New-Item -ItemType Directory -Force -Path $mdir | Out-Null
foreach ($f in $machineFiles) {
    $content = "/* Auto-generated shim: maps <machine/$f> to <hp_pa/$f>. */`n#include <hp_pa/$f>`n"
    [System.IO.File]::WriteAllText((Join-Path $mdir $f), $content, $utf8)
}

# mach/machine/ -> mach/hp_pa/
$machFiles = @(
  'boolean.h','exception.h','kern_return.h','rpc.h','syscall_sw.h',
  'thread_state.h','thread_status.h','vm_param.h','vm_types.h'
)
$mmdir = Join-Path $base 'mach\machine'
New-Item -ItemType Directory -Force -Path $mmdir | Out-Null
foreach ($f in $machFiles) {
    $content = "/* Auto-generated shim: maps <mach/machine/$f> to <mach/hp_pa/$f>. */`n#include <mach/hp_pa/$f>`n"
    [System.IO.File]::WriteAllText((Join-Path $mmdir $f), $content, $utf8)
}

Write-Output ("machine shims: " + $machineFiles.Count)
Write-Output ("mach/machine shims: " + $machFiles.Count)
