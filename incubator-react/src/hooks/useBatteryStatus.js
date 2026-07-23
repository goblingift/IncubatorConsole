import { useState, useEffect } from "react";

const BASE_URL = import.meta.env.VITE_BATTERY_STATUS_API_URL;

export function useBatteryStatus(deviceId, intervalMs = 60000) {
    const [data, setData]       = useState(null);
    const [loading, setLoading] = useState(true);
    const [error, setError]     = useState(null);

    useEffect(() => {
        setData(null);
        setLoading(true);
        setError(null);

        if (!deviceId) {
            setLoading(false);
            return;
        }

        const fetchData = async () => {
            try {
                const res  = await fetch(`${BASE_URL}/${deviceId}`);
                const json = await res.json();
                const body = typeof json.body === "string" ? JSON.parse(json.body) : json;
                if (!res.ok) throw new Error(body?.error || `Request failed: ${res.status}`);

                setData(body);
                setError(null);

                // Logged as a single-line JSON string (not a console.log(obj)
                // tree) so a multi-hour run's console history can be
                // selected and copied as one reading per line.
                console.log(JSON.stringify({
                    logged_at: new Date().toISOString(),
                    device_id: deviceId,
                    status: body.status,
                    battery_voltage: body.battery_voltage,
                    battery_percent: body.battery_percent,
                    drain_rate_percent_per_hour: body.drain_rate_percent_per_hour,
                    remaining_hours: body.remaining_hours,
                    predicted_shutdown_at: body.predicted_shutdown_at,
                }));
            } catch (err) {
                setError(err.message);
            } finally {
                setLoading(false);
            }
        };

        fetchData();
        const interval = setInterval(fetchData, intervalMs);
        return () => clearInterval(interval);
    }, [deviceId, intervalMs]);

    return { data, loading, error };
}
