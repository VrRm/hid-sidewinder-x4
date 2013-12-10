#!/bin/bash
echo -n "0003:045E:0768.0001" > /sys/bus/hid/drivers/hid-generic/unbind
echo -n "0003:045E:0768.0002" > /sys/bus/hid/drivers/hid-generic/unbind
echo -n "0003:045E:0768.0001" > /sys/bus/hid/drivers/microsoft/bind
echo -n "0003:045E:0768.0002" > /sys/bus/hid/drivers/microsoft/bind
