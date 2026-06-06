import { state } from './state.js';
import { apiFetch } from './api.js';
import { updateEditorOptions } from './editor.js';
import { updateTerminalFontSize } from './terminal.js';

export async function saveSettings() {
    try {
        await apiFetch('/settings', {
            method: 'POST',
            body: JSON.stringify({
                currentWorkspace: state.currentWorkspace,
                autoSaveEnabled: state.autoSaveEnabled,
                editor: state.editorSettings,
                terminal: state.terminalSettings,
                layout: state.layout
            })
        });
    } catch (e) {
        console.error("Failed to save settings:", e);
    }
}

export function applyAllSettings() {
    // 1. Apply Layout / CSS Variables
    const root = document.documentElement;
    root.style.setProperty('--activity-bar-width', `${state.layout.activityBarWidth}px`);
    root.style.setProperty('--sidebar-width', `${state.layout.explorerWidth}px`);
    
    // We could bind terminal height but right now it's flex-based or dragged, but if we need a set height:
    // root.style.setProperty('--terminal-height', `${state.layout.terminalHeight}px`);

    if (state.layout.compactMode) {
        document.body.classList.add('compact-mode');
    } else {
        document.body.classList.remove('compact-mode');
    }

    // 2. Apply Editor Settings
    updateEditorOptions();

    // 3. Apply Terminal Settings
    updateTerminalFontSize(state.terminalSettings.fontSize);
}

import { openSettingsTab } from './editor.js';

export function initSettingsUI() {
    const btnSettings = document.getElementById('btn-settings');
    if (btnSettings) {
        btnSettings.addEventListener('click', () => {
            populateSettingsUI();
            openSettingsTab();
        });
    }

    // Tab Switching (Scrolling)
    const treeItems = document.querySelectorAll('.settings-tree-item');
    treeItems.forEach(item => {
        item.addEventListener('click', () => {
            treeItems.forEach(t => t.classList.remove('active'));
            item.classList.add('active');
            const targetId = item.getAttribute('data-target');
            const targetEl = document.getElementById(targetId);
            if (targetEl) {
                targetEl.scrollIntoView({ behavior: 'smooth' });
            }
        });
    });

    // Search Filtering
    const searchInput = document.getElementById('settings-search');
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            const term = e.target.value.toLowerCase();
            const blocks = document.querySelectorAll('.setting-block');
            
            blocks.forEach(block => {
                const keywords = block.getAttribute('data-keywords') || '';
                const name = block.querySelector('.setting-name').textContent.toLowerCase();
                const desc = block.querySelector('.setting-desc').textContent.toLowerCase();
                
                if (term === '' || keywords.includes(term) || name.includes(term) || desc.includes(term)) {
                    block.style.display = 'flex';
                } else {
                    block.style.display = 'none';
                }
            });
        });
    }

    // --- Bind Inputs to State & Trigger Apply ---
    const bindInput = (id, stateObj, stateKey, parser) => {
        const el = document.getElementById(id);
        if (!el) return;
        el.addEventListener('change', (e) => {
            let val = e.target.type === 'checkbox' ? e.target.checked : e.target.value;
            if (parser) val = parser(val);
            state[stateObj][stateKey] = val;
            applyAllSettings();
            saveSettings();
        });
    };

    // General
    // General / Workspace
    const handleAutosave = (e) => {
        state.autoSaveEnabled = e.target.checked;
        document.getElementById('autosave-check').textContent = state.autoSaveEnabled ? '✓' : '';
        // Sync both checkboxes
        const a1 = document.getElementById('setting-autosave');
        const a2 = document.getElementById('setting-autosave-workspace');
        if (a1) a1.checked = state.autoSaveEnabled;
        if (a2) a2.checked = state.autoSaveEnabled;
        saveSettings();
    };

    const autoSaveEl1 = document.getElementById('setting-autosave');
    if (autoSaveEl1) autoSaveEl1.addEventListener('change', handleAutosave);
    
    const autoSaveEl2 = document.getElementById('setting-autosave-workspace');
    if (autoSaveEl2) autoSaveEl2.addEventListener('change', handleAutosave);

    bindInput('setting-compact-mode', 'layout', 'compactMode', null);
    bindInput('setting-activity-bar', 'layout', 'activityBarWidth', parseInt);
    bindInput('setting-explorer-width', 'layout', 'explorerWidth', parseInt);

    // Editor
    // Editor
    bindInput('setting-editor-font', 'editorSettings', 'fontSize', parseInt);
    bindInput('setting-editor-wrap', 'editorSettings', 'wordWrap', null);
    bindInput('setting-editor-minimap', 'editorSettings', 'minimap', null);
    bindInput('setting-editor-lines', 'editorSettings', 'lineNumbers', null);
    bindInput('setting-editor-tab', 'editorSettings', 'tabSize', parseInt);
    bindInput('setting-editor-theme', 'editorSettings', 'theme', null);

    // Terminal
    // Terminal
    bindInput('setting-term-font', 'terminalSettings', 'fontSize', parseInt);
    bindInput('setting-term-cursor', 'terminalSettings', 'cursorStyle', null);
    bindInput('setting-term-blink', 'terminalSettings', 'cursorBlink', null);
    bindInput('setting-term-scroll', 'terminalSettings', 'scrollback', parseInt);
    bindInput('setting-term-shell', 'terminalSettings', 'defaultShell', null);
}

function populateSettingsUI() {
    const setVal = (id, val) => {
        const el = document.getElementById(id);
        if (!el) return;
        if (el.type === 'checkbox') el.checked = val;
        else el.value = val;
    };

    setVal('setting-autosave', state.autoSaveEnabled);
    setVal('setting-autosave-workspace', state.autoSaveEnabled);
    setVal('setting-compact-mode', state.layout.compactMode);
    setVal('setting-activity-bar', state.layout.activityBarWidth);
    setVal('setting-explorer-width', state.layout.explorerWidth);

    setVal('setting-editor-font', state.editorSettings.fontSize);
    setVal('setting-editor-wrap', state.editorSettings.wordWrap);
    setVal('setting-editor-minimap', state.editorSettings.minimap);
    setVal('setting-editor-lines', state.editorSettings.lineNumbers);
    setVal('setting-editor-tab', state.editorSettings.tabSize);
    setVal('setting-editor-theme', state.editorSettings.theme);

    setVal('setting-term-font', state.terminalSettings.fontSize);
    setVal('setting-term-cursor', state.terminalSettings.cursorStyle);
    setVal('setting-term-blink', state.terminalSettings.cursorBlink);
    setVal('setting-term-scroll', state.terminalSettings.scrollback);
    setVal('setting-term-shell', state.terminalSettings.defaultShell);
}
