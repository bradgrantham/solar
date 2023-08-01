CXXFLAGS+=-Wall --std=c++17
CXXFLAGS+=-I/usr/include/modbus
LDFLAGS+=-L/usr/local/lib
LDLIBS+=-lmodbus -lsqlite3 -lpthread -lgattlib

default: modbus_and_gatt
