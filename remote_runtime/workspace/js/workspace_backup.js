// SARA Workspace Frontend Logic

let editor = null;
let currentFile = null;
let openTabs = []; // { path: string, content: string, dirty: boolean, model: MonacoModel }
let currentWorkspace = '';
let autoSaveEnabled = false;
let autoSaveTimer = null;
let clipboardOp = { type: null, path: null }; // type: 'cut' | 'copy'
let contextMenuTarget = null; // target path for context menu

// Utility to extract the SARA token from the URL or Cookies
function getAuthToken() {
    const params = new URLSearchParams(window.location.search);
    let t = params.get('token');
    if (t) return t;
    const match = document.cookie.match(new RegExp('(^| )sara_token=([^;]+)'));
    if (match) return match[2];
    return '';
}

// Ensure fetch requests include the token and proper headers
async function apiFetch(endpoint, options = {}) {
    const token = getAuthToken();
    let url = `/workspace/api${endpoint}`;
    url += (url.includes('?') ? '&' : '?') + 'token=' + token;
    const res = await fetch(url, options);
    if (!res.ok) {
        throw new Error(`API Error: ${res.status} ${res.statusText}`);
    }
    return res;
}

// ── Explorer ──────────────────────────────────────────────────────────────

async function loadTree(path, containerElement, level = 0) {
    try {
        const res = await apiFetch(`/tree${path}`);
        const data = await res.json();
        
        containerElement.innerHTML = '';
        if (data.items) {
            // Sort: folders first, then files alphabetically
            data.items.sort((a, b) => {
                if (a.isDir && !b.isDir) return -1;
                if (!a.isDir && b.isDir) return 1;
                return a.name.localeCompare(b.name);
            });

            data.items.forEach(item => {
                const el = document.createElement('div');
                
                const itemDiv = document.createElement('div');
                itemDiv.className = 'tree-item';
                itemDiv.dataset.path = item.path;
                itemDiv.dataset.isdir = item.isDir;
                itemDiv.style.paddingLeft = `${level * 12 + 10}px`;
                
                let chevron = item.isDir ?
                    `<span class="chevron"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18l6-6-6-6"/></svg></span>` :
                    `<span class="chevron hidden"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18l6-6-6-6"/></svg></span>`;
                
                let icon = item.isDir ? 
                    `<svg viewBox="0 0 32 32" width="16" height="16" fill="#519aba"><path d="M28 26H4a2 2 0 01-2-2V8a2 2 0 012-2h8l2.5 4h11.5a2 2 0 012 2v12a2 2 0 01-2 2z"/></svg>` : 
                    getFileIcon(item.name);
                
                itemDiv.innerHTML = `<div class="tree-item-content">${chevron}<span class="tree-icon">${icon}</span><span class="tree-name">${item.name}</span></div>`;
                
                el.appendChild(itemDiv);

                const childrenDiv = document.createElement('div');
                childrenDiv.className = 'tree-children';
                el.appendChild(childrenDiv);

                itemDiv.addEventListener('click', async (e) => {
                    e.stopPropagation();
                    
                    // Highlight selected
                    document.querySelectorAll('.tree-item').forEach(n => n.classList.remove('selected'));
                    itemDiv.classList.add('selected');

                    if (item.isDir) {
                        childrenDiv.classList.toggle('expanded');
                        const chev = itemDiv.querySelector('.chevron');
                        if (chev) chev.classList.toggle('expanded');

                        if (childrenDiv.classList.contains('expanded') && childrenDiv.children.length === 0) {
                            await loadTree(item.path, childrenDiv, level + 1);
                        }
                    } else {
                        await openFile(item.path);
                    }
                });

                containerElement.appendChild(el);
            });
        }
    } catch (e) {
        console.error("Failed to load tree:", e);
    }
}

// ── Editor & Tabs ────────────────────────────────────────────────────────

function initMonaco() {
    require(['vs/editor/editor.main'], function() {
        editor = monaco.editor.create(document.getElementById('monaco-container'), {
            theme: 'vs-dark',
            automaticLayout: true,
            minimap: { enabled: false },
            fontSize: 14,
            wordWrap: 'on'
        });

        editor.onDidChangeModelContent(() => {
            const tab = openTabs.find(t => t.path === currentFile);
            if (tab && !tab.dirty) {
                tab.dirty = true;
                renderTabs();
            }
            
            if (autoSaveEnabled && tab) {
                clearTimeout(autoSaveTimer);
                autoSaveTimer = setTimeout(() => {
                    saveCurrentFile();
                }, 1000); // 1 second debounce
            }
        });

        // Add Save Shortcut
        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, async function() {
            await saveCurrentFile();
        });
        
        console.log("Monaco Editor initialized.");
    });
}

