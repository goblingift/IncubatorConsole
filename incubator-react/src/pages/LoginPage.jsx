import { Authenticator } from '@aws-amplify/ui-react';
import { Navigate, useLocation } from 'react-router-dom';

export default function LoginPage() {
    const location = useLocation();
    const from = location.state?.from?.pathname || '/settings';

    return (
        <div className="max-w-4xl mx-auto px-4">
            <div className="relative mx-auto max-w-md">
                <div className="absolute -right-36 top-10 hidden md:block">
                    <div className="relative w-72 rounded-[999px] border-4 border-black bg-white px-6 py-5 shadow-lg">
                        <p className="text-center text-xl font-medium leading-snug text-red-500">
                            Benutze den Test-Account:
                        </p>

                        <div className="mt-2 space-y-1 text-center text-lg leading-snug text-red-500">
                            <p className="break-words">
                                <span className="font-medium">Email:</span><br />
                                test-incubator@gmx.net
                            </p>
                            <p>
                                <span className="font-medium">PW:</span> Secret123!
                            </p>
                        </div>

                        <div className="absolute left-[-26px] top-1/2 -translate-y-1/2 w-0 h-0 border-t-[20px] border-t-transparent border-b-[20px] border-b-transparent border-r-[28px] border-r-black" />
                        <div className="absolute left-[-20px] top-1/2 -translate-y-1/2 w-0 h-0 border-t-[16px] border-t-transparent border-b-[16px] border-b-transparent border-r-[22px] border-r-white" />
                    </div>
                </div>

                <div className="mb-4 md:hidden">
                    <div className="rounded-2xl border-2 border-black bg-white px-4 py-3 shadow-md">
                        <p className="text-center text-sm font-medium leading-relaxed text-red-500 break-words">
                            Test-Account:<br />
                            Email: test-incubator@gmx.net<br />
                            PW: Secret123!
                        </p>
                    </div>
                </div>

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
        </div>
    );
}