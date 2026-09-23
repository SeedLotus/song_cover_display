# AT32F403ACCT7 固件 GCC 构建（由 Keil MDK 工程配置转换而来）
# 用法：
#   make            构建 build/firmware.elf / .hex / .bin
#   make clean      清理
# 工具链路径可通过 TOOLCHAIN_BIN 覆盖，默认期望 arm-none-eabi-gcc 在 PATH 中：
#   make TOOLCHAIN_BIN=E:/AI/Agent_Work/tools/msys64/mingw64/bin
#
# 注意：强制 cmd.exe 作为 recipe shell。PortableGit 的 sh.exe 在 PATH 中时
# GNU make 会优先用它，而 sh 派生原生 gcc 会静默失败（实测）。
SHELL := cmd.exe

TARGET   = firmware
BUILD    = build

TOOLCHAIN_BIN ?=
ifneq ($(strip $(TOOLCHAIN_BIN)),)
PREFIX = $(TOOLCHAIN_BIN)/arm-none-eabi-
else
PREFIX = arm-none-eabi-
endif

CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# ---------- 芯片与编译参数（来自 MDK 工程：Cortex-M4F, 硬件 FPU） ----------
MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS = \
	-DAT32F403ACCT7 \
	-DUSE_STDPERIPH_DRIVER \
	-DARM_MATH_CM4 \
	-DARM_MATH_MATRIX_CHECK \
	-DARM_MATH_ROUNDING \
	-DARM_MATH_LOOPUNROLL \
	-DAT_START_F403A_V1 \
	-DPY32F403xD

INCS = \
	-Ilibraries/drivers/inc \
	-Ilibraries/cmsis/cm4/core_support \
	-Ilibraries/cmsis/cm4/device_support \
	-Iproject/inc \
	-Imiddlewares/usbd_drivers/inc \
	-Ilibraries/cmsis/dsp/include \
	-Ilibraries/cmsis/dsp/PrivateInclude \
	-Imiddlewares/ltx/inc \
	-Imiddlewares/ltx/components/app \
	-Imiddlewares/ltx/components/debug \
	-Imiddlewares/ltx/components/log \
	-Imiddlewares/ltx/components/script \
	-Imiddlewares/ltx/components/event_group \
	-Imiddlewares/ltx/components/lock \
	-Ilibraries/drivers/HAL/inc \
	-Idevices/inc

CFLAGS = $(MCU_FLAGS) $(DEFS) $(INCS) \
	-O2 -g \
	-ffunction-sections -fdata-sections \
	-fno-common -Wall -Wno-unused-variable -Wno-unused-function \
	-std=gnu11

ASFLAGS = $(MCU_FLAGS) -x assembler-with-cpp

# 项目自定义链接脚本：RAM 224K（库自带 AxC 脚本只有 96K，放不下 112.5KB 图片缓冲）
LDSCRIPT = project/gcc/AT32F403ACCT7_FLASH.ld

LDFLAGS = $(MCU_FLAGS) \
	-T$(LDSCRIPT) \
	--specs=nano.specs --specs=nosys.specs \
	-Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
	-lm

