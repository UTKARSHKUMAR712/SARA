import { getAuthToken, apiFetch } from './api.js';
import { state } from './state.js';

export let isTerminalOpen = false;

// Array of active terminal objects: { id, xterm, fitAddon, ws, containerEl, tabEl }
let terminals = [];
let activeTerminalId = null;
let terminalCounter = 1;

export function toggleTerminal() {
    const panel = document.getElementById('bottom-panel');
    const btnTerminal = document.getElementById('btn-terminal');
    
    if (panel.classList.contains('hidden')) {
        panel.classList.remove('hidden');
        if (btnTerminal) {
            document.querySelectorAll('.activity-item').forEach(el => el.classList.remove('active'));
            btnTerminal.classList.add('active');
        }
        isTerminalOpen = true;
        document.getElementById('ports-container').style.display = 'none';
        document.getElementById('terminal-tabs-container').style.display = 'flex';
        document.getElementById('terminal-container').style.display = 'block';
        if (terminals.length === 0) {
            initTerminal();
        } else {
            fitActiveTerminal();
        }
    } else {
        panel.classList.add('hidden');
        document.querySelectorAll('.activity-item').forEach(el => el.classList.remove('active'));
        isTerminalOpen = false;
    }
}

export function initTerminal() {
    const termId = 'term-' + Date.now();
    const termNameFull = `PowerShell ${terminalCounter}`;
    const termNameShort = `PS${terminalCounter}`;
    terminalCounter++;

    const tabsContainer = document.getElementById('terminal-tabs-container');
    const containerWrapper = document.getElementById('terminal-container');

    // Create Tab UI
    const tabEl = document.createElement('div');
    tabEl.className = 'terminal-tab';
    tabEl.innerHTML = `
        <span class="term-name-full">${termNameFull}</span>
        <span class="term-name-short">${termNameShort}</span>
        <div class="terminal-tab-close" title="Close Terminal">
            <svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
        </div>
    `;
    tabsContainer.appendChild(tabEl);

    // Create Content Container
    const containerEl = document.createElement('div');
    containerEl.className = 'terminal-instance';
    containerEl.id = termId;
    containerWrapper.appendChild(containerEl);

    // Init xterm
    // @ts-ignore
    const xterm = new Terminal({
        cursorBlink: state.terminalSettings.cursorBlink !== false,
        cursorStyle: state.terminalSettings.cursorStyle || 'block',
        scrollback: state.terminalSettings.scrollback || 5000,
        theme: {
            background: '#1e1e1e',
            foreground: '#cccccc'
        },
        fontFamily: 'Consolas, "Courier New", monospace',
        fontSize: state.terminalSettings.fontSize || 14
    });

    // @ts-ignore
    const fitAddon = new FitAddon.FitAddon();
    xterm.loadAddon(fitAddon);
    
    // @ts-ignore
    if (typeof WebLinksAddon !== 'undefined') {
        const webLinksAddon = new WebLinksAddon.WebLinksAddon(
            async (event, uri) => {
                try {
                    const urlObj = new URL(uri);
                    if (urlObj.hostname === 'localhost' || urlObj.hostname === '127.0.0.1' || urlObj.hostname === '0.0.0.0') {
                        const port = urlObj.port;
                        if (port) {
                            const newTab = window.open('about:blank', '_blank');
                            if (newTab) {
                                newTab.document.write('<html><body style="background:#1e1e1e;color:white;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;"><h2>Starting secure tunnel... Please wait...</h2></body></html>');
                            }
                            
                            const res = await apiFetch(`/ports/tunnel/${port}`);
                            if (res.ok) {
                                const data = await res.json();
                                if (data.url) {
                                    // Append original path and query to tunnel URL
                                    const destUrl = new URL(data.url);
                                    destUrl.pathname = urlObj.pathname;
                                    destUrl.search = urlObj.search;
                                    destUrl.hash = urlObj.hash;
                                    
                                    if (newTab) newTab.location.href = destUrl.toString();
                                    else window.open(destUrl.toString(), '_blank');
                                    return;
                                }
                            }
                            
                            if (newTab) newTab.close();
                            alert("Failed to start Cloudflare tunnel for port " + port);
                            return;
                        }
                    }
                } catch (e) {
                    // Ignore parse errors
                }
                
                // Fallback for non-localhost URLs
                window.open(uri, '_blank');
            }
        );
        xterm.loadAddon(webLinksAddon);
    }

    xterm.open(containerEl);
    
    const termObj = {
        id: termId,
        xterm: xterm,
        fitAddon: fitAddon,
        ws: null,
        containerEl: containerEl,
        tabEl: tabEl
    };
    terminals.push(termObj);

    // Switch to new terminal
    switchToTerminal(termId);

    // Bind UI Events
    tabEl.addEventListener('click', (e) => {
        if (e.target.closest('.terminal-tab-close')) {
            killTerminalInstance(termId);
        } else {
            switchToTerminal(termId);
        }
    });

    xterm.onResize((size) => {
        if (termObj.ws && termObj.ws.readyState === WebSocket.OPEN) {
            termObj.ws.send(JSON.stringify({
                type: 'resize',
                cols: size.cols,
                rows: size.rows
            }));
        }
    });

    startSessionAndConnect(termObj);
}

