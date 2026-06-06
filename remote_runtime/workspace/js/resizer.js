import { state } from './state.js';
import { applyAllSettings, saveSettings } from './settings.js';

export function initSidebarResizer() {
    const resizer = document.getElementById('sidebar-resizer');
    if (!resizer) return;
    
    let isResizing = false;
    
    resizer.addEventListener('mousedown', (e) => {
        isResizing = true;
        document.body.style.cursor = 'ew-resize';
        e.preventDefault();
    });
    
    document.addEventListener('mousemove', (e) => {
        if (!isResizing) return;
        // Calculate new width: mouse X minus activity bar width
        let newWidth = e.clientX - state.layout.activityBarWidth;
        // Constrain width
        if (newWidth < 150) newWidth = 150;
        if (newWidth > 800) newWidth = 800;
        
        state.layout.explorerWidth = newWidth;
        applyAllSettings(); // Applies CSS variables instantly
    });
    
    document.addEventListener('mouseup', () => {
        if (isResizing) {
            isResizing = false;
            document.body.style.cursor = 'default';
            saveSettings();
        }
    });
}