function getFileIcon(filename) {
    if (!filename || !filename.includes('.')) return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#8a8a8a"><path d="M22 2H8a2 2 0 00-2 2v24a2 2 0 002 2h16a2 2 0 002-2V10L22 2zm0 9V4l6 6h-6z"/></svg>`;
    const ext = filename.split('.').pop().toLowerCase();
    switch (ext) {
        case 'py': return `<svg viewBox="0 0 32 32" width="16" height="16"><path fill="#3572A5" d="M16 2.5c-7 0-6.7 3-6.7 3v3.2h6.8V11H6s-3.5-.2-3.5 4.8 3 5 3 5h2v-3.4c0-2 1.8-3.7 3.8-3.7h6.6c2 0 3.7-1.7 3.7-3.7V6s.3-3.5-5-3.5zm-2.4 2c.6 0 1 .4 1 1s-.4 1-1 1-1-.4-1-1 .4-1 1-1z"/><path fill="#FFD43B" d="M16 29.5c7 0 6.7-3 6.7-3v-3.2h-6.8V21H26s3.5.2 3.5-4.8-3-5-3-5h-2v3.4c0 2-1.8 3.7-3.8 3.7h-6.6c-2 0-3.7 1.7-3.7 3.7V26s-.3 3.5 5 3.5zm2.4-2c-.6 0-1-.4-1-1s.4-1 1-1 1 .4 1 1-.4 1-1 1z"/></svg>`;
        case 'js': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#cbcb41"><path d="M2 2v28h28V2H2zm15 22.4c-.4.6-.9 1.1-1.6 1.4-.7.3-1.6.5-2.7.5-1.5 0-2.8-.4-3.7-1.1-.9-.7-1.5-1.8-1.9-3.1l2.8-1.1c.2.8.5 1.4.9 1.8.4.4 1 .6 1.8.6.8 0 1.4-.2 1.9-.5.4-.3.6-.8.6-1.3 0-.4-.1-.7-.4-1-.2-.2-.6-.5-1.2-.7l-1.9-.6c-1-.3-1.8-.7-2.3-1.3-.5-.6-.8-1.4-.8-2.3 0-1.1.4-2.1 1.2-2.8.8-.7 2-1 3.5-1 1.3 0 2.4.3 3.2 1 .8.7 1.3 1.6 1.4 2.8l-2.7.7c-.1-.6-.3-1-.6-1.3-.3-.3-.8-.5-1.4-.5-.6 0-1.1.2-1.5.5-.3.3-.5.7-.5 1.1 0 .4.2.7.5 1 .3.2.8.5 1.5.7l1.9.6c1 .3 1.8.8 2.3 1.4.5.6.8 1.4.8 2.5 0 1.2-.4 2.2-1.1 2.9zm10.6-1c-.8.8-1.8 1.2-3.1 1.2-1.5 0-2.6-.4-3.4-1.2-.8-.8-1.2-2-1.2-3.4V12.1h3V21.6c0 .7.1 1.2.4 1.5.3.3.7.5 1.2.5.5 0 1-.2 1.3-.5.3-.3.4-.8.4-1.5v-9.5h3v9.5c0 1.4-.4 2.6-1.2 3.4z"/></svg>`;
        case 'ts': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#2b7489"><path d="M2 2v28h28V2H2zm15 22.4c-.4.6-.9 1.1-1.6 1.4-.7.3-1.6.5-2.7.5-1.5 0-2.8-.4-3.7-1.1-.9-.7-1.5-1.8-1.9-3.1l2.8-1.1c.2.8.5 1.4.9 1.8.4.4 1 .6 1.8.6.8 0 1.4-.2 1.9-.5.4-.3.6-.8.6-1.3 0-.4-.1-.7-.4-1-.2-.2-.6-.5-1.2-.7l-1.9-.6c-1-.3-1.8-.7-2.3-1.3-.5-.6-.8-1.4-.8-2.3 0-1.1.4-2.1 1.2-2.8.8-.7 2-1 3.5-1 1.3 0 2.4.3 3.2 1 .8.7 1.3 1.6 1.4 2.8l-2.7.7c-.1-.6-.3-1-.6-1.3-.3-.3-.8-.5-1.4-.5-.6 0-1.1.2-1.5.5-.3.3-.5.7-.5 1.1 0 .4.2.7.5 1 .3.2.8.5 1.5.7l1.9.6c1 .3 1.8.8 2.3 1.4.5.6.8 1.4.8 2.5 0 1.2-.4 2.2-1.1 2.9zm10.6-1c-.8.8-1.8 1.2-3.1 1.2-1.5 0-2.6-.4-3.4-1.2-.8-.8-1.2-2-1.2-3.4V12.1h3V21.6c0 .7.1 1.2.4 1.5.3.3.7.5 1.2.5.5 0 1-.2 1.3-.5.3-.3.4-.8.4-1.5v-9.5h3v9.5c0 1.4-.4 2.6-1.2 3.4z"/></svg>`;
        case 'html':
        case 'htm': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#E34F26"><path d="M4 2l2 23.5 10 2.5 10-2.5L28 2H4zm17 11h-4.5l-.5-4.5H23l.5-3H8l1.5 13H15l-.5 4-4-1-1-3.5H7l1.5 6.5 7.5 2 7.5-2L24.5 13z"/></svg>`;
        case 'css': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#1572B6"><path d="M4 2l2 23.5 10 2.5 10-2.5L28 2H4zm17 11h-4.5l-.5-4.5H23l.5-3H8l1.5 13H15l-.5 4-4-1-1-3.5H7l1.5 6.5 7.5 2 7.5-2L24.5 13z"/></svg>`;
        case 'json': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#8a8a8a"><path d="M12 8v16h8V8h-8zm6 14h-4v-12h4v12z"/><path d="M2 2h28v28H2V2zm26 26V4H4v24h24z"/></svg>`;
        case 'md': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#083fa1"><path d="M2 5v22h28V5H2zm12 17H9v-7H7v7H4V10h3v7h2v-7h3v12zm14 0h-3v-3h-2v3h-3V10h8v12zm-3-8h-2v3h2v-3z"/></svg>`;
        case 'cpp':
        case 'hpp':
        case 'c':
        case 'h': return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#519aba"><path d="M16 2l-12 7v14l12 7 12-7V9L16 2zm0 25l-9-5V11l9-5 9 5v11l-9 5z"/><path d="M20 15h-3v-3h-2v3h-3v2h3v3h2v-3h3v-2z"/></svg>`;
        default: return `<svg viewBox="0 0 32 32" width="16" height="16" fill="#8a8a8a"><path d="M22 2H8a2 2 0 00-2 2v24a2 2 0 002 2h16a2 2 0 002-2V10L22 2zm0 9V4l6 6h-6z"/></svg>`;
    }
}

