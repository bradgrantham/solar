CXXFLAGS+=-Wall
CXXFLAGS+=-I/usr/include/modbus
LDFLAGS+=-L/usr/local/lib
LDLIBS+=-lmodbus -lsqlite3

default: modbus_test1
