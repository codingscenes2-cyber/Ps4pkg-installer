TITLE       := PKGMaster PS4 Boot Test
VERSION     := 0.1.0
TITLE_ID    := PKGM00001
CONTENT_ID  := IV0000-PKGM00001_00-PKGMASTER0000001

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
INTDIR      := build
SRC         := $(wildcard src/*.cpp)
OBJS        := $(patsubst src/%.cpp,$(INTDIR)/%.o,$(SRC))
LIBS        := -lc -lkernel -lc++
TARGET      := $(CONTENT_ID).pkg
BIN_DIR     := $(TOOLCHAIN)/bin/linux
PKGTOOL     := $(BIN_DIR)/PkgTool.Core
CREATE_FSELF:= $(BIN_DIR)/create-fself
CREATE_GP4  := $(BIN_DIR)/create-gp4

CXXFLAGS    := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

.PHONY: all clean check-toolchain
all: $(TARGET)

check-toolchain:
	@test -n "$(TOOLCHAIN)" || (echo "OO_PS4_TOOLCHAIN is not set"; exit 1)
	@test -f "$(TOOLCHAIN)/link.x" || (echo "OpenOrbis link.x not found under $(TOOLCHAIN)"; exit 1)
	@test -x "$(PKGTOOL)" || (echo "PkgTool.Core missing or not executable: $(PKGTOOL)"; exit 1)
	@test -x "$(CREATE_FSELF)" || (echo "create-fself missing or not executable: $(CREATE_FSELF)"; exit 1)
	@test -x "$(CREATE_GP4)" || (echo "create-gp4 missing or not executable: $(CREATE_GP4)"; exit 1)

$(INTDIR):
	mkdir -p $@

$(INTDIR)/%.o: src/%.cpp | $(INTDIR) check-toolchain
	clang++ $(CXXFLAGS) -o $@ $<

eboot.bin: $(OBJS) | check-toolchain
	ld.lld $(INTDIR)/*.o -o $(INTDIR)/PKGMaster.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(INTDIR)/PKGMaster.elf -out=$(INTDIR)/PKGMaster.oelf --eboot "eboot.bin" --paid 0x3800000000000011

sce_sys/param.sfo: Makefile | check-toolchain
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png | check-toolchain
	$(CREATE_GP4) -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(TARGET): pkg.gp4 | check-toolchain
	$(PKGTOOL) pkg_build $< .

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 $(TARGET) sce_sys/param.sfo
