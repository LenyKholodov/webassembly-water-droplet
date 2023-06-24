Instructions:
-------------

1. Install Emscripten:

    http://emscripten.org
    
3. Build

    ```make -j```

4. Open index.html in a web browser

    Chrome doesn't support file:// XHR requests, so you need to first start a webserver, e.g.:
    
    ```./run-webserver.sh```

    and then open this URL:

    http://localhost:8080/
