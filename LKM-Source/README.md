# LKM Source

Re:Kernel Loadable Kernel Module source code. Supports GKI kernels from Android 12 (5.10) through Android 16 (6.12).

## Source Files

| File | Description |
|---|---|
| `rekernel_main.c` | Module entry/exit, init/exit glue |
| `rekernel_genl.c` | Generic Netlink transport |
| `rekernel_binder.c` | Binder transaction hooks |
| `rekernel_binder_kp.c` | Binder kprobe hooks |
| `rekernel_signal.c` | Signal hooks |
| `rekernel_netfilter.c` | Netfilter hooks |
| `rekernel_netuid.c` | Network monitor UID hashmap |
| `rekernel_frozen.c` | Task frozen-state predicate |
| `rekernel.h` | Shared header, ABI definitions |
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
    CONFIG_REKERNEL=m \
    CC="clang" \
    REKERNEL_VERSION="v1.0" \
    modules
```

## Version

The module version is defined in `rekernel.h`:

```c
#ifndef REKERNEL_VERSION
#define REKERNEL_VERSION "snapshot"
#endif
```

Pass `REKERNEL_VERSION` as a make variable to override the default `snapshot` value. The version is printed in `dmesg` on module load.
