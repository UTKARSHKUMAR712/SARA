// Utility to extract the SARA token from the URL or Cookies
export function getAuthToken() {
    const params = new URLSearchParams(window.location.search);
    let t = params.get('token');
    if (t) return t;
    const match = document.cookie.match(new RegExp('(^| )sara_token=([^;]+)'));
    if (match) return match[2];
    return '';
}

export async function apiFetch(url, options = {}) {
    const token = getAuthToken();
    let finalUrl = `/workspace/api${url}`;
    if (token) {
        finalUrl += finalUrl.includes('?') ? `&token=${token}` : `?token=${token}`;
    }
    const res = await fetch(finalUrl, options);
    if (!res.ok) {
        const errText = await res.text();
        throw new Error(`API Error ${res.status}: ${errText}`);
    }
    return res;
}
