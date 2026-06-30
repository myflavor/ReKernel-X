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
| `rekernel_x_log.h` | Centralised printk prefix + log macros |
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
#define REKERNEL_X_VERSION "dev"
#endif
```

Pass `REKERNEL_X_VERSION` as a make variable to override the default `dev` value. The version is printed in `dmesg` on module load.

## Logging

All log output is prefixed with `[ReKernel-X LKM] ` via `pr_fmt`, defined in
`rekernel_x_log.h`. Always include `rekernel_x_log.h` as the **first** header
in each `.c` so `pr_fmt` is visible to `linux/printk.h`.

Use the wrapper macros instead of raw `pr_*`:

| Macro | Level |
|---|---|
| `rekernel_x_log_info(fmt, ...)` | info |
| `rekernel_x_log_err(fmt, ...)` | error |
| `rekernel_x_log_warn(fmt, ...)` | warning |
| `rekernel_x_log_debug(fmt, ...)` | debug (compile-time gated) |

`rekernel_x_log_debug` is a hard compile-time switch — it expands to `pr_info`
when `REKERNEL_X_DEBUG` is defined, otherwise to `no_printk` and is eliminated
by the compiler (no runtime cost, no binary footprint). Enable it with:

```sh
make ... CC="clang" \
    CFLAGS_module=-DREKERNEL_X_DEBUG \
    modules
```

## Feature toggles

`CLEAN_UP_ASYNC_BINDER` (defined in `rekernel_x.h`) controls the binder
async-transaction cleanup kprobe. Remove the `#define` to disable the feature;
`register_binder_kp()` is then skipped and the `unregister_*` calls are safe
no-ops.