# ---------- 源文件（清单来自 MDK_V5 工程，共 68 个 C + 1 个 GCC 启动汇编） ----------
SRCS = \
	project/src/main.c \
	project/src/at32f403a_407_wk_config.c \
	project/src/at32f403a_407_int.c \
	project/src/wk_system.c \
	project/src/usb_app.c \
	project/src/wk_acc.c \
	project/src/wk_debug.c \
	project/src/wk_spi.c \
	project/src/wk_tmr.c \
	project/src/wk_usbfs.c \
	project/src/wk_dma.c \
	project/src/wk_gpio.c \
	project/src/myAPP_system.c \
	project/src/py32f4xx_hal_msp.c \
	project/src/myAPP_device_init.c \
	project/src/myAPP_display.c \
	project/src/myAPP_usb.c \
	project/src/cdc_class.c \
	project/src/cdc_desc.c \
	libraries/drivers/src/at32f403a_407_crm.c \
	libraries/drivers/src/at32f403a_407_tmr.c \
	libraries/drivers/src/at32f403a_407_gpio.c \
	libraries/drivers/src/at32f403a_407_pwc.c \
	libraries/drivers/src/at32f403a_407_spi.c \
	libraries/drivers/src/at32f403a_407_dma.c \
	libraries/drivers/src/at32f403a_407_flash.c \
	libraries/drivers/src/at32f403a_407_exint.c \
	libraries/drivers/src/at32f403a_407_misc.c \
	libraries/drivers/src/at32f403a_407_usb.c \
	libraries/drivers/src/at32f403a_407_acc.c \
	libraries/drivers/src/at32f403a_407_debug.c \
	libraries/cmsis/cm4/device_support/system_at32f403a_407.c \
	middlewares/usbd_drivers/src/usbd_core.c \
	middlewares/usbd_drivers/src/usbd_int.c \
	middlewares/usbd_drivers/src/usbd_sdr.c \
	libraries/cmsis/dsp/Source/BasicMathFunctions/BasicMathFunctions.c \
	libraries/cmsis/dsp/Source/BayesFunctions/BayesFunctions.c \
	libraries/cmsis/dsp/Source/CommonTables/CommonTables.c \
	libraries/cmsis/dsp/Source/ComplexMathFunctions/ComplexMathFunctions.c \
	libraries/cmsis/dsp/Source/ControllerFunctions/ControllerFunctions.c \
	libraries/cmsis/dsp/Source/DistanceFunctions/DistanceFunctions.c \
	libraries/cmsis/dsp/Source/FastMathFunctions/FastMathFunctions.c \
	libraries/cmsis/dsp/Source/FilteringFunctions/FilteringFunctions.c \
	libraries/cmsis/dsp/Source/InterpolationFunctions/InterpolationFunctions.c \
	libraries/cmsis/dsp/Source/MatrixFunctions/MatrixFunctions.c \
	libraries/cmsis/dsp/Source/QuaternionMathFunctions/QuaternionMathFunctions.c \
	libraries/cmsis/dsp/Source/StatisticsFunctions/StatisticsFunctions.c \
	libraries/cmsis/dsp/Source/SupportFunctions/SupportFunctions.c \
	libraries/cmsis/dsp/Source/SVMFunctions/SVMFunctions.c \
	libraries/cmsis/dsp/Source/TransformFunctions/TransformFunctions.c \
	middlewares/ltx/src/ltx.c \
	middlewares/ltx/components/debug/ltx_cmd.c \
	middlewares/ltx/components/debug/ltx_param.c \
	middlewares/ltx/components/log/ltx_log.c \
	middlewares/ltx/components/log/SEGGER_RTT.c \
	middlewares/ltx/components/log/SEGGER_RTT_printf.c \
	middlewares/ltx/components/script/ltx_script.c \
	middlewares/ltx/components/app/ltx_app.c \
	middlewares/ltx/components/event_group/ltx_event_group.c \
	middlewares/ltx/components/lock/ltx_lock.c \
	libraries/drivers/HAL/src/py32f4xx_hal.c \
	libraries/drivers/HAL/src/py32f4xx_hal_cortex.c \
	libraries/drivers/HAL/src/py32f4xx_hal_dma.c \
	libraries/drivers/HAL/src/py32f4xx_hal_spi.c \
	libraries/drivers/HAL/src/py32f4xx_hal_spi_ex.c \
	libraries/drivers/HAL/src/system_py32f4xx.c \
	devices/GC9A01.c \
	devices/tonearm.c

ASM_SRCS = libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f403a_407.s

OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o)) $(addprefix $(BUILD)/,$(ASM_SRCS:.s=.o))

# ---------- 规则 ----------
all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
	@$(SIZE) $(BUILD)/$(TARGET).elf

$(BUILD)/%.o: %.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	@if not exist "$(BUILD)" mkdir "$(BUILD)"
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	@if exist "$(BUILD)" rmdir /s /q "$(BUILD)"

.PHONY: all clean
