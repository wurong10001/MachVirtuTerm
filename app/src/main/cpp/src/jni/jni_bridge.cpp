/*
 * jni_bridge.cpp
 *
 * JNI layer for the HP700 (Mach kernel) VM, project VirtuTerm.
 *
 * Stage 1 : VM context lifecycle, ROM image mapping (mmap, zero-copy) and
 *           LIF volume-header validation.
 * TTY     : kernel console/tty output is fed into a bounded ring buffer and
 *           pushed to the Java TerminalView through a JNI callback
 *           (VMService.setTtyCallback / nativeSetTtyCallback).
 * Stage 2+: the CPU thread currently emits a simulated kernel boot log to
 *           prove the tty pipeline end-to-end. Once the real CPU emulator
 *           lands, route the kernel's printf/cnputc into tty_puts().
 */

#include <jni.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/log.h>

#define LOG_TAG "VirtuTerm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ---- LIF volume header (mirrors kernel/src/.../HP700/lif.h) ---- */
#define LIF_HEADER_OFF	0
#define LIF_MAGIC	0x8000

struct lif_header {
	unsigned short	magic;
	unsigned char	name[6];
};

/* ---- VM context ---- */
#define MAX_PATH_LEN	1024

typedef struct VMContext {
	void		*rom_base;	/* mmap'ed ROM image */
	size_t		rom_size;
	char		rom_path[MAX_PATH_LEN];
	char		disk_path[MAX_PATH_LEN];
	jint		mem_size;	/* requested guest RAM (MB) */
	jboolean	initialized;
	volatile jboolean running;
	/* stage 2+: cpu loop thread, framebuffer, hil queue, disk cache... */
} VMContext;

static VMContext *g_vm = NULL;

/* ------------------------------------------------------------------ */
/* TTY channel                                                         */
/* ------------------------------------------------------------------ */

#define TTY_RING_SIZE	(64 * 1024)

typedef struct TtyRing {
	char		buf[TTY_RING_SIZE];
	size_t		head;		/* next write position */
	size_t		tail;		/* next read position  */
	pthread_mutex_t	lock;
} TtyRing;

static TtyRing	g_tty;
static JavaVM	*g_jvm = NULL;
static jobject	g_ttyCallback = NULL;	/* global ref, cleared by caller */
static jmethodID g_onTtyOutput = NULL;

/*
 * tty_puts / tty_putc -- kernel console/tty output entry.
 *
 * Stage 2 wiring points:
 *   - boot-stage printf : redirect the putc in stand/HP700/boot/prf.c here;
 *   - runtime cnputc    : forward kern/printf.c's cnputc (or the HP700
 *                         iteputchar path) into tty_puts();
 *   - simplest approach : provide a putc wrapper in the android_compat layer
 *                         that calls tty_putc for every byte.
 */
static void tty_push_locked(char c)
{
	g_tty.buf[g_tty.head] = c;
	g_tty.head = (g_tty.head + 1) % TTY_RING_SIZE;
	if (g_tty.head == g_tty.tail) {		/* ring full: drop oldest */
		g_tty.tail = (g_tty.tail + 1) % TTY_RING_SIZE;
	}
}

