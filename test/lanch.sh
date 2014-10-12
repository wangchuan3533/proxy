#!/bin/sh
mkdir -p logs
valgrind --leak-check=full ../echo_server 1>logs/echo_server_log 2>logs/echo_server_error_log &
valgrind --leak-check=full ../server 1>logs/server_log 2>logs/server_error_log &
