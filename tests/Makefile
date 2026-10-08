# Host tests. These run on a PC, not on the target: what they prove is a
# property of the framing and the bookkeeping, not of the board.
# Same layout as VF_Angle_Module/tests.
#
#   make          build and run
#   make clean
#
# -Wdouble-promotion -Wfloat-conversion: CLAUDE.md 6.4 says float, never
# double. The target build cannot be given these flags without touching
# .cproject, so the host build enforces them on the same sources instead.
#
# Uses gcc from MinGW and make from STM32CubeIDE (CLAUDE.md 3): this machine
# has no other make.

# Plain "=" not "?=": make has a built-in CC=cc, so "?=" would never fire and
# the build would look for a compiler this machine does not have.
CC      = gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wdouble-promotion -Wfloat-conversion -Wconversion -O2 -I../Core/Inc

TESTS   := test_drv_modbus test_app_meas test_app_frame test_dbc_layout test_app_cfg \
           test_boot_svc test_boot_policy test_boot_led

# Phase 5: the bootloader's pure modules. Its headers come after the
# application's, as in the bootloader's own include path.
BOOT    := ../VF_TempHumi_Bootloader/Core
BOOT_INC := -I$(BOOT)/Inc

# contracts/ holds exactly one DBC by rule (contracts/README.md). All matches
# are passed on: if there are none or two, test_dbc_layout refuses to run.
DBC     := $(wildcard ../contracts/*.dbc)

all: $(addsuffix .run,$(TESTS))

%.run: %.exe
	./$<

test_dbc_layout.run: test_dbc_layout.exe
	./$< $(DBC)

test_drv_modbus.exe: test_drv_modbus.c ../Core/Src/app/drv_modbus.c
	$(CC) $(CFLAGS) -o $@ $^

test_app_meas.exe: test_app_meas.c ../Core/Src/app/app_meas.c ../Core/Src/app/drv_modbus.c
	$(CC) $(CFLAGS) -o $@ $^

test_app_frame.exe: test_app_frame.c ../Core/Src/app/app_frame.c
	$(CC) $(CFLAGS) -o $@ $^

test_dbc_layout.exe: test_dbc_layout.c ../Core/Src/app/app_frame.c
	$(CC) $(CFLAGS) -o $@ $^

test_app_cfg.exe: test_app_cfg.c ../Core/Src/app/app_cfg.c
	$(CC) $(CFLAGS) -o $@ $^

test_boot_svc.exe: test_boot_svc.c $(BOOT)/Src/app/boot_svc.c ../Core/Src/app/app_cfg.c
	$(CC) $(CFLAGS) $(BOOT_INC) -o $@ $^

test_boot_policy.exe: test_boot_policy.c ../Core/Src/app/app_cfg.c
	$(CC) $(CFLAGS) $(BOOT_INC) -o $@ $^

test_boot_led.exe: test_boot_led.c
	$(CC) $(CFLAGS) $(BOOT_INC) -o $@ $^

clean:
	rm -f *.exe

.PHONY: all clean
