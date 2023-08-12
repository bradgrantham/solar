#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <sys/time.h>
#include <unistd.h>

#include <modbus.h>
#include <sqlite3.h>

// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "gattlib.h"

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

float ble_state_of_charge = -1;
float ble_power_flow = -1;
float ble_current_flow = -1;
float ble_voltage = -1;

std::tuple<bool, std::string> get_characteristic_shell(const std::string& bluetooth_address, const std::string& uuid)
{
    static char command[512];
    static char response[512];
    sprintf(command, "gatttool -t random -b %s --char-read --uuid %s", bluetooth_address.c_str(), uuid.c_str());

    FILE *output = popen(command, "r");
    fgets(response, sizeof(response), output);
    pclose(output);

    response[strlen(response) - 1] = '\0';
    printf("response was \"%s\"\n", response);
    return {true, response};
}

bool get_gatt_characteristic_shell_u16(const char *bluetooth_address, const char *uuid, uint16_t &v)
{
    auto [success, response] = get_characteristic_shell(bluetooth_address, uuid);

    if(!success)
    {
        printf("failed to query to gatttool\n");
        return false;
    }
    unsigned int handle;
    unsigned int byte1, byte2;
    if(sscanf(response.c_str(), "handle: %x 	 value: %x %x", &handle, &byte1, &byte2) != 3) {
        printf("failed to fetch characteristic, response was: \"%s\"\n", response.c_str());
        return false;
    }
    v = byte1 + byte2 * 256;
    return true;
}

bool get_gatt_characteristic_shell_u32(const char *bluetooth_address, const char *uuid, uint32_t &v)
{
    auto [success, response] = get_characteristic_shell(bluetooth_address, uuid);

    if(!success)
    {
        printf("failed to query to gatttool\n");
        return false;
    }
    unsigned int handle;
    unsigned int byte1, byte2, byte3, byte4;
    if(sscanf(response.c_str(), "handle: %x 	 value: %x %x %x %x", &handle, &byte1, &byte2, &byte3, &byte4) != 5) {
        printf("failed to fetch characteristic, response was: \"%s\"\n", response.c_str());
        return false;
    }
    v = byte1 + (byte2 << 8u) + (byte3 << 16u) + (byte4 << 24u);
    return true;
}

void get_gatt_data_shell(const char *bluetooth_address)
{
    uint16_t u16;
    uint32_t u32;
    bool success;

    success = get_gatt_characteristic_shell_u16(bluetooth_address, uuid_str_SOC, u16);
    if(!success) {
        printf("failed to fetch SOC\n");
    } else {
        ble_state_of_charge = u16 / 100.0f;
    }

    success = get_gatt_characteristic_shell_u16(bluetooth_address, uuid_str_P, u16);
    if(!success) {
        printf("failed to fetch P\n");
    } else {
        int16_t i = u16;
        ble_power_flow = -static_cast<float>(i);
    }

    success = get_gatt_characteristic_shell_u16(bluetooth_address, uuid_str_V, u16);
    if(!success) {
        printf("failed to fetch V\n");
    } else {
        ble_voltage = u16 / 100.0f;
    }

    success = get_gatt_characteristic_shell_u32(bluetooth_address, uuid_str_A, u32);
    if(!success) {
        printf("failed to fetch A\n");
    } else {
        int32_t i = u32;
        ble_current_flow = -static_cast<float>(i) / 10000.0f;
    }
}

void get_gatt_data(gatt_connection_t* connection)
{
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
            ble_state_of_charge = i / 100.0f;
            printf("SOC is %f\n", ble_state_of_charge);
            free(buffer);
        } else {
            printf("error getting SOC: %d\n", err);
        }
    }

    // Power flow
    if ( gattlib_string_to_uuid(uuid_str_P, strlen(uuid_str_P) + 1, &uuid) == 0){
        size_t len;
        uint8_t *buffer = NULL;

        err =  gattlib_read_char_by_uuid(connection, &uuid, (void **)&buffer, &len);
        if(err == GATTLIB_SUCCESS){
            int16_t i = (buffer[1] << 8) | buffer[0];
            ble_power_flow = -static_cast<float>(i);
            printf("power flow is %f W\n", ble_power_flow);
            free(buffer);
        } else {
            printf("error getting SOC: %d\n", err);
        }
    }
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

