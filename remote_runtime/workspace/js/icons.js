export const IconRegistry = {
    getFileIcon(filename) {
        const defaultFile = `<img src="./vendor/material-icons/document.svg" class="tree-icon" />`;
        if (!filename || !filename.includes('.')) return defaultFile;
        
        const ext = filename.split('.').pop().toLowerCase();
        
        const mappings = {
            'cpp': 'cpp.svg',
            'c': 'cpp.svg',
            'hpp': 'hpp.svg',
            'h': 'h.svg',
            'py': 'python.svg',
            'js': 'javascript.svg',
            'jsx': 'react.svg',
            'ts': 'typescript.svg',
            'tsx': 'react_ts.svg',
            'html': 'html.svg',
            'htm': 'html.svg',
            'css': 'css.svg',
            'json': 'json.svg',
            'md': 'markdown.svg',
            'txt': 'document.svg',
            'map': 'javascript-map.svg'
        };

        if (mappings[ext]) {
            return `<img src="./vendor/material-icons/${mappings[ext]}" class="tree-icon" />`;
        }
        
        return defaultFile;
    },

    getFolderIcon(foldername, isOpen) {
        const folderNameLower = foldername.toLowerCase();
        const suffix = isOpen ? '-open.svg' : '.svg';
        
        const mappings = {
            'src': 'folder-src',
            'assets': 'folder', // Fallback to generic folder for now
            'public': 'folder-public',
            'docs': 'folder-docs',
            'images': 'folder-images',
            'config': 'folder-config'
        };

        const iconName = mappings[folderNameLower] || 'folder';
        return `<img src="./vendor/material-icons/${iconName}${suffix}" class="tree-icon" />`;
    }
};
