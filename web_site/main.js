let csv_file = '';
let collecting = false;

// DOM Elements
const gauges = [
    document.getElementById('g1'),
    document.getElementById('g2'),
    document.getElementById('g3'),
    document.getElementById('g4')
];
const serialOutput = document.getElementById('serialOutput');
const gpsStatus = document.getElementById('gpsStatus');

let map = null;
let marker = null;
let lastLat = null, lastLng = null;

// === Init Map After Leaflet Loads
const initMap = () => {
    if (typeof L === 'undefined') {
        console.error("Leaflet not loaded");
        return;
    }

    map = L.map('map').setView([0, 0], 17);
    L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
    }).addTo(map);

    gpsStatus.textContent = "Map ready – waiting for GPS...";
    gpsStatus.style.background = "rgba(0, 100, 200, 0.8)";
    console.log("Map initialized");
};

// Load Local Leaflet JS (assume it's in the same directory)
const leafletScript = document.createElement('script');
leafletScript.src = './leaflet.js';
leafletScript.onload = () => {
    console.log("Leaflet loaded successfully");
    initMap();
};
leafletScript.onerror = () => {
    gpsStatus.textContent = "Failed to load map";
    gpsStatus.style.background = "rgba(200,0,0,0.8)";
    console.error("Failed to load Leaflet");
};
document.head.appendChild(leafletScript);

// Update Map w/ GPS
const updateMap = (lat, lng) => {
    const valid = !isNaN(lat) && !isNaN(lng) && lat >= -90 && lat <= 90 && lng >= -180 && lng <= 180;

    if (!valid) {
        gpsStatus.textContent = "Invalid GPS";
        gpsStatus.style.background = "rgba(200,0,0,0.8)";
        return;
    }

    // If map not ready, retry
    if (!map) {
        setTimeout(() => updateMap(lat, lng), 300);
        return;
    }

    const latLng = [lat, lng];

    if (!marker) {
        marker = L.marker(latLng).addTo(map)
            .bindPopup(`<b>Location</b><br>Lat: ${lat.toFixed(6)}<br>Lng: ${lng.toFixed(6)}`)
            .openPopup();
        map.setView(latLng, 17);
    } else {
        marker.setLatLng(latLng).setPopupContent(
            `<b>Location</b><br>Lat: ${lat.toFixed(6)}<br>Lng: ${lng.toFixed(6)}`
        );
        if (!lastLat || Math.abs(lat - lastLat) > 0.0001 || Math.abs(lng - lastLng) > 0.0001) {
            map.panTo(latLng, { animate: true, duration: 1.0 });
        }
    }

    lastLat = lat;
    lastLng = lng;
    gpsStatus.textContent = `Lat: ${lat.toFixed(6)}, Lng: ${lng.toFixed(6)}`;
    gpsStatus.style.background = "rgba(0,150,0,0.8)";
};

// Serial Logging
const logToSerial = (timestamp, data) => {
    const nearBottom = serialOutput.scrollHeight - serialOutput.scrollTop <= serialOutput.clientHeight + 50;
    serialOutput.textContent += `[${timestamp}] ${data}\n`;
    if (nearBottom) {
        serialOutput.scrollTop = serialOutput.scrollHeight;
    }
};

