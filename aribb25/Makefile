PREFIX = /usr/local
MAJOR = 0
MINOR = 2
REVISION = 7
VER = $(MAJOR).$(MINOR).$(REVISION)

DEST_HEADER = $(PREFIX)/include/aribb25

# PC/SC Lite libraries and headers.
PCSC_CFLAGS ?= `pkg-config libpcsclite --cflags`
PCSC_LDLIBS ?= `pkg-config libpcsclite --libs`

CC     = gcc
CXX    = g++
CFLAGS = -O2 -fPIC -Wall $(PCSC_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

LIBS   = $(PCSC_LDLIBS)
LDFLAGS =

OBJS  = arib_std_b25.o b_cas_card.o multi2.o multi2_simd.o ts_section_parser.o
HEADERS = arib_std_b25.h arib_std_b25_error_code.h b_cas_card.h portable.h
TARGET_APP = b25
TARGET_LIB = libaribb25.so
TARGETS = $(TARGET_APP) $(TARGET_LIB)
DEPEND = Makefile.dep
SONAME = $(TARGET_LIB).$(MAJOR)

all: $(TARGETS)

clean:
	rm -f $(OBJS) td.o $(TARGETS) $(DEPEND)

$(TARGET_APP): $(OBJS) td.o
	$(CXX) $(LDFLAGS) -o $(TARGET_APP) $(OBJS) td.o $(LIBS)

$(TARGET_LIB): $(OBJS)
	$(CXX) $(LDFLAGS) -shared -o $(TARGET_LIB) $(OBJS) $(LIBS) -Wl,-soname,$(SONAME)

$(DEPEND):
	$(CC) -fPIC -MM $(OBJS:.o=.c) td.c > $@

install: $(TARGET) install-headers
	install -m755 $(TARGET_APP) $(PREFIX)/bin
	install -m755 $(TARGET_LIB) $(PREFIX)/lib/$(TARGET_LIB).$(VER)
	ln -sf $(PREFIX)/lib/$(TARGET_LIB).$(VER) $(PREFIX)/lib/$(TARGET_LIB).$(MAJOR)
	ln -sf $(PREFIX)/lib/$(TARGET_LIB).$(MAJOR) $(PREFIX)/lib/$(TARGET_LIB)
	ldconfig

install-headers:
	mkdir -p $(DEST_HEADER)
	install -m644 $(HEADERS) $(DEST_HEADER)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET_APP)
	rm -f $(PREFIX)/lib/libaribb25.*

-include $(DEPEND)
