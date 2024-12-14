#!/bin/bash

NUM_CLIENTS=1000         # Jumlah klien yang ingin dijalankan
SCRIPT="./client_script.sh"  # Lokasi skrip klien

for ((i=1; i<=NUM_CLIENTS; i++)); do
    $SCRIPT &  # Jalankan setiap klien di latar belakang
    echo "Client $i started."
done

# Tunggu semua proses selesai
wait
echo "All clients completed."
