#!/bin/bash
IP="127.0.0.1"
PORT="8081"
HOSTNAME="127.0.0.1"

while :; do
  echo -e "GET / HTTP/1.1\r\nHost: $YOUR_VIRTUAL_HOSTNAME\r\nConnection: keep-alive\r\n\r\n"
  sleep 1
done | telnet $IP $PORT
