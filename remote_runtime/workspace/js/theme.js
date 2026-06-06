import { state } from './state.js';

export async function applyTheme() {
    try {
        const themeName = state.theme || 'vs-dark';
        const res = await fetch(`./themes/${themeName}.json`);
        if (!res.ok) return;
        const themeData = await res.json();

        const root = document.documentElement;
        const overrides = state.themeOverrides || {};

        // Helper to resolve color with override
        const getColor = (component, property) => {
            if (overrides[component] && overrides[component][property]) {
                return overrides[component][property];
            }
            if (themeData.colors && themeData.colors[component] && themeData.colors[component][property]) {
                return themeData.colors[component][property];
            }
            return null; // fallback to CSS default if none specified
        };

        // Apply Activity Bar
        const abBg = getColor('activityBar', 'background');
        if (abBg) root.style.setProperty('--bg-activity', abBg);
        const abFg = getColor('activityBar', 'foreground');
        if (abFg) root.style.setProperty('--fg-activity', abFg);

        // Apply Explorer
        const exBg = getColor('explorer', 'background');
        if (exBg) root.style.setProperty('--bg-sidebar', exBg);
        const exFg = getColor('explorer', 'foreground');
        if (exFg) root.style.setProperty('--fg-sidebar', exFg);

        const isDark = themeData.type === 'dark';
        
        // Apply Editor (for UI surrounding monaco if needed)
        const edBg = getColor('editor', 'background');
        if (edBg) root.style.setProperty('--bg-base', edBg);
        const edFg = getColor('editor', 'foreground');
        if (edFg) root.style.setProperty('--fg-base', edFg);

        // Apply Terminal
        const tmBg = getColor('terminal', 'background');
        if (tmBg) root.style.setProperty('--bg-terminal', tmBg);
        const tmFg = getColor('terminal', 'foreground');
        if (tmFg) root.style.setProperty('--fg-terminal', tmFg);

        // Apply Tabs
        const tbActiveBg = getColor('tabs', 'activeBackground');
        if (tbActiveBg) root.style.setProperty('--bg-tab-active', tbActiveBg);
        const tbInactiveBg = getColor('tabs', 'inactiveBackground');
        if (tbInactiveBg) root.style.setProperty('--bg-tab', tbInactiveBg);
        const tbBorder = getColor('tabs', 'border');
        if (tbBorder) root.style.setProperty('--border-color', tbBorder);

        // Apply to Monaco
        if (typeof monaco !== 'undefined') {
            monaco.editor.defineTheme('sara-custom-theme', {
                base: isDark ? 'vs-dark' : 'vs',
                inherit: true,
                rules: themeData.monacoRules || [],
                colors: {
                    'editor.background': edBg || (isDark ? '#1e1e1e' : '#ffffff'),
                    'editor.foreground': edFg || (isDark ? '#cccccc' : '#000000')
                }
            });
            monaco.editor.setTheme('sara-custom-theme');
        }
    } catch (e) {
        console.error('Failed to apply theme:', e);
    }
}