static void tty_flush(void)
{
	if (g_ttyCallback == NULL || g_jvm == NULL) {
		g_tty.head = g_tty.tail = 0;
		return;
	}

	JNIEnv *env = NULL;
	jint rs = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
	jboolean attached = JNI_FALSE;
	if (rs == JNI_EDETACHED) {
		if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK)
			return;
		attached = JNI_TRUE;
	}
	if (env == NULL) {
		if (attached)
			g_jvm->DetachCurrentThread();
		return;
	}

	pthread_mutex_lock(&g_tty.lock);
	size_t len = (g_tty.head >= g_tty.tail)
		     ? g_tty.head - g_tty.tail
		     : TTY_RING_SIZE - g_tty.tail + g_tty.head;
	if (len == 0) {
		pthread_mutex_unlock(&g_tty.lock);
		if (attached)
			g_jvm->DetachCurrentThread();
		return;
	}
	size_t part1 = (g_tty.tail + len <= TTY_RING_SIZE)
		      ? len : TTY_RING_SIZE - g_tty.tail;
	jbyteArray arr = env->NewByteArray((jsize)len);
	if (arr != NULL) {
		jbyte *dst = env->GetByteArrayElements(arr, NULL);
		if (dst != NULL) {
			memcpy(dst, g_tty.buf + g_tty.tail, part1);
			if (part1 < len)
				memcpy(dst + part1, g_tty.buf, len - part1);
			env->ReleaseByteArrayElements(arr, dst, 0);
		}
	}
	g_tty.head = g_tty.tail = 0;
	pthread_mutex_unlock(&g_tty.lock);

	if (arr != NULL) {
		env->CallVoidMethod(g_ttyCallback, g_onTtyOutput, arr);
		env->DeleteLocalRef(arr);
	}
	if (attached)
		g_jvm->DetachCurrentThread();
}

extern "C" void tty_putc(char c)
{
	pthread_mutex_lock(&g_tty.lock);
	tty_push_locked(c);
	pthread_mutex_unlock(&g_tty.lock);
	tty_flush();
}

extern "C" void tty_puts(const char *s)
{
	if (s == NULL)
		return;
	pthread_mutex_lock(&g_tty.lock);
	while (*s)
		tty_push_locked(*s++);
	pthread_mutex_unlock(&g_tty.lock);
	tty_flush();
}

/* ------------------------------------------------------------------ */
/* JNI glue                                                            */
/* ------------------------------------------------------------------ */

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	g_jvm = vm;
	pthread_mutex_init(&g_tty.lock, NULL);
	return JNI_VERSION_1_6;
}

static char *copy_jstring(JNIEnv *env, jstring s, char *buf, size_t len)
{
	if (s == NULL)
		return NULL;
	const char *utf = env->GetStringUTFChars(s, NULL);
	if (utf == NULL)
		return NULL;
	strncpy(buf, utf, len - 1);
	buf[len - 1] = '\0';
	env->ReleaseStringUTFChars(s, utf);
	return buf;
}

/*
 * nativeInitVM(romPath, diskPath, memSize) -> boolean
 *
 * Maps the ROM image and validates its LIF volume header.
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeInitVM(
	JNIEnv *env, jobject /* this */,
	jstring romPath, jstring diskPath, jint memSize)
{
	if (g_vm != NULL) {
		munmap(g_vm->rom_base, g_vm->rom_size);
		free(g_vm);
		g_vm = NULL;
	}

	VMContext *vm = (VMContext *)calloc(1, sizeof(VMContext));
	if (vm == NULL) {
		LOGE("nativeInitVM: calloc failed");
		return JNI_FALSE;
	}

	if (copy_jstring(env, romPath, vm->rom_path, sizeof(vm->rom_path)) == NULL) {
		LOGE("nativeInitVM: bad romPath");
		free(vm);
		return JNI_FALSE;
	}
	if (diskPath != NULL)
		copy_jstring(env, diskPath, vm->disk_path, sizeof(vm->disk_path));

	vm->mem_size = memSize;
	vm->running = JNI_FALSE;
	vm->initialized = JNI_FALSE;

	int fd = open(vm->rom_path, O_RDONLY);
	if (fd < 0) {
		LOGE("nativeInitVM: cannot open ROM '%s'", vm->rom_path);
		free(vm);
		return JNI_FALSE;
	}

	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(struct lif_header)) {
		LOGE("nativeInitVM: bad ROM file size");
		close(fd);
		free(vm);
		return JNI_FALSE;
	}

	void *base = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (base == MAP_FAILED) {
		LOGE("nativeInitVM: mmap failed");
		free(vm);
		return JNI_FALSE;
	}

	vm->rom_base = base;
	vm->rom_size = (size_t)st.st_size;

	/* Validate LIF volume header at offset 0. */
	struct lif_header *hdr = (struct lif_header *)base;
	if (hdr->magic != LIF_MAGIC) {
		LOGE("nativeInitVM: bad LIF magic 0x%04x (expected 0x%04x)",
		     hdr->magic, LIF_MAGIC);
		munmap(base, vm->rom_size);
		free(vm);
		return JNI_FALSE;
	}

	LOGI("nativeInitVM: ROM '%s' mapped (%d bytes), LIF header OK, memSize=%d",
	     vm->rom_path, (int)vm->rom_size, (int)memSize);

	vm->initialized = JNI_TRUE;
	g_vm = vm;
	return JNI_TRUE;
}

