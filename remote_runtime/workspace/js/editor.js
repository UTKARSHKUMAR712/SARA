import { state } from './state.js';
import { apiFetch } from './api.js';
import { startLiveServer } from './ports.js';

export function initMonaco() {
    // @ts-ignore
    require(['vs/editor/editor.main'], function () {
        // @ts-ignore
        state.editor = monaco.editor.create(document.getElementById('monaco-container'), {
            value: '',
            language: 'plaintext',
            theme: 'vs-dark',
            automaticLayout: true,
            minimap: { enabled: false },
            fontSize: 14,
            fontFamily: "'Consolas', 'Courier New', monospace",
            wordWrap: "on"
        });
        
        state.editor.addAction({
            id: 'open-live-server',
            label: 'Open with Live Server',
            contextMenuGroupId: 'navigation',
            contextMenuOrder: 1.5,
            run: function(ed) {
                if (state.currentFile && state.currentFile.toLowerCase().endsWith('.html')) {
                    let filename = state.currentFile.replace(state.currentWorkspace, '');
                    if (filename.startsWith('/') || filename.startsWith('\\')) filename = filename.substring(1);
                    filename = filename.replace(/\\/g, '/');
                    startLiveServer(state.currentWorkspace, filename);
                } else {
                    alert("Live Server can only be started from an HTML file.");
                }
            }
        });

        state.editor.addAction({
            id: 'stop-live-server',
            label: 'Stop Live Server',
            contextMenuGroupId: 'navigation',
            contextMenuOrder: 1.6,
            run: async function(ed) {
                try {
                    await apiFetch('/live_server/stop');
                    state.liveServerRunning = false;
                } catch(e) {
                    alert("Failed to stop Live Server");
                }
            }
        });
        
        // Listen to changes for dirty tracking
        state.editor.onDidChangeModelContent(() => {
            if (state.currentFile) {
                const tab = state.openTabs.find(t => t.path === state.currentFile);
                if (tab && !tab.dirty) {
                    tab.dirty = true;
                    renderTabs();
                }
                
                // Auto-save logic
                if (state.autoSaveEnabled) {
                    if (state.autoSaveTimer) clearTimeout(state.autoSaveTimer);
                    state.autoSaveTimer = setTimeout(() => {
                        saveCurrentFile();
                    }, 1000);
                }
            }
        });
    });
}

export async function openFile(path) {
    let tab = state.openTabs.find(t => t.path === path);
    if (!tab) {
        try {
            const res = await apiFetch(`/read${path}`);
            const content = await res.text();
            
            // @ts-ignore
            const uri = monaco.Uri.file(path);
            // @ts-ignore
            const model = monaco.editor.createModel(content, undefined, uri);
            tab = { path, dirty: false, model };
            state.openTabs.push(tab);
        } catch (e) {
            console.error("Failed to open file", e);
            alert("Failed to open file: " + e.message);
            return;
        }
    }
    
    switchToTab(tab);
}

export function switchToTab(tab) {
    state.currentFile = tab.path;
    state.editor.setModel(tab.model);
    
    document.getElementById('welcome-screen').style.display = 'none';
    document.getElementById('monaco-container').style.display = 'block';
    
    renderTabs();
    
    // Highlight in tree if visible
    document.querySelectorAll('.tree-item').forEach(n => {
        // @ts-ignore
        if (n.dataset.path === tab.path) {
            n.classList.add('selected');
        } else {
            n.classList.remove('selected');
        }
    });
}

export function closeTab(path, force = false) {
    const idx = state.openTabs.findIndex(t => t.path === path);
    if (idx === -1) return;
    
    const tab = state.openTabs[idx];
    if (tab.dirty && !force) {
        if (!confirm(`Save changes to ${path.split('/').pop()}?`)) {
            return;
        }
    }

    tab.model.dispose();
    state.openTabs.splice(idx, 1);
    
    if (state.currentFile === path) {
        if (state.openTabs.length > 0) {
            switchToTab(state.openTabs[Math.max(0, idx - 1)]);
        } else {
            state.currentFile = null;
            document.getElementById('monaco-container').style.display = 'none';
            document.getElementById('welcome-screen').style.display = 'flex';
            renderTabs();
        }
    } else {
        renderTabs();
    }
}

export async function saveCurrentFile() {
    if (!state.currentFile || !state.editor) return;
    const tab = state.openTabs.find(t => t.path === state.currentFile);
    if (!tab || !tab.dirty) return;
    
    const content = state.editor.getValue();
    try {
        await apiFetch(`/write${state.currentFile}`, {
            method: 'PUT',
            body: content
        });
        tab.dirty = false;
        renderTabs();
    } catch (e) {
        console.error("Failed to save", e);
        alert("Failed to save: " + e.message);
    }
}

export function toggleAutoSave() {
    state.autoSaveEnabled = !state.autoSaveEnabled;
    document.getElementById('autosave-check').textContent = state.autoSaveEnabled ? '✓' : '';
    // Must save settings from main or explorer, we will just export this
    return state.autoSaveEnabled;
}

export function renderTabs() {
    const bar = document.getElementById('tabs-bar');
    bar.innerHTML = '';
    
    state.openTabs.forEach(tab => {
        const name = tab.path.split('/').pop();
        const el = document.createElement('div');
        el.className = 'tab';
        if (tab.path === state.currentFile) el.classList.add('active');
        if (tab.dirty) el.classList.add('dirty');
        
        const titleEl = document.createElement('div');
        titleEl.className = 'tab-title';
        titleEl.textContent = name;
        titleEl.title = tab.path;
        el.appendChild(titleEl);
        
        const closeEl = document.createElement('div');
        closeEl.className = 'tab-close';
        closeEl.addEventListener('click', (e) => {
            e.stopPropagation();
            closeTab(tab.path);
        });
        el.appendChild(closeEl);
        
        el.addEventListener('click', () => switchToTab(tab));
        
        bar.appendChild(el);
    });
}
