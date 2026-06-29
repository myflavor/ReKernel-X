# LKM Source

ReKernel-X Loadable Kernel Module source code. Supports GKI kernels from Android 12 (5.10) through Android 16 (6.12).

## Source Files

| File | Description |
|---|---|
| `rekernel_x_main.c` | Module entry/exit, init/exit glue |
| `rekernel_x_genl.c` | Generic Netlink transport |
| `rekernel_x_binder.c` | Binder transaction hooks |
| `rekernel_x_binder_kp.c` | Binder kprobe hooks |
| `rekernel_x_signal.c` | Signal hooks |
| `rekernel_x_netfilter.c` | Netfilter hooks |
| `rekernel_x_netuid.c` | Network monitor UID hashmap |
| `rekernel_x_frozen.c` | Task frozen-state predicate |
| `rekernel_x.h` | Shared header, ABI definitions |
| `Makefile` | Kernel module build rules |
| `Kconfig` | Kernel config option |

## Building

Build via the `ddk-lkm.yml` reusable workflow, or manually inside a DDK container:

```sh
# Copy sources into kernel tree
cp *.c *.h /opt/ddk/src/<KMI>/drivers/android/
cat Makefile >> /opt/ddk/src/<KMI>/drivers/android/Makefile

# Build
cd /opt/ddk/src/<KMI>
make -C /opt/ddk/kdir/<KMI> \
    M=/opt/ddk/src/<KMI>/drivers/android \
    CONFIG_REKERNEL_X=m \
    CC="clang" \
    REKERNEL_X_VERSION="v1.0" \
    modules
```

## Version

The module version is defined in `rekernel_x.h`:

```c
#ifndef REKERNEL_X_VERSION
#define REKERNEL_X_VERSION "snapshot"
#endif
```

Pass `REKERNEL_X_VERSION` as a make variable to override the default `snapshot` value. The version is printed in `dmesg` on module load.
