#!/bin/sh

kill $(ps aux | grep qemu | grep system | awk '{print $2}')