async function openFile(path) {
    // Check if already open
    let tab = openTabs.find(t => t.path === path);
    if (!tab) {
        try {
            const res = await apiFetch(`/read${path}`);
            const content = await res.text();
            
            const uri = monaco.Uri.file(path);
            const model = monaco.editor.createModel(content, undefined, uri);
            tab = { path, dirty: false, model };
            openTabs.push(tab);
        } catch (e) {
            console.error("Failed to open file", e);
            alert("Failed to open file: " + e.message);
            return;
        }
    }
    
    switchToTab(tab);
}

function switchToTab(tab) {
    currentFile = tab.path;
    editor.setModel(tab.model);
    
    document.getElementById('welcome-screen').style.display = 'none';
    document.getElementById('monaco-container').style.display = 'block';
    
    renderTabs();
    
    // Highlight in tree if visible
    document.querySelectorAll('.tree-item').forEach(n => {
        if (n.dataset.path === tab.path) {
            n.classList.add('selected');
        } else {
            n.classList.remove('selected');
        }
    });
}

function closeTab(path, force = false) {
    const idx = openTabs.findIndex(t => t.path === path);
    if (idx === -1) return;
    
    const tab = openTabs[idx];
    if (tab.dirty && !force) {
        if (!confirm(`Save changes to ${path.split('/').pop()}?`)) {
            // Cancel close
            return;
        }
        // User could click cancel, or we could implement a full dialog.
        // For now, if they click OK to save changes... wait, confirm() just returns true/false.
        // Better: simple dirty check
    }

    // Dispose model to free memory
    tab.model.dispose();
    openTabs.splice(idx, 1);
    
    if (currentFile === path) {
        if (openTabs.length > 0) {
            switchToTab(openTabs[Math.max(0, idx - 1)]);
        } else {
            currentFile = null;
            document.getElementById('monaco-container').style.display = 'none';
            document.getElementById('welcome-screen').style.display = 'flex';
            renderTabs();
        }
    } else {
        renderTabs();
    }
}

