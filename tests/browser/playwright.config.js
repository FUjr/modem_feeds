'use strict';

const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
	testDir: './specs',
	timeout: 45000,
	fullyParallel: false,
	workers: 1,
	reporter: [ [ 'line' ], [ 'json', { outputFile: 'test-results/results.json' } ] ],
	use: {
		baseURL: process.env.QMODEM_BASE_URL || 'https://10.96.210.191',
		browserName: 'chromium',
		launchOptions: {
			executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || '/usr/bin/chromium'
		},
		headless: true,
		ignoreHTTPSErrors: true,
		screenshot: 'only-on-failure',
		trace: 'retain-on-failure'
	},
	projects: [
		{
			name: 'desktop',
			use: { viewport: { width: 1280, height: 720 } }
		},
		{
			name: 'mobile',
			use: { ...devices['Pixel 5'] }
		}
	],
	outputDir: 'test-results/artifacts'
});
