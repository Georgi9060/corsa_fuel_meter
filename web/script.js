const ws = new WebSocket("ws://192.168.4.1/ws");
ws.binaryType = "arraybuffer";
const logBox = document.getElementById("logBox");

/* Currency toggle */
const toggleCurrency = document.getElementById('toggleCurrency');
const priceInput = document.getElementById('priceInput');

let latestFuelParsed = {
    ifl: 0.0,
    afl: 0.0,
    ctmp: 0,
    cfl: 0.0,
    fl6: 0.0,
    fl60: 0.0,
    disttr: 0.0,
    barop: 0.0,
};

function updateFuelDisplay() {
    const price = parseFloat(priceInput.value) || 0;
    const inCurrency = toggleCurrency.checked;

    function setCell(id, valueText, unitText) {
        const cell = document.querySelector(`#${id}`);
        if (!cell) return;
        cell.querySelector('.value').textContent = valueText;
        cell.querySelector('.unit').textContent = unitText;
    }

    if (inCurrency) {
        setCell("inst-fuel", (latestFuelParsed.ifl * price).toFixed(2), "BGN/100 km");
        setCell("avg-fuel", (latestFuelParsed.afl * price).toFixed(2), "BGN/100 km");
        setCell("cons-fuel", (latestFuelParsed.cfl * price).toFixed(2), "BGN");
        setCell("fuel-last-6", (latestFuelParsed.fl6 * 0.001 * price).toFixed(2), "BGN");
        setCell("fuel-last-60", (latestFuelParsed.fl60 * 0.001 * price).toFixed(2), "BGN");
    } else {
        setCell("inst-fuel", latestFuelParsed.ifl.toFixed(1), "L/100 km");
        setCell("avg-fuel", latestFuelParsed.afl.toFixed(1), "L/100 km");
        setCell("cons-fuel", latestFuelParsed.cfl.toFixed(2), "L");
        setCell("fuel-last-6", latestFuelParsed.fl6.toFixed(1), "mL");
        setCell("fuel-last-60", latestFuelParsed.fl60.toFixed(0), "mL");
    }

    // temperature always same
    setCell("ctmp", latestFuelParsed.ctmp, "°C");
    //DEBUG
    setCell("dist-tr", latestFuelParsed.disttr, "m");
    setCell("barop", latestFuelParsed.barop, "kPa");
}

toggleCurrency.addEventListener('change', updateFuelDisplay);
priceInput.addEventListener('input', updateFuelDisplay);


// On page load, restore any saved ESP logs
window.addEventListener("DOMContentLoaded", () => {
    updateFuelDisplay();
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

        else if (type === 'd' && parts.length >= 13) {
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
                cabtmp: parseFloat(parts[11]),
                barop: parseFloat(parts[12]),
            };

            document.querySelector('#inst-fuel .value').textContent      = parsed.ifl.toFixed(1);
            document.querySelector('#avg-fuel .value').textContent       = parsed.afl.toFixed(1);
            document.querySelector('#ctmp .value').textContent           = parsed.ctmp;
            document.querySelector('#cons-fuel .value').textContent      = parsed.cfl.toFixed(2);
            document.querySelector('#rpm .value').textContent            = parsed.rpm;
            document.querySelector('#spd .value').textContent            = parsed.spd;
            document.querySelector('#pcnt-isr .value').textContent       = parsed.pisr;
            document.querySelector('#pcnt-rpm .value').textContent       = parsed.prpm;
            document.querySelector('#pdelta .value').textContent         = parsed.pd;
            document.querySelector('#avg-pwidth .value').textContent     = parsed.pdpc.toFixed(1);
            document.querySelector('#cabtmp .value').textContent         = parsed.cabtmp.toFixed(1);
            document.querySelector('#barop .value').textContent          = parsed.barop.toFixed(1);

            return;
        }

        else if (type === 'f' && parts.length >= 9) {
            // Fuel packet
            const parsed = {
                ifl: parseFloat(parts[1]),
                afl: parseFloat(parts[2]),
                ctmp: +parts[3],
                cfl: parseFloat(parts[4]),
                fl6: parseFloat(parts[5]),
                fl60: parseFloat(parts[6]),
                disttr: parseFloat(parts[7]),
                barop: parseFloat(parts[8]),
            };

            latestFuelParsed = parsed;
            updateFuelDisplay();
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

