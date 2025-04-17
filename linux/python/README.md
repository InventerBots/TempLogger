# TempLogger Project

## Overview
TempLogger is a Python-based project designed to communicate with an Arduino device using UDP and TCP protocols. The main functionality includes listening for packets from the Arduino, calculating latency and jitter, and sending commands to start and stop data streaming.

## Project Structure
```
TempLogger
├── linux
│   ├── python
│   │   ├── basicClient.py      # Main logic for UDP and TCP client
│   │   ├── Dockerfile           # Dockerfile to build the project image
│   │   └── requirements.txt     # Python dependencies
└── README.md                    # Project documentation
```

## Requirements
- Python 3.x
- Docker

## Getting Started

### Building the Docker Image
To build the Docker image for the TempLogger project, navigate to the `linux/python` directory and run the following command:

```
docker build -t temp-logger .
```

### Running the Docker Container
After building the image, you can run the container using the following command:

```
docker run --rm --network host temp-logger
```

This will start the Python script, which will connect to the Arduino device and begin listening for UDP packets.

## Usage
Once the container is running, it will:
- Connect to the specified Arduino IP address and TCP port.
- Start listening for UDP packets containing diagnostic information.
- Print the received analog values along with latency and jitter statistics.

## Stopping the Container
To stop the container, you can use `Ctrl+C` in the terminal where the container is running. This will send a `STOP_STREAM` command to the Arduino and close the TCP connection gracefully.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.