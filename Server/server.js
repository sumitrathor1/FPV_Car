const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

let espSocket = null;

// Serve Web Dashboard to Browser Client
app.use(express.static(path.join(__dirname, '../Web')));

wss.on('connection', (ws, req) => {
    const isESP = req.url.includes('role=esp');
    console.log(`New WebSocket connection (${isESP ? 'ESP32-CAM' : 'Web Controller'})`);

    if (isESP) {
        espSocket = ws;
        console.log('ESP32-CAM connected to Cloud Server!');
    }

    ws.on('message', (message) => {
        // If message is from Web Controller, relay command to ESP32
        if (ws !== espSocket && espSocket && espSocket.readyState === WebSocket.OPEN) {
            espSocket.send(message.toString());
        }
        // If message is binary frame from ESP32 (video frame), broadcast to Web Controllers
        else if (ws === espSocket) {
            wss.clients.forEach(client => {
                if (client !== espSocket && client.readyState === WebSocket.OPEN) {
                    client.send(message);
                }
            });
        }
    });

    ws.on('close', () => {
        if (ws === espSocket) {
            console.log('ESP32-CAM disconnected!');
            espSocket = null;
        }
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`===========================================`);
    console.log(` FPV Cloud Server running on Port ${PORT}`);
    console.log(` Access dashboard at http://localhost:${PORT}`);
    console.log(`===========================================`);
});
