/*
 * jni_bridge.cpp
 *
 * JNI layer for the HP700 (Mach kernel) VM, project VirtuTerm.
 *
 * Stage 1 implementation: VM context lifecycle, ROM image mapping
 * (mmap, zero-copy) and LIF volume-header validation.  The CPU loop,
 * framebuffer, disk and debugger functions are wired up as callable
 * entry points; their full behaviour lands in later stages.
 */

#include <jni.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
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

static VMContext *vm_from_ptr(jlong vmPtr)
{
	return reinterpret_cast<VMContext *>(vmPtr);
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

/*
 * nativeStartCPU(vmPtr) -> void
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeStartCPU(
	JNIEnv *env, jobject /* this */, jlong vmPtr)
{
	VMContext *vm = vm_from_ptr(vmPtr);
	if (vm == NULL || !vm->initialized) {
		LOGE("nativeStartCPU: VM not initialized");
		return;
	}
	if (vm->running) {
		LOGI("nativeStartCPU: already running");
		return;
	}
	vm->running = JNI_TRUE;
	/* Stage 2: spawn the CPU execution thread here. */
	LOGI("nativeStartCPU: CPU loop requested (stage 2 implements the loop)");
}

/*
 * nativeStopCPU(vmPtr) -> void
 */
extern "C" JNIEXPORT void JNICALL
Java_io_mobile_VirtuTerm_VMService_nativeStopCPU(
	JNIEnv *env, jobject /* this */, jlong vmPtr)
{
	VMContext *vm = vm_from_ptr(vmPtr);
	if (vm == NULL)
		return;
	vm->running = JNI_FALSE;
	LOGI("nativeStopCPU: stop requested");
}

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
