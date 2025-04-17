FROM python:3.13.3-slim

# Set the working directory
WORKDIR /app

# Copy the requirements file
COPY requirements.txt .

# Install dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy the Python script
COPY basicClient.py .

# Command to run the Python script
CMD ["python", "basicClient.py"]