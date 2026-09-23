import React, { useState, useEffect, useRef } from 'react';

// Command Constants matching app_protocol.h
const CMD_SET_INTERVAL = 1;
const CMD_REBOOT = 2;
const CMD_OTA_START = 3;
const CMD_LED_ON = 4;
const CMD_LED_OFF = 5;

export default function ThreadMonitorDashboard() {
  const [apiUrl, setApiUrl] = useState('http://192.168.1.100:8080');
  const [nodes, setNodes] = useState([]);
  const [systemStatus, setSystemStatus] = useState(null);
  const [isLive, setIsLive] = useState(false);
  const [lastUpdated, setLastUpdated] = useState(null);
  const [selectedNode, setSelectedNode] = useState(null);
  const [commandStatus, setCommandStatus] = useState(null);
  const [intervalVal, setIntervalVal] = useState(30);
  const [history, setHistory] = useState({}); // eui64 -> array of history readings

  // Mock demo nodes for fallback when board is offline
  const mockNodes = [
    {
      eui64: "48F17EC300A1B2C1",
      label: "Greenhouse-North",
      ipv6: "fd00:db8:a0:0:1a2b:3c4d:5e6f:7a8b",
      online: true,
      fw_version: 100,
      report_interval_s: 30,
      total_reports: 1420,
      last_seen: Math.floor(Date.now() / 1000) - 12,
      reading: {
        temperature_c: 24.2,
        humidity_pct: 68.5,
        battery_mv: 3740,
        rssi_dbm: -58,
        battery_low: false
      }
    },
    {
      eui64: "48F17EC300A1B2C2",
      label: "Brooder-Coop-01",
      ipv6: "fd00:db8:a0:0:2b3c:4d5e:6f7a:8b9c",
      online: true,
      fw_version: 100,
      report_interval_s: 30,
      total_reports: 954,
      last_seen: Math.floor(Date.now() / 1000) - 25,
      reading: {
        temperature_c: 32.1,
        humidity_pct: 54.0,
        battery_mv: 3620,
        rssi_dbm: -64,
        battery_low: false
      }
    },
    {
      eui64: "48F17EC300A1B2C3",
      label: "OpenField-West",
      ipv6: "fd00:db8:a0:0:3c4d:5e6f:7a8b:9c0d",
      online: false,
      fw_version: 100,
      report_interval_s: 60,
      total_reports: 312,
      last_seen: Math.floor(Date.now() / 1000) - 340,
      reading: {
        temperature_c: 19.8,
        humidity_pct: 82.1,
        battery_mv: 3280,
        rssi_dbm: -82,
        battery_low: true
      }
    }
  ];

  // Fetch nodes and status from Border Router
  const fetchData = async () => {
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 3000);

      const [nodesRes, statusRes] = await Promise.all([
        fetch(`${apiUrl}/api/v1/nodes`, { signal: controller.signal }),
        fetch(`${apiUrl}/api/v1/status`, { signal: controller.signal }).catch(() => null)
      ]);
      clearTimeout(timeoutId);

      if (nodesRes.ok) {
        const data = await nodesRes.json();
        setNodes(data);
        setIsLive(true);
        setLastUpdated(new Date().toLocaleTimeString());

        // Update history
        setHistory(prev => {
          const next = { ...prev };
          data.forEach(n => {
            const list = next[n.eui64] || [];
            if (n.reading) {
              const item = { t: n.reading.temperature_c, h: n.reading.humidity_pct, ts: Date.now() };
              next[n.eui64] = [...list.slice(-15), item];
            }
          });
          return next;
        });

        if (statusRes && statusRes.ok) {
          const s = await statusRes.json();
          setSystemStatus(s);
        }
        return;
      }
      throw new Error("HTTP error " + nodesRes.status);
    } catch (err) {
      // Fallback to simulated demo data
      setIsLive(false);
      setNodes(prev => {
        if (prev.length === 0) return mockNodes;
        return prev.map(n => {
          if (!n.online) return n;
          const jitterT = (Math.random() - 0.5) * 0.4;
          const jitterH = (Math.random() - 0.5) * 0.8;
          return {
            ...n,
            last_seen: Math.floor(Date.now() / 1000) - Math.floor(Math.random() * 20),
            reading: {
              ...n.reading,
              temperature_c: +(n.reading.temperature_c + jitterT).toFixed(1),
              humidity_pct: +(n.reading.humidity_pct + jitterH).toFixed(1)
            }
          };
        });
      });
      setLastUpdated(new Date().toLocaleTimeString());
    }
  };

  useEffect(() => {
    fetchData();
    const timer = setInterval(fetchData, 5000);
    return () => clearInterval(timer);
  }, [apiUrl]);

  // Send downlink command
  const sendCommand = async (eui64, cmd, param = 0, data = null) => {
    setCommandStatus({ loading: true, msg: "Sending command..." });
    try {
      const res = await fetch(`${apiUrl}/api/v1/nodes/${eui64}/cmd`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd, param, data })
      });
      if (res.ok) {
        setCommandStatus({ loading: false, success: true, msg: "Command queued successfully!" });
      } else {
        setCommandStatus({ loading: false, success: false, msg: `Failed (HTTP ${res.status})` });
      }
    } catch (err) {
      setCommandStatus({
        loading: false,
        success: false,
        msg: isLive ? `Network error: ${err.message}` : "Simulated command accepted (Demo mode)"
      });
    }
    setTimeout(() => setCommandStatus(null), 4000);
  };

  const onlineCount = nodes.filter(n => n.online).length;

  return (
    <div style={{
      fontFamily: 'Inter, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif',
      backgroundColor: '#0f172a',
      color: '#e2e8f0',
      minHeight: '100vh',
      padding: '24px'
    }}>
      {/* Header bar */}
      <header style={{
        display: 'flex',
        flexWrap: 'wrap',
        justifyContent: 'space-between',
        alignItems: 'center',
        paddingBottom: '20px',
        borderBottom: '1px solid #1e293b',
        marginBottom: '24px'
      }}>
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <h1 style={{ fontSize: '24px', fontWeight: '700', margin: 0, color: '#38bdf8' }}>
              AgriNetPro
            </h1>
            <span style={{
              fontSize: '12px',
              padding: '2px 8px',
              borderRadius: '9999px',
              backgroundColor: '#1e293b',
              color: '#94a3b8',
              border: '1px solid #334155'
            }}>
              Thread Mesh Gateway
            </span>
          </div>
          <p style={{ margin: '4px 0 0 0', fontSize: '13px', color: '#64748b' }}>
            ESP Thread Border Router & Modular Sensor Telemetry Monitor
          </p>
        </div>

        {/* Connection status and API endpoint config */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', marginTop: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{
              width: '10px',
              height: '10px',
              borderRadius: '50%',
              backgroundColor: isLive ? '#22c55e' : '#f59e0b',
              boxShadow: isLive ? '0 0 8px #22c55e' : '0 0 8px #f59e0b'
            }} />
            <span style={{ fontSize: '13px', color: isLive ? '#22c55e' : '#f59e0b', fontWeight: '500' }}>
              {isLive ? 'Live Hardware Connected' : 'Demo / Offline Simulation'}
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', backgroundColor: '#1e293b', borderRadius: '8px', padding: '4px 8px', border: '1px solid #334155' }}>
            <span style={{ fontSize: '12px', color: '#94a3b8', marginRight: '6px' }}>BR IP:</span>
            <input
              type="text"
              value={apiUrl}
              onChange={(e) => setApiUrl(e.target.value)}
              style={{
                backgroundColor: 'transparent',
                border: 'none',
                color: '#f8fafc',
                fontSize: '13px',
                width: '180px',
                outline: 'none'
              }}
            />
          </div>
        </div>
      </header>

      {/* Top Overview KPI Cards */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
        gap: '16px',
        marginBottom: '28px'
      }}>
        <div style={{ backgroundColor: '#1e293b', borderRadius: '12px', padding: '16px', border: '1px solid #334155' }}>
          <div style={{ color: '#94a3b8', fontSize: '13px' }}>Active Nodes</div>
          <div style={{ fontSize: '26px', fontWeight: '700', marginTop: '6px', color: '#38bdf8' }}>
            {onlineCount} <span style={{ fontSize: '14px', color: '#64748b' }}>/ {nodes.length} total</span>
          </div>
        </div>

        <div style={{ backgroundColor: '#1e293b', borderRadius: '12px', padding: '16px', border: '1px solid #334155' }}>
          <div style={{ color: '#94a3b8', fontSize: '13px' }}>Network Status</div>
          <div style={{ fontSize: '20px', fontWeight: '600', marginTop: '10px', color: '#22c55e' }}>
            Thread 802.15.4 Active
          </div>
        </div>

        <div style={{ backgroundColor: '#1e293b', borderRadius: '12px', padding: '16px', border: '1px solid #334155' }}>
          <div style={{ color: '#94a3b8', fontSize: '13px' }}>Border Router Uptime</div>
          <div style={{ fontSize: '20px', fontWeight: '600', marginTop: '10px', color: '#f8fafc' }}>
            {systemStatus ? `${Math.floor(systemStatus.uptime_s / 3600)}h ${Math.floor((systemStatus.uptime_s % 3600)/60)}m` : 'Online'}
          </div>
        </div>

        <div style={{ backgroundColor: '#1e293b', borderRadius: '12px', padding: '16px', border: '1px solid #334155' }}>
          <div style={{ color: '#94a3b8', fontSize: '13px' }}>Last Telemetry Sync</div>
          <div style={{ fontSize: '18px', fontWeight: '500', marginTop: '12px', color: '#cbd5e1' }}>
            {lastUpdated || '--:--:--'}
          </div>
        </div>
      </div>

      {/* Main Grid: Node Cards */}
      <h2 style={{ fontSize: '18px', fontWeight: '600', marginBottom: '16px', color: '#f8fafc' }}>
        Registered Sensor Nodes
      </h2>

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(320px, 1fr))',
        gap: '20px'
      }}>
        {nodes.map((node) => {
          const isSelected = selectedNode?.eui64 === node.eui64;
          const r = node.reading || {};
          const hist = history[node.eui64] || [];

          return (
            <div
              key={node.eui64}
              onClick={() => setSelectedNode(node)}
              style={{
                backgroundColor: isSelected ? '#1e293b' : '#1e293b99',
                borderRadius: '14px',
                border: isSelected ? '2px solid #38bdf8' : '1px solid #334155',
                padding: '20px',
                cursor: 'pointer',
                transition: 'all 0.2s ease',
                boxShadow: isSelected ? '0 10px 25px -5px rgba(56, 189, 248, 0.15)' : 'none'
              }}
            >
              {/* Card Header */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '16px' }}>
                <div>
                  <h3 style={{ margin: 0, fontSize: '17px', fontWeight: '600', color: '#f8fafc' }}>
                    {node.label || 'Unnamed Node'}
                  </h3>
                  <div style={{ fontSize: '11px', fontFamily: 'monospace', color: '#64748b', marginTop: '2px' }}>
                    EUI: {node.eui64}
                  </div>
                </div>

                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{
                    width: '8px',
                    height: '8px',
                    borderRadius: '50%',
                    backgroundColor: node.online ? '#22c55e' : '#ef4444'
                  }} />
                  <span style={{ fontSize: '12px', color: node.online ? '#4ade80' : '#f87171', fontWeight: '500' }}>
                    {node.online ? 'Online' : 'Offline'}
                  </span>
                </div>
              </div>

              {/* Sensor Metrics Big Display */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '18px' }}>
                <div style={{ backgroundColor: '#0f172a80', padding: '12px', borderRadius: '10px' }}>
                  <div style={{ fontSize: '12px', color: '#94a3b8' }}>Temperature</div>
                  <div style={{ fontSize: '24px', fontWeight: '700', color: '#f43f5e', marginTop: '4px' }}>
                    {r.temperature_c != null ? `${r.temperature_c}°C` : '--'}
                  </div>
                </div>

                <div style={{ backgroundColor: '#0f172a80', padding: '12px', borderRadius: '10px' }}>
                  <div style={{ fontSize: '12px', color: '#94a3b8' }}>Humidity</div>
                  <div style={{ fontSize: '24px', fontWeight: '700', color: '#38bdf8', marginTop: '4px' }}>
                    {r.humidity_pct != null ? `${r.humidity_pct}%` : '--'}
                  </div>
                </div>
              </div>

              {/* Sparkline mini-graph if history exists */}
              {hist.length > 2 && (
                <div style={{ marginBottom: '16px' }}>
                  <div style={{ fontSize: '11px', color: '#64748b', marginBottom: '4px' }}>Live Trends</div>
                  <div style={{ display: 'flex', alignItems: 'flex-end', height: '36px', gap: '4px', backgroundColor: '#0f172a40', padding: '4px', borderRadius: '6px' }}>
                    {hist.map((h, i) => {
                      const pct = Math.min(Math.max((h.t - 10) / 30, 0.1), 1.0);
                      return (
                        <div
                          key={i}
                          title={`T: ${h.t}°C`}
                          style={{
                            flex: 1,
                            height: `${pct * 100}%`,
                            backgroundColor: '#38bdf8',
                            borderRadius: '2px',
                            opacity: 0.4 + (i / hist.length) * 0.6
                          }}
                        />
                      );
                    })}
                  </div>
                </div>
              )}

              {/* Node Metadata Footer */}
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: '8px',
                fontSize: '12px',
                color: '#94a3b8',
                borderTop: '1px solid #33415560',
                paddingTop: '12px'
              }}>
                <div>RSSI: <span style={{ color: '#cbd5e1' }}>{r.rssi_dbm || '--'} dBm</span></div>
                <div>Battery: <span style={{ color: r.battery_low ? '#f87171' : '#cbd5e1' }}>
                  {r.battery_mv ? `${(r.battery_mv/1000).toFixed(2)} V` : 'USB'}
                </span></div>
                <div>Interval: <span style={{ color: '#cbd5e1' }}>{node.report_interval_s || 30}s</span></div>
                <div>Reports: <span style={{ color: '#cbd5e1' }}>{node.total_reports || 0}</span></div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Command Control Modal / Drawer for Selected Node */}
      {selectedNode && (
        <div style={{
          position: 'fixed',
          bottom: '24px',
          right: '24px',
          width: '380px',
          backgroundColor: '#1e293b',
          borderRadius: '16px',
          border: '1px solid #38bdf8',
          boxShadow: '0 20px 40px rgba(0,0,0,0.5)',
          padding: '24px',
          zIndex: 100
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
            <h3 style={{ margin: 0, fontSize: '18px', color: '#38bdf8' }}>
              Command: {selectedNode.label}
            </h3>
            <button
              onClick={() => setSelectedNode(null)}
              style={{
                background: 'none',
                border: 'none',
                color: '#94a3b8',
                cursor: 'pointer',
                fontSize: '18px'
              }}
            >
              ✕
            </button>
          </div>

          <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '16px' }}>
            Send downlinks queued via Border Router for next SED poll cycle.
          </div>

          {commandStatus && (
            <div style={{
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '13px',
              marginBottom: '14px',
              backgroundColor: commandStatus.success ? '#065f46' : '#7f1d1d',
              color: '#f8fafc'
            }}>
              {commandStatus.msg}
            </div>
          )}

          {/* Action buttons */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <button
              onClick={() => sendCommand(selectedNode.eui64, CMD_LED_ON)}
              style={{
                padding: '10px',
                borderRadius: '8px',
                backgroundColor: '#0284c7',
                color: '#fff',
                border: 'none',
                cursor: 'pointer',
                fontWeight: '500',
                fontSize: '13px'
              }}
            >
              💡 LED ON
            </button>

            <button
              onClick={() => sendCommand(selectedNode.eui64, CMD_LED_OFF)}
              style={{
                padding: '10px',
                borderRadius: '8px',
                backgroundColor: '#334155',
                color: '#fff',
                border: 'none',
                cursor: 'pointer',
                fontWeight: '500',
                fontSize: '13px'
              }}
            >
              🌑 LED OFF
            </button>

            <button
              onClick={() => sendCommand(selectedNode.eui64, CMD_REBOOT)}
              style={{
                padding: '10px',
                borderRadius: '8px',
                backgroundColor: '#dc2626',
                color: '#fff',
                border: 'none',
                cursor: 'pointer',
                fontWeight: '500',
                fontSize: '13px'
              }}
            >
              🔄 Reboot Node
            </button>

            <button
              onClick={() => sendCommand(selectedNode.eui64, CMD_SET_INTERVAL, intervalVal)}
              style={{
                padding: '10px',
                borderRadius: '8px',
                backgroundColor: '#0d9488',
                color: '#fff',
                border: 'none',
                cursor: 'pointer',
                fontWeight: '500',
                fontSize: '13px'
              }}
            >
              ⏱ Set Interval
            </button>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <label style={{ fontSize: '12px', color: '#94a3b8' }}>Interval (s):</label>
            <input
              type="number"
              min="5"
              max="3600"
              value={intervalVal}
              onChange={(e) => setIntervalVal(+e.target.value)}
              style={{
                backgroundColor: '#0f172a',
                border: '1px solid #334155',
                color: '#fff',
                padding: '4px 8px',
                borderRadius: '6px',
                width: '70px',
                fontSize: '13px'
              }}
            />
          </div>
        </div>
      )}
    </div>
  );
}