void add_sample(sqlite3* db, int channel, float value)
{
    static char stmt[512];
    printf("channel %d = %f\n", channel, value);
    sprintf(stmt, "INSERT INTO samples(timestamp, channel_id, value) VALUES (%u, %d, %f);\n", (unsigned int)time(0), channel, value);
    sqlite3_stmt* s = NULL;
    int rc = sqlite3_prepare_v2(db, stmt, strlen(stmt) + 1, &s, NULL);

    if(rc != SQLITE_OK) {
	fprintf(stderr, "Didn't prepare INSERT statement: %s\n", sqlite3_errmsg(db));
	sqlite3_finalize(s);
	// sqlite3_close(db);
	// exit(EXIT_FAILURE);
	return;
    }
    if(s == NULL) {
	fprintf(stderr, "failed to compile statement\n");
	sqlite3_finalize(s);
	// sqlite3_close(db);
	// exit(EXIT_FAILURE);
	return;
    }

    rc = sqlite3_step(s);

    if(rc != SQLITE_DONE) {
	fprintf(stderr, "Didn't step INSERT statement: %s\n", sqlite3_errmsg(db));
	sqlite3_finalize(s);
	// sqlite3_close(db);
	// exit(EXIT_FAILURE);
	return;
    }
    rc = sqlite3_finalize(s);
    s = NULL;
    if(rc != SQLITE_OK) {
	fprintf(stderr, "Didn't finalize: %s\n", sqlite3_errmsg(db));
    }
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
    float battery_temp = -1;
    float heatsink_temp = -1;
    float target_battery_voltage = -1;
    time_t local_time = time(0);

    std::thread* t = nullptr;
    while(1) {

        local_time = time(0);

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

	target_battery_voltage = reg(mb, 51) / 32768.0 * vscale;

        modbus_close(mb);
        modbus_free(mb);

        const char* deviceID = "D0:EB:11:A0:EF:1D";

        if(false) {
            gatt_connection_t* con = gattlib_connect(NULL, deviceID, BDADDR_LE_RANDOM);
            if (con != NULL) {
                get_gatt_data(con);
                gattlib_disconnect(con);
                printf("BLE connection succeeded \n");
            } else {
                fprintf(stderr, "Failed to connect to the bluetooth device %s.\n",deviceID);
            }
        } else {
            get_gatt_data_shell(deviceID);
        }

	sqlite3* db;
	int rc = sqlite3_open("solar_data.sqlite3", &db);
	if(rc != SQLITE_OK)
	{
	    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	    sqlite3_close(db);
	    sleep(10);
	    continue;
	}

	add_sample(db, 0, battery_voltage);
	add_sample(db, 1, target_battery_voltage);
	add_sample(db, 2, charge_current);
	add_sample(db, 3, array_voltage);
	add_sample(db, 4, output_power);
	add_sample(db, 5, input_power);
	add_sample(db, 6, battery_temp);
	add_sample(db, 7, heatsink_temp);
        // Not sure about these two; Victron outputs one signed value for amps, charging is negative, draw is positive
        add_sample(db, 8, std::max(0.0f, ble_current_flow));
	add_sample(db, 10, std::max(0.0f, -ble_current_flow));
        // INSERT INTO channels VALUES (9, "Inverter Power Output");
	add_sample(db, 12, ble_state_of_charge);
	add_sample(db, 13, ble_voltage);
	add_sample(db, 14, ble_power_flow);

	rc = sqlite3_close(db);
	if(rc != SQLITE_OK) {
	    fprintf(stderr, "Can't close database: %s\n", sqlite3_errmsg(db));
	}

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

        if(t == nullptr) {
            t = new std::thread([&]{

                // HTTP
                httplib::Server svr;

                // HTTPS
                // httplib::SSLServer svr;

                svr.Get("/status", [&](const httplib::Request &, httplib::Response &res) {
                    static char str[4096];
                    std::string json = "{ ";
                    sprintf(str, "\"local-time\": %lu,", local_time);
                    json += str;
                    sprintf(str, "\"power-use\": %f,", ble_power_flow);
                    json += str;
                    sprintf(str, "\"state-of-charge\": %f,", ble_state_of_charge);
                    json += str;
                    sprintf(str, "\"shunt-voltage\": %f,", ble_voltage);
                    json += str;
                    sprintf(str, "\"shunt-current-flow\": %f,", ble_current_flow);
                    json += str;
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


        sleep(5);
    }
}
