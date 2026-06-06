import { state } from './state.js';
import { applyAllSettings, saveSettings } from './settings.js';

export function initSidebarResizer() {
    const resizer = document.getElementById('sidebar-resizer');
    if (!resizer) return;
    
    let isResizing = false;
    
    const startResize = (e) => {
        isResizing = true;
        document.body.style.cursor = 'ew-resize';
        // Prevent default only for mouse events, not touch (can block scroll if not careful, but okay for resizer)
        if (e.type === 'mousedown') e.preventDefault();
    };

    resizer.addEventListener('mousedown', startResize);
    resizer.addEventListener('touchstart', (e) => {
        startResize(e);
        // e.preventDefault(); // Sometimes needed to prevent click/scroll
    }, { passive: true });
    
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
            doResize(e.touches[0].clientX);
        }
    }, { passive: true });
    
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
