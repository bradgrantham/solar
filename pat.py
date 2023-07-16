import csv
import math

def insolation(sunrise, sunset, when, occlusion_angle):
    sun_angle = (when - sunrise) / (sunset - sunrise) * math.pi
    if when < sunrise or when > sunset:
        return 0
    if sun_angle > occlusion_angle:
        return math.sin(sun_angle)
    else:
        return 0

def sum_insolation(sunrise, sunset, begin, end, occlusion_angle):
    at_begin = insolation(sunrise, sunset, begin, occlusion_angle)
    at_end = insolation(sunrise, sunset, end, occlusion_angle)
    return (at_begin + at_end) * (end - begin) / 2

def calculate_storage(hourly_draw, hourly_power):
    last_surplus = -1
    first_surplus = -1
    for hour in xrange(0,24):
        if hourly_power[hour] > hourly_draw[hour]:
            last_surplus = hour
            if first_surplus == -1:
                first_surplus = hour

    storage = 0
    max_storage = 0
    for delta in xrange(0,24):
        hour = (last_surplus + delta) % 24
        storage = max(0, storage + hourly_draw[hour] - hourly_power[hour])
        max_storage = max(max_storage, storage)

    hourly_reserve = [0] * 24
    reserve = 0
    for delta in xrange(0,24):
        hour = (first_surplus + delta) % 24
        reserve = min(max_storage, reserve + hourly_power[hour] - hourly_draw[hour])
        # print hour, reserve


    return (max_storage, hourly_reserve)


loads = []
load_reader = csv.reader(open("loads_pat.csv", "rb"))
for row in load_reader:
    if row[0][0] == '#':
        continue
    load = {}
    load["name"] = row[0]
    load["watts"] = int(row[1])
    load["need_sine"] = (row[2].lower in ('yes','true')) or (int(row[2]) != 0)
    load["circuit_name"] = row[3]
    load["hourly"] = [float(draw) for draw in row[4:-1]]
    loads.append(load)
# ...

hourly_draw = [0] * 24

system_draw = 0
for load in loads:
    total_draw = 0
    for hour in xrange(0, 23):
        draw = load["hourly"][hour] * load["watts"]
        total_draw += draw
        hourly_draw[hour] += draw
    load["total_draw"] = total_draw
    system_draw += total_draw

print "total system draw", system_draw

# read spreadsheet with loads
# name, watts, needsine, circuitname, 0,1,2,3,4,5,6...,23

charge_controller_efficiency = .95
inverter_efficiency = .88
charge_13_hour_efficiency = .91
discharge_8_hour_efficiency = .91

system_efficiency = charge_controller_efficiency * inverter_efficiency * charge_13_hour_efficiency * discharge_8_hour_efficiency

sunrise = 6.5
sunset = 19.5

panel_elevation = 2
occlusion_height = 9
occlusion_distance = 15

battery_capacity = 110
batteries_already = 2 * battery_capacity * 12 * .9
battery_price = 220
battery_minimum = .4
battery_overbuild = 1 / (1 - battery_minimum)
# print "overbuild = ", battery_overbuild

solar_already = 400
panel_capacity = 230
panel_price = 245

occlusion_angle = math.asin((occlusion_height - panel_elevation) / float(occlusion_distance))

hour_steps = 50 # empirically determined to closely approach integral
total_insol = 0
hourly_power = [0] * 24
for step in xrange(0, 24 * hour_steps - 1):
    delta = 1 / float(hour_steps)
    when = step * delta
    insol = sum_insolation(sunrise, sunset, when, when + delta, occlusion_angle)
    total_insol += insol
    hourly_power[int(when)] += insol


# print hour_steps, total_insol
total_solar = system_draw / total_insol / (charge_controller_efficiency * inverter_efficiency)
print "solar array watts including inefficiencies", total_solar
solar_needed = max(0, total_solar - solar_already)
print "solar array watts needed", solar_needed
panels_needed = int(solar_needed / panel_capacity + .99999)
print "panels needed at %d watts: %d, $%.2f" % (panel_capacity, panels_needed, panels_needed * panel_price)

for hour in range(0, 24):
    hourly_power[hour] *= total_solar

(storage_draw, hourly_reserve) = calculate_storage(hourly_draw, hourly_power)
print "storage requirement", storage_draw
gross_battery_capacity = storage_draw / charge_13_hour_efficiency / discharge_8_hour_efficiency * battery_overbuild
print "system gross battery capacity", gross_battery_capacity
needed_capacity = max(0, gross_battery_capacity - batteries_already)
print "battery capacity needed", needed_capacity
batteries_needed = int(needed_capacity / (battery_capacity * 12) + .9999)
print "batteries needed at %d amphours: %d, $%.2f" % (battery_capacity, batteries_needed, batteries_needed * battery_price)
