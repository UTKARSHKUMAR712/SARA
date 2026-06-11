import { state } from './state.js';
import { apiFetch } from './api.js';
import { IconRegistry } from './icons.js';
import { openFile } from './editor.js';

let gitState = {
    isRepo: false,
    git_installed: true,
    branch: '',
    summary: { staged: [], unstaged: [], unpushed: 0 },
    remote: [],
    commits: []
};

let lastGitStateJson = '';

async function fetchGitStatus() {
    if (!state.currentWorkspace) return;
    try {
        const res = await apiFetch(`/git/status?workspace=${encodeURIComponent(state.currentWorkspace)}&t=${Date.now()}`);
        if (res.ok) {
            const data = await res.json();
            gitState.isRepo = data.is_repo;
            if (data.git_installed !== undefined) {
                gitState.git_installed = data.git_installed;
            }
            if (data.is_repo) {
                gitState.branch = data.branch;
                gitState.summary = data.summary;
                gitState.remote = data.remote;
            }
        }
    } catch (e) {
        console.error('Git status error:', e);
    }
}

async function fetchGitLog() {
    if (!state.currentWorkspace || !gitState.isRepo) return;
    try {
        const res = await apiFetch(`/git/log?workspace=${encodeURIComponent(state.currentWorkspace)}&t=${Date.now()}`);
        if (res.ok) {
            gitState.commits = await res.json();
        }
    } catch (e) {
        console.error('Git log error:', e);
    }
}

window.gitAction = async (action, path) => {
    try {
        const res = await apiFetch(`/git/${action}?workspace=${encodeURIComponent(state.currentWorkspace)}&path=${encodeURIComponent(path)}`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            await refreshGit();
            if (action === 'restore') {
                window.dispatchEvent(new CustomEvent('git-restored', { detail: { path } }));
            }
        } else {
            alert(`Failed to ${action} file:\n` + data.message);
        }
    } catch (e) {
        alert("Error: " + e.message);
    }
};

export async function refreshGit() {
    await fetchGitStatus();
    if (gitState.isRepo) {
        await fetchGitLog();
    }
    renderGitView();
}