/* ------------------------------------------------------------------ */
/* CPU thread (simulated boot log until the real emulator lands)       */
/* ------------------------------------------------------------------ */

static void *cpu_loop(void *arg)
{
	JNIEnv *env = NULL;
	if (g_jvm != NULL)
		g_jvm->AttachCurrentThread(&env, NULL);

	VMContext *vm = (VMContext *)arg;
	char line[256];

	/*
	 * Simulated kernel boot log.  Stage 2 replaces this whole block with
	 * the real CPU loop; every kernel printf just flows through tty_puts.
	 */
	tty_puts("\r\n*** VirtuTerm 0.1 --- HP700 (Mach 3.0) 虚拟机 ***\r\n\r\n");

	if (vm != NULL && vm->initialized) {
		snprintf(line, sizeof(line),
			 "[ROM]   映射 %s (%zu 字节)，LIF 卷头校验 OK (magic 0x%04x)\r\n",
			 vm->rom_path, vm->rom_size, LIF_MAGIC);
		tty_puts(line);
	} else {
		tty_puts("[ROM]   未找到 ROM 镜像 -> 演示模式（Stage 2 接入真实内核输出）\r\n");
	}

	static const char *bootLines[] = {
		"[CPU]   HP PA-RISC 1.1 (PA-7000)，主频 50 MHz，缓存 32K/32K\r\n",
		"[MMU]   D-TLB / I-TLB 初始化完成，页表建立\r\n",
		"[VM]    客户机物理内存 128 MB\r\n",
		"[DEV]   GSC/ASI 总线枚举: 2 个插槽\r\n",
		"[DEV]   grf0 (STI) 1280x1024 帧缓冲就绪\r\n",
		"[DEV]   hil0 键盘/鼠标控制器就绪\r\n",
		"[DEV]   lan0 (Intel 82596) 以太网复位完成\r\n",
		"[TTY]   tty0: 控制台设备就绪\r\n",
		"[MACH]  Mach 3.0 内核镜像校验通过\r\n",
		"[MACH]  bootstrap: 启动 cpu0\r\n",
		"[MACH]  vm_page_bootstrap: 32768 物理页可用\r\n",
		"[MACH]  zone_init: 建立 4 个内存区\r\n",
		"[IPC]   ipc_init: 端口命名空间建立\r\n",
		"[MACH]  调度器初始化完成\r\n",
		"[MACH]  设备启动服务 (device_server) 已注册\r\n",
		"\r\n*** 内核启动完成，控制台就绪。 ***\r\n",
		"等待 Stage 2 CPU 模拟器接管……\r\n",
	};
	for (size_t i = 0;
	     i < sizeof(bootLines) / sizeof(bootLines[0]); i++) {
		tty_puts(bootLines[i]);
		usleep(60 * 1000);	/* simulate the kernel print cadence */
	}

	if (vm != NULL)
		vm->running = JNI_FALSE;
	if (env != NULL)
		g_jvm->DetachCurrentThread();
	return NULL;
}

/*
 * nativeStartCPU() -> void
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeStartCPU(
	JNIEnv *env, jobject /* this */)
{
	if (g_vm != NULL && g_vm->running) {
		LOGI("nativeStartCPU: already running");
		return;
	}
	pthread_t tid;
	if (pthread_create(&tid, NULL, cpu_loop, g_vm) != 0) {
		LOGE("nativeStartCPU: pthread_create failed");
		return;
	}
	pthread_detach(tid);
	if (g_vm != NULL)
		g_vm->running = JNI_TRUE;
	LOGI("nativeStartCPU: CPU thread spawned");
}

