import { useState, useEffect, useCallback } from "react";
import { fetchAuthSession } from "aws-amplify/auth";

const BASE_URL = import.meta.env.VITE_ALERTS_API_URL;

export function useAlerts(deviceId, intervalMs = 15000) {
    const [data, setData]       = useState(null);
    const [loading, setLoading] = useState(true);
    const [error, setError]     = useState(null);

    const fetchAlerts = useCallback(async () => {
        try {
            const session = await fetchAuthSession();
            const idToken = session.tokens?.idToken?.toString();

            const res = await fetch(`${BASE_URL}/${deviceId}`, {
                headers: idToken ? { Authorization: `Bearer ${idToken}` } : {},
            });
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
    }, [deviceId]);

    useEffect(() => {
        setData(null);
        setLoading(true);
        setError(null);

        if (!deviceId) {
            setLoading(false);
            return;
        }

        fetchAlerts();
        const interval = setInterval(fetchAlerts, intervalMs);
        return () => clearInterval(interval);
    }, [deviceId, intervalMs, fetchAlerts]);

    return { data, loading, error };
}