function renderGitView() {
    const container = document.getElementById('git-content');
    if (!state.currentWorkspace) {
        container.innerHTML = `<div style="padding: 15px; color: #8f8f8f; text-align: center;">No workspace open.</div>`;
        return;
    }

    if (!gitState.isRepo) {
        if (!gitState.git_installed) {
            container.innerHTML = `<div style="padding: 15px; color: #e2c08d; text-align: center; font-weight: 500;">Git is not installed or not found in PATH. Please install Git to use Source Control.</div>`;
        } else {
            container.innerHTML = `<div style="padding: 15px; color: #8f8f8f; text-align: center;">The current workspace is not a Git repository.</div>`;
        }
        return;
    }

    const currentStateJson = JSON.stringify(gitState);
    
    const hasStaged = gitState.summary.staged && gitState.summary.staged.length > 0;
    const hasUnstaged = gitState.summary.unstaged && gitState.summary.unstaged.length > 0;
    const hasChanges = hasStaged || hasUnstaged;
    const unpushedCount = gitState.summary.unpushed || 0;
    const hasUpstream = gitState.summary.has_upstream !== false;
    
    // Check if we need to update the DOM
    const oldInput = document.getElementById('git-commit-msg');
    const oldMsg = oldInput ? oldInput.value : '';
    const wasFocused = oldInput && document.activeElement === oldInput;
    const selStart = oldInput ? oldInput.selectionStart : 0;
    const selEnd = oldInput ? oldInput.selectionEnd : 0;

    if (currentStateJson === lastGitStateJson && oldInput) {
        return; // No state change, no need to re-render DOM
    }
    lastGitStateJson = currentStateJson;

    const renderFiles = (files, title, listId, isStaged) => {
        if (!files || files.length === 0) return '';
        
        const sectionAction = isStaged
            ? `<button class="git-file-action" onclick="event.stopPropagation(); window.gitAction('unstage', '.')" title="Unstage All Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="5" y1="12" x2="19" y2="12"></line></svg></button>`
            : `<button class="git-file-action" onclick="event.stopPropagation(); window.gitAction('restore', '.')" title="Discard All Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><polyline points="9 14 4 9 9 4"></polyline><path d="M20 20v-7a4 4 0 0 0-4-4H4"></path></svg></button>
               <button class="git-file-action" onclick="event.stopPropagation(); window.gitAction('add', '.')" title="Stage All Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg></button>`;

        let h = `
            <div class="git-changes-section" style="margin-top: 8px;">
                <div class="git-changes-header" onclick="document.getElementById('${listId}').classList.toggle('collapsed')">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" class="chevron"><polyline points="6 9 12 15 18 9"></polyline></svg>
                    <span style="font-weight: 600; font-size: 11px;">${title}</span>
                    <div class="git-file-actions" style="margin-left: auto;">${sectionAction}</div>
                    <span class="git-badge" style="margin-left: ${isStaged ? '4px' : 'auto'}; background: rgba(255,255,255,0.1); color: #ccc;">${files.length}</span>
                </div>
                <div id="${listId}" class="git-files-list">
        `;
        
        files.forEach(f => {
            const parts = f.path.split(/[\/\\]/);
            const fileName = parts.pop();
            const relPath = parts.join('/');
            const statusClass = f.status === 'M' ? 'modified' : (f.status === 'A' || f.status === 'U' ? 'added' : 'deleted');
            
            const actions = isStaged 
                ? `<button class="git-file-action" onclick="window.gitAction('unstage', '${f.path.replace(/\\/g, '\\\\').replace(/'/g, "\\'")}')" title="Unstage Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="5" y1="12" x2="19" y2="12"></line></svg></button>`
                : `<button class="git-file-action" onclick="window.gitAction('restore', '${f.path.replace(/\\/g, '\\\\').replace(/'/g, "\\'")}')" title="Discard Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><polyline points="9 14 4 9 9 4"></polyline><path d="M20 20v-7a4 4 0 0 0-4-4H4"></path></svg></button>
                   <button class="git-file-action" onclick="window.gitAction('add', '${f.path.replace(/\\/g, '\\\\').replace(/'/g, "\\'")}')" title="Stage Changes"><svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg></button>`;
            
            h += `
                <div class="git-file-item" data-path="${f.path}">
                    <span class="git-file-icon">${IconRegistry.getFileIcon(fileName)}</span>
                    <span class="git-file-name" style="${f.status === 'D' ? 'text-decoration: line-through; opacity: 0.7;' : ''}">${fileName}</span>
                    <span class="git-file-path">${relPath}</span>
                    <span class="git-file-status status-${statusClass}">${f.status}</span>
                    <div class="git-file-actions">${actions}</div>
                </div>
            `;
        });
        h += `</div></div>`;
        return h;
    };

    let filesHtml = renderFiles(gitState.summary.staged, 'Staged Changes', 'git-staged-list', true);
    filesHtml += renderFiles(gitState.summary.unstaged, 'Changes', 'git-unstaged-list', false);

    let html = `
        <div class="git-panel-header">
            <div style="display: flex; gap: 8px; align-items: center;">
                <div class="git-branch-info" title="Current Branch">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="6" y1="3" x2="6" y2="15"></line><circle cx="18" cy="6" r="3"></circle><circle cx="6" cy="18" r="3"></circle><path d="M18 9a9 9 0 0 1-9 9"></path></svg>
                    ${gitState.branch || 'None'}
                </div>
                ${gitState.remote && gitState.remote.length > 0 ? `
                <div style="color: #858585; font-size: 11px; padding: 2px 6px; background: rgba(255,255,255,0.05); border-radius: 3px;" title="${gitState.remote[0].url}">
                    ${gitState.remote[0].name}
                </div>` : ''}
            </div>
            <div style="display: flex; gap: 2px;">
                <button class="git-icon-btn" id="btn-git-switch-branch" title="Switch Branch">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><polyline points="16 18 22 12 16 6"></polyline><polyline points="8 6 2 12 8 18"></polyline></svg>
                </button>
                <button class="git-icon-btn" id="btn-git-create-branch" title="New Branch">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
                </button>
            </div>
        </div>

        <div class="git-commit-box">
            <textarea class="git-input" id="git-commit-msg" placeholder="Message (Enter to commit)"></textarea>
            <div class="git-action-row">
                <button class="git-btn primary" id="btn-git-commit" ${!hasStaged ? 'disabled' : ''}>Commit</button>
                <button class="git-sync-btn" id="btn-git-pull" title="Pull">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><polyline points="19 12 12 19 5 12"></polyline></svg>
                </button>
                <button class="git-sync-btn" id="btn-git-push" title="Push">
                    <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="19" x2="12" y2="5"></line><polyline points="5 12 12 5 19 12"></polyline></svg>
                </button>
            </div>
        </div>

        <div class="git-summary-card">
            ${!hasUpstream ? `
            <div class="git-info-row">
                <span class="git-info-label">Upstream</span>
                <span style="color: #8f8f8f; font-size: 11px;">No upstream configured</span>
            </div>` : (unpushedCount > 0 ? `
            <div class="git-info-row">
                <span class="git-info-label">Unpushed</span>
                <span style="color: #e2c08d; font-size: 11px; font-weight: 500;">↑ ${unpushedCount} commits not pushed</span>
            </div>` : '')}
            
            ${gitState.remote && gitState.remote.length > 0 ? gitState.remote.map(r => `
            <div class="git-info-row" style="margin-top: ${!hasUpstream || unpushedCount > 0 ? '6px' : '0'}; flex-direction: column; align-items: flex-start; gap: 4px;">
                <span class="git-info-label">Remote: ${r.name}</span>
                <span class="git-info-value" style="font-size: 11px; color: #858585; word-break: break-all; text-align: left;">${r.url}</span>
            </div>`).join('') : ''}
        </div>

        ${hasChanges ? filesHtml : '<div style="padding: 10px 15px; color: #81b88b; font-size: 11px;">Clean ✓</div>'}


        <div class="git-section" style="margin-top: 10px;">
            <div class="git-section-title">Recent Commits</div>
            <div class="git-commit-list">
    `;

    if (gitState.commits && gitState.commits.length > 0) {
        gitState.commits.forEach(c => {
            html += `
                <div class="git-commit-item">
                    <div class="git-commit-msg" title="${c.message}">${c.message}</div>
                    <div class="git-commit-meta">
                        <span>${c.author} • ${c.date}</span>
                        <span class="git-commit-hash" onclick="navigator.clipboard.writeText('${c.hash}')" title="Copy to clipboard">${c.hash.substring(0, 7)}</span>
                    </div>
                </div>
            `;
        });
    } else {
        html += `<div style="color: #8f8f8f; font-size: 11px; font-style: italic; padding-left: 5px;">No commits yet.</div>`;
    }

    html += `
            </div>
        </div>
    `;

    container.innerHTML = html;

    const btnCommit = document.getElementById('btn-git-commit');
    const msgInput = document.getElementById('git-commit-msg');

    if (btnCommit && msgInput) {
        if (oldMsg) {
            msgInput.value = oldMsg;
        }
        if (wasFocused) {
            msgInput.focus();
            msgInput.setSelectionRange(selStart, selEnd);
        }

        btnCommit.addEventListener('click', handleCommit);
        
        const updateCommitBtn = () => {
            if (!hasStaged) {
                btnCommit.disabled = true;
                btnCommit.title = "No staged changes to commit";
            } else if (msgInput.value.trim() === '') {
                btnCommit.disabled = true;
                btnCommit.title = "Enter a commit message";
            } else {
                btnCommit.disabled = false;
                btnCommit.title = "Commit staged changes";
            }
        };
        msgInput.addEventListener('input', updateCommitBtn);
        updateCommitBtn();

        msgInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                if (!btnCommit.disabled) {
                    handleCommit();
                }
            }
        });
    }

    const btnPush = document.getElementById('btn-git-push');
    if (btnPush) btnPush.addEventListener('click', handlePush);

    const btnPull = document.getElementById('btn-git-pull');
    if (btnPull) btnPull.addEventListener('click', handlePull);

    const btnCreateBranch = document.getElementById('btn-git-create-branch');
    if (btnCreateBranch) btnCreateBranch.addEventListener('click', handleCreateBranch);

    const btnSwitchBranch = document.getElementById('btn-git-switch-branch');
    if (btnSwitchBranch) btnSwitchBranch.addEventListener('click', handleSwitchBranch);
}

