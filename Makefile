IWPANINFO_BACKENDS    = $(BACKENDS)
IWPANINFO_CFLAGS      = $(CFLAGS) -std=gnu99 -fstrict-aliasing -Iinclude
IWPANINFO_LDFLAGS     = -luci -lubox

IWPANINFO_LIB         = libiwpaninfo.so
IWPANINFO_LIB_LDFLAGS = $(LDFLAGS) -shared
IWPANINFO_LIB_OBJ     = iwpaninfo_utils.o iwpaninfo_lib.o

IWPANINFO_LUA         = iwpaninfo.so
IWPANINFO_LUA_LDFLAGS = $(LDFLAGS) -shared -L. -liwpaninfo -llua
IWPANINFO_LUA_OBJ     = iwpaninfo_lua.o

IWPANINFO_CLI         = iwpaninfo
IWPANINFO_CLI_LDFLAGS = $(LDFLAGS)
# get rid of independence os libiwpaninfo.so
#IWPANINFO_CLI_LDFLAGS = $(LDFLAGS) -L. -liwpaninfo
#IWPANINFO_CLI_OBJ     = iwpaninfo_cli.o
IWPANINFO_CLI_OBJ     = iwpaninfo_cli.o iwpaninfo_utils.o iwpaninfo_lib.o iwpaninfo_nl802154.o

ifneq ($(filter nl802154,$(IWPANINFO_BACKENDS)),)
	IWPANINFO_CFLAGS      += -DUSE_NL802154
	IWPANINFO_CLI_LDFLAGS += -lnl-tiny
	IWPANINFO_LIB_LDFLAGS += -lnl-tiny
	IWPANINFO_LIB_OBJ     += iwpaninfo_nl802154.o
endif

%.o: %.c
	$(CC) $(IWPANINFO_CFLAGS) $(FPIC) -c -o $@ $<

compile: clean $(IWPANINFO_LIB_OBJ) $(IWPANINFO_LUA_OBJ) $(IWPANINFO_CLI_OBJ)
	$(CC) $(IWPANINFO_LDFLAGS) $(IWPANINFO_LIB_LDFLAGS) -o $(IWPANINFO_LIB) $(IWPANINFO_LIB_OBJ)
	$(CC) $(IWPANINFO_LDFLAGS) $(IWPANINFO_LUA_LDFLAGS) -o $(IWPANINFO_LUA) $(IWPANINFO_LUA_OBJ)
	$(CC) $(IWPANINFO_LDFLAGS) $(IWPANINFO_CLI_LDFLAGS) -o $(IWPANINFO_CLI) $(IWPANINFO_CLI_OBJ)

clean:
	rm -f *.o $(IWPANINFO_LIB) $(IWPANINFO_LUA) $(IWPANINFO_CLI)
