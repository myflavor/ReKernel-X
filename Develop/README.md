# Develop

ReKernel-X 用户态客户端，封装为 Android AAR

提供 Java + JNI 封装，接入时无需手写 netlink

## 构建

需要 JDK 17、Android SDK（`platforms;android-34`、`build-tools;34.0.0`、`ndk;27.0.12077973`、`cmake;3.22.1`）

```bash
export JAVA_HOME=/path/to/jdk17
export ANDROID_HOME=/path/to/Android/Sdk
./gradlew :rekernel_x:assembleRelease
```

输出：`rekernel_x/build/outputs/aar/rekernel_x-release.aar`

## 接入示例

引入 [GitHub Releases](https://github.com/myflavor/ReKernel-X/releases) 下载的 AAR，或自行编译的 AAR

```java
import cn.myflv.kernel.ReKernelX;
import cn.myflv.kernel.ReKernelXCallback;

import static cn.myflv.kernel.ReKernelXCallback.*;

new Thread(() -> {
    if (ReKernelX.connect()) {
        ReKernelX.setCallback(new ReKernelXCallback() {
            @Override
            public void binder(int binderType, int oneway, int fromUid, int fromPid,
                               int targetUid, int targetPid, String rpcName, int code) {
                // BINDER_TRANSACTION / BINDER_REPLY / BINDER_FREE_BUFFER_FULL
            }

            @Override
            public void signal(int signal, int killerUid, int killerPid, int dstUid, int dstPid) {
            }

            @Override
            public void network(int proto, int targetUid, int dataLen) {
                // PROTO_IPV4 / PROTO_IPV6
            }
        });

        ReKernelX.pollEvent(); // 阻塞，直到 disconnect
    }
}).start();

ReKernelX.addMonitorNet(1000);
ReKernelX.delMonitorNet(1000);
ReKernelX.disconnect(); // 必须在 pollEvent 以外的线程调用
```
