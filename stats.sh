#!/bin/bash

readelf -s build/gw_boot.elf | grep ": 0000" | sort -g -k 3,3
