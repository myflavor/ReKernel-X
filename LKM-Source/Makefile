rekernel-y := rekernel_main.o rekernel_genl.o rekernel_netuid.o rekernel_frozen.o rekernel_binder.o rekernel_signal.o rekernel_netfilter.o rekernel_binder_kp.o

obj-m += rekernel.o

ifdef REKERNEL_VERSION
ccflags-y += -DREKERNEL_VERSION=\"$(REKERNEL_VERSION)\"
endif
