document.addEventListener('DOMContentLoaded', () => {
    const wsStatusDot = document.getElementById('ws-status-dot');
    const wsStatusText = document.getElementById('ws-status-text');
    const chatHistory = document.getElementById('chat-history');
    const chatInput = document.getElementById('chat-input');
    const sendBtn = document.getElementById('send-btn');
    const clearBtn = document.getElementById('clear-btn');
    const mobileMenuBtn = document.querySelector('.mobile-menu-btn');
    const sidebar = document.querySelector('.sidebar');

    const imageModal = document.getElementById('image-modal');
    const modalImg = document.getElementById('modal-img');
    const closeModalBtn = document.querySelector('.close-modal');

    let ws = null;
    let reconnectTimer = null;
    const activeMessages = {};

    window.sendCommand = function(text) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: "chat", payload: { text: text } }));
            appendMessage('user', text);
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
            const containerId = messageId ? `dash-media-${messageId}` : 'dash-media-default';
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
                const isSpotify = text.includes("*NOW PLAYING*");
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
            
            sessions.forEach((sessionText, index) => {
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
                    const contentDiv = document.createElement('div');
                    contentDiv.className = 'media-content';
                    if (index > 0) {
                        contentDiv.style.marginTop = '15px';
                        contentDiv.style.paddingTop = '15px';
                        contentDiv.style.borderTop = '1px solid var(--border-color)';
                    }
                    
                    let html = '';
                    if (imgUrl) {
                        html += `<img src="${imgUrl}" alt="Album Art" style="width: 120px; height: 120px; border-radius: 12px; object-fit: cover; box-shadow: 0 4px 15px rgba(0,0,0,0.5);">`;
                    } else {
                        html += `<img src="" style="display:none;">`;
                    }
                    html += `<div class="media-info">`;
                    html += `<div class="media-track">${track}</div>`;
                    html += `<div class="media-artist">${artist}</div>`;
                    if (album) html += `<div class="media-album">${album}</div>`;
                    if (status) html += `<div class="media-status">${status}</div>`;
                    html += `</div>`;
                    
                    contentDiv.innerHTML = html;
                    header.after(contentDiv);
                }
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

    // Format text recursively for markdown-like syntax
    function formatMessageText(text) {
        if (!text) return "";
        let html = text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
            
        // Images like [alt](url)
        html = html.replace(/\[([^\]]*)\]\(([^)]+)\)/g, '<img src="$2" alt="$1" style="max-width: 250px; max-height: 250px; object-fit: cover; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); margin-bottom: 10px; cursor: pointer; display: block;" onclick="document.getElementById(\'modal-img\').src=this.src; document.getElementById(\'image-modal\').style.display=\'flex\';">');
        // Bold
        html = html.replace(/\*(.*?)\*/g, '<strong>$1</strong>');
        // Code blocks
        html = html.replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>');
        // Inline code
        html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
        // New lines
        html = html.replace(/\n/g, '<br>');
        
        // Linkify raw URLs
        html = html.replace(/(<[^>]+>)|(https?:\/\/[^\s<]+)/g, (match, tag, url) => {
            if (tag) return tag;
            let trailing = "";
            const punc = /[.,!?;:)]+$/;
            const m = url.match(punc);
            if (m) {
                trailing = m[0];
                url = url.substring(0, url.length - trailing.length);
            }
            return `<a href="${url}" target="_blank" class="chat-link">${url}</a>${trailing}`;
        });
        
        return html;
    }

    function scrollToBottom() {
        chatHistory.scrollTop = chatHistory.scrollHeight;
    }

    function appendMessage(sender, text, photoUrl = null, keyboard = null, messageId = null) {
        if (sender === 'agent') {
            updateDashboardData(text, keyboard, messageId);
        }
        const msgDiv = document.createElement('div');
        msgDiv.className = `message ${sender}`;
        if (messageId) {
            activeMessages[messageId] = msgDiv;
        }

        if (text || photoUrl) {
            const bubbleDiv = document.createElement('div');
            bubbleDiv.className = 'bubble';
            
            if (photoUrl) {
                const img = document.createElement('img');
                img.src = photoUrl;
                img.onclick = () => {
                    modalImg.src = photoUrl;
                    imageModal.style.display = "block";
                };
                bubbleDiv.appendChild(img);
            }
            
            if (text) {
                const textSpan = document.createElement('div');
                textSpan.innerHTML = formatMessageText(text);
                bubbleDiv.appendChild(textSpan);
            }
            
            msgDiv.appendChild(bubbleDiv);
        }

        if (keyboard && keyboard.length > 0) {
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
                                ws.send(JSON.stringify({
                                    type: "callback",
                                    payload: {
                                        data: btn.callback_data,
                                        text: btn.text,
                                        message_id: messageId
                                    }
                                }));
                            } else {
                                ws.send(JSON.stringify({
                                    type: "chat",
                                    payload: {
                                        text: btn.text
                                    }
                                }));
                                appendMessage('user', btn.text);
                            }
                        }
                    };
                    rowDiv.appendChild(btnEl);
                });
                kbDiv.appendChild(rowDiv);
            });
            msgDiv.appendChild(kbDiv);
        }

        chatHistory.appendChild(msgDiv);
        scrollToBottom();
    }

    let pingInterval = null;

    function connectWebSocket() {
        const host = window.location.hostname || "127.0.0.1";
        ws = new WebSocket(`ws://${host}:9080`);

        ws.onopen = () => {
            wsStatusDot.classList.add('connected');
            wsStatusText.textContent = "Connected";
            appendMessage('system', 'System Online. Connected to SARA Local Engine.');
            clearTimeout(reconnectTimer);
            console.log("WebSocket connected!");
            
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
                } else if (data.type === "chat_reply") {
                    appendMessage('agent', data.text, data.photo, data.keyboard, data.message_id);
                } else if (data.type === "chat_edit") {
                    updateDashboardData(data.text, data.keyboard, data.message_id);
                    if (data.message_id && activeMessages[data.message_id]) {
                        const msgDiv = activeMessages[data.message_id];
                        msgDiv.innerHTML = ''; // clear old content
                        
                        if (data.text || data.photo) {
                            const bubbleDiv = document.createElement('div');
                            bubbleDiv.className = 'bubble';
                            if (data.photo) {
                                const img = document.createElement('img');
                                img.src = data.photo;
                                img.onclick = () => { modalImg.src = data.photo; imageModal.style.display = "block"; };
                                bubbleDiv.appendChild(img);
                            }
                            if (data.text) {
                                const textSpan = document.createElement('div');
                                textSpan.innerHTML = formatMessageText(data.text);
                                bubbleDiv.appendChild(textSpan);
                            }
                            msgDiv.appendChild(bubbleDiv);
                        }
                        
                        if (data.keyboard && data.keyboard.length > 0) {
                            const kbDiv = document.createElement('div');
                            kbDiv.className = 'inline-keyboard';
                            data.keyboard.forEach(row => {
                                const rowDiv = document.createElement('div');
                                rowDiv.className = 'keyboard-row';
                                row.forEach(btn => {
                                    const btnEl = document.createElement('button');
                                    btnEl.className = 'keyboard-btn';
                                    btnEl.textContent = btn.text;
                                    btnEl.onclick = () => {
                                        if (ws && ws.readyState === WebSocket.OPEN) {
                                            if (btn.callback_data) {
                                                ws.send(JSON.stringify({ type: "callback", payload: { data: btn.callback_data, text: btn.text, message_id: data.message_id } }));
                                            } else {
                                                ws.send(JSON.stringify({ type: "chat", payload: { text: btn.text } }));
                                                appendMessage('user', btn.text);
                                            }
                                        }
                                    };
                                    rowDiv.appendChild(btnEl);
                                });
                                kbDiv.appendChild(rowDiv);
                            });
                            msgDiv.appendChild(kbDiv);
                        }
                        scrollToBottom();
                    }
                }
            } catch (e) {
                console.error("Invalid WS message:", event.data);
            }
        };

        ws.onclose = () => {
            wsStatusDot.classList.remove('connected');
            wsStatusText.textContent = "Disconnected";
            appendMessage('system', 'Connection lost. Reconnecting...');
            clearInterval(pingInterval);
            reconnectTimer = setTimeout(connectWebSocket, 3000);
        };

        ws.onerror = (err) => {
            console.error("WebSocket Error:", err);
            ws.close();
        };
    }

    function sendMessage() {
        const text = chatInput.value.trim();
        if (!text) return;

        if (ws && ws.readyState === WebSocket.OPEN) {
            console.log("Sending chat message:", text);
            ws.send(JSON.stringify({
                type: "chat",
                payload: {
                    text: text
                }
            }));
            appendMessage('user', text);
            chatInput.value = '';
        } else {
            alert("Not connected to SARA.");
        }
    }

    // Event Listeners
    sendBtn.addEventListener('click', sendMessage);
    
    chatInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') sendMessage();
    });

    clearBtn.addEventListener('click', () => {
        chatHistory.innerHTML = '';
        appendMessage('system', 'Chat cleared.');
    });
    const navItems = document.querySelectorAll('.nav-item');
    const chatArea = document.querySelector('.chat-area');
    const dashArea = document.querySelector('.dashboard-area');
    
    navItems.forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            navItems.forEach(n => n.classList.remove('active'));
            item.classList.add('active');
            
            if (item.dataset.view === 'dashboard') {
                chatArea.style.display = 'none';
                dashArea.style.display = 'flex';
                // Trigger commands to load dashboard data if empty
                if (document.getElementById('dash-cpu-val').textContent === '0%') {
                    sendCommand('/monitor');
                }
                if (document.getElementById('dash-media-track').textContent === 'Waiting for media...') {
                    sendCommand('/media');
                }
            } else if (item.dataset.view === 'chat') {
                dashArea.style.display = 'none';
                chatArea.style.display = 'flex';
                scrollToBottom();
            }
            if (window.innerWidth <= 768) {
                sidebar.classList.remove('open');
            }
        });
    });
    const dashVolSlider = document.getElementById('dash-vol-slider');
    const dashBrightSlider = document.getElementById('dash-bright-slider');
    if (dashVolSlider) {
        dashVolSlider.addEventListener('change', (e) => {
            sendCommand(`/vol${e.target.value}`);
        });
    }
    if (dashBrightSlider) {
        dashBrightSlider.addEventListener('change', (e) => {
            sendCommand(`/bright${e.target.value}`);
        });
    }

    const mobileMenuBtns = document.querySelectorAll('.mobile-menu-btn');
    mobileMenuBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            sidebar.classList.toggle('open');
        });
    });

    // Close sidebar on mobile when clicking outside
    document.addEventListener('click', (e) => {
        if (window.innerWidth <= 768 && 
            !sidebar.contains(e.target) && 
            !Array.from(mobileMenuBtns).some(btn => btn.contains(e.target))) {
            sidebar.classList.remove('open');
        }
    });

    // Image modal
    closeModalBtn.onclick = () => {
        imageModal.style.display = "none";
    }
    
    window.onclick = (e) => {
        if (e.target == imageModal) {
            imageModal.style.display = "none";
        }
    }

    // Init
    connectWebSocket();
});
