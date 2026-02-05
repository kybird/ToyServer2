class App {
    constructor() {
        this.canvas = document.getElementById('gameCanvas');
        this.ctx = this.canvas.getContext('2d');
        this.width = window.innerWidth;
        this.height = window.innerHeight;

        // View State
        this.scale = 20.0;
        this.offsetX = this.width / 2;
        this.offsetY = this.height / 2;
        this.isDragging = false;
        this.lastMouseX = 0;
        this.lastMouseY = 0;

        // Data Management
        this.latestState = null;
        this.lastFrameTime = 0;
        this.frameCount = 0;
        this.followingPlayerId = null;
        this.selectedRoomId = null; // null means auto-pick room with players

        // WebSocket
        this.wsUrl = 'ws://localhost:9002';
        this.ws = null;
        this.reconnectTimer = null;

        this.init();
        this.resetView();
    }

    init() {
        window.addEventListener('resize', () => this.resize());
        this.resize();

        // Mouse Events
        this.canvas.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.lastMouseX = e.clientX;
            this.lastMouseY = e.clientY;
            this.canvas.style.cursor = 'grabbing';

            // Turn off follow if user starts dragging
            const chkFollow = document.getElementById('chk-follow');
            if (chkFollow && chkFollow.checked) {
                chkFollow.checked = false;
                this.updateFollowIndicator();
            }
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
            const zoomSensitivity = 0.0015;
            const delta = -e.deltaY * zoomSensitivity;
            this.scale = Math.max(0.05, Math.min(100.0, this.scale * (1 + delta)));
        });

        // Toggle Listeners
        document.getElementById('chk-follow').addEventListener('change', () => {
            this.updateFollowIndicator();
        });

        this.connect();
        requestAnimationFrame((t) => this.loop(t));
    }

    updateFollowIndicator() {
        const indicator = document.getElementById('follow-indicator');
        const chk = document.getElementById('chk-follow');
        if (indicator && chk) {
            indicator.style.display = chk.checked ? 'block' : 'none';
        }
    }

    resize() {
        this.width = window.innerWidth;
        this.height = window.innerHeight;
        this.canvas.width = this.width;
        this.canvas.height = this.height;
    }

    connect() {
        if (this.ws) return;
        this.updateStatus(false);
        console.log("Attempting to connect to", this.wsUrl);

        this.ws = new WebSocket(this.wsUrl);

        this.ws.onopen = () => {
            console.log("Connected to server");
            this.updateStatus(true);
            if (this.reconnectTimer) {
                clearInterval(this.reconnectTimer);
                this.reconnectTimer = null;
            }
        };

        this.ws.onclose = () => {
            console.warn("Disconnected from server. Reconnecting...");
            this.updateStatus(false);
            this.ws = null;
            if (!this.reconnectTimer) {
                this.reconnectTimer = setInterval(() => this.connect(), 3000);
            }
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);

                // Management for multiple rooms
                if (data.reset) {
                    if (this.selectedRoomId === null || data.rid === this.selectedRoomId) {
                        this.latestState = null;
                        this.updateStats({ t: 0, p: [], m: [] });
                    }
                    return;
                }

                // If no room selected, pick the first one with players
                if (this.selectedRoomId === null) {
                    if (data.p && data.p.length > 0) {
                        this.selectedRoomId = data.rid;
                        console.log("Auto-selected Room:", this.selectedRoomId);
                    }
                }

                // Only update if it's the selected room or we are in auto mode
                if (this.selectedRoomId === null || data.rid === this.selectedRoomId) {
                    this.latestState = data;
                    this.updateStats(data);
                }
            } catch (e) {
                console.error("Parse Error:", e, event.data);
            }
        };

        this.ws.onerror = (err) => {
            console.error("WebSocket Error:", err);
        };
    }

    updateStatus(connected) {
        const el = document.getElementById('connection-status');
        if (el) {
            el.textContent = connected ? 'Connected' : 'Disconnected';
            el.className = 'status ' + (connected ? 'connected' : 'disconnected');
        }
    }

    updateStats(data) {
        if (data.t !== undefined) document.getElementById('tick').textContent = data.t;
        if (data.p) document.getElementById('player-count').textContent = data.p.length;
        if (data.m) document.getElementById('monster-count').textContent = data.m.length;

        // Show Room ID in UI
        const ridEl = document.getElementById('room-id');
        if (ridEl) ridEl.textContent = data.rid || "N/A";
    }

    loop(timestamp) {
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
        const chkFollow = document.getElementById('chk-follow');
        if (chkFollow && chkFollow.checked && this.latestState && this.latestState.p && this.latestState.p.length > 0) {
            let target = this.latestState.p.find(p => p.id === this.followingPlayerId);
            if (!target) {
                target = this.latestState.p[0];
                this.followingPlayerId = target.id;
            }

            this.offsetX = (this.width / 2) - (target.x * this.scale);
            this.offsetY = (this.height / 2) + (target.y * this.scale);
        }
    }

    render() {
        this.ctx.fillStyle = '#1a1a1a';
        this.ctx.fillRect(0, 0, this.width, this.height);

        this.ctx.save();
        this.ctx.translate(this.offsetX, this.offsetY);
        this.ctx.scale(this.scale, -this.scale);

        this.drawGrid();

        if (this.latestState) {
            if (this.latestState.m && document.getElementById('chk-monsters').checked) {
                this.drawMonsters(this.latestState.m);
            }
            if (this.latestState.pr) {
                this.drawProjectiles(this.latestState.pr);
            }
            if (this.latestState.p && document.getElementById('chk-players').checked) {
                this.drawPlayers(this.latestState.p);
            }
        } else {
            // No data message
            this.ctx.save();
            this.ctx.scale(1 / this.scale, -1 / this.scale);
            this.ctx.fillStyle = "#666";
            this.ctx.font = "20px Arial";
            this.ctx.textAlign = "center";
            this.ctx.fillText("WAITING FOR LIVE DATA...", 0, 0);
            this.ctx.restore();
        }

        this.ctx.restore();
    }

    drawGrid() {
        if (!document.getElementById('chk-grid').checked) return;
        const size = 1000;
        const step = 5;
        this.ctx.strokeStyle = '#333';
        this.ctx.lineWidth = 1 / this.scale;

        this.ctx.beginPath();
        for (let x = -size; x <= size; x += step) {
            this.ctx.moveTo(x, -size); this.ctx.lineTo(x, size);
        }
        for (let y = -size; y <= size; y += step) {
            this.ctx.moveTo(-size, y); this.ctx.lineTo(size, y);
        }
        this.ctx.stroke();

        this.ctx.strokeStyle = '#444';
        this.ctx.lineWidth = 2 / this.scale;
        this.ctx.beginPath();
        this.ctx.moveTo(-size, 0); this.ctx.lineTo(size, 0);
        this.ctx.moveTo(0, -size); this.ctx.lineTo(0, size);
        this.ctx.stroke();
    }

    drawMonsters(monsters) {
        this.ctx.fillStyle = '#f44336';
        monsters.forEach(m => {
            this.ctx.beginPath();
            this.ctx.arc(m.x, m.y, 0.5, 0, Math.PI * 2);
            this.ctx.fill();
        });
    }

    drawProjectiles(projectiles) {
        this.ctx.fillStyle = '#2196f3';
        projectiles.forEach(pr => {
            this.ctx.beginPath();
            this.ctx.arc(pr.x, pr.y, 0.2, 0, Math.PI * 2);
            this.ctx.fill();
        });
    }

    drawPlayers(players) {
        players.forEach(p => {
            this.ctx.fillStyle = (p.id === this.followingPlayerId) ? '#ffeb3b' : '#4caf50';
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, 0.6, 0, Math.PI * 2);
            this.ctx.fill();

            // Direction triangle if 'l' is present
            if (p.l !== undefined) {
                this.ctx.fillStyle = 'white';
                this.ctx.beginPath();
                const dir = p.l ? -1 : 1; // l=1 is Left
                this.ctx.moveTo(p.x + dir * 0.7, p.y);
                this.ctx.lineTo(p.x + dir * 0.4, p.y + 0.2);
                this.ctx.lineTo(p.x + dir * 0.4, p.y - 0.2);
                this.ctx.fill();
            }

            this.ctx.save();
            this.ctx.translate(p.x, p.y);
            this.ctx.scale(1 / this.scale, -1 / this.scale);
            this.ctx.fillStyle = 'white';
            this.ctx.font = 'bold 12px Arial';
            this.ctx.textAlign = 'center';
            this.ctx.fillText(`P${p.id}`, 0, -15);
            this.ctx.restore();
        });
    }

    resetView() {
        this.scale = Math.min(this.width, this.height) / 50.0;
        this.offsetX = this.width / 2;
        this.offsetY = this.height / 2;
        this.selectedRoomId = null; // Re-scan rooms

        const chkFollow = document.getElementById('chk-follow');
        if (chkFollow) chkFollow.checked = false;
        this.updateFollowIndicator();
    }
}

const app = new App();
window.app = app;
