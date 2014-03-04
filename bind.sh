#!/bin/bash
echo -n 0003:045E:0768.0001> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:0768.0002> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:0768.0003> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:0768.0004> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:0768.0005> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:0768.0006> /sys/bus/hid/drivers/hid-generic/unbind

echo -n 0003:045E:0768.0001 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:0768.0002 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:0768.0003 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:0768.0004 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:0768.0005 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:0768.0006 > /sys/bus/hid/drivers/microsoft/bind

echo -n 0003:045E:074B.0001> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:074B.0002> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:074B.0003> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:074B.0004> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:074B.0005> /sys/bus/hid/drivers/hid-generic/unbind
echo -n 0003:045E:074B.0006> /sys/bus/hid/drivers/hid-generic/unbind

echo -n 0003:045E:074B.0001 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:074B.0002 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:074B.0003 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:074B.0004 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:074B.0005 > /sys/bus/hid/drivers/microsoft/bind
echo -n 0003:045E:074B.0006 > /sys/bus/hid/drivers/microsoft/bind
