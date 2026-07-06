import { useEffect, useRef, useState } from 'react';
import { useDevice } from '../context/DeviceContext';

export default function DeviceSelector() {
    const { devices, selectedDevice, selectedDeviceId, setSelectedDevice, loading } = useDevice();
    const [open, setOpen] = useState(false);
    const ref = useRef(null);

    useEffect(() => {
        const handler = (e) => {
            if (ref.current && !ref.current.contains(e.target)) setOpen(false);
        };
        document.addEventListener('mousedown', handler);
        return () => document.removeEventListener('mousedown', handler);
    }, []);

    const displayName = selectedDevice?.name ?? selectedDeviceId ?? '—';

    return (
        <div ref={ref} className="relative">
            <button
                onClick={() => setOpen(o => !o)}
                className="flex items-center gap-2 bg-white border border-gray-200 rounded-xl px-3 py-2 text-sm font-medium text-gray-700 shadow-sm hover:bg-gray-50 transition"
            >
                <DeviceIcon />
                <span>{loading ? 'Loading...' : displayName}</span>
                <ChevronIcon open={open} />
            </button>

            {open && (
                <div className="absolute right-0 mt-2 w-56 bg-white border border-gray-200 rounded-xl shadow-lg z-50 overflow-hidden">
                    {devices.length === 0 ? (
                        <p className="px-4 py-3 text-sm text-gray-400">No devices found</p>
                    ) : (
                        devices.map(device => {
                            const id   = device.device_id ?? device;
                            const name = device.name ?? id;
                            return (
                                <button
                                    key={id}
                                    onClick={() => { setSelectedDevice(id); setOpen(false); }}
                                    className={`w-full flex items-center gap-2 px-4 py-3 text-sm text-left transition hover:bg-blue-50 ${
                                        id === selectedDeviceId ? 'bg-blue-50 font-semibold text-blue-700' : 'text-gray-700'
                                    }`}
                                >
                                    <DeviceIcon small />
                                    {name}
                                </button>
                            );
                        })
                    )}
                </div>
            )}
        </div>
    );
}

function DeviceIcon({ small }) {
    const size = small ? 14 : 16;
    return (
        <svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="shrink-0 text-gray-500">
            <rect x="2" y="3" width="20" height="14" rx="2" />
            <path d="M8 21h8M12 17v4" />
        </svg>
    );
}

function ChevronIcon({ open }) {
    return (
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round"
            className={`shrink-0 text-gray-400 transition-transform ${open ? 'rotate-180' : ''}`}>
            <path d="M6 9l6 6 6-6" />
        </svg>
    );
}