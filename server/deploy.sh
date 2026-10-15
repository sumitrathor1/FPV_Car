#!/usr/bin/env bash
set -e

echo "=== Deploying FPV Car on Azure VM ==="
APP_DIR="/home/azureuser/FPV_Car"

if [ ! -d "$APP_DIR/.git" ]; then
    echo "Cloning repository..."
    git clone https://github.com/sumitrathor1/FPV_Car.git "$APP_DIR"
else
    echo "Pulling latest changes..."
    cd "$APP_DIR"
    git fetch origin
    git reset --hard origin/main
fi

cd "$APP_DIR/server"

# Create virtualenv if not exists
if [ ! -d "venv" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv venv
fi

echo "Installing/updating dependencies..."
venv/bin/pip install --upgrade pip
venv/bin/pip install -r requirements.txt

# Setup systemd service
echo "Configuring systemd service..."
sudo cp fpv-car.service /etc/systemd/system/fpv-car.service
sudo systemctl daemon-reload
sudo systemctl enable fpv-car.service
sudo systemctl restart fpv-car.service

# Setup Nginx
echo "Configuring Nginx..."
sudo cp nginx.conf /etc/nginx/sites-available/default
sudo nginx -t
sudo systemctl reload nginx

echo "=== Deployment Successful! ==="
sudo systemctl status fpv-car.service --no-pager
