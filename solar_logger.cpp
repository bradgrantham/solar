#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h> /* POSIX terminal control definitions */
#include <map>
#include <string>

#include <sqlite3.h>
#include <modbus.h>

namespace pentametric
{

enum format_type {
    FORMAT1,
    FORMAT2,
    FORMAT3,
    FORMAT2B,
    FORMAT4,
    FORMAT5,
    FORMAT6,
};

struct display_type
{
    display_type() {}
    display_type(ssize_t length_, format_type type_, std::string description_) :
        length(length_),
        type(type_)
    { }
    ssize_t length;
    format_type type;
};

std::map<int, display_type> display_values;

void init_display_types()
{
    display_values[3] = display_type(2, FORMAT1, "Average Battery1 Volts");

    display_values[5] = display_type(3, FORMAT2, "Amps on Channel 1");
    display_values[6] = display_type(3, FORMAT2, "Amps on Channel 2");

    display_values[8] = display_type(3, FORMAT2, "Average Amps on Channel 1");
    display_values[9] = display_type(3, FORMAT2, "Average Amps on Channel 2");
    display_values[10] = display_type(3, FORMAT2, "Average Amps on Channel 3");

    display_values[12] = display_type(3, FORMAT3, "Amp-hours on Channel 1");
    display_values[13] = display_type(3, FORMAT3, "Amp-hours on Channel 2");
    // display_values[15] = display_type(4, FORMAT4, "Amp-hours on Channel 3");

    display_values[23] = display_type(3, FORMAT2, "Watts1");
    display_values[24] = display_type(3, FORMAT2, "Watts2");

    display_values[21] = display_type(4, FORMAT5, "Watthours1");
    display_values[22] = display_type(4, FORMAT5, "Watthours2");

    display_values[26] = display_type(1, FORMAT6, "Battery 1 Percent Full");
    display_values[27] = display_type(1, FORMAT6, "Battery 2 Percent Full");
}

unsigned char calc_checksum(unsigned char *buf, int n)
{
    unsigned int sum = 0;
    for(int i = 0; i < n; i++)
	sum += buf[i];
    return 0xff - sum;
}

float convert_to_float(unsigned char *buffer, int type)
{
    switch(type)
    {
	case FORMAT1:
	{
	    int foo = ((buffer[1] << 8) | buffer[0]) & 0x7ff;
	    return foo / 20.0;
	}
	case FORMAT2:
	case FORMAT3:
	{
	    if(false) {
		bool complement = (buffer[2] & 0x80) != 0;
		int foo;
		foo = ((buffer[2] & 0x7f) << 16) | (buffer[1] << 8) | buffer[0];
		if(complement)
		    foo ^= 0x7fffff;
		foo *= -1;
		return foo / 100.0;
	    } else {
		int foo = (buffer[2] << 24) | (buffer[1] << 16) | (buffer[0] << 8);
		return (foo >> 8) / -100.0;
	    }
	}
	case FORMAT2B:
	{
	    return 0;
	}
	case FORMAT4:
	{
	    return 0;
	}
	case FORMAT5:
	{
	    return 0;
	}
	case FORMAT6:
	{
	    return buffer[0];
	}
	default:
	{
	    return 0;
	}
    }
}

struct port
{
    port(const char *file_) :
        serial(-1),
        filename(file_)
	{reopen();}
    int serial;
    const char *filename;
    void reopen();
    void shut();
    bool load_value(int which, float *value);
};

void port::shut()
{
    if(serial != -1)
	close(serial);
    serial = -1;
}

bool port::load_value(int which, float *value)
{
    if(serial == -1)
	reopen();

    unsigned char buffer[8];
    buffer[0] = 0x81;
    buffer[1] = which;
    buffer[2] = display_values[which].length;
    buffer[3] = calc_checksum(buffer, 3);
    printf("write [%02x, %02x, %02x, %02x]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    unsigned char result[16];

    ssize_t count;
    count = write(serial, buffer, 4);
    if(count != 4) {
	perror("write");
	fprintf(stderr, "wrote only %u bytes out of 4\n", count);
	shut();
	return false;
    }

    sleep(1);
    count = read(serial, result, display_values[which].length + 1);
    if(count != display_values[which].length + 1) {
	perror("read");
	fprintf(stderr, "read only %d bytes out of %u\n", count, display_values[which].length + 1);
	shut();
	return false;
    }
    *value = convert_to_float(result, display_values[which].type);
    return true;
}


void port::reopen()
{
    if(serial != -1)
	shut();
    int baud = B2400;
    struct termios options;
    serial = open(filename, O_RDWR /* | O_NONBLOCK */);

    if(serial == -1) {
	fprintf(stderr, "couldn't open serial port file\n");
	exit(EXIT_FAILURE);
    }

    /*
     * get the current options 
     */
    tcgetattr(serial, &options);

    /*
     * set raw input, 1 second timeout 
     */
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;


#if 0
    options.c_iflag &= ~INPCK;	/* Enable parity checking */
    options.c_iflag |= IGNPAR;

    options.c_cflag &= ~PARENB;	/* Clear parity enable */
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    options.c_cflag &= ~CRTSCTS;

    // options.c_iflag &= ~(IXON | IXOFF | IXANY); /* no flow control */

    options.c_oflag &= ~(IXON | IXOFF | IXANY);	/* no flow control */
#endif

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;	/* No output processing */
    options.c_iflag &= ~INLCR;	/* Don't convert linefeeds */
    options.c_iflag &= ~ICRNL;	/* Don't convert linefeeds */

    /*
     * Miscellaneous stuff 
     */
    options.c_cflag |= (CLOCAL | CREAD);	/* Enable receiver, set
						 * local */
    /*
     * Linux seems to have problem with the following ??!! 
     */
    options.c_cflag |= (IXON | IXOFF);	/* Software flow control */
    options.c_lflag = 0;	/* no local flags */
    options.c_cflag |= HUPCL;	/* Drop DTR on close */

    cfsetispeed(&options, baud);
    int speed = cfgetispeed(&options);
    printf("set tty input to speed %d, expected %d\n", speed, baud);
    cfsetospeed(&options, baud);
    speed = cfgetospeed(&options);
    printf("set tty output to speed %d, expected %d\n", speed, baud);

    /*
     * Clear the line 
     */
    tcflush(serial, TCIFLUSH);

    if (tcsetattr(serial, TCSANOW, &options) != 0) {
	perror("setting serial tc");
	return;
    }
    tcflush(serial, TCIFLUSH);
}

};


/*
    float bat_term, bat_sense, in_power, out_power;
    printf("battery voltage %f\n", mb.reg(38) / 32768.0 * vscale);
    printf("battery terminal voltage %f\n", bat_term = mb.reg(25) / 32768.0 * vscale);
    printf("battery sense voltage %f\n", bat_sense = mb.reg(26) / 32768.0 * vscale);
    printf("battery wire efficiency = %f %%\n", 100 * bat_term / bat_sense);
    printf("target battery voltage %f\n", mb.reg(51) / 32768.0 * vscale);
    printf("charge current %f\n", mb.reg(39) / 32768.0 * iscale);
    printf("array voltage %f\n", mb.reg(27) / 32768.0 * vscale);
    printf("input power %f\n", in_power = mb.reg(59) / 131072.0 * iscale * vscale);
    printf("output power %f\n", out_power = mb.reg(58) / 131072.0 * iscale * vscale);
    printf("conversion efficiency = %f %%\n", 100 * out_power / in_power);
    printf("bat temp %d\n", mb.reg(37) * 9 / 5 + 32);
    printf("heatsink temp %d\n", mb.reg(35) * 9 / 5 + 32);
*/

char* address;
int port = 502;
const int slave = 1;
float timeout = 15.0;
int reset_sleep = 60;

struct modbus {
    modbus_t *mb;
    const char *addr;
    int port;
    int slave;
    float timeout;
    int reset_sleep;
    modbus(const char* addr_, int port_, int slave_, float timeout_, int reset_sleep_) :
        mb(NULL),
        addr(addr_),
        port(port_),
        slave(slave_),
        timeout(timeout_),
        reset_sleep(reset_sleep_)
    {
	while(!reopen()) {
	    fprintf(stderr, "failed to open modbus, sleep for %d\n", reset_sleep);
	    sleep(reset_sleep);
	}
    }
    ~modbus() {
	modbus_free(mb);
	modbus_close(mb);
    }
    bool reopen();
    int reg(int r);
};

int modbus::reg(int r)
{
    uint16_t tab_reg[32];
    int result;

    while((result = modbus_read_registers(mb, r, 1, tab_reg)) == -1) {
        int last_error = errno;
        fprintf(stderr, "failure on address %d: \"%s\"\n", r, modbus_strerror(last_error));

	while(!reopen()) {
	    fprintf(stderr, "failed to open modbus, sleep for %d\n", reset_sleep);
	    sleep(reset_sleep);
	}
    }

    return tab_reg[0];
}

bool modbus::reopen()
{
    if(mb != NULL) {
	modbus_close(mb);
	modbus_free(mb);
    }

    mb = modbus_new_tcp(address, port);
    if(mb == NULL) {
        fprintf(stderr, "failed to open modbus at %s, port %d\n", address, port);
	return false;
    }
    int result = modbus_connect(mb);
    if(result == -1)
    {
        int last_error = errno;
        fprintf(stderr, "modbus_connect: %s\n", modbus_strerror(last_error));
	modbus_close(mb);
	modbus_free(mb);
	mb = NULL;
	return false;
    }

    result = modbus_set_slave(mb, slave);
    if(result == -1) {
        int last_error = errno;
        fprintf(stderr, "set slave error: %s\n", modbus_strerror(last_error));
	modbus_close(mb);
	modbus_free(mb);
	mb = NULL;
	return false;
    }

    uint32_t sec = (uint32_t)timeout;
    uint32_t usec = (uint32_t)((timeout - (uint32_t)timeout) * 1e6);
    modbus_set_byte_timeout(mb, sec, usec);
    modbus_set_response_timeout(mb, sec, usec);

    return true;
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
    char *progname = argv[0];
    argc--;
    argv++;

    pentametric::init_display_types();

    pentametric::port penta("/dev/ttyUSB0");

    address = strdup("192.168.2.2");

    while(argc > 0) {
	if(strcmp(argv[0], "--server") == 0) {
	    if(argc < 2) {
		fprintf(stderr, "expected address argument for \"--server\"\n");
		exit(EXIT_FAILURE);
	    }
	    free(address);
	    address = strdup(argv[1]);
	    argc -= 2;
	    argv += 2;
	} else if(strcmp(argv[0], "--port") == 0) {
	    if(argc < 2) {
		fprintf(stderr, "expected port argument for \"--port\"\n");
		exit(EXIT_FAILURE);
	    }
	    port = atoi(argv[1]);
	    argc -= 2;
	    argv += 2;
	} else {
	    fprintf(stderr, "unknown argument \"%s\"\n", argv[0]);
	    exit(EXIT_FAILURE);
	}
    }

    modbus mb(address, port, slave, timeout, reset_sleep);

    float vscale = mb.reg(0) + mb.reg(1) / 65536.0;
    float iscale = mb.reg(2) + mb.reg(3) / 65536.0;

    while(1)
    {
	sqlite3* db;
	int rc;
	rc = sqlite3_open("solar_data.sqlite3", &db);
	if(rc != SQLITE_OK)
	{
	    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	    sqlite3_close(db);
	    sleep(10);
	    continue;
	}

	float battery_voltage = mb.reg(38) / 32768.0 * vscale;
	add_sample(db, 0, battery_voltage);
	float target_battery_voltage = mb.reg(51) / 32768.0 * vscale;
	add_sample(db, 1, target_battery_voltage);
	float charge_current = mb.reg(39) / 32768.0 * iscale;
	add_sample(db, 2, charge_current);
	float array_voltage = mb.reg(27) / 32768.0 * vscale;
	add_sample(db, 3, array_voltage);
	float output_power = mb.reg(58) / 131072.0 * iscale * vscale;
	add_sample(db, 4, output_power);
	float input_power = mb.reg(59) / 131072.0 * iscale * vscale;
	add_sample(db, 5, input_power);
	float battery_fahrenheit = mb.reg(37) * 9 / 5 + 32;
	add_sample(db, 6, battery_fahrenheit);
	float heatsink_fahrenheit = mb.reg(35) * 9 / 5 + 32;
	add_sample(db, 7, heatsink_fahrenheit);

        bool success;
	float battery_amps = -1e6, load_amps = -1e6, battery_full = -1e6;
	success = penta.load_value(8, &battery_amps);
	if(success)
	    add_sample(db, 10, battery_amps);
	success = penta.load_value(9, &load_amps);
	if(success)
	    add_sample(db, 8, load_amps);
	success = penta.load_value(26, &battery_full);
	if(success)
	    add_sample(db, 12, battery_full);

	printf("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", battery_voltage, target_battery_voltage, array_voltage, charge_current, input_power, output_power, battery_fahrenheit, heatsink_fahrenheit, battery_amps, load_amps, battery_full);
	fflush(stdout);

	rc = sqlite3_close(db);
	if(rc != SQLITE_OK) {
	    fprintf(stderr, "Can't close database: %s\n", sqlite3_errmsg(db));
	}
	sleep(5);
    }
}