async function saveCurrentFile() {
    if (!currentFile || !editor) return;
    const tab = openTabs.find(t => t.path === currentFile);
    if (!tab || !tab.dirty) return;
    
    const content = editor.getValue();
    try {
        await apiFetch(`/write${currentFile}`, {
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

function renderTabs() {
    const bar = document.getElementById('tabs-bar');
    bar.innerHTML = '';
    
    openTabs.forEach(tab => {
        const name = tab.path.split('/').pop();
        const el = document.createElement('div');
        el.className = 'tab';
        if (tab.path === currentFile) el.classList.add('active');
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

// ── File & Folder Creation ────────────────────────────────────────────────

function getTargetDirectory() {
    if (!currentWorkspace) return null;
    let targetPath = currentWorkspace;
    const selected = document.querySelector('.tree-item.selected');
    if (selected) {
        if (selected.dataset.isdir === 'true') {
            targetPath = selected.dataset.path;
        } else {
            let parts = selected.dataset.path.split(/[\/\\]/);
            parts.pop();
            targetPath = parts.join('/') || '/';
        }
    }
    return targetPath;
}

async function createNewFile() {
    const targetPath = getTargetDirectory();
    if (!targetPath) return;
    
    const name = prompt("Enter new file name:");
    if (!name) return;
    
    const fullPath = targetPath + (targetPath.endsWith('/') || targetPath.endsWith('\\') ? '' : '/') + name;
    try {
        await apiFetch(`/write${fullPath}`, { method: 'PUT', body: '' });
        refreshExplorer();
        openFile(fullPath);
    } catch (e) {
        alert("Failed to create file: " + e.message);
    }
}

async function createNewFolder() {
    const targetPath = getTargetDirectory();
    if (!targetPath) return;
    
    const name = prompt("Enter new folder name:");
    if (!name) return;
    
    const fullPath = targetPath + (targetPath.endsWith('/') || targetPath.endsWith('\\') ? '' : '/') + name;
    try {
        await apiFetch(`/mkdir${fullPath}`, { method: 'POST' });
        refreshExplorer();
    } catch (e) {
        alert("Failed to create folder: " + e.message);
    }
}

// ── Context Menu & File Operations ────────────────────────────────────────

function hideContextMenu() {
    document.getElementById('context-menu').style.display = 'none';
    contextMenuTarget = null;
}

document.addEventListener('click', hideContextMenu);

document.getElementById('sidebar').addEventListener('contextmenu', (e) => {
    e.preventDefault();
    e.stopPropagation();

    const itemDiv = e.target.closest('.tree-item');
    
    // Clear selection
    document.querySelectorAll('.tree-item').forEach(n => n.classList.remove('selected'));
    
    if (itemDiv) {
        itemDiv.classList.add('selected');
        contextMenuTarget = {
            path: itemDiv.dataset.path,
            isDir: itemDiv.dataset.isdir === 'true',
            name: itemDiv.querySelector('.tree-name').textContent
        };
    } else {
        contextMenuTarget = null;
    }
    
    const menu = document.getElementById('context-menu');
    menu.style.display = 'block';
    
    // Ensure menu stays within viewport
    let x = e.clientX;
    let y = e.clientY;
    const rect = menu.getBoundingClientRect();
    if (x + rect.width > window.innerWidth) x = window.innerWidth - rect.width;
    if (y + rect.height > window.innerHeight) y = window.innerHeight - rect.height;
    
    menu.style.left = `${x}px`;
    menu.style.top = `${y}px`;
    
    // Toggle options based on target
    const isFileOrFolder = contextMenuTarget ? 'block' : 'none';
    document.getElementById('ctx-cut').style.display = isFileOrFolder;
    document.getElementById('ctx-copy').style.display = isFileOrFolder;
    document.getElementById('ctx-copypath').style.display = isFileOrFolder;
    document.getElementById('ctx-copyrelpath').style.display = isFileOrFolder;
    document.getElementById('ctx-rename').style.display = isFileOrFolder;
    document.getElementById('ctx-delete').style.display = isFileOrFolder;
    document.getElementById('ctx-div-1').style.display = isFileOrFolder;
    document.getElementById('ctx-div-2').style.display = isFileOrFolder;
    
    const isFolderOrEmpty = (!contextMenuTarget || contextMenuTarget.isDir) ? 'block' : 'none';
    document.getElementById('ctx-new-file').style.display = isFolderOrEmpty;
    document.getElementById('ctx-new-folder').style.display = isFolderOrEmpty;
    document.getElementById('ctx-div-new').style.display = isFolderOrEmpty;
    
    // Paste available if clipboard has data and we are in a workspace
    const pasteOpt = document.getElementById('ctx-paste');
    if (clipboardOp.path && currentWorkspace) {
        pasteOpt.style.display = 'block';
    } else {
        pasteOpt.style.display = 'none';
    }
});

// Context Menu Actions
document.getElementById('ctx-new-file').addEventListener('click', createNewFile);
document.getElementById('ctx-new-folder').addEventListener('click', createNewFolder);

document.getElementById('ctx-cut').addEventListener('click', () => {
    const target = contextMenuTarget;
    if (target) clipboardOp = { type: 'cut', path: target.path };
});
document.getElementById('ctx-copy').addEventListener('click', () => {
    const target = contextMenuTarget;
    if (target) clipboardOp = { type: 'copy', path: target.path };
});
document.getElementById('ctx-copypath').addEventListener('click', () => {
    const target = contextMenuTarget;
    if (target) navigator.clipboard.writeText(target.path);
});
document.getElementById('ctx-copyrelpath').addEventListener('click', () => {
    const target = contextMenuTarget;
    if (target && currentWorkspace) {
        let rel = target.path.replace(currentWorkspace, '');
        if (rel.startsWith('/') || rel.startsWith('\\')) rel = rel.substring(1);
        navigator.clipboard.writeText(rel);
    }
});

document.getElementById('ctx-paste').addEventListener('click', async () => {
    const target = contextMenuTarget;
    if (!clipboardOp.path || !currentWorkspace) return;
    
    let targetPath = currentWorkspace;
    if (target) {
        if (target.isDir) {
            targetPath = target.path;
        } else {
            let parts = target.path.split(/[\/\\]/);
            parts.pop();
            targetPath = parts.join('/') || '/';
        }
    }
    
    const fromPath = clipboardOp.path;
    const filename = fromPath.split(/[\/\\]/).pop();
    const toPath = targetPath + (targetPath.endsWith('/') || targetPath.endsWith('\\') ? '' : '/') + filename;
    
    try {
        const action = clipboardOp.type === 'cut' ? 'rename' : 'copy';
        await apiFetch(`/${action}${fromPath}?to=${encodeURIComponent(toPath)}`, { method: 'POST' });
        if (clipboardOp.type === 'cut') clipboardOp = { type: null, path: null };
        refreshExplorer();
    } catch (e) {
        alert("Failed to paste: " + e.message);
    }
});

document.getElementById('ctx-rename').addEventListener('click', async () => {
    const target = contextMenuTarget;
    if (!target) return;
    const newName = prompt("Enter new name:", target.name);
    if (!newName || newName === target.name) return;
    
    let parts = target.path.split(/[\/\\]/);
    parts.pop();
    let parentPath = parts.join('/');
    if (!parentPath) parentPath = '/';
    let newPath = parentPath + (parentPath.endsWith('/') ? '' : '/') + newName;
    
    try {
        await apiFetch(`/rename${target.path}?to=${encodeURIComponent(newPath)}`, { method: 'POST' });
        refreshExplorer();
        
        // Update open tabs if a file was renamed
        if (!target.isDir) {
            let tab = openTabs.find(t => t.path === target.path);
            if (tab) {
                tab.path = newPath;
                if (currentFile === target.path) currentFile = newPath;
                renderTabs();
            }
        }
    } catch (e) {
        alert("Rename failed: " + e.message);
    }
});

document.getElementById('ctx-delete').addEventListener('click', async () => {
    const target = contextMenuTarget;
    if (!target) return;
    if (!confirm(`Are you sure you want to delete '${target.name}'?`)) return;
    
    try {
        await apiFetch(`/delete${target.path}`, { method: 'POST' });
        closeTab(target.path, true); // force close if open
        refreshExplorer();
    } catch (e) {
        alert("Delete failed: " + e.message);
    }
});

// ── Open Folder Modal & State ──────────────────────────────────────────────

function toggleAutoSave() {
    autoSaveEnabled = !autoSaveEnabled;
    saveSettings();
    document.getElementById('autosave-check').textContent = autoSaveEnabled ? '✓' : '';
}

async function saveSettings() {
    try {
        await apiFetch('/settings', {
            method: 'POST',
            body: JSON.stringify({ currentWorkspace, autoSaveEnabled })
        });
    } catch (e) {
        console.error("Failed to save settings:", e);
    }
}

function refreshExplorer() {
    const treeRoot = document.getElementById('explorer-tree');
    const emptyState = document.getElementById('sidebar-empty');
    if (!currentWorkspace) {
        treeRoot.style.display = 'none';
        emptyState.style.display = 'block';
    } else {
        emptyState.style.display = 'none';
        treeRoot.style.display = 'block';
        loadTree(currentWorkspace, treeRoot);
    }
}

async function loadFolderPicker(path) {
    const list = document.getElementById('folder-picker-list');
    list.innerHTML = 'Loading...';
    try {
        // Fetch the tree for the given absolute path
        const res = await apiFetch(`/tree${path}`);
        const data = await res.json();
        list.innerHTML = '';
        
        // Add ".." to go up if possible
        if (path.length > 1) {
            const upDiv = document.createElement('div');
            upDiv.className = 'picker-item';
            upDiv.innerHTML = `<span class="tree-icon">📁</span> ..`;
            upDiv.addEventListener('click', () => {
                let parts = path.split(/[\/\\]/);
                parts.pop(); // remove current
                if (parts.length > 0 && parts[parts.length-1] === '') parts.pop(); // remove trailing empty
                let upPath = parts.join('/') || '/';
                if (path.match(/^[a-zA-Z]:[\/\\]?$/)) upPath = '/'; // If it's C:\, go up to /
                document.getElementById('folder-path-input').value = upPath;
                loadFolderPicker(upPath);
            });
            list.appendChild(upDiv);
        }

        if (data.items) {
            data.items.filter(i => i.isDir).sort((a,b) => a.name.localeCompare(b.name)).forEach(item => {
                const el = document.createElement('div');
                el.className = 'picker-item';
                el.innerHTML = `<span class="tree-icon">📁</span> ${item.name}`;
                el.addEventListener('click', () => {
                    document.getElementById('folder-path-input').value = item.path;
                    loadFolderPicker(item.path);
                });
                list.appendChild(el);
            });
        }
    } catch (e) {
        list.innerHTML = `<div style="color:red; padding:10px;">Failed to load folder: ${e.message}</div>`;
    }
}

function openFolderModal() {
    const modal = document.getElementById('folder-modal');
    modal.style.display = 'flex';
    let initPath = currentWorkspace || '/';
    document.getElementById('folder-path-input').value = initPath;
    loadFolderPicker(initPath);
}

function closeFolderModal() {
    document.getElementById('folder-modal').style.display = 'none';
}

async function confirmFolder() {
    const path = document.getElementById('folder-path-input').value.trim();
    if (path) {
        try {
            const res = await apiFetch(`/tree${path}`);
            if (!res.ok) throw new Error("Could not access path");
            
            currentWorkspace = path;
            saveSettings();
            
            closeFolderModal();
            refreshExplorer();
        } catch (e) {
            alert("Invalid folder path or access denied.");
        }
    }
}

// ── Startup ───────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', async () => {
    // 1. Initial State
    try {
        const res = await apiFetch('/settings');
        if (res.ok) {
            const settings = await res.json();
            if (settings.currentWorkspace) currentWorkspace = settings.currentWorkspace;
            if (settings.autoSaveEnabled !== undefined) autoSaveEnabled = settings.autoSaveEnabled;
        }
    } catch (e) {
        console.error("Failed to load settings:", e);
    }
    
    document.getElementById('autosave-check').textContent = autoSaveEnabled ? '✓' : '';
    
    if (currentWorkspace) {
        refreshExplorer();
    } else {
        document.getElementById('sidebar-empty').style.display = 'block';
    }
    
    // 2. Load Monaco Editor
    initMonaco();
    
    // 3. UI interactions
    document.getElementById('btn-explorer').addEventListener('click', () => {
        const sb = document.getElementById('sidebar');
        sb.classList.toggle('show');
    });

    // Menu Bar
    document.getElementById('menu-new-file').addEventListener('click', createNewFile);
    document.getElementById('menu-new-folder').addEventListener('click', createNewFolder);
    document.getElementById('menu-save').addEventListener('click', saveCurrentFile);
    document.getElementById('menu-autosave').addEventListener('click', toggleAutoSave);

    // Sidebar Action Icons
    document.getElementById('btn-new-file').addEventListener('click', createNewFile);
    document.getElementById('btn-new-folder').addEventListener('click', createNewFolder);
    
    // Double click on empty space in explorer
    document.getElementById('explorer-tree').addEventListener('dblclick', (e) => {
        if (!e.target.closest('.tree-item')) {
            createNewFile();
        }
    });

    // Folder Modal
    document.getElementById('btn-open-folder').addEventListener('click', openFolderModal);
    document.getElementById('menu-open-folder').addEventListener('click', () => {
        // Also close the dropdown by blurring or similar, but CSS hover handles it.
        openFolderModal();
    });
    document.querySelector('.close-modal').addEventListener('click', closeFolderModal);
    document.getElementById('btn-cancel-folder').addEventListener('click', closeFolderModal);
    document.getElementById('btn-confirm-folder').addEventListener('click', confirmFolder);
    
    document.getElementById('folder-path-input').addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            loadFolderPicker(e.target.value);
        }
    });

    // Keyboard shortcut for saving globally
    document.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key === 's') {
            e.preventDefault();
            saveCurrentFile();
        }
        // Ctrl+` or Ctrl+~ for terminal
        if ((e.ctrlKey || e.metaKey) && (e.key === '`' || e.key === '~')) {
            e.preventDefault();
            toggleTerminal();
        }
    });

    document.getElementById('btn-close-panel').addEventListener('click', toggleTerminal);
});

