CXXFLAGS+=-Wall --std=c++17
CXXFLAGS+=-I/usr/include/modbus
LDFLAGS+=-L/usr/local/lib
LDLIBS+=-lmodbus -lsqlite3 -lpthread

default: modbus_test1
