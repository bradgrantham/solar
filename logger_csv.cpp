#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <unistd.h>
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
        fprintf(stderr, "failure on address %d\n", r);
        fprintf(stderr, "%s\n", modbus_strerror(last_error));
    }

    return tab_reg[0];
}

int main(int argc, char **argv)
{
    modbus_t *mb;
    int result;
    bool print_header = false;

    if((argc > 1) && (strcmp(argv[1], "--header") == 0)) {
	print_header = true;
    }

    timeval tv;

    mb = modbus_new_tcp(address, port);
    if(mb == NULL) {
        fprintf(stderr, "failed to open modbus at %s, port %d\n", address, port);
        exit(EXIT_FAILURE);
    }
    modbus_connect(mb);

    result = modbus_set_slave(mb, slave);
    if(result == -1) {
        int last_error = errno;
        fprintf(stderr, "set slave error\n");
        fprintf(stderr, "%s\n", modbus_strerror(last_error));
    }

    tv.tv_sec = 15;
    tv.tv_usec = 0;
    modbus_set_byte_timeout(mb, &tv);
    modbus_set_response_timeout(mb, &tv);

    float vscale = reg(mb, 0) + reg(mb, 1) / 65536.0;
    float iscale = reg(mb, 2) + reg(mb, 3) / 65536.0;

    if(print_header)
	printf("time, battery voltage, target battery voltage, charge current, array voltage, output power\n");

    while(1)
    {
	float battery_voltage = reg(mb, 38) / 32768.0 * vscale;
	float target_battery_voltage = reg(mb, 51) / 32768.0 * vscale;
	float charge_current = reg(mb, 39) / 32768.0 * iscale;
	float array_voltage = reg(mb, 27) / 32768.0 * vscale;
	float output_power = reg(mb, 58) / 131072.0 * iscale * vscale;
	printf("%d, %f, %f, %f, %f, %f\n", time(0), battery_voltage, target_battery_voltage, charge_current, array_voltage, output_power);
	fflush(stdout);
	sleep(5);
    }

    modbus_close(mb);
    modbus_free(mb);
}
