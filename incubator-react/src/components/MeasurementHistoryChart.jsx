import { useMemo, useState } from "react";
import {
    ResponsiveContainer,
    LineChart,
    Line,
    XAxis,
    YAxis,
    CartesianGrid,
    Tooltip,
    Legend,
} from "recharts";
import { useDevice } from "../context/DeviceContext";
import { useMeasurementHistory } from "../hooks/useMeasurementHistory";
import { MAX_VISIBLE_SERIES, assignSeriesColors } from "../hooks/useSeriesColors";
import { MEASUREMENT_FIELDS, MEASUREMENT_FIELDS_BY_KEY, DEFAULT_VISIBLE_FIELDS } from "../constants/measurementFields";

const RANGES = [
    { key: "1h", label: "Last Hour" },
    { key: "24h", label: "Last 24h" },
    { key: "7d", label: "Last 7 Days" },
];

const INK = { secondary: "#52514e", muted: "#898781", grid: "#e1e0d9", axis: "#c3c2b7" };

function formatTimeTick(epochSeconds, range) {
    const date = new Date(Number(epochSeconds) * 1000);
    return range === "7d"
        ? date.toLocaleDateString("en-US", { month: "short", day: "numeric" })
        : date.toLocaleTimeString("en-US", { hour: "2-digit", minute: "2-digit" });
}

function formatFullTime(epochSeconds) {
    return new Date(Number(epochSeconds) * 1000).toLocaleString("en-US");
}

function buildChartData(points, visibleFields) {
    const ranges = {};
    for (const field of visibleFields) {
        if (field.kind !== "numeric") continue;
        const values = points
            .map(p => p[field.key])
            .filter(v => v !== undefined && v !== null)
            .map(Number);
        ranges[field.key] = values.length ? { min: Math.min(...values), max: Math.max(...values) } : null;
    }

    return points.map(point => {
        const row = { timestamp: Number(point.timestamp) };
        for (const field of visibleFields) {
            const raw = point[field.key];
            if (raw === undefined || raw === null) continue;
            const value = Number(raw);
            row[`${field.key}__raw`] = value;
            if (field.kind === "boolean") {
                row[`${field.key}__norm`] = value;
            } else {
                const range = ranges[field.key];
                row[`${field.key}__norm`] = range && range.max > range.min
                    ? (value - range.min) / (range.max - range.min)
                    : 0.5;
            }
        }
        return row;
    });
}

export default function MeasurementHistoryChart() {
    const { selectedDeviceId } = useDevice();
    const [range, setRange] = useState("24h");
    const [visibleKeys, setVisibleKeys] = useState(DEFAULT_VISIBLE_FIELDS);
    const [colorByKey, setColorByKey] = useState(() => assignSeriesColors(DEFAULT_VISIBLE_FIELDS, {}));
    const [view, setView] = useState("chart");

    const { data, loading, error } = useMeasurementHistory(selectedDeviceId, range);

    const visibleFields = useMemo(
        () => MEASUREMENT_FIELDS.filter(f => visibleKeys.includes(f.key)),
        [visibleKeys]
    );
    const chartData = useMemo(
        () => (data ? buildChartData(data, visibleFields) : []),
        [data, visibleFields]
    );

    const toggleField = (key) => {
        const isActive = visibleKeys.includes(key);
        if (!isActive && visibleKeys.length >= MAX_VISIBLE_SERIES) return;

        const next = isActive ? visibleKeys.filter(k => k !== key) : [...visibleKeys, key];
        setVisibleKeys(next);
        setColorByKey(prev => assignSeriesColors(next, prev));
    };

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
                {MEASUREMENT_FIELDS.map(field => {
                    const active = visibleKeys.includes(field.key);
                    const disabled = !active && visibleKeys.length >= MAX_VISIBLE_SERIES;
                    return (
                        <button
                            key={field.key}
                            onClick={() => toggleField(field.key)}
                            disabled={disabled}
                            title={disabled ? `Up to ${MAX_VISIBLE_SERIES} metrics can be shown at once — hide one to add another.` : undefined}
                            className={`inline-flex items-center gap-1.5 rounded-full px-3 py-1 text-xs font-medium border transition ${
                                active
                                    ? "border-transparent text-white"
                                    : disabled
                                        ? "border-gray-100 text-gray-300 cursor-not-allowed"
                                        : "border-gray-200 text-gray-600 hover:border-gray-300"
                            }`}
                            style={active ? { backgroundColor: colorByKey[field.key] } : undefined}
                        >
                            {field.label}
                        </button>
                    );
                })}
            </div>

            {error && data && (
                <p className="text-xs text-red-500">Couldn't refresh: {error}</p>
            )}

            {visibleKeys.length === 0 ? (
                <p className="text-gray-400 text-sm">Select at least one metric to plot.</p>
            ) : data && data.length === 0 ? (
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
                                domain={[0, 1]}
                                ticks={[0, 0.25, 0.5, 0.75, 1]}
                                tickFormatter={(v) => `${Math.round(v * 100)}%`}
                                stroke={INK.axis}
                                tick={{ fill: INK.muted, fontSize: 12 }}
                                width={44}
                            />
                            <Tooltip content={<HistoryTooltip />} />
                            <Legend content={(props) => <HistoryLegend payload={props.payload} />} />
                            {visibleFields.map(field => (
                                <Line
                                    key={field.key}
                                    dataKey={`${field.key}__norm`}
                                    name={field.label}
                                    stroke={colorByKey[field.key]}
                                    strokeWidth={2}
                                    dot={false}
                                    activeDot={{ r: 4 }}
                                    type={field.kind === "boolean" ? "stepAfter" : "monotone"}
                                    isAnimationActive={false}
                                    connectNulls
                                />
                            ))}
                        </LineChart>
                    </ResponsiveContainer>
                </div>
            ) : (
                <HistoryTable data={data} visibleFields={visibleFields} />
            )}
        </div>
    );
}

