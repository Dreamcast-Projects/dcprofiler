# What we are building
TARGET = profile.elf

# List of all source files
SRCS = main.c profiler.c

# List of files to exclude from instrumentation
EXCLUDE_FILES = profiler.c

# Set compiler flags
CFLAGS = -g -finstrument-functions

all: clean $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS)
	-rm -f PROF.CDI
	-rm -rf ISO

$(TARGET): $(SRCS) 
	kos-cc $(CFLAGS) -finstrument-functions-exclude-file-list=$(EXCLUDE_FILES) $^ -o $@ $(DATAOBJS) $(OBJEXTRA)

profileip: $(TARGET)
	sudo /opt/toolchains/dc-utils/dc-tool-ip -c "/opt/toolchains/dc-projects/profiling" -t 192.168.1.137 -x profile.elf

profileser: $(TARGET)
	sudo /opt/toolchains/dc-utils/dc-tool-ser -c "/opt/toolchains/dc-projects/profiling" -t /dev/cu.usbserial-ABSCDWND -b 115200 -x profile.elf

dot: 
	$(KOS_UTILS)/pvtrace profile.elf

image: dot
	dot -Tjpg graph.dot -o graph.jpg

run: $(TARGET)
	$(KOS_SERIAL_LOADER) $(TARGET)

runip: $(TARGET)
	$(KOS_IP_LOADER) $(TARGET)

rei: dist
	$(REICAST) PROF.CDI

lxd: dist
	$(LXDREAM) PROF.CDI

fly: dist
	$(FLYCAST) PROF.CDI

dist: inst
	$(KOS_STRIP) $(TARGET)
	$(KOS_OBJCOPY) -O binary $(TARGET) prog.bin
	$(KOS_SCRAMBLE) prog.bin 1ST_READ.BIN
	mkdir -p ISO
	cp 1ST_READ.BIN ISO
	mkisofs -C 0,11702 -V DCTEST -G ${KOS_BASE}/IP.BIN -J -R -l -o PROF.ISO ISO
	$(CREATE_CDI) PROF.ISO PROF.CDI
	rm -f PROF.ISO
	rm -f prog.BIN
	rm -f 1ST_READ.BIN
	rm -rf ISO
	rm $(TARGET)

