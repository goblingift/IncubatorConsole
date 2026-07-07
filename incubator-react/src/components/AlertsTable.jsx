import { useMemo, useState } from "react";
import { useAlerts } from "../hooks/useAlerts";
import { useDevice } from "../context/DeviceContext";

const FIELD_LABELS = {
    temperature_celsius: "Temperature",
    humidity_rh: "Humidity",
    co2_ppm: "CO₂",
    light_intensity_avg_24h: "Light (24h avg)",
    sound_intensity: "Sound (raw)",
    weight_gram: "Weight",
    pitch_deg: "Pitch",
    roll_deg: "Roll",
    voltage: "Voltage",
    current: "Current",
    water_level: "Water Level",
};

const COLUMNS = [
    { key: "timestamp", label: "Time" },
    { key: "field", label: "Field" },
    { key: "value", label: "Value" },
    { key: "bound", label: "Bound" },
    { key: "threshold", label: "Threshold" },
    { key: "checked_at", label: "Checked At" },
];

function formatTime(epochSeconds) {
    return new Date(Number(epochSeconds) * 1000).toLocaleString("en-US");
}

export default function AlertsTable() {
    const { selectedDeviceId } = useDevice();
    const { data, loading, error } = useAlerts(selectedDeviceId);
    const [sortKey, setSortKey] = useState("timestamp");
    const [sortDir, setSortDir] = useState("desc");

    const sorted = useMemo(() => {
        if (!data) return [];
        return [...data].sort((a, b) => {
            const av = a[sortKey];
            const bv = b[sortKey];
            if (av === bv) return 0;
            const cmp = av > bv ? 1 : -1;
            return sortDir === "asc" ? cmp : -cmp;
        });
    }, [data, sortKey, sortDir]);

    const toggleSort = (key) => {
        if (key === sortKey) {
            setSortDir(dir => (dir === "asc" ? "desc" : "asc"));
        } else {
            setSortKey(key);
            setSortDir("desc");
        }
    };

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">No device selected.</p>;
    if (loading)           return <p className="text-gray-400 text-sm">Loading alerts…</p>;
    if (error)             return <p className="text-red-500 text-sm">Error: {error}</p>;
    if (sorted.length === 0) return <p className="text-gray-400 text-sm">No alerts for this device.</p>;

    return (
        <div className="overflow-x-auto">
            <table className="w-full text-sm text-left">
                <thead>
                    <tr className="border-b border-gray-200">
                        {COLUMNS.map(col => (
                            <th
                                key={col.key}
                                onClick={() => toggleSort(col.key)}
                                className="cursor-pointer select-none px-3 py-2 font-medium text-gray-500 hover:text-gray-700 whitespace-nowrap"
                            >
                                {col.label}
                                {sortKey === col.key ? (sortDir === "asc" ? " ▲" : " ▼") : ""}
                            </th>
                        ))}
                    </tr>
                </thead>
                <tbody>
                    {sorted.map(alert => (
                        <tr key={alert.alert_id} className="border-b border-gray-100 last:border-0">
                            <td className="px-3 py-2 whitespace-nowrap">{formatTime(alert.timestamp)}</td>
                            <td className="px-3 py-2 whitespace-nowrap">{FIELD_LABELS[alert.field] ?? alert.field}</td>
                            <td className="px-3 py-2">{alert.value}</td>
                            <td className="px-3 py-2 capitalize">{alert.bound}</td>
                            <td className="px-3 py-2">{alert.threshold}</td>
                            <td className="px-3 py-2 whitespace-nowrap">{formatTime(alert.checked_at)}</td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}
