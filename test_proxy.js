const http = require('http');
http.createServer((req, res) => {
    console.log(req.method, req.url);
    console.log(req.headers);
    res.end('ok');
}).listen(5173, () => console.log('Listening on 5173'));
