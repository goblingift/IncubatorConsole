import { useState, useEffect, useRef } from "react";

const BASE_URL = import.meta.env.VITE_MEASUREMENTS_RANGE_API_URL;

export function useMeasurementHistory(deviceId, range, intervalMs = 60000) {
    const [data, setData]       = useState(null);
    const [loading, setLoading] = useState(true);
    const [error, setError]     = useState(null);
    const requestId = useRef(0);

    useEffect(() => {
        if (!deviceId) {
            setData(null);
            setLoading(false);
            setError(null);
            return;
        }

        setLoading(true);
        setError(null);
        const currentRequest = ++requestId.current;

        const fetchData = async () => {
            try {
                const res  = await fetch(`${BASE_URL}/${deviceId}?range=${range}`);
                const json = await res.json();
                const body = typeof json.body === "string" ? JSON.parse(json.body) : json;
                if (!res.ok) throw new Error(body?.error || `Request failed: ${res.status}`);

                if (currentRequest === requestId.current) {
                    setData(body);
                    setError(null);
                }
            } catch (err) {
                if (currentRequest === requestId.current) setError(err.message);
            } finally {
                if (currentRequest === requestId.current) setLoading(false);
            }
        };

        // Note: `data` is intentionally not reset here — the chart holds its
        // previous render (dimmed via `loading`) instead of flashing empty
        // while a refetch (poll, range switch, device switch) is in flight.
        fetchData();
        const interval = setInterval(fetchData, intervalMs);
        return () => clearInterval(interval);
    }, [deviceId, range, intervalMs]);

    return { data, loading, error };
}
