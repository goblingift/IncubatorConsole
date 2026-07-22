import { useBatteryStatus } from "../hooks/useBatteryStatus";
import { useDevice } from "../context/DeviceContext";

// Meter fill carries severity (good -> warning -> critical); the unfilled
// track stays a plain neutral gray rather than a lighter step of the same
// hue, since the fill color itself already changes per tier - a same-ramp
// track only makes sense when the ramp is fixed per meter.
function severityFor(percent) {
    if (percent >= 50) return { fill: "bg-emerald-500", text: "text-emerald-600" };
    if (percent >= 20) return { fill: "bg-amber-500",   text: "text-amber-600" };
    return { fill: "bg-red-500", text: "text-red-600" };
}

export default function BatteryStatus() {
    const { selectedDeviceId } = useDevice();
    const { data, loading, error } = useBatteryStatus(selectedDeviceId);

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">No device selected.</p>;
    if (loading)           return <p className="text-gray-400 text-sm">Loading battery status…</p>;
    if (error)             return <p className="text-red-500 text-sm">Error: {error}</p>;

    if (data.status === "no_data") {
        return <p className="text-gray-400 text-sm">{data.message}</p>;
    }

    const severity = severityFor(data.battery_percent);

    return (
        <div className="space-y-4">
            {!data.curve_calibrated && (
                <p className="flex items-center gap-2 text-xs text-amber-700 bg-amber-50 border border-amber-200 rounded-lg px-3 py-2">
                    <span aria-hidden="true">⚠</span>
                    Uncalibrated: battery % is estimated from a placeholder discharge curve, not real measured data yet.
                </p>
            )}

            <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
                <div className="bg-gray-50 rounded-xl p-4">
                    <p className="text-xs text-gray-400 mb-1">Battery voltage</p>
                    <p className="text-3xl font-bold text-sky-600">{data.battery_voltage} V</p>
                </div>

                <div className="bg-gray-50 rounded-xl p-4">
                    <p className="text-xs text-gray-400 mb-1">Battery</p>
                    <p className={`text-3xl font-bold ${severity.text}`}>{data.battery_percent}%</p>
                    <div className="mt-2 h-2 w-full rounded-full bg-gray-200 overflow-hidden">
                        <div
                            className={`h-full rounded-full ${severity.fill}`}
                            style={{ width: `${Math.max(0, Math.min(100, data.battery_percent))}%` }}
                        />
                    </div>
                </div>

                <div className="bg-gray-50 rounded-xl p-4">
                    <p className="text-xs text-gray-400 mb-1">Drain rate</p>
                    <p className="text-3xl font-bold text-orange-500">
                        {data.drain_rate_percent_per_hour != null ? `${data.drain_rate_percent_per_hour}%/h` : "—"}
                    </p>
                </div>

                <div className="bg-gray-50 rounded-xl p-4">
                    <p className="text-xs text-gray-400 mb-1">Remaining runtime</p>
                    <p className="text-3xl font-bold text-indigo-500">
                        {data.remaining_hours != null ? `${data.remaining_hours} h` : "—"}
                    </p>
                </div>
            </div>

            {(data.status === "insufficient_data" || data.status === "stable_or_charging") && (
                <p className="text-xs text-gray-400">{data.message}</p>
            )}

            {data.status === "draining" && (
                <p className="text-xs text-gray-400">
                    Predicted shutdown: {new Date(data.predicted_shutdown_at * 1000).toLocaleString("en-US")}
                </p>
            )}
        </div>
    );
}
