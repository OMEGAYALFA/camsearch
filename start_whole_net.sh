#!/usr/bin/env bash

CAMSEARCH="./camsearch $*"

$CAMSEARCH -f '1.0.0.0' -t '2.255.255.255'
# 3.0.0.0/8 General Electric Company
# 4.0.0.0/8 Level 3 Communications Inc.
$CAMSEARCH -f '5.0.0.0' -t '6.255.255.255'
# 7.0.0.0/8 DoD
# 8.0.0.0/8 Level 3 Communications Inc.
# 9.0.0.0/8 IBM
# 10.0.0.0/8 Private
# 11.0.0.0/8 DoD
# 12.0.0.0/8 AT&T
$CAMSEARCH -f '13.0.0.0' -t '14.255.255.255'
# 15.0.0.0/8 Hewlett-Packard
# 16.0.0.0/8 Hewlett-Packard
# 17.0.0.0/8 Apple
# 18.0.0.0/8 MIT
# 19.0.0.0/8 Ford
# 20.0.0.0/8 Computer Sciences Corporation
# 21.0.0.0/8 DoD
# 22.0.0.0/8 DoD
$CAMSEARCH -f '23.0.0.0' -t '24.255.255.255'
# 25.0.0.0/8  ??? UK Ministry of Defence ???
# 26.0.0.0/8 DoD
$CAMSEARCH -f '27.0.0.0' -t '27.255.255.255'
# 28.0.0.0/8 DoD
# 29.0.0.0/8 DoD
# 30.0.0.0/8 DoD
$CAMSEARCH -f '31.0.0.0' -t '31.255.255.255'
# 32.0.0.0/8 AT&T
# 33.0.0.0/8 DoD
# 34.0.0.0/8 Halliburton Company
$CAMSEARCH -f '35.0.0.0' -t '37.255.255.255'
# 38.0.0.0/8 PSINet, Inc.
$CAMSEARCH -f '39.0.0.0' -t '47.255.255.255'
# 44.0.0.0/8 Amateur Radio Digital Communication
# 48.0.0.0/8 DoD
$CAMSEARCH -f '49.0.0.0' -t '50.255.255.255'
# 51.0.0.0/8 ???? Formerly UK Government Department for Work and Pensions.
$CAMSEARCH -f '52.0.0.0' -t '54.255.255.255'
# 55.0.0.0/8 DoD
# 56.0.0.0/8 US Postal Service
$CAMSEARCH -f '57.0.0.0' '-t 126.255.255.255'
# 127.0.0.0/8 Loopback
$CAMSEARCH -f '128.0.0.0' -t '168.255.255.255'
# $CAMSEARCH -f '169' TODO
$CAMSEARCH -f '170.0.0.0' -t '171.255.255.255'
# $CAMSEARCH -f '172' TODO
$CAMSEARCH -f '173.0.0.0' -t '191.255.255.255'
# $CAMSEARCH -f '192' TODO
$CAMSEARCH -f '193.0.0.0' -t '213.255.255.255'
# 214.0.0.0/8 DoD
# 215.0.0.0/8 DoD
$CAMSEARCH -f '216.0.0.0' -t '223.255.255.255'
# 224.0.0.0/8 - 255.0.0.0 Reserved
