import { useEffect, useState } from 'react';
import { fetchAuthSession } from 'aws-amplify/auth';
import { useDevice } from '../context/DeviceContext';

const SETTINGS_API_URL = import.meta.env.VITE_SETTINGS_API_URL;

const DEFAULTS = {
    temperature_target: 37.5,
    temperature_min:    36.0,
    temperature_max:    39.0,
    humidity_target:    55,
    humidity_min:       45,
    humidity_max:       70,
    co2_max:            5000,
    light_max:          12,
    gyroscope_x_max:    10,
    gyroscope_y_max:    10,
    gyroscope_z_max:    10,
    sound_max:          80,
    weight_min:         0,
    weight_max:         5000,
};

export default function SettingsPage() {
    const { selectedDeviceId } = useDevice();
    const [settings, setSettings]           = useState(DEFAULTS);
    const [loadingSettings, setLoadingSettings] = useState(true);
    const [saving, setSaving]               = useState(false);
    const [statusMessage, setStatusMessage] = useState('');
    const [errorMessage, setErrorMessage]   = useState('');

    useEffect(() => {
        setSettings(DEFAULTS);
        setStatusMessage('');
        setErrorMessage('');

        if (!selectedDeviceId) { setLoadingSettings(false); return; }

        setLoadingSettings(true);

        const loadSettings = async () => {
            try {

                const response = await fetch(`${SETTINGS_API_URL}/${selectedDeviceId}`, {
                    method: 'GET',
                    headers: { 'Content-Type': 'application/json' },
                });

                const data = await response.json();
                if (!response.ok) throw new Error(data?.message || `GET failed: ${response.status}`);

                setSettings(prev => ({
                    ...prev,
                    ...Object.fromEntries(
                        Object.entries(data)
                            .filter(([k, v]) => k in DEFAULTS && v !== undefined && v !== null)
                            .map(([k, v]) => [k, Number(v)])
                    ),
                }));
            } catch (error) {
                console.error('Error loading settings:', error);
                setErrorMessage(error.message || 'Fehler beim Laden der Sollwerte');
            } finally {
                setLoadingSettings(false);
            }
        };

        loadSettings();
    }, [selectedDeviceId]);

    const set = (field) => (e) =>
        setSettings(prev => ({ ...prev, [field]: Number(e.target.value) }));

    const saveSettings = async () => {
        try {
            setSaving(true);
            setErrorMessage('');
            setStatusMessage('');

            const session = await fetchAuthSession();
            const idToken = session.tokens?.idToken?.toString();
            if (!idToken) throw new Error('Kein ID-Token gefunden. Bitte neu anmelden.');

            const response = await fetch(`${SETTINGS_API_URL}/${selectedDeviceId}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    Authorization: `Bearer ${idToken}`,
                },
                body: JSON.stringify({ device_id: selectedDeviceId, ...settings }),
            });

            const data = await response.json();
            if (!response.ok) throw new Error(data?.message || `POST failed: ${response.status}`);

            setStatusMessage('Sollwerte erfolgreich gespeichert');
        } catch (error) {
            console.error('Error saving settings:', error);
            setErrorMessage(error.message || 'Fehler beim Speichern der Sollwerte');
        } finally {
            setSaving(false);
        }
    };

    if (loadingSettings) {
        return (
            <div className="rounded-2xl border border-zinc-200 bg-white p-6 shadow-sm">
                <h2 className="text-xl font-semibold text-zinc-900">Sollwerte</h2>
                <p className="mt-4 text-zinc-600">Lade gespeicherte Sollwerte...</p>
            </div>
        );
    }

    return (
        <div className="rounded-2xl border border-zinc-200 bg-white p-6 shadow-sm space-y-8 max-w-2xl mx-auto">
            <div>
                <h2 className="text-xl font-semibold text-zinc-900">Sollwerte</h2>
                <p className="mt-1 text-sm text-zinc-500">
                    Grenzwerte und Zielwerte für den Inkubator konfigurieren.
                </p>
            </div>

            <Section title="Temperatur">
                <SliderRow label="Zielwert" value={settings.temperature_target} onChange={set('temperature_target')} min={-10} max={60} step={0.1} unit="°C" />
                <SliderRow label="Minimum"  value={settings.temperature_min}    onChange={set('temperature_min')}    min={-10} max={60} step={0.1} unit="°C" />
                <SliderRow label="Maximum"  value={settings.temperature_max}    onChange={set('temperature_max')}    min={-10} max={60} step={0.1} unit="°C" />
            </Section>

            <Section title="Luftfeuchte">
                <SliderRow label="Zielwert" value={settings.humidity_target} onChange={set('humidity_target')} min={0} max={100} step={1} unit="%" />
                <SliderRow label="Minimum"  value={settings.humidity_min}    onChange={set('humidity_min')}    min={0} max={100} step={1} unit="%" />
                <SliderRow label="Maximum"  value={settings.humidity_max}    onChange={set('humidity_max')}    min={0} max={100} step={1} unit="%" />
            </Section>

            <Section title="CO₂">
                <SliderRow label="Maximum" value={settings.co2_max} onChange={set('co2_max')} min={400} max={40000} step={100} unit="ppm" />
            </Section>

            <Section title="Beleuchtung">
                <SliderRow label="Lichtstunden pro Tag" value={settings.light_max} onChange={set('light_max')} min={0} max={24} step={0.5} unit="h" />
            </Section>

            <Section title="Lautstärke">
                <SliderRow label="Maximum" value={settings.sound_max} onChange={set('sound_max')} min={0} max={120} step={1} unit="dB" />
            </Section>

            <Section title="Gewicht">
                <SliderRow label="Minimum" value={settings.weight_min} onChange={set('weight_min')} min={0} max={20000} step={10} unit="g" />
                <SliderRow label="Maximum" value={settings.weight_max} onChange={set('weight_max')} min={0} max={20000} step={10} unit="g" />
            </Section>

            <Section title="Gyroskop (max. Auslenkung)">
                <NumberRow label="X" value={settings.gyroscope_x_max} onChange={set('gyroscope_x_max')} unit="°/s" />
                <NumberRow label="Y" value={settings.gyroscope_y_max} onChange={set('gyroscope_y_max')} unit="°/s" />
                <NumberRow label="Z" value={settings.gyroscope_z_max} onChange={set('gyroscope_z_max')} unit="°/s" />
            </Section>

            <div className="flex items-center gap-3 pt-2 border-t border-zinc-100">
                <button
                    onClick={saveSettings}
                    disabled={saving || !selectedDeviceId}
                    className="rounded-xl bg-zinc-900 px-4 py-2 text-sm font-medium text-white transition hover:bg-zinc-700 disabled:cursor-not-allowed disabled:opacity-50"
                >
                    {saving ? 'Speichern...' : 'Sollwerte speichern'}
                </button>
                {statusMessage && <p className="text-sm text-green-600">{statusMessage}</p>}
                {errorMessage  && <p className="text-sm text-red-600">{errorMessage}</p>}
            </div>
        </div>
    );
}

function Section({ title, children }) {
    return (
        <div>
            <h3 className="text-xs font-semibold text-zinc-400 uppercase tracking-widest mb-3">{title}</h3>
            <div className="space-y-4">{children}</div>
        </div>
    );
}

function SliderRow({ label, value, onChange, min, max, step, unit }) {
    const display = step < 1 ? value.toFixed(1) : value;
    return (
        <div>
            <div className="flex items-center justify-between mb-1">
                <label className="text-sm font-medium text-zinc-700">{label}</label>
                <span className="text-sm font-semibold text-zinc-900">{display} {unit}</span>
            </div>
            <input type="range" min={min} max={max} step={step} value={value} onChange={onChange} className="w-full" />
        </div>
    );
}

function NumberRow({ label, value, onChange, min, unit }) {
    return (
        <div className="flex items-center gap-4">
            <label className="text-sm font-medium text-zinc-700 w-6">{label}</label>
            <input
                type="number"
                min={min}
                value={value}
                onChange={onChange}
                className="flex-1 rounded-lg border border-zinc-200 px-3 py-1.5 text-sm text-zinc-900 focus:outline-none focus:ring-2 focus:ring-zinc-400"
            />
            {unit && <span className="text-sm text-zinc-500 w-8">{unit}</span>}
        </div>
    );
}