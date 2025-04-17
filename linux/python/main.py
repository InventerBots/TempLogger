import time
import threading
from flask import Flask, render_template, Response

import basicClient

app = Flask(__name__)

temp_ch1 = None
temp_ch2 = None
temp_ch3 = None

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/status')
def status():
    return "Server is running!"

@app.route('/stream')
def stream():
    def generate():
        global temp_ch1
        global temp_ch2
        global temp_ch3

        while True:
            if temp_ch1 is not None:
                yield f"data: {round(temp_ch1, 2)}\n\n"
            if temp_ch2 is not None:
                yield f"data: {round(temp_ch2, 2)}\n\n"
            if temp_ch3 is not None:
                yield f"data: {round(temp_ch3, 2)}\n\n"
            time.sleep(1)

    return Response(generate(), mimetype='text/event-stream')

def start_flask_server():
    # Run Flask app on a separate thread
    app.run(host='0.0.0.0', port=8080, debug=False)

if __name__ == "__main__":
    # Start Flask server in a separate thread
    flask_thread = threading.Thread(target=start_flask_server, daemon=True)
    flask_thread.start()

    # Start the basic client
    tcp_sock = basicClient.main()

    try:
        while True:
            temp_ch1 = basicClient.temp_ch1
            temp_ch2 = basicClient.temp_ch2
            temp_ch3 = basicClient.temp_ch3
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[TCP] Sending STOP_STREAM...")
        tcp_sock.sendall(b"STOP_STREAM\n")
        tcp_sock.close()
        running = False
        print("[TCP] Connection closed")