import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './components/App';
import { AuthenticationProvider, WalletProvider } from './utils/context';
import { HashRouter as Router } from 'react-router-dom';
import GA from './utils/GoogleAnalytics';
import 'antd/dist/antd.min.css';

ReactDOM.render(
    <AuthenticationProvider>
        <WalletProvider>
            <Router>
                {GA.init() && <GA.RouteTracker />}
                <App />
            </Router>
        </WalletProvider>
    </AuthenticationProvider>,
    document.getElementById('root'),
);

if (module.hot) {
    module.hot.accept();
}
