# LKM 源码

## 结构

- `rkx.c`：模块入口
- `rkx_genl.c`：Generic Netlink 通信
- `rkx_binder.c`：Binder 钩子
- `rkx_binder_kp.c`：Binder 异步缓存清理（kprobe）
- `rkx_signal.c`：信号钩子
- `rkx_netfilter.c`：网络钩子
- `rkx_netuid.c`：网络监控 UID 管理
- `rkx_frozen.c`：进程冻结状态判断
- `rkx.h`：公共头文件与 ABI 定义
- `rkx_log.h`：日志宏
- `Makefile`：编译规则

## 编译

可通过 `ddk-lkm.yml` 工作流编译，或在 DDK 容器中手动编译

源码需放到内核树的 `drivers/rekernel_x` 下：

```sh
cp -r ./LKM-Source /opt/ddk/src/<KMI>/drivers/rekernel_x

cd /opt/ddk/src/<KMI>
make -C /opt/ddk/kdir/<KMI> \
    M=/opt/ddk/src/<KMI>/drivers/rekernel_x \
    CC="clang" \
    RKX_VERSION="v1.0" \
    modules
```
