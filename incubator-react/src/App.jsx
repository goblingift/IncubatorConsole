import { useState, useEffect } from 'react';

function App() {
  const [temp, setTemp] = useState(37); // Aktuell
  const [hum, setHum] = useState(60);
  const [targetTemp, setTargetTemp] = useState(36.5);
  const [targetHum, setTargetHum] = useState(50);
  const [alarm, setAlarm] = useState(false);

  return (
      <div className="min-h-screen bg-gradient-to-br from-blue-50 to-indigo-100 p-8">
        <header className="text-center mb-12">
          <h1 className="text-4xl font-bold text-gray-800">Incubator Dashboard</h1>
          <p className="text-lg text-gray-600 mt-2">Version: {import.meta.env.APP_VERSION}</p>
        </header>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 max-w-6xl mx-auto">
          {/* Aktuelle Werte */}
          <div className="bg-white p-8 rounded-2xl shadow-xl">
            <h2 className="text-2xl font-semibold text-gray-700 mb-4">Aktuell</h2>
            <div className="space-y-4">
              <div><span className="text-3xl font-bold text-blue-600">{temp}°C</span> Temp</div>
              <div><span className="text-3xl font-bold text-green-600">{hum}%</span> Feuchte</div>
            </div>
          </div>

          {/* Sollwerte Sliders */}
          <div className="bg-white p-8 rounded-2xl shadow-xl">
            <h2 className="text-2xl font-semibold text-gray-700 mb-4">Sollwerte</h2>
            <div className="space-y-6">
              <div>
                <label className="block text-sm font-medium mb-2">Temp: {targetTemp}°C</label>
                <input type="range" min="30" max="40" step="0.1" value={targetTemp} onChange={e => setTargetTemp(e.target.value)} className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-blue-500" />
              </div>
              <div>
                <label className="block text-sm font-medium mb-2">Feuchte: {targetHum}%</label>
                <input type="range" min="30" max="90" value={targetHum} onChange={e => setTargetHum(e.target.value)} className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-green-500" />
              </div>
            </div>
          </div>

          {/* Alarm */}
          <div className="bg-white p-8 rounded-2xl shadow-xl">
            <h2 className="text-2xl font-semibold text-gray-700 mb-4">Alarm</h2>
            <button onClick={() => setAlarm(!alarm)} className={`w-full py-3 px-6 rounded-xl font-bold ${alarm ? 'bg-red-500 text-white' : 'bg-gray-200 text-gray-700 hover:bg-red-100'}`}>
              {alarm ? 'Alarm AKTIV' : 'Alarm EIN'}
            </button>
          </div>
        </div>
      </div>
  );
}

export default App;