-include ../config-host.mak

CFLAGS=-Wall -O2 -g
#CFLAGS+=-msse2
LDFLAGS=

ifeq ($(ARCH),i386)
TESTS=linux-test testthread sha1-i386 test-i386 runcom
endif
ifeq ($(ARCH),x86_64)
TESTS=test-x86_64
endif
TESTS+=sha1# test_path
#TESTS+=test_path

QEMU=../i386-user/qemu-i386

all: $(TESTS)

hello-i386: hello-i386.c
	$(CC) -nostdlib $(CFLAGS) -static $(LDFLAGS) -o $@ $<
	strip $@

testthread: testthread.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lpthread

test_path: test_path.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	./$@ || { rm $@; exit 1; }

# i386/x86_64 emulation test (test various opcodes) */
test-i386: test-i386.c test-i386-code16.S test-i386-vm86.S \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) $(CFLAGS) $(LDFLAGS) -static -o $@ \
              test-i386.c test-i386-code16.S test-i386-vm86.S -lm

test-x86_64: test-i386.c \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) $(CFLAGS) $(LDFLAGS) -static -o $@ test-i386.c -lm

ifeq ($(ARCH),i386)
test: test-i386
	./test-i386 > test-i386.ref
else
test:
endif
	$(QEMU) test-i386 > test-i386.out
	@if diff -u test-i386.ref test-i386.out ; then echo "Auto Test OK"; fi
ifeq ($(ARCH),i386)
	$(QEMU) -no-code-copy test-i386 > test-i386.out
	@if diff -u test-i386.ref test-i386.out ; then echo "Auto Test OK (no code copy)"; fi
endif

# generic Linux and CPU test
linux-test: linux-test.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lm

# speed test
sha1-i386: sha1.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

sha1: sha1.c
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

speed: sha1 sha1-i386
	time ./sha1
	time $(QEMU) ./sha1-i386

# vm86 test
runcom: runcom.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# NOTE: -fomit-frame-pointer is currently needed : this is a bug in libqemu
qruncom: qruncom.c ../i386-user/libqemu.a
	$(CC) $(CFLAGS) -fomit-frame-pointer $(LDFLAGS) -I../target-i386 -I.. -I../i386-user -I../fpu \
              -o $@ $< -L../i386-user -lqemu -lm

# arm test
hello-arm: hello-arm.o
	arm-linux-ld -o $@ $<

hello-arm.o: hello-arm.c
	arm-linux-gcc -Wall -g -O2 -c -o $@ $<

# MIPS test
hello-mips: hello-mips.c
	mips-linux-gnu-gcc -nostdlib -static -mno-abicalls -fno-PIC -mabi=32 -Wall -Wextra -g -O2 -o $@ $<

hello-mipsel: hello-mips.c
	mipsel-linux-gnu-gcc -nostdlib -static -mno-abicalls -fno-PIC -mabi=32 -Wall -Wextra -g -O2 -o $@ $<

# XXX: find a way to compile easily a test for each arch
test2:
	@for arch in i386 arm armeb sparc ppc mips mipsel; do \
           ../$${arch}-user/qemu-$${arch} $${arch}/ls -l linux-test.c ; \
        done

clean:
	rm -f *~ *.o test-i386.out test-i386.ref \
           test-x86_64.log test-x86_64.ref qruncom $(TESTS)