// --- Terminal Logic ---
let terminal = null;
let terminalFitAddon = null;
let terminalWs = null;
let isTerminalOpen = false;

function toggleTerminal() {
    const panel = document.getElementById('bottom-panel');
    if (panel.classList.contains('hidden')) {
        panel.classList.remove('hidden');
        isTerminalOpen = true;
        if (!terminal) {
            initTerminal();
        } else {
            setTimeout(() => { terminalFitAddon.fit(); }, 50);
        }
    } else {
        panel.classList.add('hidden');
        isTerminalOpen = false;
    }
}

function initTerminal() {
    const container = document.getElementById('terminal-container');
    container.innerHTML = '';
    
    terminal = new Terminal({
        cursorBlink: true,
        theme: {
            background: '#1e1e1e',
            foreground: '#cccccc'
        },
        fontFamily: 'Consolas, "Courier New", monospace',
        fontSize: 14
    });
    
    terminalFitAddon = new FitAddon.FitAddon();
    terminal.loadAddon(terminalFitAddon);
    
    terminal.open(container);
    terminalFitAddon.fit();
    
    connectTerminalWs();
    
    const resizeObserver = new ResizeObserver(() => {
        if (isTerminalOpen) {
            try { terminalFitAddon.fit(); } catch (e) {}
        }
    });
    resizeObserver.observe(container);
}

function connectTerminalWs() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const cols = terminal.cols || 80;
    const rows = terminal.rows || 24;
    const wsUrl = `${protocol}//${window.location.host}/ws_terminal?token=${getAuthToken()}&cols=${cols}&rows=${rows}`;
    
    terminalWs = new WebSocket(wsUrl);
    
    terminalWs.onopen = () => {
        terminal.writeln('Connected to SARA Terminal...');
    };
    
    terminalWs.onmessage = (event) => {
        terminal.write(event.data);
    };
    
    terminalWs.onclose = () => {
        terminal.writeln('\\r\\n[Disconnected]');
    };
    
    terminal.onData((data) => {
        if (terminalWs.readyState === WebSocket.OPEN) {
            terminalWs.send(data);
        }
    });
}

