import SensorDashboard from '../components/SensorDashboard';
import MeasurementHistoryChart from '../components/MeasurementHistoryChart';

export default function DashboardPage() {
    return (
        <div className="max-w-6xl mx-auto space-y-6">
            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-4">Current Values</h2>
                <SensorDashboard />
            </div>
            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-4">Measurement History</h2>
                <MeasurementHistoryChart />
            </div>
        </div>
    );
}