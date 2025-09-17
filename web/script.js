const ws = new WebSocket("ws://192.168.4.1/ws");
ws.binaryType = "arraybuffer";
const logBox = document.getElementById("logBox");

// On page load, restore any saved ESP logs
window.addEventListener("DOMContentLoaded", () => {
    const savedLogs = sessionStorage.getItem("esp32Logs");
    if (savedLogs) {
        // Split saved logs into lines and append spans
        savedLogs.split("\n").forEach(line => {
            if (!line) return;
            const span = document.createElement("span");
            if (line.startsWith('W') || line.includes(' W ')) {
                span.classList.add('warn');
            } else if (line.startsWith('E') || line.includes(' E ')) {
                span.classList.add('error');
            } else {
                span.classList.add('info');
            }
            span.textContent = line + "\n";
            logBox.appendChild(span);
        });
        logBox.scrollTop = logBox.scrollHeight;
    }
});

ws.onopen = () => {
    console.log("Connected to ESP32 WebSocket");
    const pageName = window.location.pathname.split("/").pop(); 
    ws.send(JSON.stringify({ type: "page_open", page: pageName }));
}

ws.onmessage = (event) => {
    const msg = event.data.trim();

    // Handle text packets first
    if (msg[1] === '|') {
        const parts = msg.split('|');
        const type = parts[0];

        if (type === 'c' && parts.length >= 10) {
            // Comms packet
            const parsed = {
                load: +parts[1],
                ctmp: +parts[2],
                rpm: +parts[3],
                spd: +parts[4],
                itmp: +parts[5],
                maf: +parts[6],
                trtl: +parts[7],
                attc: +parts[8],
                succ: +parts[9],
            };

            document.querySelector('#load .value').textContent    = parsed.load;
            document.querySelector('#trtl .value').textContent    = parsed.trtl;
            document.querySelector('#ctmp .value').textContent    = parsed.ctmp;
            document.querySelector('#itmp .value').textContent    = parsed.itmp;
            document.querySelector('#rpm .value').textContent     = parsed.rpm;
            document.querySelector('#spd .value').textContent     = parsed.spd;
            document.querySelector('#maf .value').textContent     = parsed.maf;
            document.querySelector('#success .value').textContent = `${parsed.succ}/${parsed.attc}`;
            return;
        }

        else if (type === 'd' && parts.length >= 11) {
            // Debug fuel packet
            const parsed = {
                ifl: parseFloat(parts[1]),
                afl: parseFloat(parts[2]),
                ctmp: +parts[3],
                cfl: parseFloat(parts[4]),
                rpm: +parts[5],
                spd: +parts[6],
                prpm: +parts[7],
                pisr: +parts[8],
                pd: parseFloat(parts[9]),
                pdpc: parseFloat(parts[10]),
            };

            document.querySelector('#inst-fuel .value').textContent = parsed.ifl.toFixed(1);
            document.querySelector('#avg-fuel .value').textContent  = parsed.afl.toFixed(1);
            document.querySelector('#ctmp .value').textContent      = parsed.ctmp;
            document.querySelector('#cons-fuel .value').textContent = parsed.cfl.toFixed(2);
            document.querySelector('#rpm .value').textContent       = parsed.rpm;
            document.querySelector('#spd .value').textContent       = parsed.spd;
            document.querySelector('#pcnt-rpm .value').textContent  = parsed.prpm;
            document.querySelector('#pcnt-isr .value').textContent  = parsed.pisr;
            document.querySelector('#pdelta .value').textContent    = parsed.pd;
            document.querySelector('#pdeltapc .value').textContent  = parsed.pdpc.toFixed(1);
            return;
        }

        else if (type === 'f' && parts.length >= 5) {
            // Fuel packet
            const parsed = {
                ifl: parseFloat(parts[1]),
                afl: parseFloat(parts[2]),
                ctmp: +parts[3],
                cfl: parseFloat(parts[4]),
            };

            document.querySelector('#inst-fuel .value').textContent = parsed.ifl.toFixed(1);
            document.querySelector('#avg-fuel .value').textContent  = parsed.afl.toFixed(1);
            document.querySelector('#ctmp .value').textContent      = parsed.ctmp;
            document.querySelector('#cons-fuel .value').textContent = parsed.cfl.toFixed(2);
            return;
        }
    }

    // Text/JSON case 
    let parsed;
    try {
        parsed = JSON.parse(msg);
    } catch (e) {
        parsed = null;
    }

    if (parsed && parsed.type === "filler") {
        // Do stuff

    } else if (parsed && parsed.type === "filler2") {
        // Do other stuff

    } else {
        // ESP32 console logs
        const logBox = document.getElementById('logBox');
        let logs = sessionStorage.getItem("esp32Logs") || "";
        logs += msg + "\n";
        sessionStorage.setItem("esp32Logs", logs);

        const span = document.createElement("span");
        if (msg.startsWith('W') || msg.includes(' W ')) {
            span.classList.add('warn');
        } else if (msg.startsWith('E') || msg.includes(' E ')) {
            span.classList.add('error');
        } else {
            span.classList.add('info');
        }
        span.textContent = msg + "\n";
        logBox.appendChild(span);
        logBox.scrollTop = logBox.scrollHeight;
    }
};

ws.onclose = () => console.log("WebSocket connection closed");
ws.onerror = (error) => console.error("WebSocket error:", error);

// In-page console logger
(function() {
    const oldLog = console.log;
    const logBoxDiv = document.getElementById("inPageConsole"); // static element

    if (logBoxDiv) { // only override console.log if the element exists
        logBoxDiv.style.cssText = "background:#111;color:#fa0;padding:10px;max-height:200px;overflow:auto;";

        console.log = function(...args) {
            oldLog.apply(console, args);
            logBoxDiv.textContent += args.join(" ") + "\n";
            logBoxDiv.scrollTop = logBoxDiv.scrollHeight;
        };
    }
})();

function downloadLog() {
    // Extract plain text (ignores colors)
    const text = logBox.innerText;

    // Create unique filename (timestamp based)
    const now = new Date();
    const timestamp = now.toISOString().replace(/[:.]/g, "-"); // safe filename
    const filename = "esp32_log_" + timestamp + ".txt";

    // Make blob & trigger download
    const blob = new Blob([text], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
}

