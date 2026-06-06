import { state } from './state.js';
import { apiFetch } from './api.js';
import { initMonaco, saveCurrentFile, toggleAutoSave, closeTab, renderTabs } from './editor.js';
import { refreshExplorer, createNewFile, createNewFolder, openFolderModal, closeFolderModal, confirmFolder, loadFolderPicker, hideContextMenu } from './explorer.js';
import { toggleTerminal, killTerminal, initTerminal } from './terminal.js';
import { initPortsPanel, startLiveServer } from './ports.js';
import { initSettingsUI, applyAllSettings, saveSettings } from './settings.js';
import { initSidebarResizer } from './resizer.js';

document.addEventListener('DOMContentLoaded', async () => {
    // 1. Initial State
    try {
        const res = await apiFetch('/settings');
        if (res.ok) {
            const settings = await res.json();
            if (settings.currentWorkspace) state.currentWorkspace = settings.currentWorkspace;
            if (settings.autoSaveEnabled !== undefined) state.autoSaveEnabled = settings.autoSaveEnabled;
            
            if (settings.editor) Object.assign(state.editorSettings, settings.editor);
            if (settings.terminal) Object.assign(state.terminalSettings, settings.terminal);
            if (settings.layout) Object.assign(state.layout, settings.layout);
            if (settings.theme) state.theme = settings.theme;
            if (settings.themeOverrides) state.themeOverrides = settings.themeOverrides;
            
            if (settings.openFiles) state._pendingOpenFiles = settings.openFiles;
            if (settings.currentFile) state._pendingCurrentFile = settings.currentFile;
        }
    } catch (e) {
        console.error("Failed to load settings:", e);
    }
    
    applyAllSettings();
    initSettingsUI();
    initSidebarResizer();
    
    document.getElementById('autosave-check').textContent = state.autoSaveEnabled ? '✓' : '';
    
    if (state.currentWorkspace) {
        refreshExplorer();
    } else {
        document.getElementById('sidebar-empty').style.display = 'block';
    }
    
    // 2. Load Monaco Editor
    initMonaco(async () => {
        // Restore open files
        if (state._pendingOpenFiles && state._pendingOpenFiles.length > 0) {
            const { openFile, openSettingsTab, switchToTab } = await import('./editor.js');
            for (const path of state._pendingOpenFiles) {
                if (path === 'settings://') {
                    openSettingsTab();
                } else {
                    await openFile(path);
                }
            }
        }
        if (state._pendingCurrentFile) {
            const { switchToTab, openSettingsTab } = await import('./editor.js');
            const tab = state.openTabs.find(t => t.path === state._pendingCurrentFile);
            if (tab) {
                switchToTab(tab);
            } else if (state._pendingCurrentFile === 'settings://') {
                openSettingsTab();
            }
        }
    });
    
    // 3. UI interactions
    document.getElementById('btn-explorer').addEventListener('click', (e) => {
        const sb = document.getElementById('sidebar');
        sb.classList.toggle('hide-sidebar');
        if (window.innerWidth <= 768) {
            sb.classList.toggle('show');
        }
        e.currentTarget.classList.toggle('active', !sb.classList.contains('hide-sidebar'));
    });



    document.getElementById('menu-new-file').addEventListener('click', createNewFile);
    document.getElementById('menu-new-folder').addEventListener('click', createNewFolder);
    document.getElementById('menu-save').addEventListener('click', saveCurrentFile);
    document.getElementById('menu-autosave').addEventListener('click', () => {
        state.autoSaveEnabled = toggleAutoSave();
        saveSettings();
    });

    document.getElementById('btn-new-file').addEventListener('click', createNewFile);
    document.getElementById('btn-new-folder').addEventListener('click', createNewFolder);
    
    document.getElementById('explorer-tree').addEventListener('dblclick', (e) => {
        // @ts-ignore
        if (!e.target.closest('.tree-item')) {
            createNewFile();
        }
    });

    document.getElementById('btn-open-folder').addEventListener('click', openFolderModal);
    document.getElementById('menu-open-folder').addEventListener('click', () => {
        openFolderModal();
    });
    document.querySelector('.close-modal').addEventListener('click', closeFolderModal);
    document.getElementById('btn-cancel-folder').addEventListener('click', closeFolderModal);
    document.getElementById('btn-confirm-folder').addEventListener('click', confirmFolder);
    
    document.getElementById('folder-path-input').addEventListener('keydown', (e) => {
        // @ts-ignore
        if (e.key === 'Enter') {
            // @ts-ignore
            loadFolderPicker(e.target.value);
        }
    });

    document.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key === 's') {
            e.preventDefault();
            saveCurrentFile();
        }
        if ((e.ctrlKey || e.metaKey) && e.key === ',') {
            e.preventDefault();
            toggleTerminal();
        }
    });

    document.getElementById('btn-terminal').addEventListener('click', () => {
        const bottomPanel = document.getElementById('bottom-panel');
        const portsContainer = document.getElementById('ports-container');
        
        // If panel is hidden, always open it and show terminal
        if (bottomPanel.classList.contains('hidden')) {
            toggleTerminal();
        } else {
            // If panel is open but showing ports, just switch to terminal
            if (portsContainer && portsContainer.style.display === 'block') {
                // ports.js handles the switching logic via its own listener
            } else {
                // If panel is open and showing terminal, toggle it (hide)
                toggleTerminal();
            }
        }
    });
    
    document.getElementById('btn-add-terminal').addEventListener('click', () => {
        initTerminal(); // now creates a new terminal instance
    });
    
    initPortsPanel();
    
    // Panel Resizer logic
    const resizer = document.getElementById('panel-resizer');
    const bottomPanel = document.getElementById('bottom-panel');
    let isResizing = false;
    let startY, startHeight;
    
    const startPanelResize = (e) => {
        isResizing = true;
        startY = e.type.startsWith('touch') ? e.touches[0].clientY : e.clientY;
        startHeight = parseInt(document.defaultView.getComputedStyle(bottomPanel).height, 10);
        document.body.style.cursor = 'ns-resize';
        if (e.type === 'mousedown') e.preventDefault();
    };

    resizer.addEventListener('mousedown', startPanelResize);
    resizer.addEventListener('touchstart', (e) => {
        startPanelResize(e);
        if (e.cancelable) e.preventDefault();
    }, { passive: false });
    
    const doPanelResize = (clientY) => {
        if (!isResizing) return;
        const dy = startY - clientY;
        const newHeight = startHeight + dy;
        if (!bottomPanel.classList.contains('maximized')) {
            bottomPanel.style.height = `${newHeight}px`;
        }
    };

    window.addEventListener('mousemove', (e) => doPanelResize(e.clientY));
    window.addEventListener('touchmove', (e) => {
        if (isResizing && e.touches.length > 0) {
            if (e.cancelable) e.preventDefault();
            doPanelResize(e.touches[0].clientY);
        }
    }, { passive: false });
    
    const stopPanelResize = () => {
        if (isResizing) {
            isResizing = false;
            document.body.style.cursor = 'default';
            window.dispatchEvent(new Event('resize'));
        }
    };

    window.addEventListener('mouseup', stopPanelResize);
    window.addEventListener('touchend', stopPanelResize);
    window.addEventListener('touchcancel', stopPanelResize);
    
    // Maximize Panel logic
    document.getElementById('btn-maximize-panel').addEventListener('click', (e) => {
        bottomPanel.classList.toggle('maximized');
        const icon = e.currentTarget.querySelector('polyline');
        if (bottomPanel.classList.contains('maximized')) {
            icon.setAttribute('points', '6 9 12 15 18 9'); // down chevron
        } else {
            icon.setAttribute('points', '18 15 12 9 6 15'); // up chevron
        }
        setTimeout(() => window.dispatchEvent(new Event('resize')), 50);
    });

    document.getElementById('btn-minimize-panel').addEventListener('click', () => {
        toggleTerminal();
    });
    document.getElementById('btn-kill-terminal').addEventListener('click', killTerminal);
    
    // Context Menu Logic
    document.addEventListener('click', hideContextMenu);

    document.getElementById('sidebar').addEventListener('contextmenu', (e) => {
        e.preventDefault();
        e.stopPropagation();

        // @ts-ignore
        const itemDiv = e.target.closest('.tree-item');
        document.querySelectorAll('.tree-item').forEach(n => n.classList.remove('selected'));
        
        if (itemDiv) {
            itemDiv.classList.add('selected');
            state.contextMenuTarget = {
                path: itemDiv.dataset.path,
                isDir: itemDiv.dataset.isdir === 'true',
                name: itemDiv.querySelector('.tree-name').textContent
            };
        } else {
            state.contextMenuTarget = null;
        }
        
        const menu = document.getElementById('context-menu');
        menu.style.display = 'block';
        
        let x = e.clientX;
        let y = e.clientY;
        const rect = menu.getBoundingClientRect();
        if (x + rect.width > window.innerWidth) x = window.innerWidth - rect.width;
        if (y + rect.height > window.innerHeight) y = window.innerHeight - rect.height;
        
        menu.style.left = `${x}px`;
        menu.style.top = `${y}px`;
        
        const isFileOrFolder = state.contextMenuTarget ? 'block' : 'none';
        document.getElementById('ctx-cut').style.display = isFileOrFolder;
        document.getElementById('ctx-copy').style.display = isFileOrFolder;
        document.getElementById('ctx-copypath').style.display = isFileOrFolder;
        document.getElementById('ctx-copyrelpath').style.display = isFileOrFolder;
        document.getElementById('ctx-rename').style.display = isFileOrFolder;
        document.getElementById('ctx-delete').style.display = isFileOrFolder;
        document.getElementById('ctx-div-1').style.display = isFileOrFolder;
        document.getElementById('ctx-div-2').style.display = isFileOrFolder;
        
        // Show Live Server only for HTML files
        const isHtml = state.contextMenuTarget && !state.contextMenuTarget.isDir && state.contextMenuTarget.name.toLowerCase().endsWith('.html');
        const lsItem = document.getElementById('ctx-live-server');
        lsItem.style.display = isHtml ? 'block' : 'none';
        document.getElementById('ctx-div-live').style.display = isHtml ? 'block' : 'none';
        
        if (state.liveServerRunning) {
            lsItem.innerHTML = `<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" style="margin-right: 8px; vertical-align: middle;"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect></svg> Stop Live Server`;
        } else {
            lsItem.innerHTML = `<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" style="margin-right: 8px; vertical-align: middle;"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"></path></svg> Open with Live Server`;
        }
        
        const isFolderOrEmpty = (!state.contextMenuTarget || state.contextMenuTarget.isDir) ? 'block' : 'none';
        document.getElementById('ctx-new-file').style.display = isFolderOrEmpty;
        document.getElementById('ctx-new-folder').style.display = isFolderOrEmpty;
        document.getElementById('ctx-div-new').style.display = isFolderOrEmpty;
        
        const pasteOpt = document.getElementById('ctx-paste');
        if (state.clipboardOp.path && state.currentWorkspace) {
            pasteOpt.style.display = 'block';
        } else {
            pasteOpt.style.display = 'none';
        }
    });

    document.getElementById('ctx-new-file').addEventListener('click', createNewFile);
    document.getElementById('ctx-new-folder').addEventListener('click', createNewFolder);
    
    document.getElementById('ctx-live-server').addEventListener('click', async () => {
        if (state.contextMenuTarget && state.currentWorkspace) {
            if (state.liveServerRunning) {
                try {
                    await apiFetch('/live_server/stop');
                    state.liveServerRunning = false;
                } catch(e) {}
                return;
            }
            let filename = state.contextMenuTarget.path.replace(state.currentWorkspace, '');
            if (filename.startsWith('/') || filename.startsWith('\\')) filename = filename.substring(1);
            filename = filename.replace(/\\/g, '/');
            startLiveServer(state.currentWorkspace, filename);
        }
    });

    document.getElementById('ctx-cut').addEventListener('click', () => {
        if (state.contextMenuTarget) state.clipboardOp = { type: 'cut', path: state.contextMenuTarget.path };
    });
    document.getElementById('ctx-copy').addEventListener('click', () => {
        if (state.contextMenuTarget) state.clipboardOp = { type: 'copy', path: state.contextMenuTarget.path };
    });
    document.getElementById('ctx-copypath').addEventListener('click', () => {
        if (state.contextMenuTarget) navigator.clipboard.writeText(state.contextMenuTarget.path);
    });
    document.getElementById('ctx-copyrelpath').addEventListener('click', () => {
        if (state.contextMenuTarget && state.currentWorkspace) {
            let rel = state.contextMenuTarget.path.replace(state.currentWorkspace, '');
            if (rel.startsWith('/') || rel.startsWith('\\')) rel = rel.substring(1);
            navigator.clipboard.writeText(rel);
        }
    });

    document.getElementById('ctx-paste').addEventListener('click', async () => {
        if (!state.clipboardOp.path || !state.currentWorkspace) return;
        
        let targetPath = state.currentWorkspace;
        if (state.contextMenuTarget) {
            if (state.contextMenuTarget.isDir) {
                targetPath = state.contextMenuTarget.path;
            } else {
                let parts = state.contextMenuTarget.path.split(/[\/\\]/);
                parts.pop();
                targetPath = parts.join('/') || '/';
            }
        }
        
        const fromPath = state.clipboardOp.path;
        const filename = fromPath.split(/[\/\\]/).pop();
        const toPath = targetPath + (targetPath.endsWith('/') || targetPath.endsWith('\\') ? '' : '/') + filename;
        
        try {
            const action = state.clipboardOp.type === 'cut' ? 'rename' : 'copy';
            await apiFetch(`/${action}${fromPath}?to=${encodeURIComponent(toPath)}`, { method: 'POST' });
            if (state.clipboardOp.type === 'cut') state.clipboardOp = { type: null, path: null };
            refreshExplorer();
        } catch (e) {
            alert("Failed to paste: " + e.message);
        }
    });

    document.getElementById('ctx-rename').addEventListener('click', async () => {
        const target = state.contextMenuTarget;
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
            
            if (!target.isDir) {
                let tab = state.openTabs.find(t => t.path === target.path);
                if (tab) {
                    tab.path = newPath;
                    if (state.currentFile === target.path) state.currentFile = newPath;
                    renderTabs();
                }
            }
        } catch (e) {
            alert("Rename failed: " + e.message);
        }
    });

    document.getElementById('ctx-delete').addEventListener('click', async () => {
        const target = state.contextMenuTarget;
        if (!target) return;
        if (!confirm(`Are you sure you want to delete '${target.name}'?`)) return;
        
        try {
            await apiFetch(`/delete${target.path}`, { method: 'POST' });
            closeTab(target.path, true);
            refreshExplorer();
        } catch (e) {
            alert("Delete failed: " + e.message);
        }
    });
});
