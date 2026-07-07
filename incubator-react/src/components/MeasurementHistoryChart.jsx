import { useMemo, useState } from "react";
import {
    ResponsiveContainer,
    LineChart,
    Line,
    XAxis,
    YAxis,
    CartesianGrid,
    Tooltip,
} from "recharts";
import { useDevice } from "../context/DeviceContext";
import { useMeasurementHistory } from "../hooks/useMeasurementHistory";
import { MEASUREMENT_FIELDS, MEASUREMENT_FIELDS_BY_KEY } from "../constants/measurementFields";

const RANGES = [
    { key: "1h", label: "Last Hour" },
    { key: "24h", label: "Last 24h" },
    { key: "7d", label: "Last 7 Days" },
];

// Single-series accent — matches the app's primary blue (nav/buttons). A
// legend and a multi-color palette only earn their keep once more than one
// series shares an axis; with one metric at a time, one consistent accent
// reads calmer than a color that jumps around on every metric switch.
const ACCENT = "#2a78d6";
const INK = { muted: "#898781", grid: "#e1e0d9", axis: "#c3c2b7" };

const DEFAULT_FIELD_KEY = "temperature_celsius";

function formatTimeTick(epochSeconds, range) {
    const date = new Date(Number(epochSeconds) * 1000);
    return range === "7d"
        ? date.toLocaleDateString("en-US", { month: "short", day: "numeric" })
        : date.toLocaleTimeString("en-US", { hour: "2-digit", minute: "2-digit" });
}

function formatFullTime(epochSeconds) {
    return new Date(Number(epochSeconds) * 1000).toLocaleString("en-US");
}

function buildChartData(points, field) {
    return points
        .filter(p => p[field.key] !== undefined && p[field.key] !== null)
        .map(p => ({ timestamp: Number(p.timestamp), value: Number(p[field.key]) }));
}

// Y-axis scales to the selected metric's own min/max (with ~10% padding),
// instead of a fixed or shared scale — a boolean field always uses a fixed
// [0,1]-ish domain since ON/OFF is the only meaningful range for it.
function computeDomain(chartData, kind) {
    if (kind === "boolean") return [-0.1, 1.1];

    const values = chartData.map(p => p.value);
    if (values.length === 0) return [0, 1];

    const min = Math.min(...values);
    const max = Math.max(...values);
    if (min === max) {
        const pad = min === 0 ? 1 : Math.abs(min) * 0.1;
        return [min - pad, max + pad];
    }
    const pad = (max - min) * 0.1;
    return [min - pad, max + pad];
}

function formatValue(value, field) {
    if (field.kind === "boolean") return value >= 0.5 ? "ON" : "OFF";
    const rounded = Math.round(value * 100) / 100;
    return `${rounded}${field.unit ? ` ${field.unit}` : ""}`;
}

