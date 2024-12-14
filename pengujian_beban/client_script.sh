#!/bin/bash
SERVER_IP="192.168.34.52"  # IP server
SERVER_PORT=8080       # Port server

# Username dan pesan
USERNAME="user_$$"  # Gunakan PID unik sebagai username
MESSAGE="Hello from client $$"

# Kirim username dan pesan ke server
{
    echo $USERNAME  # Kirim username
    echo $MESSAGE   # Kirim pesan
} | nc $SERVER_IP $SERVER_PORT
