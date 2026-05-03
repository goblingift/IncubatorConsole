import { createContext, useContext, useEffect, useRef, useState } from 'react';

const DEVICES_API_URL = import.meta.env.VITE_DEVICES_API_URL;
const STORAGE_KEY = 'selectedDeviceId';

const DeviceContext = createContext(null);

export function DeviceProvider({ children }) {
    const [devices, setDevices]               = useState([]);
    const [loading, setLoading]               = useState(true);
    const [error, setError]                   = useState(null);
    const [selectedDeviceId, setSelectedDeviceIdState] = useState(
        () => localStorage.getItem(STORAGE_KEY) ?? null
    );

    useEffect(() => {
        if (!DEVICES_API_URL) {
            setLoading(false);
            return;
        }
        fetch(DEVICES_API_URL)
            .then(r => r.json())
            .then(data => {
                setDevices(data);
                if (!localStorage.getItem(STORAGE_KEY) && data.length > 0) {
                    const firstId = data[0].device_id ?? data[0];
                    setSelectedDeviceIdState(firstId);
                    localStorage.setItem(STORAGE_KEY, firstId);
                }
            })
            .catch(err => setError(err.message))
            .finally(() => setLoading(false));
    }, []);

    const setSelectedDevice = (deviceId) => {
        setSelectedDeviceIdState(deviceId);
        localStorage.setItem(STORAGE_KEY, deviceId);
    };

    const selectedDevice = devices.find(d => (d.device_id ?? d) === selectedDeviceId) ?? null;

    return (
        <DeviceContext.Provider value={{ devices, selectedDevice, selectedDeviceId, setSelectedDevice, loading, error }}>
            {children}
        </DeviceContext.Provider>
    );
}

export function useDevice() {
    return useContext(DeviceContext);
}