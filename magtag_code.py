# SPDX-FileCopyrightText: 2020 Brent Rubell for Adafruit Industries
#
# SPDX-License-Identifier: MIT

import os
import ipaddress
import ssl
import wifi
import socketpool
import adafruit_requests
import board

import supervisor
supervisor.status_bar.display = False

display = board.DISPLAY
# display.root_group.hidden = False

# URLs to fetch from
STATUS_URL = "http://192.168.1.35:8080/status"

if False:
    print("ESP32-S2 WebClient Test")

    print(f"My MAC address: {[hex(i) for i in wifi.radio.mac_address]}")

    print("Available WiFi networks:")
    for network in wifi.radio.start_scanning_networks():
        print("\t%s\t\tRSSI: %d\tChannel: %d" % (str(network.ssid, "utf-8"),
                                                 network.rssi, network.channel))
    wifi.radio.stop_scanning_networks()

print(f"Connecting to {os.getenv('WIFI_SSID')}")
wifi.radio.connect(os.getenv("WIFI_SSID"), os.getenv("WIFI_PASSWORD"))
print(f"Connected to {os.getenv('WIFI_SSID')}")
print(f"My IP address: {wifi.radio.ipv4_address}")

if False:
    ping_ip = ipaddress.IPv4Address("8.8.8.8")
    ping = wifi.radio.ping(ip=ping_ip) * 1000
    if ping is not None:
        print(f"Ping google.com: {ping} ms")
    else:
        ping = wifi.radio.ping(ip=ping_ip)
        print(f"Ping google.com: {ping} ms")

pool = socketpool.SocketPool(wifi.radio)
requests = adafruit_requests.Session(pool, ssl.create_default_context())

print(f"Fetching json from {STATUS_URL}")
response = requests.get(STATUS_URL)

# print("-" * 40)

print(f"Battery Positive : {response.json()['battery-terminal']}V")
print(f"Bus Voltage : {response.json()['battery-voltage']}V")
print(f"Output Power : {response.json()['output-power']}W")
print(f"Heatsink Temperature : {response.json()['heatsink-temp']} F")

# print("-" * 40)
# display.root_group.hidden = True

# print("Done")