function HistoryLegend({ payload }) {
    if (!payload || payload.length === 0) return null;
    return (
        <div className="flex flex-wrap justify-center gap-x-4 gap-y-1 pt-2">
            {payload.map(entry => (
                <span key={entry.dataKey} className="inline-flex items-center gap-1.5 text-xs" style={{ color: INK.secondary }}>
                    <span className="inline-block h-0.5 w-3 rounded-full" style={{ backgroundColor: entry.color }} />
                    {entry.value}
                </span>
            ))}
        </div>
    );
}

function HistoryTooltip({ active, label, payload }) {
    if (!active || !payload || payload.length === 0) return null;
    return (
        <div className="rounded-lg border border-gray-200 bg-white px-3 py-2 shadow-lg text-xs">
            <p className="mb-1 text-gray-400">{formatFullTime(label)}</p>
            <div className="space-y-1">
                {payload.map(entry => {
                    const key = entry.dataKey.replace("__norm", "");
                    const field = MEASUREMENT_FIELDS_BY_KEY[key];
                    const raw = entry.payload[`${key}__raw`];
                    return (
                        <div key={entry.dataKey} className="flex items-center gap-2">
                            <span className="inline-block h-0.5 w-3 rounded-full" style={{ backgroundColor: entry.color }} />
                            <span className="text-gray-500">{field?.label ?? key}</span>
                            <span className="ml-auto font-semibold text-gray-900">
                                {field?.kind === "boolean" ? (raw === 1 ? "ON" : "OFF") : `${raw} ${field?.unit ?? ""}`}
                            </span>
                        </div>
                    );
                })}
            </div>
        </div>
    );
}

function HistoryTable({ data, visibleFields }) {
    return (
        <div className="max-h-96 overflow-x-auto overflow-y-auto">
            <table className="w-full text-left text-sm">
                <thead className="sticky top-0 bg-white">
                    <tr className="border-b border-gray-200">
                        <th className="whitespace-nowrap px-3 py-2 font-medium text-gray-500">Time</th>
                        {visibleFields.map(field => (
                            <th key={field.key} className="whitespace-nowrap px-3 py-2 font-medium text-gray-500">
                                {field.label}{field.unit ? ` (${field.unit})` : ""}
                            </th>
                        ))}
                    </tr>
                </thead>
                <tbody>
                    {data.map(point => (
                        <tr key={point.timestamp} className="border-b border-gray-100 last:border-0">
                            <td className="whitespace-nowrap px-3 py-2">{formatFullTime(point.timestamp)}</td>
                            {visibleFields.map(field => (
                                <td key={field.key} className="px-3 py-2">
                                    {point[field.key] === undefined || point[field.key] === null
                                        ? "—"
                                        : field.kind === "boolean"
                                            ? (Number(point[field.key]) === 1 ? "ON" : "OFF")
                                            : `${point[field.key]}${field.unit ? ` ${field.unit}` : ""}`}
                                </td>
                            ))}
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}
