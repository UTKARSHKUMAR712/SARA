import { apiFetch } from './api.js';
import { state } from './state.js';
import { fitActiveTerminal } from './terminal.js';

export function initPortsPanel() {
    const btnPorts = document.getElementById('btn-ports');
    const portsContainer = document.getElementById('ports-container');
    const terminalContainer = document.getElementById('terminal-tabs-container');
    const terminalContent = document.getElementById('terminal-container');
    const bottomPanel = document.getElementById('bottom-panel');
    const btnTerminal = document.getElementById('btn-terminal');
    
    // Toggle Ports
    if (btnPorts) {
        btnPorts.addEventListener('click', () => {
            if (btnPorts.classList.contains('active')) {
                bottomPanel.classList.add('hidden');
                btnPorts.classList.remove('active');
                return;
            }
            // Remove active from others, make this active
            document.querySelectorAll('.activity-item').forEach(el => el.classList.remove('active'));
            btnPorts.classList.add('active');
            
            // Show bottom panel if hidden
            if (bottomPanel.classList.contains('hidden')) {
                bottomPanel.classList.remove('hidden');
            }
            
            // Show ports, hide terminal
            portsContainer.style.display = 'block';
            terminalContainer.style.display = 'none';
            terminalContent.style.display = 'none';
            startPolling();
        });
    }
    
    // If they click Terminal, we should stop polling and show terminal
    if (btnTerminal) {
        btnTerminal.addEventListener('click', () => {
            document.querySelectorAll('.activity-item').forEach(el => el.classList.remove('active'));
            btnTerminal.classList.add('active');
            
            portsContainer.style.display = 'none';
            terminalContainer.style.display = 'flex';
            terminalContent.style.display = 'block';
            stopPolling();
            fitActiveTerminal();
        });
    }
}

let pollingInterval = null;

function startPolling() {
    if (pollingInterval) return;
    pollPorts();
    pollingInterval = setInterval(pollPorts, 3000);
}

function stopPolling() {
    if (pollingInterval) {
        clearInterval(pollingInterval);
        pollingInterval = null;
    }
}

async function pollPorts() {
    try {
        const res = await apiFetch('/ports');
        if (res.ok) {
            const ports = await res.json();
            state.liveServerRunning = !!ports.find(p => p.port === 5500);
            renderPorts(ports);
        }
    } catch (e) {
        console.error("Failed to poll ports", e);
    }
}

