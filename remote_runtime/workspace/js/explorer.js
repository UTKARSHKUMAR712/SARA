import { state } from './state.js';
import { apiFetch } from './api.js';
import { IconRegistry } from './icons.js';
import { openFile, closeTab, renderTabs } from './editor.js';
import { saveSettings } from './settings.js';

export function refreshExplorer() {
    const treeRoot = document.getElementById('explorer-tree');
    const emptyState = document.getElementById('sidebar-empty');
    if (!state.currentWorkspace) {
        treeRoot.style.display = 'none';
        emptyState.style.display = 'block';
    } else {
        emptyState.style.display = 'none';
        treeRoot.style.display = 'block';
        loadTree(state.currentWorkspace, treeRoot);
    }
}

export async function loadTree(path, container) {
    try {
        const res = await apiFetch(`/tree${path}`);
        const data = await res.json();
        
        container.innerHTML = '';
        if (data.items) {
            renderTreeNodes(container, data.items);
        }
    } catch (e) {
        container.innerHTML = `<div style="color:red; padding: 10px;">Error loading tree</div>`;
        console.error(e);
    }
}

export function renderTreeNodes(container, items) {
    // Filter out .git directory
    items = items.filter(item => item.name !== '.git');
    
    // Sort directories first, then files alphabetically
    items.sort((a, b) => {
        if (a.isDir && !b.isDir) return -1;
        if (!a.isDir && b.isDir) return 1;
        return a.name.localeCompare(b.name);
    });

    items.forEach(item => {
        const itemDiv = document.createElement('div');
        itemDiv.className = 'tree-node';
        
        const row = document.createElement('div');
        row.className = 'tree-item';
        if (item.path === state.currentFile) row.classList.add('selected');
        row.dataset.path = item.path;
        row.dataset.isdir = item.isDir;
        
        const chevron = item.isDir ? 
            `<span class="chevron"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18l6-6-6-6"/></svg></span>` : 
            `<span class="chevron hidden"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18l6-6-6-6"/></svg></span>`;
        
        let icon = item.isDir ? 
            IconRegistry.getFolderIcon(item.name, false) : 
            IconRegistry.getFileIcon(item.name);
        
        row.innerHTML = `<div class="tree-item-content">${chevron}${icon}<span class="tree-name">${item.name}</span></div>`;
        
        itemDiv.appendChild(row);
        
        if (item.isDir) {
            const childrenContainer = document.createElement('div');
            childrenContainer.className = 'tree-children';
            childrenContainer.dataset.loaded = "false";
            itemDiv.appendChild(childrenContainer);
            
            row.addEventListener('click', async (e) => {
                e.stopPropagation();
                // Select
                document.querySelectorAll('.tree-item').forEach(n => n.classList.remove('selected'));
                row.classList.add('selected');
                
                // Toggle expand
                const chev = row.querySelector('.chevron');
                if (childrenContainer.classList.contains('expanded')) {
                    childrenContainer.classList.remove('expanded');
                    chev.classList.remove('expanded');
                    // Update icon to closed
                    row.querySelector('.tree-icon').outerHTML = IconRegistry.getFolderIcon(item.name, false);
                } else {
                    childrenContainer.classList.add('expanded');
                    chev.classList.add('expanded');
                    // Update icon to open
                    row.querySelector('.tree-icon').outerHTML = IconRegistry.getFolderIcon(item.name, true);
                    
                    if (childrenContainer.dataset.loaded === "false") {
                        childrenContainer.innerHTML = '<div style="padding-left:20px; color:#888;">Loading...</div>';
                        try {
                            const res = await apiFetch(`/tree${item.path}`);
                            const data = await res.json();
                            childrenContainer.innerHTML = '';
                            if (data.items) {
                                renderTreeNodes(childrenContainer, data.items);
                            }
                            childrenContainer.dataset.loaded = "true";
                        } catch (err) {
                            childrenContainer.innerHTML = '<div style="padding-left:20px; color:red;">Error</div>';
                        }
                    }
                }
            });
        } else {
            row.addEventListener('click', (e) => {
                e.stopPropagation();
                document.querySelectorAll('.tree-item').forEach(n => n.classList.remove('selected'));
                row.classList.add('selected');
                openFile(item.path);
            });
        }
        
        container.appendChild(itemDiv);
    });
}

export function getTargetDirectory() {
    if (!state.currentWorkspace) return null;
    let targetPath = state.currentWorkspace;
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

export async function createNewFile() {
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

export async function createNewFolder() {
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

export function hideContextMenu() {
    document.getElementById('context-menu').style.display = 'none';
    state.contextMenuTarget = null;
}

// ── Folder Picker ──────────────────────────────────────────

export async function loadFolderPicker(path) {
    const list = document.getElementById('folder-picker-list');
    list.innerHTML = 'Loading...';
    try {
        const res = await apiFetch(`/tree${path}`);
        const data = await res.json();
        list.innerHTML = '';
        
        if (path.length > 1) {
            const upDiv = document.createElement('div');
            upDiv.className = 'picker-item';
            upDiv.innerHTML = `${IconRegistry.getFolderIcon('folder', false)} ..`;
            upDiv.addEventListener('click', () => {
                let parts = path.split(/[\/\\]/);
                parts.pop();
                if (parts.length > 0 && parts[parts.length-1] === '') parts.pop();
                let upPath = parts.join('/') || '/';
                if (path.match(/^[a-zA-Z]:[\/\\]?$/)) upPath = '/';
                document.getElementById('folder-path-input').value = upPath;
                loadFolderPicker(upPath);
            });
            list.appendChild(upDiv);
        }

        if (data.items) {
            data.items.filter(i => i.isDir).sort((a,b) => a.name.localeCompare(b.name)).forEach(item => {
                const el = document.createElement('div');
                el.className = 'picker-item';
                el.innerHTML = `${IconRegistry.getFolderIcon(item.name, false)} ${item.name}`;
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

export function openFolderModal() {
    const modal = document.getElementById('folder-modal');
    modal.style.display = 'flex';
    let initPath = state.currentWorkspace || '/';
    // @ts-ignore
    document.getElementById('folder-path-input').value = initPath;
    loadFolderPicker(initPath);
}

export function closeFolderModal() {
    document.getElementById('folder-modal').style.display = 'none';
}

export async function confirmFolder() {
    // @ts-ignore
    const path = document.getElementById('folder-path-input').value.trim();
    if (path) {
        try {
            const res = await apiFetch(`/tree${path}`);
            if (!res.ok) throw new Error("Could not access path");
            
            state.currentWorkspace = path;
            saveSettings();
            
            closeFolderModal();
            refreshExplorer();
        } catch (e) {
            alert("Invalid folder path or access denied.");
        }
    }
}
