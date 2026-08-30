package io.mobile.VirtuTerm;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * 轻量终端文本缓冲区。
 *
 * 以“物理行”为单位保存文本，不关心屏幕宽度：行可以很长，
 * 由 {@link TerminalView} 在渲染时按当前列数做视觉回绕。
 * 这样缓冲区与屏幕尺寸/旋转无关，历史内容不会因换行而重排。
 *
 * 支持的控制字符：
 *   \r   回车（光标回列首）
 *   \n   换行（追加新行）
 *   \b   退格
 *   \t   制表（步进 8 列）
 *   \f   清屏
 *
 * 线程模型：所有方法由 UI 线程调用（native 回调通过 Handler post 到 UI 线程），
 * 方法仍加 synchronized 以防未来接入其他线程。
 */
public class TerminalBuffer {

    /** 保留的最大历史行数，超出后丢弃最旧行。 */
    public static final int MAX_LINES = 2000;

    private final List<StringBuilder> lines = new ArrayList<>();
    private int cursorCol = 0;

    public TerminalBuffer() {
        lines.add(new StringBuilder());
    }

    /** 追加 UTF-8 原始字节流（native tty 输出直接使用）。 */
    public synchronized void write(byte[] data) {
        write(new String(data, StandardCharsets.UTF_8));
    }

    /** 追加文本，逐字符解析控制码。 */
    public synchronized void write(String text) {
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            switch (c) {
                case '\r':
                    cursorCol = 0;
                    break;
                case '\n':
                    newline();
                    break;
                case '\b':
                    cursorCol = Math.max(0, cursorCol - 1);
                    break;
                case '\t':
                    cursorCol = (cursorCol / 8 + 1) * 8;
                    break;
                case '\f':
                    clear();
                    break;
                default:
                    if (c >= 0x20) {
                        putChar(c);
                    }
                    break;
            }
        }
    }

    /** 退格删除：删除光标前一字符（区别于控制码 '\b' 仅把光标回退一列）。 */
    public synchronized void backspace() {
        if (cursorCol <= 0) {
            return;
        }
        StringBuilder line = last();
        cursorCol--;
        if (cursorCol < line.length()) {
            line.deleteCharAt(cursorCol);
        }
    }

    private void newline() {
        lines.add(new StringBuilder());
        cursorCol = 0;
        trim();
    }

    private void putChar(char c) {
        StringBuilder line = last();
        while (line.length() < cursorCol) {
            line.append(' ');
        }
        if (cursorCol < line.length()) {
            line.setCharAt(cursorCol, c);
        } else {
            line.append(c);
        }
        cursorCol++;
    }

    private void trim() {
        while (lines.size() > MAX_LINES) {
            lines.remove(0);
        }
    }

    /** 清空所有内容。 */
    public synchronized void clear() {
        lines.clear();
        lines.add(new StringBuilder());
        cursorCol = 0;
    }

    public synchronized int getLineCount() {
        return lines.size();
    }

    /** 返回第 index 行的文本（读侧不做线程同步复制，调用方须在 UI 线程）。 */
    public synchronized CharSequence getLine(int index) {
        return lines.get(index);
    }

    public synchronized int getCursorCol() {
        return cursorCol;
    }

    private StringBuilder last() {
        return lines.get(lines.size() - 1);
    }
}
