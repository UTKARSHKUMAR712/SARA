import { state } from './state.js';
import { apiFetch } from './api.js';
import { openFile } from './editor.js';
import { IconRegistry } from './icons.js';

export function initSearch() {
    const searchInput = document.getElementById('search-input');
    const searchResults = document.getElementById('search-results');

    searchInput.addEventListener('keydown', async (e) => {
        if (e.key === 'Enter') {
            const q = searchInput.value.trim();
            if (!q) {
                searchResults.innerHTML = '';
                return;
            }
            
            searchResults.innerHTML = '<div style="padding: 10px; color: var(--text-muted);">Searching...</div>';
            
            try {
                const dir = encodeURIComponent(state.currentWorkspace);
                const query = encodeURIComponent(q);
                const res = await apiFetch(`/search_code?q=${query}&dir=${dir}`);
                const matches = await res.json();
                
                renderResults(matches);
            } catch (error) {
                searchResults.innerHTML = `<div style="padding: 10px; color: var(--error-color);">Search failed: ${error.message}</div>`;
            }
        }
    });

    function renderResults(matches) {
        if (matches.length === 0) {
            searchResults.innerHTML = '<div style="padding: 10px; color: var(--text-muted);">No results found.</div>';
            return;
        }

        // Group by file
        const grouped = {};
        for (const m of matches) {
            if (!grouped[m.file]) grouped[m.file] = [];
            grouped[m.file].push(m);
        }

        searchResults.innerHTML = '';

        for (const [file, fileMatches] of Object.entries(grouped)) {
            const groupDiv = document.createElement('div');
            groupDiv.className = 'search-file-group';
            
            const header = document.createElement('div');
            header.className = 'search-file-header';
            
            // Get icon
            const parts = file.split('/');
            const filename = parts[parts.length - 1];
            const iconSvg = IconRegistry.getFileIcon(filename);
            
            header.innerHTML = `${iconSvg} <span>${file}</span> <span style="margin-left:auto; font-size:11px; opacity:0.6; font-weight:normal;">${fileMatches.length}</span>`;
            
            // Toggle visibility of matches when header is clicked
            let isOpen = true;
            header.addEventListener('click', () => {
                isOpen = !isOpen;
                matchesContainer.style.display = isOpen ? 'block' : 'none';
            });
            
            const matchesContainer = document.createElement('div');
            matchesContainer.className = 'search-matches';
            
            for (const m of fileMatches) {
                const item = document.createElement('div');
                item.className = 'search-match-item';
                
                // Highlight the match
                const idx = m.content.toLowerCase().indexOf(searchInput.value.trim().toLowerCase());
                let displayContent = m.content;
                if (idx !== -1) {
                    const qlen = searchInput.value.trim().length;
                    displayContent = m.content.substring(0, idx) + 
                        `<span class="search-match-highlight">${m.content.substring(idx, idx + qlen)}</span>` + 
                        m.content.substring(idx + qlen);
                }
                
                // Escape HTML but keep highlight
                // Simple trick: escape all then replace the highlight marker back
                const tempDiv = document.createElement('div');
                tempDiv.textContent = m.content;
                let escaped = tempDiv.innerHTML;
                const safeQ = searchInput.value.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                const reg = new RegExp(`(${safeQ})`, 'gi');
                escaped = escaped.replace(reg, '<span class="search-match-highlight">$1</span>');
                
                item.innerHTML = `<span class="search-match-line-num">${m.line}</span> <span class="search-match-text">${escaped}</span>`;
                
                item.addEventListener('click', async () => {
                    const absPath = state.currentWorkspace + (state.currentWorkspace.endsWith('/') || state.currentWorkspace.endsWith('\\') ? '' : '/') + file;
                    await openFile(absPath);
                    if (state.editor) {
                        state.editor.revealLineInCenter(m.line);
                        const q = searchInput.value.trim();
                        const idx = m.content.toLowerCase().indexOf(q.toLowerCase());
                        if (idx !== -1) {
                            state.editor.setSelection({
                                startLineNumber: m.line,
                                startColumn: idx + 1,
                                endLineNumber: m.line,
                                endColumn: idx + 1 + q.length
                            });
                        } else {
                            state.editor.setPosition({lineNumber: m.line, column: 1});
                        }
                        state.editor.focus();
                    }
                });
                
                matchesContainer.appendChild(item);
            }
            
            groupDiv.appendChild(header);
            groupDiv.appendChild(matchesContainer);
            searchResults.appendChild(groupDiv);
        }
    }
}
