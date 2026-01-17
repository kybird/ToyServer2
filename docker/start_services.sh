#!/bin/bash

# Check if docker-compose is installed
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "Error: docker-compose or 'docker compose' is not installed."
    exit 1
fi

echo "Starting NATS and Redis services..."
docker compose up -d

echo "Services status:"
docker compose ps
