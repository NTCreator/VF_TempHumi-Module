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

TESTS   := test_drv_modbus test_app_meas

all: $(addsuffix .run,$(TESTS))

%.run: %.exe
	./$<

test_drv_modbus.exe: test_drv_modbus.c ../Core/Src/app/drv_modbus.c
	$(CC) $(CFLAGS) -o $@ $^

test_app_meas.exe: test_app_meas.c ../Core/Src/app/app_meas.c ../Core/Src/app/drv_modbus.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.exe

.PHONY: all clean
