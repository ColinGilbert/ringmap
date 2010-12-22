#!/bin/sh

tail -f /var/log/messages | grep "ringmap\|RINGMAP\|rm_8254"
