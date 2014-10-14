#!/bin/sh
mkdir -p logs

if [ x$1 == xstop ]; then
    ps aux | grep 511 | grep server | grep -v grep | grep -v nginx | awk '{print $2}' | xargs kill
else
    valgrind --leak-check=full ../echo_server 1>logs/echo_server_log 2>logs/echo_server_error_log &
    valgrind --leak-check=full ../server 1>logs/server_log 2>logs/server_error_log &
fi
