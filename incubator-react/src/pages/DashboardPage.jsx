import { useState } from 'react';
import SensorDashboard from '../components/SensorDashboard';

export default function DashboardPage() {
    const [alarm, setAlarm] = useState(false);

    return (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 max-w-6xl mx-auto">
            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-4">Aktuelle Werte</h2>
                <SensorDashboard />
            </div>

            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-4">Alarm</h2>
                <button
                    onClick={() => setAlarm(!alarm)}
                    className={`w-full py-3 px-6 rounded-xl font-bold ${
                        alarm
                            ? 'bg-red-500 text-white'
                            : 'bg-gray-200 text-gray-700 hover:bg-red-100'
                    }`}
                >
                    {alarm ? 'Alarm AKTIV' : 'Alarm EIN'}
                </button>
            </div>
        </div>
    );
}