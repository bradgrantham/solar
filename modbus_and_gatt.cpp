#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <sys/time.h>
#include <unistd.h>
#include <modbus.h>

// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "gattlib.h"

volatile bool g_operation_completed;

/*
As Documented at
  https://community.victronenergy.com/questions/93919/victron-bluetooth-ble-protocol-publication.html
*/

const char* uuid_str_SOC 	= "65970fff-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_V 		= "6597ed8d-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_A 		= "6597ed8c-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_P 		= "6597ed8e-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_Consumed	= "6597eeff-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_tempK	= "6597edec-4bda-4c1e-af4b-551c4cf74769";
const char* uuid_str_t2go	= "65970ffe-4bda-4c1e-af4b-551c4cf74769";

float state_of_charge = -1;

static void connect_cb(gatt_connection_t* connection,  void* user_data)
{
    if (connection != NULL) {

        uuid_t uuid;
        int err = GATTLIB_SUCCESS;

        printf("Connected to BLE\n");

        // get SOC
        if ( gattlib_string_to_uuid(uuid_str_SOC, strlen(uuid_str_SOC) + 1, &uuid) == 0){
            size_t len;
            uint8_t *buffer = NULL;

            err =  gattlib_read_char_by_uuid(connection, &uuid, (void **)&buffer, &len);
            if(err == GATTLIB_SUCCESS){
                int16_t i = (buffer[1] << 8) | buffer[0];
                state_of_charge = i / 100.0f;
                printf("SOC is %f\n", state_of_charge);
                free(buffer);
            }
        }
    }
    g_operation_completed = true;
}


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
    float bat_term = -1;
    float bat_sense = -1;
    float input_power = -1;
    float output_power = -1;
    float battery_voltage = -1;
    float charge_current = -1;
    float array_voltage = -1;
    int battery_temp = -1;
    int heatsink_temp = -1;

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
                sprintf(str, "\"state-of-charge\": %f,", state_of_charge);
                json += str;
                sprintf(str, "\"battery-sense\": %f,", bat_sense);
                json += str;
                sprintf(str, "\"battery-voltage\": %f,", battery_voltage);
                json += str;
                sprintf(str, "\"battery-temp\": %d,", battery_temp);
                json += str;
                sprintf(str, "\"heatsink-temp\": %d,", heatsink_temp);
                json += str;
                sprintf(str, "\"array-voltage\": %f,", array_voltage);
                json += str;
                sprintf(str, "\"charge-current\": %f", charge_current);
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
        battery_temp = reg(mb, 37) / 9 / 5 + 32;
        heatsink_temp = reg(mb, 35) / 9 / 5 + 32;

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
        printf("bat temp %d\n", battery_temp);
        printf("heatsink temp %d\n", heatsink_temp);
        puts("");

        modbus_close(mb);
        modbus_free(mb);

        gatt_connection_t* con;

        const char* deviceID = "CA:9C:B9:8D:E3:70";

        g_operation_completed = false;

        con = gattlib_connect_async(NULL, deviceID, BDADDR_LE_RANDOM, connect_cb, 0);
        if (con == NULL) {
            fprintf(stderr, "Failed to connect to the bluetooth device %s.\n",deviceID);
        } else {
            while (!g_operation_completed);
            gattlib_disconnect(con);
            printf("BLE connection succeeded \n");
        }

        sleep(5);
    }
}
