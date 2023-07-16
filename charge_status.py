import time
import csv
import sys

solar_csv = csv.reader(sys.stdin)

# from some website
charge_table = {
    12.70: 100,
    12.64: 95,
    12.58: 90,
    12.52: 85,
    12.46: 80,
    12.40: 75,
    12.36: 70,
    12.32: 65,
    12.28: 60,
    12.24: 55,
    12.20: 50,
    12.16: 45,
    12.12: 40,
    12.08: 35,
    12.04: 30,
    12.00: 25,
    11.98: 20,
    11.96: 15,
    11.94: 10,
    11.92: 5,
    11.90: 0,
}

# from windsun.com deep cycle battery faq
charge_table = {
    12.70: 100,
    12.5: 90,
    12.42: 80,
    12.32: 70,
    12.20: 60,
    12.06: 50,
    11.9: 40,
    11.75: 30,
    11.58: 20,
    11.31: 10,
    10.5: 0,
}

def charge_state(bat):
    # depends on temperature as well
    closest_lt = (-100, 0)
    closest_gt = (100, 100)
    for (voltage, percent) in charge_table.iteritems():
	if (voltage < bat) and (voltage > closest_lt[0]):
	    closest_lt = (voltage, percent)
	if (voltage > bat) and (voltage < closest_gt[0]):
	    closest_gt = (voltage, percent)
    voltage_diff = closest_gt[0] - closest_lt[0]
    percent_diff = closest_gt[1] - closest_lt[1]
    fraction = (bat - closest_lt[0]) / voltage_diff
    return closest_lt[1] + percent_diff * fraction

history_length = 200
header = False
history = False
for row in solar_csv:
    if not header:
	header = row
	continue
    (seconds, bat, target, charge_current, array_voltage, output_power) = [float(v) for v in row]
    if bat > 4 * 12.7:
	percent = 100
    elif bat < 4 * 11.9:
	percent = 0
    else:
	percent = charge_state(bat / 4)

    if not history:
	history = history_length * [[seconds, percent],]
    else:
	del history[0]
	history.append([seconds, percent],)

    remaining = 86400
    remaining50 = 86400

    if False:
	if history[-1][0] - history[0][0] > 0:
	    dpdt = -(history[-1][1] - history[0][1]) / (history[-1][0] - history[0][0])
	    if dpdt > 0:
		remaining50 = (percent - 50) / dpdt
		remaining = (percent - 0) / dpdt
    else:
	rate = 0
	total = 0
	for i in xrange(1, len(history)):
	    if history[i][0] > history[i - 1][0]:
		rate -= float(history[i][1] - history[i - 1][1]) / float(history[i][0] - history[i - 1][0])
		# print history[i][1], history[i - 1][1], history[i][0], history[i - 1][0], rate
		total += 1
	if total > 0 and rate > 0:
	    rate /= total
	    remaining50 = max(0, (percent - 50) / rate)
	    remaining = max(0, (percent - 0) / rate)

    rem50_time = "%02d:%02d:%02d" % (remaining50 / 3600, (remaining50 % 3600) / 60, remaining50 % 60)
    rem_time = "%02d:%02d:%02d" % (remaining / 3600, (remaining % 3600) / 60, remaining % 60)

    prettytime = time.strftime("%H:%M:%I %p", time.localtime(int(seconds)))
    print ", ".join([str(v) for v in (prettytime, bat, target, charge_current, array_voltage, output_power, percent, rem50_time, rem_time)])
