class App {
    constructor() {
        this.canvas = document.getElementById('gameCanvas');
        this.ctx = this.canvas.getContext('2d');
        this.width = window.innerWidth;
        this.height = window.innerHeight;

        // Settings
        this.resetView(); // [Fix] Set initial scale based on view 40
        this.wsUrl = 'ws://localhost:9002';
        this.isDragging = false;
        this.lastMouseX = 0;
        this.lastMouseY = 0;

        // Data
        this.latestState = null;
        this.lastFrameTime = 0;
        this.frameCount = 0;
        this.fps = 0;

        // Connection
        this.ws = null;
        this.reconnectTimer = null;

        this.init();
    }

    init() {
        // Apply control panel styling
        const controlPanel = document.getElementById('control-panel');
        if (controlPanel) { // [Fix] Defensive check for control panel existence
            controlPanel.style.display = 'flex';
            controlPanel.style.gap = '15px';
            controlPanel.style.alignItems = 'center';
            controlPanel.style.flexWrap = 'wrap'; // [Fix] Allow wrapping if many controls
            controlPanel.style.justifyContent = 'center';
            controlPanel.style.maxWidth = '90vw';
        }

        // Resinze Handler
        window.addEventListener('resize', () => this.resize());
        this.resize();

        // Mouse Controls
        this.canvas.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.lastMouseX = e.clientX;
            this.lastMouseY = e.clientY;
            this.canvas.style.cursor = 'grabbing';
        });

        window.addEventListener('mouseup', () => {
            this.isDragging = false;
            this.canvas.style.cursor = 'grab';
        });

        window.addEventListener('mousemove', (e) => {
            if (this.isDragging) {
                this.offsetX += e.clientX - this.lastMouseX;
                this.offsetY += e.clientY - this.lastMouseY;
                this.lastMouseX = e.clientX;
                this.lastMouseY = e.clientY;
            }
        });

        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            const zoomSensitivity = 0.001;
            const delta = -e.deltaY * zoomSensitivity;
            const newScale = Math.max(0.1, Math.min(5.0, this.scale + delta));

            // Zoom towards mouse
            // Simple zoom for now (center based) - can improve later
            this.scale = newScale;
        });

        // Initialize Connection
        this.connect();

        // Start Loop
        requestAnimationFrame((t) => this.loop(t));
    }

    resize() {
        this.width = window.innerWidth;
        this.height = window.innerHeight;
        this.canvas.width = this.width;
        this.canvas.height = this.height;
    }

    connect() {
        if (this.ws) return;

        console.log(`Connecting to ${this.wsUrl}...`);
        this.updateStatus(false);

        this.ws = new WebSocket(this.wsUrl);
        this.ws.binaryType = "arraybuffer"; // For future optimization

        this.ws.onopen = () => {
            console.log('Connected!');
            this.updateStatus(true);
            if (this.reconnectTimer) {
                clearInterval(this.reconnectTimer);
                this.reconnectTimer = null;
            }
        };

        this.ws.onclose = () => {
            console.warn('Disconnected. Reconnecting in 3s...');
            this.updateStatus(false);
            this.ws = null;
            if (!this.reconnectTimer) {
                this.reconnectTimer = setInterval(() => this.connect(), 3000);
            }
        };

        this.ws.onerror = (err) => {
            console.error('WebSocket Error:', err);
            this.ws.close();
        };

        this.ws.onmessage = (event) => {
            try {
                // Currently expecting JSON text
                const data = JSON.parse(event.data);

                if (data.reset) {
                    this.latestState = null;
                    this.updateStats({ t: 0, p: [], m: [] });
                    return;
                }

                this.latestState = data;
                this.updateStats(data);
            } catch (e) {
                console.error('Data parsing error:', e);
            }
        };
    }

    updateStatus(connected) {
        const el = document.getElementById('connection-status');
        if (connected) {
            el.textContent = 'Connected';
            el.className = 'status connected';
        } else {
            el.textContent = 'Disconnected';
            el.className = 'status disconnected';
        }
    }

    updateStats(data) {
        if (data.t) document.getElementById('tick').textContent = data.t;
        if (data.p) document.getElementById('player-count').textContent = data.p.length;
        if (data.m) document.getElementById('monster-count').textContent = data.m.length;
    }

    loop(timestamp) {
        // FPS Calc
        if (timestamp - this.lastFrameTime >= 1000) {
            document.getElementById('fps').textContent = this.frameCount;
            this.frameCount = 0;
            this.lastFrameTime = timestamp;
        }
        this.frameCount++;

        this.update();
        this.render();

        requestAnimationFrame((t) => this.loop(t));
    }

    update() {
        // Interpolation or prediction logic will go here
    }

    render() {
        // Clear
        this.ctx.fillStyle = '#1e1e1e';
        this.ctx.fillRect(0, 0, this.width, this.height);

        this.ctx.save();

        // Transform Camera
        this.ctx.translate(this.offsetX, this.offsetY);
        this.ctx.scale(this.scale, -this.scale); // [Fix] Invert Y to match Unity (Y+ is Up)

        // Draw Grid
        this.drawGrid();

        // Draw Game Objects if data exists
        if (this.latestState) {
            if (document.getElementById('chk-monsters').checked && this.latestState.m) {
                this.drawMonsters(this.latestState.m);
            }
            if (document.getElementById('chk-players').checked && this.latestState.p) {
                this.drawPlayers(this.latestState.p);
            }
            if (this.latestState.pr) {
                // [Fix] Defensive check for element existence
                const chk = document.getElementById('chk-projectiles');
                if (!chk || chk.checked) {
                    this.drawProjectiles(this.latestState.pr);
                }
            }
        }

        this.ctx.restore();
    }

    drawProjectiles(projectiles) {
        this.ctx.fillStyle = '#2196f3'; // Blue
        projectiles.forEach(pr => {
            this.ctx.beginPath();
            // Projectile size approx 0.2 radius
            this.ctx.arc(pr.x, pr.y, 0.2, 0, Math.PI * 2);
            this.ctx.fill();
        });
    }

    drawGrid() {
        if (!document.getElementById('chk-grid').checked) return;

        const size = 1000;
        const step = 5;

        this.ctx.strokeStyle = '#333';
        this.ctx.lineWidth = 0.05;

        this.ctx.beginPath();
        for (let x = -size; x <= size; x += step) {
            this.ctx.moveTo(x, -size);
            this.ctx.lineTo(x, size);
        }
        for (let y = -size; y <= size; y += step) {
            this.ctx.moveTo(-size, y);
            this.ctx.lineTo(size, y);
        }
        this.ctx.stroke();

        // Axis
        this.ctx.strokeStyle = '#555';
        this.ctx.lineWidth = 0.1;
        this.ctx.beginPath();
        this.ctx.moveTo(-size, 0); this.ctx.lineTo(size, 0);
        this.ctx.moveTo(0, -size); this.ctx.lineTo(0, size);
        this.ctx.stroke();
    }

    drawMonsters(monsters) {
        this.ctx.fillStyle = '#f44336'; // Red
        monsters.forEach(m => {
            this.ctx.beginPath();
            this.ctx.arc(m.x, m.y, 0.5, 0, Math.PI * 2);
            this.ctx.fill();

            // ID Label - Only show if zoomed in
            if (this.scale > 10) {
                this.ctx.save();
                this.ctx.translate(m.x, m.y);
                // [Fix] Invert Y scale back to 100% for text to be upright
                this.ctx.scale(1 / this.scale, -1 / this.scale);
                this.ctx.fillStyle = 'white';
                this.ctx.font = '12px Arial';
                this.ctx.textAlign = 'center';
                this.ctx.fillText(m.id, 0, -15); // Text is now above the object (in inverted space)
                this.ctx.restore();
            }
        });
    }

    drawPlayers(players) {
        this.ctx.fillStyle = '#4caf50'; // Green
        players.forEach(p => {
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, 0.5, 0, Math.PI * 2);
            this.ctx.fill();

            // ID Label
            this.ctx.save();
            this.ctx.translate(p.x, p.y);
            // [Fix] Invert Y scale back for text
            this.ctx.scale(1 / this.scale, -1 / this.scale);
            this.ctx.fillStyle = 'white';
            this.ctx.font = '14px Arial';
            this.ctx.textAlign = 'center';
            this.ctx.fillText(`P${p.id}`, 0, -20);
            this.ctx.restore();
        });
    }

    resetView() {
        // [Fix] World view roughly 40 units
        this.scale = Math.min(this.width, this.height) / 40.0;
        this.offsetX = this.width / 2;
        this.offsetY = this.height / 2;
    }
}

const app = new App();
window.app = app; // For debugging access
