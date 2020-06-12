ifneq ($(CONFIG_ARM64), y)
KBUILD_CFLAGS := $(filter-out -O2, $(KBUILD_CFLAGS))
KBUILD_AFLAGS := $(filter-out -mfpu=vfp, $(KBUILD_AFLAGS))
ccflags-y := -O1
asflags-y := -mfpu=neon -mfloat-abi=softfp
CFLAGS_cpuburn_main.o = -I$(src)
obj-$(CONFIG_XXX_CPUBURN) += cpuburn_sysfs.o \
				 cpuburn_main.o \
				 routine_32.o \
				 vfp_helper.o
else
#CFLAGS_REMOVE_cpuburn.o += -mgeneral-regs-only
CFLAGS_cpuburn_main.o = -I$(src)
obj-$(CONFIG_XXX_CPUBURN) += cpuburn_sysfs.o \
				 cpuburn_main.o \
				 routine.o
endif
