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
    
    chatInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') sendMessage();
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
