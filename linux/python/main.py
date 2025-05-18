import time
import threading
import json
import os
import psycopg2
from flask import Flask, render_template, Response

import basicClient

app = Flask(__name__)

DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = os.getenv("DB_PORT", 5432)
DB_NAME = os.getenv("DB_NAME", "templogger")
DB_USER = os.getenv("DB_USER", "templogger")
DB_PASSWORD = os.getenv("DB_PASSWORD", "")

temp_ch1 = None
temp_ch2 = None
temp_ch3 = None
stream_delay = 1  # Default delay in seconds

# TODO: temps should use a rolling buffer
# TODo: add diagnostics to the web interface

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/status')
def status():
    return "Server is running!"

@app.route('/set_delay/<int:delay>')
def set_delay(delay):
    global stream_delay
    stream_delay = max(0.1, delay)  # Ensure a minimum delay of 0.1 seconds
    return f"Stream delay set to {stream_delay} seconds"

@app.route('/stream')
def stream():
    def generate():
        global temp_ch1
        global temp_ch2
        global temp_ch3

        while True:
            temp_ch1 = round(temp_ch1, 2) if temp_ch1 is not None else None
            temp_ch2 = round(temp_ch2, 2) if temp_ch2 is not None else None
            temp_ch3 = round(temp_ch3, 2) if temp_ch3 is not None else None

            data = {
                "tempCh1": temp_ch1,
                "tempCh2": temp_ch2,
                "tempCh3": temp_ch3
            }
            
            yield f"data: {json.dumps(data)}\n\n"
            time.sleep(stream_delay)

    return Response(generate(), mimetype='text/event-stream')

def start_flask_server():
    # Run Flask app on a separate thread
    app.run(host='0.0.0.0', port=8080, debug=False)

def getDBConnection():
    try:
        conn = psycopg2.connect(
            host=DB_HOST,
            port=DB_PORT,
            dbname=DB_NAME,
            user=DB_USER,
            password=DB_PASSWORD
        )
        return conn
    except Exception as e:
        print(f"Error connecting to database: {e}")
        return None

if __name__ == "__main__":
    # Start Flask server in a separate thread
    flask_thread = threading.Thread(target=start_flask_server, daemon=True)
    flask_thread.start()

    # Start the basic client rpi = 20ms
    tcp_sock = basicClient.main(20)

    try:
        while True:
            temp_ch1 = basicClient.temp_ch1
            temp_ch2 = basicClient.temp_ch2
            temp_ch3 = basicClient.temp_ch3
            time.sleep(1)
    except KeyboardInterrupt:
        basicClient.stopStream()
        print("[TCP] Connection closed")