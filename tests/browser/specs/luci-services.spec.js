'use strict';

const { test, expect } = require('@playwright/test');

const routes = {
	sms: '/cgi-bin/luci/admin/modem/qmodem/sms',
	voip: '/cgi-bin/luci/admin/modem/qmodem/qmodem-voip',
	sip: '/cgi-bin/luci/admin/modem/qmodem/qmodem-sipd'
};

async function login(page) {
	await page.goto('/cgi-bin/luci/', { waitUntil: 'networkidle' });
	const password = page.locator('input[name="luci_password"]');
	if (!(await password.count()))
		return;
	await page.locator('input[name="luci_username"]').fill(process.env.QMODEM_USERNAME || 'admin');
	await password.fill(process.env.QMODEM_PASSWORD || '');
	await Promise.all([
		page.waitForNavigation({ waitUntil: 'domcontentloaded' }),
		page.locator('button[type="submit"], input[type="submit"]').first().click()
	]);
	await expect(password).not.toBeVisible();
	await expect(page.locator('body')).not.toHaveClass(/node-main-login/);
}

async function openPage(page, route) {
	const response = await page.goto(route, { waitUntil: 'networkidle' });
	expect(response, `no response for ${route}`).not.toBeNull();
	expect(response.status(), `${route} returned HTTP ${response.status()}`).toBeLessThan(400);
	await expect(page.locator('.cbi-map, .qvoip-page').first()).toBeVisible();
	await expect(page.locator('body')).not.toContainText('RPCError');
}

test('SMS, VoIP and SIPD pages render with separated settings', async ({ page }, testInfo) => {
	const pageErrors = [];
	page.on('pageerror', (error) => pageErrors.push(error.message));

	await login(page);
	pageErrors.length = 0;

	await openPage(page, routes.sms);
	await expect(page.locator('#sms_modem_selector, .alert-message.warning').first()).toBeVisible();
	await page.screenshot({ path: testInfo.outputPath('sms.png'), fullPage: true });

	await openPage(page, routes.voip);
	await expect(page.locator('[id="cbid.qmodem_voip.main.enabled"]')).toBeVisible();
	await expect(page.locator('[id^="cbid.qmodem_sip."]')).toHaveCount(0);
	await page.screenshot({ path: testInfo.outputPath('voip.png'), fullPage: true });

	await openPage(page, routes.sip);
	await expect(page.locator('[id="cbid.qmodem_sip.inbound.enabled"]')).toBeVisible();
	await expect(page.locator('[id="cbid.qmodem_sip.outbound.enabled"]')).toBeVisible();
	await expect(page.locator('[id="cbid.qmodem_sip.main.rtp_start"]')).toBeVisible();
	await page.screenshot({ path: testInfo.outputPath('sipd.png'), fullPage: true });

	expect(pageErrors, pageErrors.join('\n')).toEqual([]);
});
