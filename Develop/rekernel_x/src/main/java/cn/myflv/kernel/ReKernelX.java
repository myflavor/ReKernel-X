package cn.myflv.kernel;

public class ReKernelX {

    static {
        System.loadLibrary("ReKernelX");
    }

    public static final int FREE_ASYNC_SKIP = 1;
    public static final int FREE_ASYNC_BY_CODE = 2;
    public static final int FREE_ASYNC_BY_DATA = 3;

    public static native void setCallback(ReKernelXCallback callback);

    public static native boolean connect();

    public static native void disconnect();

    public static native void pollEvent();

    public static native boolean addMonitorNet(int uid);

    public static native boolean delMonitorNet(int uid);

    public static native boolean addFreeAsync(String rpcName, int code, int strategy);

    public static native boolean delFreeAsync(String rpcName, int code);

}
