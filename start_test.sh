#!/bin/sh
LOG_DIR=test/logs
mkdir -p $LOG_DIR

if [ x$1 == xstop ]; then
    ps aux | grep 511 | grep server | grep -v grep | grep -v nginx | awk '{print $2}' | xargs kill
else
    valgrind --leak-check=full --track-origins=yes ./echo_server 1>$LOG_DIR/echo_server_log 2>$LOG_DIR/echo_server_error_log &
    valgrind --leak-check=full --track-origins=yes ./server 1>$LOG_DIR/server_log 2>$LOG_DIR/server_error_log &
fi