function renderPorts(ports) {
    const tbody = document.getElementById('ports-table-body');
    tbody.innerHTML = '';
    
    if (ports.length === 0) {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td colspan="4" style="padding: 15px; color: #888; text-align: center;">No active ports detected. Start a development server.</td>`;
        tbody.appendChild(tr);
        return;
    }
    
    ports.forEach(port => {
        const tr = document.createElement('tr');
        tr.style.borderBottom = "1px solid var(--border-color)";
        
        const previewUrl = window.location.origin + `/preview/${port.port}/`;
        
        const actionTd = document.createElement('td');
        actionTd.style.padding = '8px';
        actionTd.style.display = 'flex';
        actionTd.style.gap = '6px';
        actionTd.style.flexWrap = 'wrap';
        actionTd.style.alignItems = 'center';

        const btnPreview = document.createElement('button');
        btnPreview.className = 'primary-btn';
        btnPreview.style.padding = '4px 8px';
        btnPreview.style.margin = '0';
        btnPreview.textContent = 'Cloudflare';

        const btnLocal = document.createElement('button');
        btnLocal.className = 'primary-btn';
        btnLocal.style.padding = '4px 8px';
        btnLocal.style.margin = '0';
        btnLocal.style.backgroundColor = '#4CAF50';
        btnLocal.style.borderColor = '#4CAF50';
        btnLocal.textContent = 'Local LAN';
        btnLocal.onclick = () => {
            const localUrl = window.location.protocol + '//' + window.location.hostname + ':' + port.port;
            window.open(localUrl, '_blank');
        };
        btnPreview.onclick = async () => {
            btnPreview.textContent = 'Starting Tunnel...';
            btnPreview.disabled = true;
            
            // Open window synchronously to bypass popup blockers
            const newTab = window.open('about:blank', '_blank');
            if (newTab) {
                newTab.document.write('<html><body style="background:#1e1e1e;color:white;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;"><h2>Starting secure tunnel... Please wait...</h2></body></html>');
            }

            try {
                const res = await apiFetch(`/ports/tunnel/${port.port}`);
                if (res.ok) {
                    const data = await res.json();
                    if (data.url) {
                        if (newTab) newTab.location.href = data.url;
                        else window.open(data.url, '_blank');
                    } else {
                        if (newTab) newTab.close();
                        alert("Failed to get tunnel URL");
                    }
                } else {
                    if (newTab) newTab.close();
                    alert("Failed to start tunnel");
                }
            } catch (e) {
                if (newTab) newTab.close();
                alert("Error starting tunnel: " + e);
            }
            btnPreview.textContent = 'Cloudflare';
            btnPreview.disabled = false;
        };

        const btnCopy = document.createElement('button');
        btnCopy.className = 'secondary-btn';
        btnCopy.style.padding = '4px 8px';
        btnCopy.style.margin = '0';
        btnCopy.textContent = 'Copy URL';
        btnCopy.onclick = async () => {
            btnCopy.textContent = 'Getting URL...';
            btnCopy.disabled = true;
            try {
                const res = await apiFetch(`/ports/tunnel/${port.port}`);
                if (res.ok) {
                    const data = await res.json();
                    if (data.url) {
                        navigator.clipboard.writeText(data.url);
                        btnCopy.textContent = 'Copied!';
                        setTimeout(() => btnCopy.textContent = 'Copy URL', 1500);
                    }
                }
            } catch(e) {}
            btnCopy.disabled = false;
        };
        
        const btnKill = document.createElement('button');
        btnKill.className = 'secondary-btn';
        btnKill.style.padding = '4px 8px';
        btnKill.style.margin = '0';
        btnKill.style.color = '#ff6b6b';
        btnKill.style.borderColor = '#ff6b6b';
        btnKill.textContent = 'Close Port';
        btnKill.onclick = async () => {
            if(confirm(`Are you sure you want to kill the process on port ${port.port}?`)) {
                try {
                    if (port.port === 5500) await apiFetch('/live_server/stop');
                    else await apiFetch(`/ports/kill/${port.port}`);
                    btnKill.textContent = 'Closed';
                    btnKill.disabled = true;
                } catch(e) { alert("Failed to close port"); }
            }
        };
        
        actionTd.appendChild(btnPreview);
        actionTd.appendChild(btnLocal);
        actionTd.appendChild(btnCopy);
        actionTd.appendChild(btnKill);

        tr.innerHTML = `
            <td style="padding: 8px; font-family: monospace; font-size: 13px;">${port.port}</td>
            <td style="padding: 8px; color: #4CAF50;">${port.status}</td>
            <td style="padding: 8px; font-family: monospace;">localhost:${port.port}</td>
        `;
        tr.appendChild(actionTd);
        tbody.appendChild(tr);
    });
    
    document.querySelectorAll('.open-preview-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            window.open(e.target.dataset.url, '_blank');
        });
    });
    
    document.querySelectorAll('.copy-url-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            navigator.clipboard.writeText(e.target.dataset.url);
            const originalText = e.target.textContent;
            e.target.textContent = 'Copied!';
            setTimeout(() => { e.target.textContent = originalText; }, 2000);
        });
    });
}

export async function startLiveServer(directory, filename) {
    try {
        const res = await apiFetch(`/live_server/start?dir=${encodeURIComponent(directory)}`);
        if (res.ok) {
            // Open panel
            const bottomPanel = document.getElementById('bottom-panel');
            bottomPanel.classList.remove('hidden');
            document.getElementById('tab-ports').click();
            
            if (filename) {
                window.open(window.location.origin + `/preview/5500/${filename}`, '_blank');
            }
        } else {
            const err = await res.json();
            alert("Failed to start Live Server: " + (err.error || "Unknown error"));
        }
    } catch (e) {
        alert("Failed to start Live Server: " + e.message);
    }
}
