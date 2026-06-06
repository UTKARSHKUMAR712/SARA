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
        lineDecorationsWidth: 10
    },
    terminalSettings: {
        fontSize: 14,
        cursorBlink: true,
        cursorStyle: 'block',
        scrollback: 5000,
        defaultShell: 'powershell'
    },
    layout: {
        activityBarWidth: 36,
        explorerWidth: 200,
        tabHeight: 35,
        tabPadding: 10,
        terminalHeight: 280,
        compactMode: false
    },
    theme: 'vs-dark',
    themeOverrides: {
        activityBar: {},
        explorer: {},
        editor: {},
        terminal: {},
        tabs: {}
    }
};