// Gauge Update
const updateGaugeElement = (gaugeElement, value, gaugeNumber) => {
    let gaugeValueSpan;

    switch (gaugeNumber) {
        case 0: // RPM
            const rpmMax = 8000;
            const rpmValue = Math.max(0, Math.min(rpmMax, value));
            gaugeValueSpan = gaugeElement.querySelector('.gauge-value');
            let rpmPercent = Math.max(0, Math.min(100, (rpmValue / rpmMax) * 100));
            gaugeElement.style.setProperty('--percent', rpmPercent + '%');
            gaugeValueSpan.textContent = isNaN(rpmValue) ? 'NaN' : Math.round(rpmValue);
            break;

        case 1: // AFR
            const afrValue = (((value / 1023.0) * 5) * 2.375) + 7.3125;
            gaugeValueSpan = gaugeElement.querySelector('.gauge-value');
            let afrPercent = Math.max(0, Math.min(100, ((afrValue - 8.5) / (18 - 8.5)) * 100));
            gaugeElement.style.setProperty('--percent', afrPercent + '%');
            gaugeValueSpan.textContent = (afrValue > 18 || afrValue < 8.5) ? 'SENSOR ERROR' : afrValue.toFixed(2);
            break;

        case 2: // TPS
            const tps_range = [0.64, 4.20];
            const tpsValue = Math.max(0, Math.min(100, ((((value / 1023.0) * 5) - tps_range[0]) / (tps_range[1] - tps_range[0])) * 100.0));
            gaugeValueSpan = gaugeElement.querySelector('.gauge-value');
            gaugeElement.style.setProperty('--percent', tpsValue + '%');
            gaugeValueSpan.textContent = Math.round(tpsValue) + '%';
            break;

        case 3: // MAP
            const mapMax = 250;
            const mapValue = Math.max(0, Math.min(mapMax, value));
            gaugeValueSpan = gaugeElement.querySelector('.gauge-value');
            let mapPercent = Math.max(0, Math.min(100, (mapValue / mapMax) * 100));
            gaugeElement.style.setProperty('--percent', mapPercent + '%');
            gaugeValueSpan.textContent = isNaN(mapValue) ? 'NaN' : mapValue.toFixed(1);
            break;
    }
};

// Button Functions
const toggleCollection = () => {
    collecting = !collecting;
    const btn = document.getElementById('toggleCollectionBtn');
    btn.textContent = collecting ? 'Stop Collection' : 'Start Collection';
};

const downloadTextFile = (content, fileName) => {
    const blob = new Blob([content], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = fileName || 'data.csv';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    window.URL.revokeObjectURL(url);
};

const clearData = () => {
    csv_file = '';
    serialOutput.textContent = '';
};

// Data Polling
const getData = () => {
    const xhr = new XMLHttpRequest();
    xhr.open('GET', 'http://192.168.4.1/data', true);
    xhr.timeout = 5000;

    const timestamp = new Date().toLocaleTimeString();

    xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) {
            const rawData = xhr.responseText.trim();
            logToSerial(timestamp, 'RX: ' + rawData);

            const values = rawData.split(',').map(v => parseFloat(v.trim()));
            const validCount = values.filter(v => !isNaN(v)).length;

            if (validCount >= 4) {
                // Update gauges
                gauges.forEach((gauge, i) => {
                    updateGaugeElement(gauge, values[i], i);
                });

                // GPS: indices 4 and 5
                if (values.length >= 6 && !isNaN(values[4]) && !isNaN(values[5])) {
                    updateMap(values[4], values[5]);
                }

                // CSV Logging
                if (collecting) {
                    csv_file += `${timestamp},${rawData}\n`;
                }

                document.querySelector('.title').textContent = 'POWER ON';
                document.querySelector('.header').style.background = 'rgb(76, 175, 80)';
            } else {
                logToSerial(timestamp, 'ERROR: Expected 4+ values, got ' + validCount);
                document.querySelector('.title').textContent = 'DATA ERROR';
                document.querySelector('.header').style.background = 'rgba(253, 29, 29, 1)';
            }
        } else {
            logToSerial(timestamp, `HTTP Error: ${xhr.status}`);
            document.querySelector('.title').textContent = 'COMM ERROR';
            document.querySelector('.header').style.background = 'rgba(253, 29, 29, 1)';
        }
    };

    xhr.onerror = () => {
        logToSerial(timestamp, 'Network error');
        document.querySelector('.title').textContent = 'NO CONNECTION';
        document.querySelector('.header').style.background = 'rgba(253, 29, 29, 1)';
    };

    xhr.ontimeout = () => {
        logToSerial(timestamp, 'Request timeout');
        document.querySelector('.title').textContent = 'TIMEOUT';
        document.querySelector('.header').style.background = 'rgba(253, 29, 29, 1)';
    };

    xhr.send();
};

// Start polling every 200ms
let dataInterval = setInterval(getData, 500);