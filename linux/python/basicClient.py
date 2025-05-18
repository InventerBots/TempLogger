import socket
import threading
import json
import time
import os

# Config
ARDUINO_IP = os.getenv("ARDUINO_IP", "10.32.1.25")
TCP_PORT = 6000
UDP_PORT = 5000
BUFFER_SIZE = 1024
RPI = 100

# Control flags
running = True

# Diagnostics state
prev_receive_time = None
prev_arduino_time = None
latencies = []
jitters = []

# Global variable to store data from Arduino UDP stream
# TODO: rename this variable to something more meaningful
# TODO: add the rest of the analog inputs
temp_ch1 = None
temp_ch2 = None
temp_ch3 = None

def udp_listener():
    global prev_receive_time
    global prev_arduino_time
    global temp_ch1
    global temp_ch2
    global temp_ch3

    diag_len = 1000

    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind(('', UDP_PORT))

    print("[UDP] Listening for packets with diagnostics...")

    arduino_start_time = None
    client_start_time = None

    while running:
        try:
            data, addr = udp_sock.recvfrom(BUFFER_SIZE)
            recv_time = time.time() * 1000  # current time in ms
            message = data.decode()
            packet = json.loads(message)

            arduino_time = float(packet.get("timestamp", 0))
            temp_ch1 = packet.get("tempCH0", 0)
            temp_ch2 = packet.get("tempCH1", 0)
            temp_ch3 = packet.get("tempCH2", 0)

            # Update current value
            # temp_ch1 = analog_value

            # Sync Arduino and Client clocks using first packet
            if arduino_start_time is None:
                arduino_start_time = arduino_time
                client_start_time = recv_time

            # Calculate estimated "real" send time based on relative offset
            arduino_send_time = client_start_time + (arduino_time - arduino_start_time)
            latency = arduino_send_time - recv_time
            latencies.append(latency)

            # Jitter
            if prev_receive_time and prev_arduino_time:
                interval_local = recv_time - prev_receive_time
                interval_remote = arduino_time - prev_arduino_time
                jitter = abs(interval_local - interval_remote)
                jitters.append(jitter)
            else:
                jitter = 0.0

            prev_receive_time = recv_time
            prev_arduino_time = arduino_time

            # Stats
            avg_latency = sum(latencies) / len(latencies)
            avg_jitter = sum(jitters) / len(jitters) if jitters else 0

            # print(f"[UDP] Value: {analog_value} | Latency: {latency:.1f} ms | Jitter: {jitter:.1f} ms | Avg Latency: {avg_latency:.1f} | Avg Jitter: {avg_jitter:.1f}")

            # Limit the size of average lists to avoid memory issues
            if len(latencies) > diag_len or len(jitters) > diag_len:
                latencies.pop(0)
                jitters.pop(0)

        except Exception as e:
            print("[UDP] Error:", e)

def main(rpi=RPI):
    global running

    # Start listener thread
    listener_thread = threading.Thread(target=udp_listener, daemon=True)
    listener_thread.start()
    tcp_sock = startStream(rpi)

    return tcp_sock

def startStream(rpi):
    global running

    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_sock.connect((ARDUINO_IP, TCP_PORT))
    print("[TCP] Connected to", ARDUINO_IP)
    cmd = f"START_STREAM {rpi}\n"
    tcp_sock.sendall(cmd.encode())
    print(f"[TCP] Sent {cmd}")
    running = True

    return tcp_sock

def stopStream():
    global running

    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_sock.connect((ARDUINO_IP, TCP_PORT))
    print("[TCP] Sending STOP_STREAM...")
    tcp_sock.sendall(b"STOP_STREAM\n")
    tcp_sock.close()
    print("[TCP] Connection closed")
    running = False
    
    return tcp_sock

if __name__ == "__main__":
    tcp_sock = main()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        stopStream()
        print("[TCP] Connection closed")
