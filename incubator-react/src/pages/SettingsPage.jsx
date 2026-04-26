import { useState } from 'react';

export default function SettingsPage() {
    const [targetTemp, setTargetTemp] = useState(36.5);
    const [targetHum, setTargetHum] = useState(50);

    return (
        <div className="max-w-3xl mx-auto">
            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-6">Sollwerte</h2>

                <div className="space-y-6">
                    <div>
                        <label className="block text-sm font-medium mb-2">
                            Temperatur: {targetTemp}°C
                        </label>
                        <input
                            type="range"
                            min="30"
                            max="40"
                            step="0.1"
                            value={targetTemp}
                            onChange={(e) => setTargetTemp(e.target.value)}
                            className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-blue-500"
                        />
                    </div>

                    <div>
                        <label className="block text-sm font-medium mb-2">
                            Luftfeuchte: {targetHum}%
                        </label>
                        <input
                            type="range"
                            min="30"
                            max="90"
                            value={targetHum}
                            onChange={(e) => setTargetHum(e.target.value)}
                            className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-green-500"
                        />
                    </div>

                    <button className="mt-4 w-full bg-blue-600 text-white py-3 px-6 rounded-xl font-bold hover:bg-blue-700">
                        Speichern
                    </button>
                </div>
            </div>
        </div>
    );
}