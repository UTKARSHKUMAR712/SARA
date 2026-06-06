import { state } from './state.js';
import { applyAllSettings, saveSettings } from './settings.js';

export function initSidebarResizer() {
    const resizer = document.getElementById('sidebar-resizer');
    if (!resizer) return;
    
    let isResizing = false;
    
    const startResize = (e) => {
        isResizing = true;
        document.body.style.cursor = 'ew-resize';
        if (e.type === 'mousedown') e.preventDefault();
    };

    resizer.addEventListener('mousedown', startResize);
    resizer.addEventListener('touchstart', (e) => {
        startResize(e);
        if (e.cancelable) e.preventDefault(); // Prevent click/scroll
    }, { passive: false });
    
    const doResize = (clientX) => {
        if (!isResizing) return;
        let newWidth = clientX - state.layout.activityBarWidth;
        if (newWidth < 150) newWidth = 150;
        if (newWidth > 800) newWidth = 800;
        
        state.layout.explorerWidth = newWidth;
        applyAllSettings();
    };

    document.addEventListener('mousemove', (e) => doResize(e.clientX));
    document.addEventListener('touchmove', (e) => {
        if (isResizing && e.touches.length > 0) {
            if (e.cancelable) e.preventDefault();
            doResize(e.touches[0].clientX);
        }
    }, { passive: false });
    
    const stopResize = () => {
        if (isResizing) {
            isResizing = false;
            document.body.style.cursor = 'default';
            saveSettings();
        }
    };

    document.addEventListener('mouseup', stopResize);
    document.addEventListener('touchend', stopResize);
    document.addEventListener('touchcancel', stopResize);
}
