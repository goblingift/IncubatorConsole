import { useEffect, useState } from 'react';
import { fetchAuthSession } from 'aws-amplify/auth';
import { useDevice } from '../context/DeviceContext';

const SETTINGS_API_URL = import.meta.env.VITE_SETTINGS_API_URL;

export default function SettingsPage() {
    const { selectedDeviceId } = useDevice();
    const [targetTemperature, setTargetTemperature] = useState(36.5);
    const [targetHumidity, setTargetHumidity] = useState(50);

    const [loadingSettings, setLoadingSettings] = useState(true);
    const [saving, setSaving] = useState(false);
    const [statusMessage, setStatusMessage] = useState('');
    const [errorMessage, setErrorMessage] = useState('');

    useEffect(() => {
        const loadSettings = async () => {
            try {
                setLoadingSettings(true);
                setErrorMessage('');
                setStatusMessage('');

                const response = await fetch(`${SETTINGS_API_URL}/${selectedDeviceId}`, {
                    method: 'GET',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                });

                const data = await response.json();

                if (!response.ok) {
                    throw new Error(data?.message || `GET failed: ${response.status}`);
                }

                if (data?.target_temperature !== undefined) {
                    setTargetTemperature(Number(data.target_temperature));
                }

                if (data?.target_humidity !== undefined) {
                    setTargetHumidity(Number(data.target_humidity));
                }
            } catch (error) {
                console.error('Error loading settings:', error);
                setErrorMessage(error.message || 'Fehler beim Laden der Sollwerte');
            } finally {
                setLoadingSettings(false);
            }
        };

        loadSettings();
    }, []);

    const saveSettings = async () => {
        try {
            setSaving(true);
            setErrorMessage('');
            setStatusMessage('');

            const session = await fetchAuthSession();
            const idToken = session.tokens?.idToken?.toString();

            if (!idToken) {
                throw new Error('Kein ID-Token gefunden. Bitte neu anmelden.');
            }

            const response = await fetch(`${SETTINGS_API_URL}/${selectedDeviceId}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    Authorization: `Bearer ${idToken}`,
                },
                body: JSON.stringify({
                    device_id: selectedDeviceId,
                    target_temperature: targetTemperature,
                    target_humidity: targetHumidity,
                }),
            });

            const data = await response.json();

            if (!response.ok) {
                throw new Error(data?.message || `POST failed: ${response.status}`);
            }

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
        <div className="rounded-2xl border border-zinc-200 bg-white p-6 shadow-sm">
            <h2 className="text-xl font-semibold text-zinc-900">Sollwerte</h2>
            <p className="mt-2 text-sm text-zinc-600">
                Hier kannst du Temperatur und Luftfeuchtigkeit für den Inkubator festlegen.
            </p>

            <div className="mt-6 space-y-6">
                <div>
                    <div className="mb-2 flex items-center justify-between">
                        <label className="text-sm font-medium text-zinc-700">
                            Zieltemperatur
                        </label>
                        <span className="text-sm font-semibold text-zinc-900">
              {targetTemperature.toFixed(1)} °C
            </span>
                    </div>

                    <input
                        type="range"
                        min="30"
                        max="40"
                        step="0.1"
                        value={targetTemperature}
                        onChange={(e) => setTargetTemperature(Number(e.target.value))}
                        className="w-full"
                    />
                </div>

                <div>
                    <div className="mb-2 flex items-center justify-between">
                        <label className="text-sm font-medium text-zinc-700">
                            Ziel-Luftfeuchtigkeit
                        </label>
                        <span className="text-sm font-semibold text-zinc-900">
              {targetHumidity} %
            </span>
                    </div>

                    <input
                        type="range"
                        min="30"
                        max="80"
                        step="1"
                        value={targetHumidity}
                        onChange={(e) => setTargetHumidity(Number(e.target.value))}
                        className="w-full"
                    />
                </div>
            </div>

            <div className="mt-6 flex items-center gap-3">
                <button
                    onClick={saveSettings}
                    disabled={saving}
                    className="rounded-xl bg-zinc-900 px-4 py-2 text-sm font-medium text-white transition hover:bg-zinc-700 disabled:cursor-not-allowed disabled:opacity-50"
                >
                    {saving ? 'Speichern...' : 'Sollwerte speichern'}
                </button>

                {statusMessage && (
                    <p className="text-sm text-green-600">{statusMessage}</p>
                )}

                {errorMessage && (
                    <p className="text-sm text-red-600">{errorMessage}</p>
                )}
            </div>
        </div>
    );
}