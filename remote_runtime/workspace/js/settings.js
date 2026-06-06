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
                editor: state.editor,
                terminal: state.terminal,
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
    updateTerminalFontSize(state.terminal.fontSize);
}

export function initSettingsUI() {
    const btnSettings = document.getElementById('btn-settings');
    const modal = document.getElementById('settings-modal');
    const btnClose = document.getElementById('btn-close-settings');

    if (!btnSettings || !modal) return;

    // Open/Close Modal
    btnSettings.addEventListener('click', () => {
        populateSettingsUI();
        modal.style.display = 'flex';
    });

    btnClose.addEventListener('click', () => {
        modal.style.display = 'none';
    });

    // Tab Switching
    const tabs = document.querySelectorAll('.settings-tab');
    const panels = document.querySelectorAll('.settings-panel');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            panels.forEach(p => p.classList.remove('active'));
            tab.classList.add('active');
            document.getElementById(tab.dataset.target).classList.add('active');
        });
    });

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
    bindInput('setting-autosave', '', 'autoSaveEnabled', null);
    // Since autoSaveEnabled is directly on state, not state.editor, we handle it specially:
    const autoSaveEl = document.getElementById('setting-autosave');
    if (autoSaveEl) {
        autoSaveEl.addEventListener('change', (e) => {
            state.autoSaveEnabled = e.target.checked;
            document.getElementById('autosave-check').textContent = state.autoSaveEnabled ? '✓' : '';
            saveSettings();
        });
    }

    bindInput('setting-compact-mode', 'layout', 'compactMode', null);
    bindInput('setting-activity-bar', 'layout', 'activityBarWidth', parseInt);
    bindInput('setting-explorer-width', 'layout', 'explorerWidth', parseInt);

    // Editor
    bindInput('setting-editor-font', 'editor', 'fontSize', parseInt);
    bindInput('setting-editor-wrap', 'editor', 'wordWrap', null);
    bindInput('setting-editor-minimap', 'editor', 'minimap', null);
    bindInput('setting-editor-lines', 'editor', 'lineNumbers', null);
    bindInput('setting-editor-tab', 'editor', 'tabSize', parseInt);
    bindInput('setting-editor-theme', 'editor', 'theme', null);

    // Terminal
    bindInput('setting-term-font', 'terminal', 'fontSize', parseInt);
    bindInput('setting-term-cursor', 'terminal', 'cursorStyle', null);
    bindInput('setting-term-blink', 'terminal', 'cursorBlink', null);
    bindInput('setting-term-scroll', 'terminal', 'scrollback', parseInt);
    bindInput('setting-term-shell', 'terminal', 'defaultShell', null);
}

function populateSettingsUI() {
    const setVal = (id, val) => {
        const el = document.getElementById(id);
        if (!el) return;
        if (el.type === 'checkbox') el.checked = val;
        else el.value = val;
    };

    setVal('setting-autosave', state.autoSaveEnabled);
    setVal('setting-compact-mode', state.layout.compactMode);
    setVal('setting-activity-bar', state.layout.activityBarWidth);
    setVal('setting-explorer-width', state.layout.explorerWidth);

    setVal('setting-editor-font', state.editor.fontSize);
    setVal('setting-editor-wrap', state.editor.wordWrap);
    setVal('setting-editor-minimap', state.editor.minimap);
    setVal('setting-editor-lines', state.editor.lineNumbers);
    setVal('setting-editor-tab', state.editor.tabSize);
    setVal('setting-editor-theme', state.editor.theme);

    setVal('setting-term-font', state.terminal.fontSize);
    setVal('setting-term-cursor', state.terminal.cursorStyle);
    setVal('setting-term-blink', state.terminal.cursorBlink);
    setVal('setting-term-scroll', state.terminal.scrollback);
    setVal('setting-term-shell', state.terminal.defaultShell);
}
