import { useState, useEffect } from "react";

const BASE_URL = import.meta.env.VITE_LIGHT_AVERAGE_STATUS_API_URL;

export function useLightAverage(deviceId, intervalMs = 60000) {
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
