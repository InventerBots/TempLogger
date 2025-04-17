import time
import threading
from flask import Flask, render_template, Response

import basicClient

app = Flask(__name__)

current_value = None

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/status')
def status():
    return "Server is running!"

@app.route('/stream')
def stream():
    def generate():
        global current_value
        while True:
            if current_value is not None:
                yield f"data: {round(current_value, 2)}\n\n"
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
            # global current_value
            current_value = basicClient.current_value
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[TCP] Sending STOP_STREAM...")
        tcp_sock.sendall(b"STOP_STREAM\n")
        tcp_sock.close()
        running = False
        print("[TCP] Connection closed")