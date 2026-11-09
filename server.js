const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// In-memory system state
let systemState = {
  temperature: 28.2,
  temperatureValid: true,
  moisture: 87,
  moistureValid: true,
  rawMoisture: 1745,
  pump: false,
  mode: "AUTO",
  manualPumpState: false,
  lastSeen: null
};

// 1. ESP32 Telemetry Endpoint
// ESP32 sends its live readings every 2-3s and receives latest desired mode/pump state
app.post('/api/telemetry', (req, res) => {
  const data = req.body;
  if (!data) {
    return res.status(400).json({ error: "Missing telemetry payload" });
  }

  // Update telemetry from ESP32
  if (data.temperature !== undefined) systemState.temperature = Number(data.temperature);
  if (data.temperatureValid !== undefined) systemState.temperatureValid = Boolean(data.temperatureValid);
  if (data.moisture !== undefined) systemState.moisture = Number(data.moisture);
  if (data.moistureValid !== undefined) systemState.moistureValid = Boolean(data.moistureValid);
  if (data.rawMoisture !== undefined) systemState.rawMoisture = Number(data.rawMoisture);
  
  // In AUTO mode, ESP32 dictates pump status based on thresholds
  // In MANUAL mode, cloud holds user requested pump state
  if (systemState.mode === "AUTO") {
    systemState.pump = Boolean(data.pump);
  } else {
    systemState.pump = Boolean(systemState.manualPumpState);
  }

  systemState.lastSeen = Date.now();

  // Return desired control commands back to ESP32
  res.json({
    success: true,
    mode: systemState.mode,
    manualPump: systemState.manualPumpState
  });
});

// 2. Browser Telemetry & Status Endpoint
app.get('/api/status', (req, res) => {
  const isOnline = systemState.lastSeen ? (Date.now() - systemState.lastSeen < 12000) : false;
  res.json({
    ...systemState,
    isOnline,
    secondsSinceLastSeen: systemState.lastSeen ? Math.round((Date.now() - systemState.lastSeen) / 1000) : null
  });
});

// 3. Browser Mode Control (Auto / Manual)
app.post('/api/mode', (req, res) => {
  const { mode } = req.body;
  if (mode !== "auto" && mode !== "manual") {
    return res.status(400).json({ error: "Mode must be 'auto' or 'manual'" });
  }

  systemState.mode = mode.toUpperCase();
  res.json({ success: true, mode: systemState.mode });
});

// 4. Browser Manual Pump Control (ON / OFF)
app.post('/api/pump', (req, res) => {
  const { state } = req.body;
  if (systemState.mode !== "MANUAL") {
    return res.status(409).json({ error: "Please switch to MANUAL mode first!" });
  }

  systemState.manualPumpState = Boolean(state === 1 || state === true || state === "1");
  systemState.pump = systemState.manualPumpState;

  res.json({ success: true, pump: systemState.pump });
});

// Fallback to index.html for frontend
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
  console.log(`Cloud server running on port ${PORT}`);
});

