export const state = {
    editor: null, // Monaco instance
    currentFile: null,
    openTabs: [], // { path: string, content: string, dirty: boolean, model: MonacoModel }
    currentWorkspace: '',
    autoSaveEnabled: false,
    autoSaveTimer: null,
    clipboardOp: { type: null, path: null }, // type: 'cut' | 'copy'
    liveServerRunning: false,
    contextMenuTarget: null, // target path for context menu
    
    // New nested settings structure
    editorSettings: {
        fontSize: 14,
        wordWrap: true,
        minimap: true,
        lineNumbers: true,
        tabSize: 4,
        theme: 'vs-dark'
    },
    terminalSettings: {
        fontSize: 14,
        cursorBlink: true,
        cursorStyle: 'block',
        scrollback: 5000,
        defaultShell: 'powershell'
    },
    layout: {
        activityBarWidth: 48,
        explorerWidth: 260,
        terminalHeight: 280,
        compactMode: false
    }
};
