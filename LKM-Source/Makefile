rekernel_x-y := rekernel_x_main.o rekernel_x_genl.o rekernel_x_netuid.o rekernel_x_frozen.o rekernel_x_binder.o rekernel_x_signal.o rekernel_x_netfilter.o rekernel_x_binder_kp.o

obj-m += rekernel_x.o

ifdef REKERNEL_X_VERSION
ccflags-y += -DREKERNEL_X_VERSION=\"$(REKERNEL_X_VERSION)\"
endif
