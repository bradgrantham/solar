#include <cstdio>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h> /* POSIX terminal control definitions */
#include <map>

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

float load_value(int serial_fd, int which)
{
    unsigned char buffer[8];
    buffer[0] = 0x81;
    buffer[1] = which;
    buffer[2] = display_values[which].length;
    buffer[3] = calc_checksum(buffer, 3);
    printf("write [%02x, %02x, %02x, %02x]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    unsigned char result[16];

    ssize_t count;
    count = write(serial_fd, buffer, 4);
    if(count != 4) {
	perror("write");
	fprintf(stderr, "wrote only %u bytes out of 4\n", count);
	return 0.0;
    }

    sleep(1);
    ssize_t total = 0;
    do{
	count = read(serial_fd, result + total, display_values[which].length + 1 - total);
	if(count < 0) {
	    perror("read");
	    return 0.0;
	}
	printf("read %d bytes\n", count);
	total += count;
    } while(count < display_values[which].length + 1);
    if(total != display_values[which].length + 1) {
	perror("read");
	fprintf(stderr, "read only %d bytes out of %u\n", total, display_values[which].length + 1);
	return 0.0;
    }
    printf("buffer = [");
    for(int i = 0; i < display_values[which].length + 1; i++)
	printf("%02x, ", result[i]);
    printf("]\n");
    return convert_to_float(result, display_values[which].type);
}

};

const char *serial_name = "/dev/ttyUSB0";

int main(int argc, char **argv)
{
    pentametric::init_display_types();
    int baud = B2400;
    struct termios options;
    int serial = open(serial_name, O_RDWR /* | O_NONBLOCK */);

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
	return (0);
    }
    tcflush(serial, TCIFLUSH);


    if(serial == -1) {
	fprintf(stderr, "couldn't open serial port file\n");
	exit(EXIT_FAILURE);
    }

    // 82, 03, 02, 79

    // printf("average battery 1 volts: %f\n", pentametric::load_value(serial, 3));
    printf("channel 1 average amps: %f\n", pentametric::load_value(serial, 8));
    printf("channel 2 average amps: %f\n", pentametric::load_value(serial, 9));
    // printf("battery full: %f\n", pentametric::load_value(serial, 26));
    // printf("channel 1 amps: %f\n", pentametric::load_value(serial, 5));
    // printf("channel 2 amps: %f\n", pentametric::load_value(serial, 6));
}