async function handleCommit() {
    const msgInput = document.getElementById('git-commit-msg');
    const message = msgInput.value.trim();
    if (!message) {
        alert("Please enter a commit message.");
        msgInput.focus();
        return;
    }
    
    document.getElementById('btn-git-commit').textContent = "Committing...";
    document.getElementById('btn-git-commit').disabled = true;

    try {
        const res = await apiFetch(`/git/commit?workspace=${encodeURIComponent(state.currentWorkspace)}&message=${encodeURIComponent(message)}`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            msgInput.value = '';
            await refreshGit();
        } else {
            alert("Commit failed:\n" + data.message);
        }
    } catch (e) {
        alert("Commit error: " + e.message);
    }
}

async function handlePush() {
    const btn = document.getElementById('btn-git-push');
    btn.textContent = "Pushing...";
    btn.disabled = true;
    try {
        const res = await apiFetch(`/git/push?workspace=${encodeURIComponent(state.currentWorkspace)}`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            await refreshGit();
        } else {
            alert("Push failed:\n" + data.message);
        }
    } catch (e) {
        alert("Push error: " + e.message);
    }
    btn.textContent = "Push";
    btn.disabled = false;
}

async function handlePull() {
    const btn = document.getElementById('btn-git-pull');
    btn.textContent = "Pulling...";
    btn.disabled = true;
    try {
        const res = await apiFetch(`/git/pull?workspace=${encodeURIComponent(state.currentWorkspace)}`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            await refreshGit();
            window.dispatchEvent(new CustomEvent('workspace-files-changed'));
        } else {
            alert("Pull failed:\n" + data.message);
        }
    } catch (e) {
        alert("Pull error: " + e.message);
    }
    btn.textContent = "Pull";
    btn.disabled = false;
}

async function handleCreateBranch() {
    const name = prompt("Enter new branch name:");
    if (!name) return;
    try {
        const res = await apiFetch(`/git/branch?workspace=${encodeURIComponent(state.currentWorkspace)}&name=${encodeURIComponent(name)}&create=true`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            await refreshGit();
        } else {
            alert("Failed to create branch:\n" + data.message);
        }
    } catch (e) {
        alert("Error: " + e.message);
    }
}

async function handleSwitchBranch() {
    const name = prompt("Enter branch name to switch to:");
    if (!name) return;
    try {
        const res = await apiFetch(`/git/branch?workspace=${encodeURIComponent(state.currentWorkspace)}&name=${encodeURIComponent(name)}&create=false`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
            await refreshGit();
            window.dispatchEvent(new CustomEvent('workspace-files-changed'));
        } else {
            alert("Failed to switch branch:\n" + data.message);
        }
    } catch (e) {
        alert("Error: " + e.message);
    }
}

export function initGit() {
    document.getElementById('btn-git-refresh').addEventListener('click', async () => {
        const icon = document.querySelector('#btn-git-refresh svg');
        icon.style.opacity = '0.5';
        await refreshGit();
        icon.style.opacity = '1';
    });
}
