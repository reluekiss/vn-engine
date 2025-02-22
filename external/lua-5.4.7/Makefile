PLAT= guess

INSTALL_TOP= ../../../build/lua/
INSTALL_INC= $(INSTALL_TOP)/include
INSTALL_LIB= $(INSTALL_TOP)/lib

INSTALL= install -p
INSTALL_DATA= $(INSTALL) -m 0644
MKDIR= mkdir -p
RM= rm -f

PLATS= guess aix bsd c89 freebsd generic ios linux linux-readline macosx mingw posix solaris

TO_INC= lua.h luaconf.h lualib.h lauxlib.h
TO_LIB= liblua.a

all:	$(PLAT) install

install:
	cd src && $(MKDIR) $(INSTALL_INC) $(INSTALL_LIB)
	cd src && $(INSTALL_DATA) $(TO_INC) $(INSTALL_INC)
	cd src && $(INSTALL_DATA) $(TO_LIB) $(INSTALL_LIB)

clean:
	rm -rf install 

.PHONY: $(PLATS) install
