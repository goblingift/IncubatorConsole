import SensorDashboard from '../components/SensorDashboard';

export default function DashboardPage() {
    return (
        <div className="max-w-6xl mx-auto">
            <div className="bg-white p-8 rounded-2xl shadow-xl">
                <h2 className="text-2xl font-semibold text-gray-700 mb-4">Current Values</h2>
                <SensorDashboard />
            </div>
        </div>
    );
}