import { useSensorData } from "../hooks/useSensorData";
import { useDevice } from "../context/DeviceContext";

export default function SensorDashboard() {
    const { selectedDeviceId } = useDevice();
    const { data, loading, error } = useSensorData(selectedDeviceId);

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">No device selected.</p>;
    if (loading)           return <p className="text-gray-400 text-sm">Loading sensor data…</p>;
    if (error)             return <p className="text-red-500 text-sm">Error: {error}</p>;

    const timestamp = new Date(Number(data.timestamp)).toLocaleString("en-US");

    return (
        <div className="space-y-4">
            <p className="text-xs text-gray-400">Last reading: 🕐 {timestamp}</p>

            <div className="grid grid-cols-2 lg:grid-cols-3 gap-4">
                <SensorCard label="Temperature" value={`${data.temperature_celsius} °C`} valueColor="text-blue-600" />
                <SensorCard label="Humidity"    value={`${data.humidity_rh} %`}          valueColor="text-green-600" />
                <SensorCard label="CO₂"         value={`${data.co2_ppm} ppm`}            valueColor="text-purple-500" />
                <SensorCard label="Weight"      value={`${data.weight_gram} g`}          valueColor="text-yellow-500" />
                <SensorCard label="Light"       value={`${data.light_intensity}`}         valueColor="text-orange-400" />
                <SensorCard label="Sound"       value={`${data.sound_intensity} dB`}     valueColor="text-gray-600" />
            </div>
        </div>
    );
}

function SensorCard({ label, value, valueColor }) {
    return (
        <div className="bg-gray-50 rounded-xl p-4">
            <p className="text-xs text-gray-400 mb-1">{label}</p>
            <p className={`text-3xl font-bold ${valueColor}`}>{value}</p>
        </div>
    );
}