// 统一 API 封装：自动携带 JWT，统一处理业务错误码
const API = {
    getToken() {
        return localStorage.getItem('token');
    },
    setToken(token) {
        localStorage.setItem('token', token);
    },
    clearToken() {
        localStorage.removeItem('token');
        localStorage.removeItem('user');
    },
    async request(url, options = {}) {
        const headers = { ...(options.headers || {}) };
        if (options.body && typeof options.body === 'string') {
            headers['Content-Type'] = 'application/json';
        }
        const token = this.getToken();
        if (token) {
            headers['Authorization'] = 'Bearer ' + token;
        }
        const resp = await fetch(url, { ...options, headers });
        let data;
        try {
            data = await resp.json();
        } catch (e) {
            throw new Error('服务器响应异常');
        }
        if (data.code !== 0) {
            if (data.code === 401) {
                this.clearToken();
                window.location.href = '/login';
            }
            throw new Error(data.message || '请求失败');
        }
        return data.data;
    },
    post(url, body) {
        return this.request(url, { method: 'POST', body: JSON.stringify(body) });
    },
    get(url) {
        return this.request(url, { method: 'GET' });
    },
    del(url) {
        return this.request(url, { method: 'DELETE' });
    }
};

// 字节数格式化
function formatBytes(bytes) {
    if (bytes == null) return '-';
    if (bytes === 0) return '0 B';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return (bytes / Math.pow(1024, i)).toFixed(i === 0 ? 0 : 1) + ' ' + units[i];
}
