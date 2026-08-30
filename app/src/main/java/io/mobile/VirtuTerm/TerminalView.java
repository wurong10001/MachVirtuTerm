package io.mobile.VirtuTerm;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

/**
 * Termux 风格终端渲染控件。
 *
 * - 等宽字体，深色背景，逐字符 Canvas 渲染；
 * - 缓冲行按屏幕列数做视觉回绕（缓冲区本身不换行）；
 * - 支持触摸上下滚动查看历史，跟随输出（自动滚动）开关；
 * - 光标块闪烁提示当前输出位置；
 * - 点击终端弹出输入法键盘，输入文本回显到终端
 *   （通过 {@link #setInputListener} 可把最终提交的输入转发给内核）。
 */
public class TerminalView extends View {

    /** 输入监听：输入法最终提交的文本（UTF-8 字节），供内核输入通道使用。 */
    public interface InputListener {
        void onInput(byte[] data);
    }

    private static final int FONT_SIZE_SP = 11;
    private static final int CURSOR_BLINK_MS = 500;
    private static final int TAB_STEP = 8;

    private final TerminalBuffer buffer = new TerminalBuffer();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint cursorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Handler handler = new Handler(Looper.getMainLooper());

    private int cols = 80;
    private int rows = 24;
    private int fontWidth = 1;
    private int fontHeight = 1;
    private int baseline = 0;

    /** 距底部的可视行偏移（0 = 跟随输出）。 */
    private int scrollOffset = 0;
    private boolean followOutput = true;
    private boolean cursorVisible = true;
    private boolean cursorBlinkEnabled = true;

    private InputListener inputListener = null;

    /* IME：软键盘像素高度（用于光标遮挡检测）与弹出前滚动状态。 */
    private int imeHeightPx = 0;
    private int savedScrollOffset = 0;
    private boolean savedFollowOutput = true;

    /* 触摸状态：区分“点击弹键盘”与“拖动滚动”。 */
    private float downX = 0f;
    private float downY = 0f;
    private long downTime = 0L;
    private float lastTouchY = 0f;
    private boolean scrolling = false;
    private final int touchSlop;

    private final Runnable cursorBlinkRunnable = new Runnable() {
        @Override
        public void run() {
            if (!cursorBlinkEnabled) {
                return;
            }
            cursorVisible = !cursorVisible;
            invalidate();
            handler.postDelayed(this, CURSOR_BLINK_MS);
        }
    };

    public TerminalView(Context context) {
        this(context, null);
    }

