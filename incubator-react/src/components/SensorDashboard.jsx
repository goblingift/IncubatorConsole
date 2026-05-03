import { useEffect, useState } from "react";
import { useSensorData } from "../hooks/useSensorData";
import { useDevice } from "../context/DeviceContext";

const SETTINGS_API_URL = import.meta.env.VITE_SETTINGS_API_URL;

export default function SensorDashboard() {
    const { selectedDeviceId } = useDevice();
    const { data, loading, error } = useSensorData(selectedDeviceId);
    const [settings, setSettings] = useState(null);

    useEffect(() => {
        if (!selectedDeviceId) return;
        fetch(`${SETTINGS_API_URL}/${selectedDeviceId}`)
            .then(r => r.json())
            .then(d => setSettings(d))
            .catch(() => {});
    }, [selectedDeviceId]);

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">Kein Gerät ausgewählt.</p>;
    if (loading)           return <p className="text-gray-400 text-sm">Lade Sensordaten…</p>;
    if (error)             return <p className="text-red-500 text-sm">Fehler: {error}</p>;

    const timestamp = new Date(Number(data.timestamp)).toLocaleString("de-DE");

    const targetTemp     = settings?.temperature_target != null ? `${Number(settings.temperature_target).toFixed(1)} °C` : null;
    const targetHumidity = settings?.humidity_target   != null ? `${Number(settings.humidity_target)} %`                : null;

    return (
        <div className="space-y-4">
            <p className="text-xs text-gray-400">Letzte Messung: 🕐 {timestamp}</p>

            <div className="grid grid-cols-2 lg:grid-cols-3 gap-4">
                <SensorCard label="Temperatur"  value={`${data.temperature_celsius} °C`} valueColor="text-blue-600"   target={targetTemp} />
                <SensorCard label="Luftfeuchte" value={`${data.humidity_rh} %`}          valueColor="text-green-600"  target={targetHumidity} />
                <SensorCard label="CO₂"         value={`${data.co2_ppm} ppm`}            valueColor="text-purple-500" />
                <SensorCard label="Gewicht"     value={`${data.weight_gram} g`}          valueColor="text-yellow-500" />
                <SensorCard label="Lichtstärke" value={`${data.light_intensity}`}         valueColor="text-orange-400" />
                <SensorCard label="Lautstärke"  value={`${data.sound_intensity} dB`}     valueColor="text-gray-600" />
            </div>
        </div>
    );
}

function SensorCard({ label, value, valueColor, target }) {
    return (
        <div className="bg-gray-50 rounded-xl p-4">
            <p className="text-xs text-gray-400 mb-1">{label}</p>
            <p className={`text-3xl font-bold ${valueColor}`}>{value}</p>
            {target != null && (
                <p className="text-xs text-gray-400 mt-1">Sollwert: {target}</p>
            )}
        </div>
    );
}