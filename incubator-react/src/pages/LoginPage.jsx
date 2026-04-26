import { Authenticator } from '@aws-amplify/ui-react';
import { Navigate, useLocation } from 'react-router-dom';

export default function LoginPage() {
    const location = useLocation();
    const from = location.state?.from?.pathname || '/settings';

    return (
        <div className="max-w-md mx-auto px-4">
            <div className="bg-white p-6 md:p-8 rounded-2xl shadow-xl overflow-hidden">
                <h2 className="text-2xl font-semibold text-gray-700 mb-6 text-center">
                    Anmeldung
                </h2>

                <div className="auth-wrapper">
                    <Authenticator loginMechanisms={['email']} hideSignUp={true}>
                        {({ user }) => {
                            if (user) {
                                return <Navigate to={from} replace />;
                            }

                            return null;
                        }}
                    </Authenticator>
                </div>
            </div>
        </div>
    );
}