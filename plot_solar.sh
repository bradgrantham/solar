NOW=`date +"%s"`
ONE_DAY_AGO=`expr $NOW - 86400`
ONE_HOUR_AGO=`expr $NOW - 3600`

cd /home/grantham/solar

cat > /var/www/index_new.html <<HTML
<html>
<body>
<a href="http://192.168.2.2/">Solar Charge Controller Live Data View</a><p><p>
HTML

while IFS='@' read channel range output ylabel title
do
    echo $title
# Solar Power Output

    sqlite3 solar_data.sqlite3 > tmp.csv <<END
.timeout 5000
.mode tabs
SELECT timestamp-25200,value from samples where channel_id=$channel and timestamp>$range and timestamp<$NOW;
END

    gnuplot <<END
set terminal png size 1024,768
set output "/tmp/plot.png"
set xdata time
set format x "%I:%M"
set timefmt "%s"
set ylabel "$ylabel"
set xlabel "Time"
plot "tmp.csv" using 1:2 title '$title' with lines
END

mv /tmp/plot.png $output

    base=`basename $output`
    echo '<a href="'$base'">'$title'<a><p>' >> /var/www/index_new.html

done << GRAPHS
10@$ONE_HOUR_AGO@/var/www/battery_amps_hour.png@Amps@Battery Amps, One Hour
8@$ONE_HOUR_AGO@/var/www/inverter_amps_hour.png@Amps@Inverter Amps, One Hour
12@$ONE_HOUR_AGO@/var/www/battery_capacity_hour.png@Percent@Battery Capacity, One Hour
5@$ONE_HOUR_AGO@/var/www/solar_power_hour.png@Watts@Solar Power, One Hour
7@$ONE_HOUR_AGO@/var/www/heatsink_temperature_hour.png@Degrees Fahrenheit@Charge Controller Heatsink Temperature, One Hour
0@$ONE_HOUR_AGO@/var/www/battery_voltage_hour.png@Volts@Battery Bank Voltage, One Hour
10@$ONE_DAY_AGO@/var/www/battery_amps_day.png@Amps@Battery Amps, 24 Hours
8@$ONE_DAY_AGO@/var/www/inverter_amps_day.png@Amps@Inverter Amps, 24 Hours
12@$ONE_DAY_AGO@/var/www/battery_capacity_day.png@Percent@Battery Capacity, 24 Hours
5@$ONE_DAY_AGO@/var/www/solar_power_day.png@Watts@Solar Power, 24 Hours
7@$ONE_DAY_AGO@/var/www/heatsink_temperature_day.png@Degrees Fahrenheit@Charge Controller Heatsink Temperature, 24 Hours
0@$ONE_DAY_AGO@/var/www/battery_voltage_day.png@Volts@Battery Bank Voltage, 24 Hours
GRAPHS
	# float battery_amps = pentametric::load_value(serial, 8);
	# add_sample(db, 10, heatsink_fahrenheit);
	# float load_amps = pentametric::load_value(serial, 9);
	# add_sample(db, 8, heatsink_fahrenheit);
	# float battery_full = pentametric::load_value(serial, 26);
	# add_sample(db, 12, heatsink_fahrenheit);

cat >> /var/www/index_new.html <<HTML
</body>
</html>
HTML

mv /var/www/index_new.html /var/www/index.html
