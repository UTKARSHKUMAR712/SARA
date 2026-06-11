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
        // Dashboard logic moved to dashboard.js
    }
    
    function buildDashboardControls(containerId, keyboard, messageId) {
        // Dashboard logic moved to dashboard.js
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
    
    // Autocomplete Logic
    const suggestionsContainer = document.getElementById('suggestions-container');
    const COMMANDS = [
        { cmd: "/dock", desc: "Open Media Dock" },
        { cmd: "/system", desc: "Open System Dock" },
        { cmd: "/status", desc: "Runtime status" },
        { cmd: "/help", desc: "Show all commands" },
        { cmd: "/media", desc: "Open Media controls" },
        { cmd: "/desktop", desc: "Start Remote Desktop" },
        { cmd: "/desktopshutdown", desc: "Stop Remote Desktop" },
        { cmd: "/terminal", desc: "Start browser terminal" },
        { cmd: "/terminal_admin", desc: "Start admin browser terminal" },
        { cmd: "/sararestart", desc: "Restart SARA and Cloudflare completely" },
        { cmd: "/sarashutdown", desc: "Kill SARA completely (no restart)" },
        { cmd: "/screenshot", desc: "Take screenshot" },
        { cmd: "/photo", desc: "Webcam photo" },
        { cmd: "/monitor", desc: "Live system stats" },
        { cmd: "/tasks", desc: "Scheduled tasks" },
        { cmd: "/rules", desc: "Event automation rules" },
        { cmd: "/files", desc: "Manage files via File Browser" },
        { cmd: "/filesshutdown", desc: "Shutdown File Browser completely" },
        { cmd: "/workspace", desc: "Open SARA IDE workspace" },
        { cmd: "/workspaceshutdown", desc: "Shutdown SARA IDE workspace" },
        { cmd: "/sp_help", desc: "Spotify commands list" }
    ];
    let selectedSuggestionIndex = -1;

    function renderSuggestions(matches) {
        suggestionsContainer.innerHTML = '';
        if (matches.length === 0) {
            suggestionsContainer.style.display = 'none';
            return;
        }
        
        matches.forEach((match, index) => {
            const div = document.createElement('div');
            div.className = 'suggestion-item' + (index === selectedSuggestionIndex ? ' selected' : '');
            div.innerHTML = `<span class="suggestion-cmd">${match.cmd}</span><span class="suggestion-desc">${match.desc}</span>`;
            
            div.onmousedown = (e) => {
                e.preventDefault();
                chatInput.value = match.cmd;
                suggestionsContainer.style.display = 'none';
                chatInput.focus();
            };
            
            suggestionsContainer.appendChild(div);
        });
        suggestionsContainer.style.display = 'flex';
    }

    chatInput.addEventListener('input', () => {
        const val = chatInput.value;
        if (val.startsWith('/')) {
            const query = val.toLowerCase();
            const matches = COMMANDS.filter(c => c.cmd.toLowerCase().startsWith(query));
            selectedSuggestionIndex = -1;
            renderSuggestions(matches);
        } else {
            suggestionsContainer.style.display = 'none';
        }
    });

    chatInput.addEventListener('keydown', (e) => {
        const isSuggestionsVisible = suggestionsContainer.style.display === 'flex';
        const items = suggestionsContainer.querySelectorAll('.suggestion-item');
        
        if (isSuggestionsVisible && items.length > 0) {
            if (e.key === 'ArrowDown') {
                e.preventDefault();
                selectedSuggestionIndex = (selectedSuggestionIndex + 1) % items.length;
                renderSuggestions(COMMANDS.filter(c => c.cmd.toLowerCase().startsWith(chatInput.value.toLowerCase())));
                return;
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                selectedSuggestionIndex = (selectedSuggestionIndex - 1 + items.length) % items.length;
                renderSuggestions(COMMANDS.filter(c => c.cmd.toLowerCase().startsWith(chatInput.value.toLowerCase())));
                return;
            } else if (e.key === 'Enter') {
                if (selectedSuggestionIndex >= 0) {
                    e.preventDefault();
                    chatInput.value = items[selectedSuggestionIndex].querySelector('.suggestion-cmd').textContent;
                    suggestionsContainer.style.display = 'none';
                    return;
                }
            } else if (e.key === 'Escape') {
                suggestionsContainer.style.display = 'none';
            }
        }

        if (e.key === 'Enter' && (!isSuggestionsVisible || selectedSuggestionIndex === -1)) {
            sendMessage();
            suggestionsContainer.style.display = 'none';
        }
    });

    chatInput.addEventListener('blur', () => {
        suggestionsContainer.style.display = 'none';
    });

    clearBtn.addEventListener('click', () => {
        chatHistory.innerHTML = '';
        appendMessage('system', 'Chat cleared.');
    });
    // Dashboard tabs and sliders removed

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
