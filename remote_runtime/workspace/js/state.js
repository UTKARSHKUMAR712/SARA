export const state = {
    editor: null,
    currentFile: null,
    openTabs: [], // { path: string, content: string, dirty: boolean, model: MonacoModel }
    currentWorkspace: '',
    autoSaveEnabled: false,
    autoSaveTimer: null,
    clipboardOp: { type: null, path: null }, // type: 'cut' | 'copy'
    liveServerRunning: false,
    contextMenuTarget: null // target path for context menu
};