export default function MeasurementHistoryChart() {
    const { selectedDeviceId } = useDevice();
    const [range, setRange] = useState("24h");
    const [selectedKey, setSelectedKey] = useState(DEFAULT_FIELD_KEY);
    const [view, setView] = useState("chart");

    const { data, loading, error } = useMeasurementHistory(selectedDeviceId, range);
    const field = MEASUREMENT_FIELDS_BY_KEY[selectedKey];

    const chartData = useMemo(() => (data ? buildChartData(data, field) : []), [data, field]);
    const domain = useMemo(() => computeDomain(chartData, field.kind), [chartData, field.kind]);

    if (!selectedDeviceId) return <p className="text-gray-400 text-sm">No device selected.</p>;
    if (loading && !data) return <p className="text-gray-400 text-sm">Loading measurement history…</p>;
    if (error && !data) return <p className="text-red-500 text-sm">Error: {error}</p>;

    return (
        <div className="space-y-4">
            <div className="flex flex-wrap items-center justify-between gap-3">
                <div className="flex gap-2">
                    {RANGES.map(r => (
                        <button
                            key={r.key}
                            onClick={() => setRange(r.key)}
                            className={`px-3 py-1.5 rounded-lg text-sm font-medium transition ${
                                range === r.key ? "bg-blue-600 text-white" : "bg-gray-100 text-gray-600 hover:bg-gray-200"
                            }`}
                        >
                            {r.label}
                        </button>
                    ))}
                </div>
                <div className="flex gap-2">
                    <button
                        onClick={() => setView("chart")}
                        className={`px-3 py-1.5 rounded-lg text-sm font-medium transition ${
                            view === "chart" ? "bg-blue-600 text-white" : "bg-gray-100 text-gray-600 hover:bg-gray-200"
                        }`}
                    >
                        Chart
                    </button>
                    <button
                        onClick={() => setView("table")}
                        className={`px-3 py-1.5 rounded-lg text-sm font-medium transition ${
                            view === "table" ? "bg-blue-600 text-white" : "bg-gray-100 text-gray-600 hover:bg-gray-200"
                        }`}
                    >
                        Table
                    </button>
                </div>
            </div>

            <div className="flex flex-wrap gap-2">
                {MEASUREMENT_FIELDS.map(f => (
                    <button
                        key={f.key}
                        onClick={() => setSelectedKey(f.key)}
                        className={`inline-flex items-center rounded-full px-3 py-1 text-xs font-medium transition ${
                            f.key === selectedKey
                                ? "bg-blue-600 text-white"
                                : "bg-gray-100 text-gray-600 hover:bg-gray-200"
                        }`}
                    >
                        {f.label}
                    </button>
                ))}
            </div>

            {error && data && (
                <p className="text-xs text-red-500">Couldn't refresh: {error}</p>
            )}

            <p className="text-xs text-gray-400">
                {field.label}{field.unit ? ` (${field.unit})` : ""}
            </p>

            {chartData.length === 0 ? (
                <p className="text-gray-400 text-sm">No data for this range.</p>
            ) : view === "chart" ? (
                <div className={`transition-opacity ${loading ? "opacity-50" : "opacity-100"}`}>
                    <ResponsiveContainer width="100%" height={360}>
                        <LineChart data={chartData} margin={{ top: 8, right: 16, left: 0, bottom: 8 }}>
                            <CartesianGrid stroke={INK.grid} vertical={false} />
                            <XAxis
                                dataKey="timestamp"
                                type="number"
                                domain={["dataMin", "dataMax"]}
                                tickFormatter={(v) => formatTimeTick(v, range)}
                                stroke={INK.axis}
                                tick={{ fill: INK.muted, fontSize: 12 }}
                            />
                            <YAxis
                                domain={domain}
                                ticks={field.kind === "boolean" ? [0, 1] : undefined}
                                tickFormatter={(v) => (
                                    field.kind === "boolean"
                                        ? (v >= 0.5 ? "ON" : "OFF")
                                        : v.toLocaleString("en-US", { maximumFractionDigits: 2 })
                                )}
                                stroke={INK.axis}
                                tick={{ fill: INK.muted, fontSize: 12 }}
                                width={56}
                            />
                            <Tooltip content={<HistoryTooltip field={field} />} />
                            <Line
                                dataKey="value"
                                stroke={ACCENT}
                                strokeWidth={2}
                                dot={false}
                                activeDot={{ r: 4 }}
                                type={field.kind === "boolean" ? "stepAfter" : "monotone"}
                                isAnimationActive={false}
                                connectNulls
                            />
                        </LineChart>
                    </ResponsiveContainer>
                </div>
            ) : (
                <HistoryTable data={chartData} field={field} />
            )}
        </div>
    );
}

function HistoryTooltip({ active, label, payload, field }) {
    if (!active || !payload || payload.length === 0) return null;
    return (
        <div className="rounded-lg border border-gray-200 bg-white px-3 py-2 shadow-lg text-xs">
            <p className="mb-1 text-gray-400">{formatFullTime(label)}</p>
            <p className="font-semibold text-gray-900">{formatValue(payload[0].value, field)}</p>
        </div>
    );
}

function HistoryTable({ data, field }) {
    return (
        <div className="max-h-96 overflow-x-auto overflow-y-auto">
            <table className="w-full text-left text-sm">
                <thead className="sticky top-0 bg-white">
                    <tr className="border-b border-gray-200">
                        <th className="whitespace-nowrap px-3 py-2 font-medium text-gray-500">Time</th>
                        <th className="whitespace-nowrap px-3 py-2 font-medium text-gray-500">
                            {field.label}{field.unit ? ` (${field.unit})` : ""}
                        </th>
                    </tr>
                </thead>
                <tbody>
                    {data.map(point => (
                        <tr key={point.timestamp} className="border-b border-gray-100 last:border-0">
                            <td className="whitespace-nowrap px-3 py-2">{formatFullTime(point.timestamp)}</td>
                            <td className="px-3 py-2">{formatValue(point.value, field)}</td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}
