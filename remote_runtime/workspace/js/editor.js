import { state } from './state.js';
import { apiFetch } from './api.js';
import { startLiveServer } from './ports.js';
import { IconRegistry } from './icons.js';

export function initMonaco() {
    // @ts-ignore
    require(['vs/editor/editor.main'], function () {
        // @ts-ignore
        state.editor = monaco.editor.create(document.getElementById('monaco-container'), {
            value: '',
            language: 'plaintext',
            theme: state.editorSettings.theme || 'vs-dark',
            automaticLayout: true,
            minimap: { enabled: state.editorSettings.minimap !== false },
            fontSize: state.editorSettings.fontSize || 14,
            fontFamily: "'Consolas', 'Courier New', monospace",
            wordWrap: state.editorSettings.wordWrap ? "on" : "off",
            lineNumbers: state.editorSettings.lineNumbers ? "on" : "off",
            tabSize: state.editorSettings.tabSize || 4
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
    
    // Check if it's the settings tab
    if (tab.isSettings) {
        document.getElementById('welcome-screen').style.display = 'none';
        document.getElementById('monaco-container').style.display = 'none';
        document.getElementById('settings-container').style.display = 'flex';
        renderTabs();
        return;
    }
    
    document.getElementById('settings-container').style.display = 'none';
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

    if (tab.model) {
        tab.model.dispose();
    }
    state.openTabs.splice(idx, 1);
    
    if (state.currentFile === path) {
        if (state.openTabs.length > 0) {
            switchToTab(state.openTabs[Math.max(0, idx - 1)]);
        } else {
            state.currentFile = null;
            document.getElementById('settings-container').style.display = 'none';
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
        
        if (tab.isSettings) {
            titleEl.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="tab-icon" style="width: 14px; height: 14px; margin-right: 6px; vertical-align: middle;"><path d="M12 15a3 3 0 100-6 3 3 0 000 6z"/><path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-2 2 2 2 0 01-2-2v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06a1.65 1.65 0 00.33-1.82 1.65 1.65 0 00-1.51-1H3a2 2 0 01-2-2 2 2 0 012-2h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 010-2.83 2 2 0 012.83 0l.06.06a1.65 1.65 0 001.82.33H9a1.65 1.65 0 001-1.51V3a2 2 0 012-2 2 2 0 012 2v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 0 2 2 0 010 2.83l-.06.06a1.65 1.65 0 00-.33 1.82V9a1.65 1.65 0 001.51 1H21a2 2 0 012 2 2 2 0 01-2 2h-.09a1.65 1.65 0 00-1.51 1z"/></svg>Settings`;
        } else {
            const iconHtml = IconRegistry.getFileIcon(name);
            titleEl.innerHTML = `<span style="margin-right: 6px; display: inline-flex; align-items: center; vertical-align: middle; width: 14px; height: 14px;">${iconHtml}</span>${name}`;
        }
        
        titleEl.title = tab.isSettings ? 'Workspace Settings' : tab.path;
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

export function updateEditorOptions() {
    if (typeof monaco === 'undefined' || !state.editor) return;
    // @ts-ignore
    monaco.editor.setTheme(state.editorSettings.theme || 'vs-dark');
    state.editor.updateOptions({
        fontSize: state.editorSettings.fontSize || 14,
        wordWrap: state.editorSettings.wordWrap ? 'on' : 'off',
        minimap: { enabled: state.editorSettings.minimap !== false },
        lineNumbers: state.editorSettings.lineNumbers ? 'on' : 'off',
        tabSize: state.editorSettings.tabSize || 4
    });
}

export function openSettingsTab() {
    let tab = state.openTabs.find(t => t.isSettings);
    if (!tab) {
        tab = { path: 'settings://', dirty: false, isSettings: true };
        state.openTabs.push(tab);
    }
    switchToTab(tab);
}
