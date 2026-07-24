import { useSensorData } from "../hooks/useSensorData";
import { useLightAverage } from "../hooks/useLightAverage";
import { useDevice } from "../context/DeviceContext";
import { MEASUREMENT_FIELDS_BY_KEY } from "../constants/measurementFields";

const ACTUATOR_FIELDS = ["relay_state_1", "relay_state_2", "relay_state_3", "relay_state_4", "humidifier_state"];

export default function SensorDashboard() {
    const { selectedDeviceId } = useDevice();
    const { data, loading, error } = useSensorData(selectedDeviceId);

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">No device selected.</p>;
    if (loading)           return <p className="text-gray-400 text-sm">Loading sensor data…</p>;
    if (error)             return <p className="text-red-500 text-sm">Error: {error}</p>;

    const timestamp = new Date(Number(data.timestamp) * 1000).toLocaleString("en-US");

    return (
        <div className="space-y-4">
            <p className="text-xs text-gray-400">Last reading: 🕐 {timestamp}</p>

            <div className="grid grid-cols-2 lg:grid-cols-3 gap-4">
                <SensorCard label="Temperature" value={`${data.temperature_celsius} °C`} valueColor="text-blue-600" />
                <SensorCard label="Humidity"    value={`${data.humidity_rh} %`}          valueColor="text-green-600" />
                <SensorCard label="CO₂"         value={`${data.co2_ppm} ppm`}            valueColor="text-purple-500" />
                <SensorCard label="Weight"      value={`${data.weight_gram} g`}          valueColor="text-yellow-500" />
                <SensorCard label="Light"       value={`${data.light_intensity}`}         valueColor="text-orange-400" />
                <LightSleepCard deviceId={selectedDeviceId} />
                <SensorCard label="Sound (raw)" value={`${data.sound_intensity}`}        valueColor="text-gray-600" />
                <SensorCard label="Voltage"     value={`${data.voltage} V`}              valueColor="text-sky-600" />
                <SensorCard label="Current"     value={`${data.current} A`}              valueColor="text-indigo-500" />
                <SensorCard label="Water Level" value={`${data.water_level} %`}          valueColor="text-cyan-600" />
                <SensorCard label="Pitch"       value={`${data.pitch_deg}°`}             valueColor="text-fuchsia-500" />
                <SensorCard label="Roll"        value={`${data.roll_deg}°`}              valueColor="text-rose-500" />
            </div>

            <div>
                <p className="text-xs text-gray-400 mb-2">Actuators</p>
                <div className="flex flex-wrap gap-2">
                    {ACTUATOR_FIELDS.map(key => (
                        <StatusBadge
                            key={key}
                            label={MEASUREMENT_FIELDS_BY_KEY[key].label}
                            on={Number(data[key]) === 1}
                        />
                    ))}
                </div>
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

function LightSleepCard({ deviceId }) {
    const { data, loading, error } = useLightAverage(deviceId);

    if (loading) return <SensorCard label="Sleep-friendly hours (24h)" value="…" valueColor="text-orange-300" />;
    if (error || !data || data.status === "no_data") {
        return <SensorCard label="Sleep-friendly hours (24h)" value="N/A" valueColor="text-orange-300" />;
    }

    const { sleep_friendly_hours, light_sleep_min_hours, passing } = data;
    return (
        <div className="bg-gray-50 rounded-xl p-4">
            <p className="text-xs text-gray-400 mb-1">Sleep-friendly hours (24h)</p>
            <p className={`text-3xl font-bold ${passing ? "text-green-600" : "text-red-500"}`}>
                {sleep_friendly_hours}h <span className="text-base font-normal text-gray-400">/ need {light_sleep_min_hours}h</span>
            </p>
        </div>
    );
}

function StatusBadge({ label, on }) {
    return (
        <span className={`inline-flex items-center gap-1.5 rounded-full px-3 py-1 text-xs font-medium ${
            on ? "bg-green-100 text-green-700" : "bg-gray-100 text-gray-500"
        }`}>
            <span className={`h-1.5 w-1.5 rounded-full ${on ? "bg-green-500" : "bg-gray-400"}`} />
            {label}: {on ? "ON" : "OFF"}
        </span>
    );
}