/*
 * nativeStopCPU() -> void
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeStopCPU(
	JNIEnv *env, jobject /* this */)
{
	if (g_vm != NULL)
		g_vm->running = JNI_FALSE;
	LOGI("nativeStopCPU: stop requested");
}

/*
 * nativeSetTtyCallback(TtyCallback) -> void
 *
 * Registers (or clears) the Java callback receiving kernel tty output.
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeSetTtyCallback(
	JNIEnv *env, jobject /* this */, jobject cb)
{
	if (g_ttyCallback != NULL) {
		env->DeleteGlobalRef(g_ttyCallback);
		g_ttyCallback = NULL;
		g_onTtyOutput = NULL;
	}
	if (cb != NULL) {
		jclass cls = env->GetObjectClass(cb);
		g_onTtyOutput = env->GetMethodID(cls, "onTtyOutput", "([B)V");
		if (g_onTtyOutput == NULL) {
			LOGE("nativeSetTtyCallback: cannot resolve onTtyOutput");
			env->ExceptionClear();
			return;
		}
		g_ttyCallback = env->NewGlobalRef(cb);
	}
	LOGI("nativeSetTtyCallback: %s", cb != NULL ? "registered" : "cleared");
}

/* ------------------------------------------------------------------ */
/* Stage 2/3/5 entry points (stubs until their stages land)            */
/* ------------------------------------------------------------------ */

/*
 * nativeInjectKey(vmPtr, keyCode, isDown) -> void
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeInjectKey(
	JNIEnv *env, jobject /* this */,
	jlong vmPtr, jint keyCode, jboolean isDown)
{
	/* Stage 2: translate Android keyCode to HP HIL scan code. */
	LOGI("nativeInjectKey: keyCode=%d isDown=%d", (int)keyCode, (int)isDown);
}

/*
 * nativeGetFrameBuffer(vmPtr) -> DirectByteBuffer
 */
extern "C" JNIEXPORT jobject JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeGetFrameBuffer(
	JNIEnv *env, jobject /* this */, jlong vmPtr)
{
	/* Stage 2: return a DirectByteBuffer over the STI framebuffer. */
	return NULL;
}

/*
 * nativeGetScreenWidth(vmPtr) -> int
 */
extern "C" JNIEXPORT jint JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeGetScreenWidth(
	JNIEnv *env, jobject /* this */, jlong vmPtr)
{
	/* Stage 2: current STI resolution width. */
	return 0;
}

/*
 * nativeGetScreenHeight(vmPtr) -> int
 */
extern "C" JNIEXPORT jint JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeGetScreenHeight(
	JNIEnv *env, jobject /* this */, jlong vmPtr)
{
	/* Stage 2: current STI resolution height. */
	return 0;
}

/*
 * nativeWriteDisk(vmPtr, offset, data) -> int
 */
extern "C" JNIEXPORT jint JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeWriteDisk(
	JNIEnv *env, jobject /* this */,
	jlong vmPtr, jlong offset, jbyteArray data)
{
	/* Stage 3: write-through + write-back disk cache. */
	LOGI("nativeWriteDisk: offset=%lld", (long long)offset);
	return -1;
}

/*
 * nativeReadDisk(vmPtr, offset, length) -> byte[]
 */
extern "C" JNIEXPORT jbyteArray JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeReadDisk(
	JNIEnv *env, jobject /* this */,
	jlong vmPtr, jlong offset, jint length)
{
	/* Stage 3: disk read path. */
	LOGI("nativeReadDisk: offset=%lld len=%d", (long long)offset, (int)length);
	return NULL;
}

/*
 * nativeAttachDebugger(vmPtr, port) -> boolean
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeAttachDebugger(
	JNIEnv *env, jobject /* this */, jlong vmPtr, jint port)
{
	/* Stage 5: KGDB-over-TCP, GDB Remote Serial Protocol. */
	LOGI("nativeAttachDebugger: port=%d", (int)port);
	return JNI_FALSE;
}
