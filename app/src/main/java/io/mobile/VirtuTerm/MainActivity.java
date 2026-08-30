package io.mobile.VirtuTerm;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.appbar.MaterialToolbar;

import java.io.File;

import io.mobile.VirtuTerm.databinding.ActivityMainBinding;

/**
 * 主活动：Termux 风格终端。
 *
 * 界面上方为工具栏（VM 状态指示），中间是终端显示区，
 * 底部为快捷操作栏（清屏 / 跟随输出 / 电源）。
 * 内核 tty 输出通过 JNI 回调实时写入终端。
 */
public class MainActivity extends AppCompatActivity {

    private static final int VM_MEM_SIZE_MB = 128;

    private ActivityMainBinding binding;
    private TerminalView terminal;
    private MaterialToolbar toolbar;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private boolean vmRunning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        terminal = binding.terminal;
        toolbar = binding.toolbar;
        setVmStatus(false);

        // 内核 tty 输出 -> 终端（native 线程回调，切回主线程写入）
        VMService.setTtyCallback(data -> ui.post(() -> terminal.write(data)));

        // 终端输入 -> 内核（Stage 2：注入 HIL/串口队列；当前阶段仅终端回显）
        terminal.setInputListener(data -> {
            // VMService.nativeInjectKey(...) / 串口写入口将在 Stage 2 接入
        });

        binding.btnClear.setOnClickListener(v -> terminal.clearScreen());
        binding.btnScroll.setOnClickListener(v -> {
            boolean follow = terminal.toggleFollowOutput();
            binding.btnScroll.setSelected(follow);
            binding.btnScroll.setColorFilter(getColor(follow
                    ? R.color.terminal_green : R.color.toolbar_icon));
        });
        binding.btnPower.setOnClickListener(v -> togglePower());
    }

    private void togglePower() {
        if (vmRunning) {
            stopVm();
        } else {
            startVm();
        }
    }

    private void startVm() {
        String romPath = new File(getFilesDir(), "rom.hp700").getAbsolutePath();
        boolean ok = VMService.nativeInitVM(romPath, null, VM_MEM_SIZE_MB);
        if (ok) {
            terminal.write("[APP]   ROM 加载成功，启动内核...\r\n");
        } else {
            terminal.write("[APP]   未找到 ROM 镜像，以演示模式启动（Stage 2 接入真实内核输出）\r\n");
        }
        VMService.nativeStartCPU();
        vmRunning = true;
        setVmStatus(true);
    }

    private void stopVm() {
        VMService.nativeStopCPU();
        vmRunning = false;
        setVmStatus(false);
        terminal.write("\r\n[APP]   虚拟机已停止。\r\n");
    }

    private void setVmStatus(boolean running) {
        toolbar.setSubtitle(running
                ? getString(R.string.toolbar_subtitle_running)
                : getString(R.string.toolbar_subtitle_stopped));
        toolbar.setSubtitleTextColor(getColor(running
                ? R.color.status_running
                : R.color.status_stopped));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        VMService.setTtyCallback(null);
        VMService.nativeStopCPU();
    }
}
