#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <sys/time.h>
#include <modbus.h>

// var Ib = new ScaledValueDisplayClass(MBID, 58, "W", "fD4", "Output Power", 1);
// var Vb = new ScaledValueDisplayClass(MBID, 38, "V", "fD0", "Battery Voltage", 1);
// var VbT = new ScaledValueDisplayClass(MBID, 51, "V", "fD1", "Target Voltage", 1);
// var IbC = new ScaledValueDisplayClass(MBID, 39, "A", "fD2", "Charge Current", 1);
// var Ib = new ScaledValueDisplayClass(MBID, 58, "W", "fD4", "Output Power", 1);
// var BTemp = new TempDisplayClass(MBID, 37, "fDBT", "Battery");
// var HSTemp = new TempDisplayClass(MBID, 35, "fDHST", "Heak Sink");

const char* address = "192.168.2.2";
const int port = 502;
const int slave = 1;

int reg(modbus_t *mb, int r)
{
    uint16_t tab_reg[32];

    int result = modbus_read_registers(mb, r, 1, tab_reg);

    if(result == -1) {
        int last_error = errno;
        printf("failure on address %d\n", r);
        printf("%s\n", modbus_strerror(last_error));
    }

    return tab_reg[0];
}

int main(int argc, char **argv)
{
    modbus_t *mb;
    int result;

    timeval tv;

    mb = modbus_new_tcp(address, port);
    if(mb == NULL) {
        printf("failed to open modbus at %s, port %d\n", address, port);
        exit(EXIT_FAILURE);
    }
    modbus_connect(mb);

    result = modbus_set_slave(mb, slave);
    if(result == -1) {
        int last_error = errno;
        printf("set slave error\n");
        printf("%s\n", modbus_strerror(last_error));
    }

    tv.tv_sec = 10;
    tv.tv_usec = 0;
    modbus_set_byte_timeout(mb, &tv);
    modbus_set_response_timeout(mb, &tv);

    float vscale = reg(mb, 0) + reg(mb, 1) / 65536.0;
    float iscale = reg(mb, 2) + reg(mb, 3) / 65536.0;

    float bat_term, bat_sense, in_power, out_power;
    printf("battery voltage %f\n", reg(mb, 38) / 32768.0 * vscale);
    printf("battery terminal voltage %f\n", bat_term = reg(mb, 25) / 32768.0 * vscale);
    printf("battery sense voltage %f\n", bat_sense = reg(mb, 26) / 32768.0 * vscale);
    printf("battery wire efficiency = %f %%\n", 100 * bat_term / bat_sense);
    printf("target battery voltage %f\n", reg(mb, 51) / 32768.0 * vscale);
    printf("charge current %f\n", reg(mb, 39) / 32768.0 * iscale);
    printf("array voltage %f\n", reg(mb, 27) / 32768.0 * vscale);
    printf("input power %f\n", in_power = reg(mb, 59) / 131072.0 * iscale * vscale);
    printf("output power %f\n", out_power = reg(mb, 58) / 131072.0 * iscale * vscale);
    printf("conversion efficiency = %f %%\n", 100 * out_power / in_power);
    printf("bat temp %d\n", reg(mb, 37) * 9 / 5 + 32);
    printf("heatsink temp %d\n", reg(mb, 35) * 9 / 5 + 32);

    modbus_close(mb);
    modbus_free(mb);
}
