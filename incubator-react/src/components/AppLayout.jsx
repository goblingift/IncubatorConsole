import { Link, Outlet, useLocation } from 'react-router-dom';
import DeviceSelector from './DeviceSelector';

export default function AppLayout() {
    const location = useLocation();

    const navLinkClass = (path) =>
        `px-4 py-2 rounded-lg font-medium transition ${
            location.pathname === path
                ? 'bg-blue-600 text-white'
                : 'bg-white text-gray-700 hover:bg-blue-100'
        }`;

    return (
        <div className="min-h-screen bg-gradient-to-br from-blue-50 to-indigo-100 p-8">
            <header className="relative mb-8">
                <div className="text-center">
                    <h1 className="text-4xl font-bold text-gray-800">Incubator Dashboard</h1>
                    <p className="text-lg text-gray-600 mt-2">
                        Version: {import.meta.env.APP_VERSION}
                    </p>

                    <nav className="flex justify-center gap-4 mt-6">
                        <Link to="/" className={navLinkClass('/')}>
                            Dashboard
                        </Link>
                        <Link to="/settings" className={navLinkClass('/settings')}>
                            Einstellungen
                        </Link>
                    </nav>
                </div>

                <div className="absolute top-0 right-0">
                    <DeviceSelector />
                </div>
            </header>

            <main>
                <Outlet />
            </main>
        </div>
    );
}