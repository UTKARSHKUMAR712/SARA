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
