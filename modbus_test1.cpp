#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <sys/time.h>
#include <unistd.h>
#include <modbus.h>

// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

// var Ib = new ScaledValueDisplayClass(MBID, 58, "W", "fD4", "Output Power", 1);
// var Vb = new ScaledValueDisplayClass(MBID, 38, "V", "fD0", "Battery Voltage", 1);
// var VbT = new ScaledValueDisplayClass(MBID, 51, "V", "fD1", "Target Voltage", 1);
// var IbC = new ScaledValueDisplayClass(MBID, 39, "A", "fD2", "Charge Current", 1);
// var Ib = new ScaledValueDisplayClass(MBID, 58, "W", "fD4", "Output Power", 1);
// var BTemp = new TempDisplayClass(MBID, 37, "fDBT", "Battery");
// var HSTemp = new TempDisplayClass(MBID, 35, "fDHST", "Heak Sink");

const char* address = "192.168.2.180";
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
    float bat_term;
    float bat_sense;
    float input_power;
    float output_power;
    float battery_voltage;
    float charge_current;
    float array_voltage;
    float battery_temp;
    float heatsink_temp;

    std::thread* t;
    if(true) {
        t = new std::thread([&]{

            // HTTP
            httplib::Server svr;

            // HTTPS
            // httplib::SSLServer svr;

            svr.Get("/status", [&](const httplib::Request &, httplib::Response &res) {
                static char str[4096];
                std::string json = "{ ";
                sprintf(str, "\"battery-sense\": %f,", bat_sense);
                json += str;
                sprintf(str, "\"battery-terminal\": %f,", bat_term);
                json += str;
                sprintf(str, "\"battery-voltage\": %f,", battery_voltage);
                json += str;
                sprintf(str, "\"battery-temp\": %f,", battery_temp);
                json += str;
                sprintf(str, "\"heatsink-temp\": %f,", heatsink_temp);
                json += str;
                sprintf(str, "\"array-voltage\": %f,", array_voltage);
                json += str;
                sprintf(str, "\"charge-current\": %f,", charge_current);
                json += str;
                sprintf(str, "\"output-power\": %f", output_power);
                json += str;
                json += "}";
                res.set_content(json, "application/json");
            });

            svr.listen("0.0.0.0", 8080);
        });
    }

    while(1) {
        modbus_t *mb;
        int result;
        mb = modbus_new_tcp(address, port);
        if(mb == NULL) {
            printf("failed to open modbus at %s, port %d\n", address, port);
            exit(EXIT_FAILURE);
        }
        if (modbus_connect(mb) == -1) {
            fprintf(stderr, "Connection failed: %d, %s\n", errno, modbus_strerror(errno));
            modbus_free(mb);
            exit(EXIT_FAILURE);
        }

        result = modbus_set_slave(mb, slave);
        if(result == -1) {
            int last_error = errno;
            printf("set slave error\n");
            printf("%s\n", modbus_strerror(last_error));
        }

        uint32_t sec = 10;
        uint32_t usec = 0;
        modbus_set_byte_timeout(mb, sec, usec);
        modbus_set_response_timeout(mb, sec, usec);

        float vscale = reg(mb, 0) + reg(mb, 1) / 65536.0;
        float iscale = reg(mb, 2) + reg(mb, 3) / 65536.0;

        bat_term = reg(mb, 25) / 32768.0 * vscale;
        battery_voltage = reg(mb, 38) / 32768.0 * vscale;
        bat_sense = reg(mb, 26) / 32768.0 * vscale;
        charge_current = reg(mb, 39) / 32768.0 * iscale;
        array_voltage = reg(mb, 27) / 32768.0 * vscale;
        input_power = reg(mb, 59) / 131072.0 * iscale * vscale;
        output_power = reg(mb, 58) / 131072.0 * iscale * vscale;
        battery_temp = reg(mb, 37) * 9.0 / 5 + 32;
        heatsink_temp = reg(mb, 35) * 9.0 / 5 + 32;

        printf("battery voltage %f\n", battery_voltage);
        printf("battery terminal voltage %f\n", bat_term);
        printf("battery sense voltage %f\n", bat_sense);
        // printf("battery wire efficiency = %f %%\n", 100 * bat_term / bat_sense);
        // printf("target battery voltage %f\n", reg(mb, 51) / 32768.0 * vscale);
        printf("charge current %f\n", charge_current);
        printf("array voltage %f\n", array_voltage);
        printf("input power %f\n", input_power);
        printf("output power %f\n", output_power);
        // printf("conversion efficiency = %f %%\n", 100 * output_power / input_power);
        printf("bat temp %f\n", battery_temp);
        printf("heatsink temp %f\n", heatsink_temp);
        puts("");

        modbus_close(mb);
        modbus_free(mb);
        sleep(5);
    }
}
