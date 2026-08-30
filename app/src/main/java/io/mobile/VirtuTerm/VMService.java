package io.mobile.VirtuTerm;

/**
 * HP700 VM 的 JNI 桥接服务。
 *
 * 持有 native 方法声明，并把内核 tty 输出回调分发到 UI 层。
 * 所有 native 回调在 native 线程上触发，UI 侧须自行切回主线程。
 */
public final class VMService {

    /** 内核 tty 输出回调：收到内核 console/tty 写出的原始字节。 */
    public interface TtyCallback {
        void onTtyOutput(byte[] data);
    }

    static {
        System.loadLibrary("VirtuTerm");
    }

    private VMService() {
    }

    /** 注册 tty 输出回调（传 null 取消注册并释放 native 引用）。 */
    public static void setTtyCallback(TtyCallback callback) {
        nativeSetTtyCallback(callback);
    }

    /**
     * 初始化 VM：映射 ROM 镜像并校验 LIF 卷头。
     *
     * @return true 表示 ROM 有效；false 表示 ROM 缺失/损坏（仍可进入演示模式）。
     */
    public static native boolean nativeInitVM(String romPath, String diskPath, int memSize);

    /** 启动 CPU 执行（在独立线程中运行）。 */
    public static native void nativeStartCPU();

    /** 请求停止 CPU 执行。 */
    public static native void nativeStopCPU();

    /** 注入键盘事件（Stage 2 实现）。 */
    public static native void nativeInjectKey(long vmPtr, int keyCode, boolean isDown);

    /** 获取 STI 帧缓冲 DirectByteBuffer（Stage 2 实现）。 */
    public static native Object nativeGetFrameBuffer(long vmPtr);

    /** 当前帧缓冲宽度（Stage 2 实现）。 */
    public static native int nativeGetScreenWidth(long vmPtr);

    /** 当前帧缓冲高度（Stage 2 实现）。 */
    public static native int nativeGetScreenHeight(long vmPtr);

    /** 写虚拟磁盘（Stage 3 实现）。 */
    public static native int nativeWriteDisk(long vmPtr, long offset, byte[] data);

    /** 读虚拟磁盘（Stage 3 实现）。 */
    public static native byte[] nativeReadDisk(long vmPtr, long offset, int length);

    /** 启动 KGDB 远程调试监听（Stage 5 实现）。 */
    public static native boolean nativeAttachDebugger(long vmPtr, int port);

    private static native void nativeSetTtyCallback(TtyCallback callback);
}
