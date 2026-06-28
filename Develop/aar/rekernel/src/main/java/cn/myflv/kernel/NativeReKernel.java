package cn.myflv.kernel;

public class NativeReKernel {

    static {
        System.loadLibrary("ReKernel");
    }

    public static native boolean startListening(ReKernelCallback callback);

    public static native void stopListening();


    public static native boolean addMonitorNet(int uid);
    public static native boolean delMonitorNet(int uid);


}
