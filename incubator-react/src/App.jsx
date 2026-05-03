import { Routes, Route } from 'react-router-dom';
import AppLayout from './components/AppLayout';
import DashboardPage from './pages/DashboardPage';
import SettingsPage from './pages/SettingsPage';
import LoginPage from './pages/LoginPage';
import ProtectedRoute from './components/ProtectedRoute';
import { DeviceProvider } from './context/DeviceContext';

function App() {
    return (
        <DeviceProvider>
            <Routes>
                <Route path="/" element={<AppLayout />}>
                    <Route index element={<DashboardPage />} />
                    <Route path="login" element={<LoginPage />} />
                    <Route
                        path="settings"
                        element={
                            <ProtectedRoute>
                                <SettingsPage />
                            </ProtectedRoute>
                        }
                    />
                </Route>
            </Routes>
        </DeviceProvider>
    );
}

export default App;