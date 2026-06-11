document.addEventListener('DOMContentLoaded', () => {
    const wsStatusDot  = document.getElementById('ws-status-dot');
    const wsStatusText  = document.getElementById('ws-status-text');
    const mobileMenuBtn = document.querySelector('.mobile-menu-btn');
    const sidebar       = document.querySelector('.sidebar');
    const sidebarOverlay = document.getElementById('sidebar-overlay');

    const imageModal  = document.getElementById('image-modal');
    const modalImg    = document.getElementById('modal-img');
    const closeModalBtn = document.querySelector('.close-modal');

    let ws = null;
    let reconnectTimer = null;
    const activeMessages = {};

    window.sendCommand = function(text) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: "chat", payload: { text: text } }));
        }
    };

    function updateDashboardData(text, keyboard, messageId) {
        if (!text) return;
        
        // System Monitor parser
        if (text.includes("**SYSTEM MONITOR**")) {
            const cpuMatch = text.match(/CPU: \[.*?\] (\d+)%/);
            if (cpuMatch) {
                document.getElementById('dash-cpu-val').textContent = cpuMatch[1] + '%';
                document.getElementById('dash-cpu-fill').style.width = cpuMatch[1] + '%';
            }
            const ramMatch = text.match(/RAM: \[.*?\] (\d+)% \((.*?)\)/);
            if (ramMatch) {
                document.getElementById('dash-ram-val').textContent = ramMatch[1] + '% (' + ramMatch[2] + ')';
                document.getElementById('dash-ram-fill').style.width = ramMatch[1] + '%';
            }
            const gpuMatch = text.match(/GPU: \[.*?\] (\d+)%/);
            if (gpuMatch) {
                const el = document.getElementById('dash-gpu-val');
                if (el) el.textContent = gpuMatch[1] + '%';
                const fEl = document.getElementById('dash-gpu-fill');
                if (fEl) fEl.style.width = gpuMatch[1] + '%';
            }
            const diskMatch = text.match(/Disk C:\s*(.*?) GB/);
            if (diskMatch) {
                const el = document.getElementById('dash-disk-val');
                if (el) el.textContent = diskMatch[1] + ' GB';
                const parts = diskMatch[1].split('/');
                if (parts.length === 2) {
                    const used = parseFloat(parts[0]);
                    const total = parseFloat(parts[1]);
                    const p = Math.round((used/total)*100);
                    const fEl = document.getElementById('dash-disk-fill');
                    if (fEl) fEl.style.width = p + '%';
                }
            }
            const netDownMatch = text.match(/↓\s*([\d.]+)\s*MB\/s/);
            if (netDownMatch) {
                const el = document.getElementById('dash-net-down');
                if (el) el.textContent = netDownMatch[1] + ' MB/s';
            }
            const netUpMatch = text.match(/↑\s*([\d.]+)\s*MB\/s/);
            if (netUpMatch) {
                const el = document.getElementById('dash-net-up');
                if (el) el.textContent = netUpMatch[1] + ' MB/s';
            }
            const topMatch = text.match(/⚠️ \*\*TOP PROCESS\*\*\n(.*?) \((.*?)\)/);
            if (topMatch) {
                document.getElementById('dash-top-process').textContent = `⚠️ Top Process: ${topMatch[1]} (${topMatch[2]})`;
            }
            if (keyboard) {
                buildDashboardControls('dash-system-controls', keyboard, messageId);
            }
        }
        // Media parser (SARA Media or Spotify Dock)
        if (text.includes("SARA Media") || text.includes("*NOW PLAYING*")) {
            const isSpotify = text.includes("*NOW PLAYING*");
            const containerId = isSpotify ? 'dash-media-spotify' : 'dash-media-local';
            let mediaContainer = document.getElementById(containerId);
            
            if (!mediaContainer) {
                // Hide default if exists
                const def = document.getElementById('dash-media-default');
                if (def) def.style.display = 'none';

                mediaContainer = document.createElement('div');
                mediaContainer.id = containerId;
                mediaContainer.className = 'widget media-widget';
                
                const header = document.createElement('div');
                header.className = 'widget-header';
                header.innerHTML = `<i class="${isSpotify ? 'fa-brands fa-spotify' : 'fa-solid fa-music'}"></i> ${isSpotify ? 'Spotify' : 'Media'}`;
                
                const controls = document.createElement('div');
                controls.className = 'widget-controls';
                controls.id = `${containerId}-controls`;

                mediaContainer.appendChild(header);
                mediaContainer.appendChild(controls);
                
                document.getElementById('media-widgets-container').appendChild(mediaContainer);
            }

            const oldContents = mediaContainer.querySelectorAll('.media-content');
            oldContents.forEach(el => el.remove());

            const sessions = text.split('\n---\n');
            const header = mediaContainer.querySelector('.widget-header');
            
            let parsedSessions = [];

            sessions.forEach((sessionText) => {
                let track = 'Unknown Track';
                let artist = 'Unknown Artist';
                let album = '';
                let status = '';
                let imgUrl = '';
                
                if (sessionText.includes("SARA Media")) {
                    const tMatch = sessionText.match(/🏷 \*\*(.*?)\*\*/);
                    const arMatch = sessionText.match(/👤 (.*?)\n/);
                    const alMatch = sessionText.match(/💿 (.*?)\n/);
                    const sMatch = sessionText.match(/(▶️ Playing|⏸️ Paused|⏹️ Stopped).*?`([^`]+)`/);
                    const iMatch = sessionText.match(/\[([^\]]*)\]\(([^)]+)\)/);
                    if (tMatch) track = tMatch[1];
                    if (arMatch) artist = arMatch[1];
                    if (alMatch) album = alMatch[1];
                    if (sMatch) status = sMatch[1].replace('▶️ ', '').replace('⏸️ ', '') + ' ' + sMatch[2];
                    if (iMatch) imgUrl = iMatch[2];
                } else if (sessionText.includes("*NOW PLAYING*")) {
                    const infoMatch = sessionText.match(/\*NOW PLAYING\*\n\n\*(.*?)\*\n([^\n]*)\n/);
                    const timeMatch = sessionText.match(/(\d+:\d+ \/ \d+:\d+)/);
                    const iMatch = sessionText.match(/\[([^\]]*)\]\(([^)]+)\)/);
                    if (infoMatch) {
                        track = infoMatch[1];
                        artist = infoMatch[2];
                    }
                    if (timeMatch) status = timeMatch[1];
                    if (iMatch) imgUrl = iMatch[2];
                }
                
                if (track !== 'Unknown Track' || artist !== 'Unknown Artist') {
                    parsedSessions.push({ track, artist, album, status, imgUrl });
                }
            });

            // Sort so that playing sessions appear first
            parsedSessions.sort((a, b) => {
                const aPlaying = a.status && a.status.toLowerCase().includes('playing');
                const bPlaying = b.status && b.status.toLowerCase().includes('playing');
                if (aPlaying && !bPlaying) return -1;
                if (!aPlaying && bPlaying) return 1;
                return 0;
            });

            parsedSessions.forEach((session, index) => {
                const contentDiv = document.createElement('div');
                contentDiv.className = 'media-content';
                if (index > 0) {
                    contentDiv.style.marginTop = '14px';
                    contentDiv.style.paddingTop = '14px';
                    contentDiv.style.borderTop = '1px solid rgba(255,255,255,0.07)';
                }
                
                let html = '';
                if (session.imgUrl) {
                    html += `<img src="${session.imgUrl}" alt="Album Art" class="media-art">`;
                }
                html += `<div class="media-info">`;
                html += `<div class="media-track">${session.track}</div>`;
                html += `<div class="media-artist">${session.artist}</div>`;
                if (session.album) html += `<div class="media-album">${session.album}</div>`;
                if (session.status) html += `<div class="media-status">${session.status}</div>`;
                html += `</div>`;
                
                contentDiv.innerHTML = html;
                header.after(contentDiv);
            });

            if (keyboard) {
                buildDashboardControls(`${containerId}-controls`, keyboard, messageId);
            }
        }
    }
    
    function buildDashboardControls(containerId, keyboard, messageId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        container.innerHTML = '';
        const kbDiv = document.createElement('div');
        kbDiv.className = 'inline-keyboard';
        
        keyboard.forEach(row => {
            const rowDiv = document.createElement('div');
            rowDiv.className = 'keyboard-row';
            row.forEach(btn => {
                const btnEl = document.createElement('button');
                btnEl.className = 'keyboard-btn';
                btnEl.textContent = btn.text;
                btnEl.onclick = () => {
                    if (ws && ws.readyState === WebSocket.OPEN) {
                        if (btn.callback_data) {
                            ws.send(JSON.stringify({ type: "callback", payload: { data: btn.callback_data, text: btn.text, message_id: messageId } }));
                        }
                    }
                };
                rowDiv.appendChild(btnEl);
            });
            kbDiv.appendChild(rowDiv);
        });
        container.appendChild(kbDiv);
    }

    // Chat message logic removed

    let pingInterval = null;

    function connectWebSocket() {
        const host = window.location.hostname || "127.0.0.1";
        ws = new WebSocket(`ws://${host}:9080`);

        ws.onopen = () => {
            wsStatusDot.classList.add('connected');
            wsStatusText.textContent = "Connected";
            clearTimeout(reconnectTimer);
            console.log("WebSocket connected!");
            
            sendCommand('/monitor');
            sendCommand('/media');
            sendCommand('/sp dock');
            
            pingInterval = setInterval(() => {
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: "ping" }));
                }
            }, 10000);
        };

        ws.onmessage = (event) => {
            console.log("WS Recv:", event.data);
            try {
                const data = JSON.parse(event.data);
                if (data.type === "toast") {
                    const toast = document.createElement('div');
                    toast.textContent = "💡 " + data.text;
                    toast.style.cssText = "position:absolute; top:20px; left:50%; transform:translateX(-50%); background:rgba(0,0,0,0.8); color:white; padding:10px 20px; border-radius:20px; z-index:1000; transition:opacity 0.5s;";
                    document.body.appendChild(toast);
                    setTimeout(() => { toast.style.opacity = "0"; setTimeout(() => toast.remove(), 500); }, 2000);
                } else if (data.type === "chat_reply" || data.type === "chat_edit") {
                    updateDashboardData(data.text, data.keyboard, data.message_id);
                }
            } catch (e) {
                console.error("Invalid WS message:", event.data);
            }
        };

        ws.onclose = () => {
            wsStatusDot.classList.remove('connected');
            wsStatusText.textContent = "Disconnected";
            clearInterval(pingInterval);
            reconnectTimer = setTimeout(connectWebSocket, 3000);
        };

        ws.onerror = (err) => {
            console.error("WebSocket Error:", err);
            ws.close();
        };
    }

    // Navigation logic handled by standard links
    const dashVolSlider = document.getElementById('dash-vol-slider');
    const dashBrightSlider = document.getElementById('dash-bright-slider');
    const dashVolVal = document.getElementById('dash-vol-val');
    const dashBrightVal = document.getElementById('dash-bright-val');

    if (dashVolSlider && dashVolVal) {
        const savedVol = localStorage.getItem('sara_vol') || 50;
        dashVolSlider.value = savedVol;
        dashVolVal.textContent = savedVol + '%';

        dashVolSlider.addEventListener('input', (e) => {
            dashVolVal.textContent = e.target.value + '%';
        });

        dashVolSlider.addEventListener('change', (e) => {
            const val = e.target.value;
            localStorage.setItem('sara_vol', val);
            sendCommand(`/vol${val}`);
        });
    }

    if (dashBrightSlider && dashBrightVal) {
        const savedBright = localStorage.getItem('sara_bright') || 50;
        dashBrightSlider.value = savedBright;
        dashBrightVal.textContent = savedBright + '%';

        dashBrightSlider.addEventListener('input', (e) => {
            dashBrightVal.textContent = e.target.value + '%';
        });

        dashBrightSlider.addEventListener('change', (e) => {
            const val = e.target.value;
            localStorage.setItem('sara_bright', val);
            sendCommand(`/bright${val}`);
        });
    }

    // Mobile menu toggle
    if (mobileMenuBtn) {
        mobileMenuBtn.addEventListener('click', () => {
            sidebar.classList.toggle('open');
            if (sidebarOverlay) sidebarOverlay.classList.toggle('open');
        });
    }
    if (sidebarOverlay) {
        sidebarOverlay.addEventListener('click', () => {
            sidebar.classList.remove('open');
            sidebarOverlay.classList.remove('open');
        });
    }

    // Image modal
    if (closeModalBtn) {
        closeModalBtn.onclick = () => { imageModal.style.display = 'none'; };
    }
    window.addEventListener('click', (e) => {
        if (e.target === imageModal) imageModal.style.display = 'none';
    });

    // Init
    connectWebSocket();
});
