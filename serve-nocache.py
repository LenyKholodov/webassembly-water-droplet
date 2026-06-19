#!/usr/bin/env python3
# Serves dist/ with aggressive no-cache headers so a page reload always picks up
# a fresh build (Cursor's web view otherwise caches index.wasm/index.js hard).
import http.server
import socketserver
import os
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8090
# host: pass "0.0.0.0" (default) to expose on the LAN (mobile testing), or "127.0.0.1" for localhost only
HOST = sys.argv[2] if len(sys.argv) > 2 else "0.0.0.0"
DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dist")


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".data": "application/octet-stream",
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    with Server((HOST, PORT), NoCacheHandler) as httpd:
        print(f"no-cache server serving {DIRECTORY} at http://{HOST}:{PORT}/ (reachable on the LAN if HOST=0.0.0.0)")
        httpd.serve_forever()