    public TerminalView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TerminalView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        paint.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.NORMAL));
        paint.setTextSize(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP, FONT_SIZE_SP, getResources().getDisplayMetrics()));
        paint.setColor(0xFFD6D6D6);
        cursorPaint.setColor(0x8C7BD88F);
        setFocusable(true);
        setFocusableInTouchMode(true);
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        // 监听软键盘 insets：弹出/收起时同步 IME 高度，并保持光标可见
        ViewCompat.setOnApplyWindowInsetsListener(this, new androidx.core.view.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsetsCompat onApplyWindowInsets(View v, WindowInsetsCompat insets) {
                int ime = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom;
                if (ime != imeHeightPx) {
                    onImeInsetsChanged(ime);
                }
                return insets;
            }
        });
    }

    /* ------------------------------------------------------------------ */
    /* 对外 API                                                            */
    /* ------------------------------------------------------------------ */

    /** 写入内核 tty 输出的原始字节。 */
    public void write(byte[] data) {
        buffer.write(data);
        if (followOutput) {
            if (imeHeightPx > 0) {
                ensureCursorVisible();
            } else {
                scrollOffset = 0;
            }
        }
        invalidate();
    }

    /** 写入文本（调试/测试用）。 */
    public void write(String text) {
        write(text.getBytes());
    }

    /** 清空屏幕。 */
    public void clearScreen() {
        buffer.clear();
        scrollOffset = 0;
        followOutput = true;
        invalidate();
    }

    /** 切换跟随输出模式，返回切换后的状态。 */
    public boolean toggleFollowOutput() {
        followOutput = !followOutput;
        if (followOutput) {
            scrollOffset = 0;
        }
        invalidate();
        return followOutput;
    }

    public boolean isFollowOutput() {
        return followOutput;
    }

    /** 注册输入监听（最终提交的输入文本，UTF-8 字节），传 null 取消。 */
    public void setInputListener(InputListener listener) {
        this.inputListener = listener;
    }

    /* ------------------------------------------------------------------ */
    /* 输入法（IME）                                                       */
    /* ------------------------------------------------------------------ */

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // 终端风格：多行文本（回车换行）、无拼写建议、不做 action 按钮
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_NORMAL
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        outAttrs.initialSelStart = buffer.getCursorCol();
        outAttrs.initialSelEnd = buffer.getCursorCol();
        return new TermInputConnection();
    }

    /** 请求焦点并弹出软键盘。 */
    private void showInput() {
        if (!hasFocus()) {
            requestFocus();
        }
        InputMethodManager imm =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.showSoftInput(this, 0);
        }
    }

    /**
     * 把一行输入写入终端（回显），并做行尾归一化：\n、\r 都显示为 \r\n。
     */
    private void echoInput(String text) {
        StringBuilder out = new StringBuilder(text.length());
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == '\r' || c == '\n') {
                out.append("\r\n");
            } else {
                out.append(c);
            }
        }
        write(out.toString());
    }

    /**
     * 输入法连接：commitText / setComposingText / deleteSurroundingText
     * 都转发为终端回显，最终提交的文本经 {@link #inputListener} 传出。
     */
    private final class TermInputConnection extends BaseInputConnection {

        /** 未提交的组合文本（如拼音候选），回显时先删除再重写。 */
        private final StringBuilder composing = new StringBuilder();

        TermInputConnection() {
            super(TerminalView.this, true);
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            if (text == null) {
                return true;
            }
            clearComposing();
            String s = text.toString();
            echoInput(s);
            if (inputListener != null && !s.isEmpty()) {
                inputListener.onInput(s.getBytes());
            }
            return true;
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            if (text == null) {
                text = "";
            }
            clearComposing();
            composing.append(text);
            echoInput(text.toString());
            return true;
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            clearComposing();
            for (int i = 0; i < beforeLength; i++) {
                buffer.backspace();
            }
            invalidate();
            return true;
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                switch (event.getKeyCode()) {
                    case KeyEvent.KEYCODE_ENTER:
                        // 部分输入法（如 Gboard）回车走按键事件而非 commitText
                        clearComposing();
                        echoInput("\r\n");
                        return true;
                    case KeyEvent.KEYCODE_DEL:
                        clearComposing();
                        buffer.backspace();
                        invalidate();
                        return true;
                    default:
                        break;
                }
            }
            return true;
        }

        @Override
        public boolean performEditorAction(int actionCode) {
            // IME_ACTION_NONE / UNSPECIFIED：无 action 定义时回车当作换行
            if (actionCode == EditorInfo.IME_ACTION_NONE
                    || actionCode == EditorInfo.IME_ACTION_UNSPECIFIED) {
                clearComposing();
                echoInput("\r\n");
            }
            return true;
        }

        /** 删除当前组合文本（若存在）。 */
        private void clearComposing() {
            if (composing.length() > 0) {
                for (int i = 0; i < composing.length(); i++) {
                    buffer.backspace();
                }
                composing.setLength(0);
                invalidate();
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* 生命周期 / 尺寸                                                     */
    /* ------------------------------------------------------------------ */

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        handler.removeCallbacks(cursorBlinkRunnable);
        handler.postDelayed(cursorBlinkRunnable, CURSOR_BLINK_MS);
    }

    @Override
    protected void onDetachedFromWindow() {
        handler.removeCallbacks(cursorBlinkRunnable);
        super.onDetachedFromWindow();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        measureFont();
        cols = Math.max(1, w / fontWidth);
        rows = Math.max(1, h / fontHeight);
        ensureCursorVisible();
    }

    private void measureFont() {
        Paint.FontMetricsInt fm = paint.getFontMetricsInt();
        fontHeight = Math.max(1, fm.descent - fm.ascent);
        baseline = -fm.ascent;
        fontWidth = Math.max(1, (int) paint.measureText("M"));
    }

    /* ------------------------------------------------------------------ */
    /* 绘制                                                                */
    /* ------------------------------------------------------------------ */

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.drawColor(0xFF0E0E0E);

        int lineCount = buffer.getLineCount();
        if (lineCount == 0) {
            return;
        }

        // 每行按 cols 分段，得到视觉显示行数
        int totalDisplayLines = 0;
        int[] segmentsPerLine = new int[lineCount];
        for (int i = 0; i < lineCount; i++) {
            int len = Math.max(1, buffer.getLine(i).length());
            int seg = (len + cols - 1) / cols;
            segmentsPerLine[i] = seg;
            totalDisplayLines += seg;
        }

        int maxScroll = Math.max(0, totalDisplayLines - rows);
        if (scrollOffset > maxScroll) {
            scrollOffset = maxScroll;
        }
        int startDisplay = totalDisplayLines - rows - scrollOffset;
        if (startDisplay < 0) {
            startDisplay = 0;
        }

        // 光标所在逻辑行 / 段（当前输出总在最后一行）
        int cursorLineIdx = lineCount - 1;
        int cursorCol = buffer.getCursorCol();
        int cursorSegment = cursorCol / cols;
        boolean cursorInView = false;
        int cursorX = 0;
        int cursorRowInView = 0;

        paint.setColor(0xFFD6D6D6);
        int display = 0;
        int y = baseline;
        for (int i = 0; i < lineCount; i++) {
            CharSequence line = buffer.getLine(i);
            int segCount = segmentsPerLine[i];
            for (int k = 0; k < segCount; k++) {
                if (display >= startDisplay && display < startDisplay + rows) {
                    int from = k * cols;
                    int to = Math.min(line.length(), from + cols);
                    canvas.drawText(line, from, to, 0, y, paint);
                    y += fontHeight;
                }
                if (i == cursorLineIdx && k == cursorSegment) {
                    cursorInView = true;
                    cursorX = (cursorCol % cols) * fontWidth;
                    cursorRowInView = display;
                }
                display++;
            }
            if (display >= startDisplay + rows) {
                break;
            }
        }

        // 光标块（仅跟随输出且光标在可视区内时绘制；
        // IME 自动滚动时 scrollOffset>0 但 followOutput 仍为 true，光标照常显示）
        if (cursorVisible && cursorInView && followOutput) {
            int top = (cursorRowInView - startDisplay) * fontHeight;
            canvas.drawRect(cursorX, top, cursorX + fontWidth, top + fontHeight, cursorPaint);
        }
    }

    /* ------------------------------------------------------------------ */
    /* 触摸滚动                                                            */
    /* ------------------------------------------------------------------ */

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = event.getX();
                downY = event.getY();
                downTime = event.getEventTime();
                lastTouchY = downY;
                scrolling = false;
                return true;
            case MotionEvent.ACTION_MOVE:
                float dy = event.getY() - lastTouchY;
                if (!scrolling) {
                    // 超过触摸阈值才判定为滚动，普通点击留给 ACTION_UP 弹键盘
                    if (Math.abs(event.getY() - downY) > touchSlop
                            || Math.abs(event.getX() - downX) > touchSlop) {
                        scrolling = true;
                    }
                }
                if (!scrolling) {
                    return true;
                }
                lastTouchY = event.getY();
                int deltaLines = (int) (dy / fontHeight);
                if (deltaLines != 0) {
                    scrollOffset = clamp(scrollOffset - deltaLines,
                            0, computeMaxScroll());
                    followOutput = (scrollOffset == 0);
                    invalidate();
                }
                return true;
            case MotionEvent.ACTION_UP:
                if (!scrolling && isTap(event)) {
                    showInput();
                }
                scrolling = false;
                return true;
            case MotionEvent.ACTION_CANCEL:
                scrolling = false;
                return true;
            default:
                return super.onTouchEvent(event);
        }
    }

    /** 判断是否为一次短按点击（位移与时长都小于阈值）。 */
    private boolean isTap(MotionEvent upEvent) {
        return Math.abs(upEvent.getX() - downX) <= touchSlop
                && Math.abs(upEvent.getY() - downY) <= touchSlop
                && (upEvent.getEventTime() - downTime) < ViewConfiguration.getTapTimeout();
    }

    /* ------------------------------------------------------------------ */
    /* IME 遮挡与光标可见性                                                 */
    /* ------------------------------------------------------------------ */

    /** IME insets 变化回调：弹出时记住原滚动状态并滚到光标可见，收起时恢复。 */
    private void onImeInsetsChanged(int imePx) {
        boolean wasVisible = imeHeightPx > 0;
        imeHeightPx = imePx;
        boolean nowVisible = imePx > 0;
        if (nowVisible && !wasVisible) {
            savedScrollOffset = scrollOffset;
            savedFollowOutput = followOutput;
            ensureCursorVisible();
        } else if (!nowVisible && wasVisible) {
            scrollOffset = savedScrollOffset;
            followOutput = savedFollowOutput;
        }
        invalidate();
    }

    /**
     * 若光标所在行被软键盘遮挡，自动向上滚动使其可见。
     * 仅在跟随输出模式下生效，避免打扰用户手动查看历史。
     */
    private void ensureCursorVisible() {
        if (imeHeightPx <= 0 || !followOutput) {
            return;
        }
        int lineCount = buffer.getLineCount();
        if (lineCount == 0) {
            return;
        }
        int total = 0;
        for (int i = 0; i < lineCount; i++) {
            int len = Math.max(1, buffer.getLine(i).length());
            total += (len + cols - 1) / cols;
        }
        // 光标在最后一个逻辑行的最后一段（最新一行）
        int cursorDisplay = total - 1;
        int startDisplay = Math.max(0, total - rows - scrollOffset);
        int cursorBottom = (cursorDisplay - startDisplay) * fontHeight + fontHeight;
        int visibleBottom = getHeight() - imeHeightPx;
        if (cursorBottom > visibleBottom) {
            // 需要额外向上滚动多少行才能让光标行露出 IME 上沿
            int needed = (cursorBottom - visibleBottom + fontHeight - 1) / fontHeight;
            scrollOffset = clamp(scrollOffset + needed, 0, Math.max(0, total - rows));
            invalidate();
        }
    }

    private int computeMaxScroll() {
        int lineCount = buffer.getLineCount();
        int total = 0;
        for (int i = 0; i < lineCount; i++) {
            int len = Math.max(1, buffer.getLine(i).length());
            total += (len + cols - 1) / cols;
        }
        return Math.max(0, total - rows);
    }

    private static int clamp(int v, int min, int max) {
        return Math.max(min, Math.min(max, v));
    }
}
