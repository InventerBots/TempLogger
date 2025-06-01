FROM python:3.13.3-slim

# Set the working directory
WORKDIR /app

# Copy the requirements file
COPY requirements.txt .

# Install dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy the Python script
COPY main.py .
COPY basicClient.py .
COPY templates/ ./templates/

# EXPOSE 8080 5000 6000

# ENV "ARDUINO_IP"="10.32.1.25"

ENTRYPOINT ["python", "main.py"]