const http = require('http');
const fs = require('fs');
const path = require('path');
const port = parseInt(process.argv[2]) || 3456;
const dir = path.join(__dirname);
http.createServer((req, res) => {
  const file = req.url === '/' ? '/reader-menu-c6-1to1.html' : req.url;
  const fp = path.join(dir, file);
  fs.readFile(fp, (err, data) => {
    if (err) { res.writeHead(404); res.end('Not found'); return; }
    const ext = path.extname(fp);
    const ct = ext === '.html' ? 'text/html' : ext === '.css' ? 'text/css' : 'text/plain';
    res.writeHead(200, {'Content-Type': ct + '; charset=utf-8'});
    res.end(data);
  });
}).listen(port, () => console.log(`Serving on http://localhost:${port}`));
