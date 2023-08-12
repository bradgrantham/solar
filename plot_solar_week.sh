NOW=1346603333
THEN=`expr $NOW - 86400 \* 7`

cat > solar_graphs/week.html <<HTML
<html>
<body>
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
    echo '<a href="'$base'">'$title'<a><p>' >> solar_graphs/week.html

done << GRAPHS
10@$THEN@solar_graphs/battery_amps_week.png@Amps@Battery Amps, Week
8@$THEN@solar_graphs/inverter_amps_week.png@Amps@Inverter Amps, Week
12@$THEN@solar_graphs/battery_capacity_week.png@Percent@Battery Capacity, Week
5@$THEN@solar_graphs/solar_power_week.png@Watts@Solar Power, Week
7@$THEN@solar_graphs/heatsink_temperature_week.png@Degrees Fahrenheit@Charge Controller Heatsink Temperature, Week
0@$THEN@solar_graphs/battery_voltage_week.png@Volts@Battery Bank Voltage, Week
GRAPHS
cat >> solar_graphs/week.html <<HTML
</body>
</html>
HTML
