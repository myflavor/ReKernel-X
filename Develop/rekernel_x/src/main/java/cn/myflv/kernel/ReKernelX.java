package cn.myflv.kernel;

public class ReKernelX {

    static {
        System.loadLibrary("ReKernelX");
    }

    /**
     * Install (or replace) the business-event callback. May be called with
     * {@code null} to clear a previously installed callback. Must be set
     * before {@link #pollEvent()} will dispatch anything.
     *
     * The callback runs on whatever thread is blocked in {@link #pollEvent()};
     * its implementation MUST NOT call {@link #connect()}, {@link #disconnect()}
     * or {@link #pollEvent()} — doing so would self-deadlock.
     */
    public static native void setCallback(ReKernelXCallback callback);

    /**
     * Non-blocking: build the netlink socket, resolve the genl family and join
     * the multicast group. Returns {@code true} on success.
     *
     * @return {@code true} if connected and ready to {@link #pollEvent()}
     */
    public static native boolean connect();

    /**
     * Call from a thread OTHER than the one blocked in {@link #pollEvent()}
     * (typically the thread that called {@link #connect()}). Shuts down the
     * socket, which wakes {@link #pollEvent()} and makes it return;
     * {@link #pollEvent()} closes the fd on its way out.
     */
    public static native void disconnect();

    /**
     * BLOCKS the calling thread. Receives and dispatches kernel events until
     * the connection drops ({@link #disconnect()} or a fatal recv error).
     * Transient errors are retried; on overflow some events are dropped.
     * Returning means "disconnected"; the caller owns any reconnect logic.
     */
    public static native void pollEvent();

    public static native boolean addMonitorNet(int uid);

    public static native boolean delMonitorNet(int uid);

}
