import { useEffect, useState } from 'react';
import { Navigate, useLocation } from 'react-router-dom';
import { getCurrentUser } from 'aws-amplify/auth';

export default function ProtectedRoute({ children }) {
    const [status, setStatus] = useState('loading');
    const location = useLocation();

    useEffect(() => {
        let isMounted = true;

        async function checkAuth() {
            try {
                await getCurrentUser();
                if (isMounted) setStatus('authenticated');
            } catch {
                if (isMounted) setStatus('unauthenticated');
            }
        }

        checkAuth();

        return () => {
            isMounted = false;
        };
    }, []);

    if (status === 'loading') {
        return <p className="text-center text-gray-500">Checking login…</p>;
    }

    if (status === 'unauthenticated') {
        return <Navigate to="/login" replace state={{ from: location }} />;
    }

    return children;
}