function switchToTerminal(id) {
    activeTerminalId = id;
    terminals.forEach(t => {
        if (t.id === id) {
            t.tabEl.classList.add('active');
            t.containerEl.classList.add('active');
            setTimeout(() => {
                t.fitAddon.fit();
                t.xterm.focus();
            }, 50);
        } else {
            t.tabEl.classList.remove('active');
            t.containerEl.classList.remove('active');
        }
    });
}

export function fitActiveTerminal() {
    const active = terminals.find(t => t.id === activeTerminalId);
    if (active) {
        setTimeout(() => active.fitAddon.fit(), 50);
    }
}

async function startSessionAndConnect(termObj) {
    try {
        const parentToken = getAuthToken();
        const shell = state.terminalSettings.defaultShell === 'cmd' ? 'cmd.exe' : 'powershell.exe';
        const res = await fetch(`/api/new_session?parent_token=${parentToken}&shell=${shell}`);
        if (!res.ok) throw new Error("Failed to create session");
        const data = await res.json();
        
        connectTerminalWs(termObj, data.session_id, data.token);
    } catch (e) {
        termObj.xterm.writeln(`\r\n[Error starting session: ${e.message}]`);
    }
}

function connectTerminalWs(termObj, sessionId, token) {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const cols = termObj.xterm.cols || 80;
    const rows = termObj.xterm.rows || 24;
    const wsUrl = `${protocol}//${window.location.host}/ws/${sessionId}?token=${token}&cols=${cols}&rows=${rows}`;
    
    const ws = new WebSocket(wsUrl);
    termObj.ws = ws;
    
    ws.onopen = () => {
        if (state.currentWorkspace) {
            setTimeout(() => {
                if (ws.readyState === WebSocket.OPEN) {
                    const cdCmd = `cd "${state.currentWorkspace}"\r`;
                    ws.send(JSON.stringify({ type: 'input', data: btoa(unescape(encodeURIComponent(cdCmd))) }));
                    
                    setTimeout(() => {
                        if (ws.readyState === WebSocket.OPEN) {
                            const clearCmd = `clear\r`;
                            ws.send(JSON.stringify({ type: 'input', data: btoa(unescape(encodeURIComponent(clearCmd))) }));
                        }
                    }, 100);
                }
            }, 500); // Wait for PowerShell to fully initialize
        }
    };
    
    ws.onmessage = (event) => {
        try {
            let text = event.data.trim();
            // Handle concatenated JSON strings e.g. {}{...}
            if (text.includes('}{')) {
                text = '[' + text.replace(/\}\{/g, '},{') + ']';
                const arr = JSON.parse(text);
                arr.forEach(processMsg);
            } else {
                // Also handle potential newline separated JSON
                const lines = text.split('\n');
                lines.forEach(line => {
                    if (line.trim()) processMsg(JSON.parse(line.trim()));
                });
            }

            function processMsg(msg) {
                if (msg.type === 'output' && msg.data) {
                    const str = atob(msg.data);
                    const u8 = new Uint8Array(str.length);
                    for(let i=0; i<str.length; i++) {
                        u8[i] = str.charCodeAt(i);
                    }
                    termObj.xterm.write(u8);
                }
            }
        } catch(e) {
            console.error("Terminal WS Error", e, "Raw Data:", event.data);
        }
    };
    
    ws.onclose = () => {
        termObj.xterm.writeln('\r\n[Disconnected]');
    };
    
    termObj.xterm.onData((data) => {
        if (ws && ws.readyState === WebSocket.OPEN) {
            const b64 = btoa(unescape(encodeURIComponent(data)));
            ws.send(JSON.stringify({ type: 'input', data: b64 }));
        }
    });
}

export function killTerminalInstance(id) {
    const idx = terminals.findIndex(t => t.id === id);
    if (idx === -1) return;
    
    const termObj = terminals[idx];
    if (termObj.ws) {
        termObj.ws.close();
    }
    termObj.xterm.dispose();
    termObj.tabEl.remove();
    termObj.containerEl.remove();
    
    terminals.splice(idx, 1);
    
    if (terminals.length > 0) {
        // Switch to adjacent terminal
        const newActiveIdx = Math.max(0, idx - 1);
        switchToTerminal(terminals[newActiveIdx].id);
    } else {
        // Close panel if no terminals left
        const panel = document.getElementById('bottom-panel');
        panel.classList.add('hidden');
        isTerminalOpen = false;
        activeTerminalId = null;
    }
}

// Global kill terminal button (kills active terminal)
export function killTerminal() {
    if (activeTerminalId) {
        killTerminalInstance(activeTerminalId);
    }
}

// Watch for overall container resize to refit the active terminal
const resizeObserver = new ResizeObserver(() => {
    if (isTerminalOpen) {
        fitActiveTerminal();
    }
});
// Since this is an ES module, the DOM is already parsed when this runs.
const termContainer = document.getElementById('terminal-container');
if (termContainer) {
    resizeObserver.observe(termContainer);
}

export function updateTerminalFontSize(size) {
    terminals.forEach(t => {
        t.xterm.options.fontSize = size;
        t.fitAddon.fit();
    });
}
