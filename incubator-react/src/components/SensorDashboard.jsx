import { useSensorData } from "../hooks/useSensorData";

export default function SensorDashboard() {
    const { data, loading, error } = useSensorData(10000);

    if (loading) return <p className="text-gray-400 text-sm">Lade Sensordaten…</p>;
    if (error)   return <p className="text-red-500 text-sm">Fehler: {error}</p>;

    const timestamp = new Date(Number(data.sort_key)).toLocaleString("de-DE");

    return (
        <div className="space-y-4">
            <p className="text-xs text-gray-400">Letzte Messung: 🕐 {timestamp}</p>

            <div>
                <span className="text-3xl font-bold text-blue-600">{data.temperature_celsius}°C</span>
                <span className="text-gray-500 ml-2">Temperatur</span>
            </div>

            <div>
                <span className="text-3xl font-bold text-green-600">{data.humidity_rh}%</span>
                <span className="text-gray-500 ml-2">Luftfeuchte</span>
            </div>

            <div>
                <span className="text-2xl font-semibold text-purple-500">{data.co2_ppm} ppm</span>
                <span className="text-gray-500 ml-2">CO₂</span>
            </div>

            <div>
                <span className="text-2xl font-semibold text-yellow-500">{data.weight_gram} g</span>
                <span className="text-gray-500 ml-2">Gewicht (g)</span>
            </div>

            <div>
                <span className="text-2xl font-semibold text-orange-400">{data.light_intensity}</span>
                <span className="text-gray-500 ml-2">Lichtstärke</span>
            </div>

            <div>
                <span className="text-2xl font-semibold text-gray-600">{data.sound_intensity} dB</span>
                <span className="text-gray-500 ml-2">Lautstärke (dB)</span>
            </div>
        </div>
    );
}