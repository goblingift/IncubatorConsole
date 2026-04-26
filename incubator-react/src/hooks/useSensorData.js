import { useState, useEffect } from "react";

export function useSensorData(intervalMs = 10000) {
    const [data, setData]       = useState(null);
    const [loading, setLoading] = useState(true);
    const [error, setError]     = useState(null);

    useEffect(() => {
        const fetchData = async () => {
            try {
                const res  = await fetch(import.meta.env.VITE_API_URL);
                const json = await res.json();
                // API Gateway wraps body as a string → parse it
                const body = typeof json.body === "string" ? JSON.parse(json.body) : json;
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
    }, [intervalMs]);

    return { data, loading, error };
}