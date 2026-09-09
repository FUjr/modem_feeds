'use strict';
'require view';
'require poll';
'require dom';
'require ui';
'require form';
'require uci';
'require rpc as luciRpc';
'require qmodem-voip.rpc as rpc';
'require qmodem-voip.reducer as reducer';
'require qmodem-voip.media as media';
'require qmodem-voip.surface as surface';

const POLL_SECONDS = 1, WORKLET_URL = L.resource('qmodem-voip/audio-worklet.js');
const STATE_LABELS = {
	disabled: _('Disabled'), idle: _('Ready'), outgoing_setup: _('Outgoing'), incoming_ringing: _('Incoming call'), early_media: _('Early media'), active: _('Active'), terminating: _('Ending call'), recovering: _('Recovering'), fault: _('Fault')
};
const STATE_STYLES = {
	outgoing_setup: 'warning', incoming_ringing: 'warning', early_media: 'warning', active: 'success', terminating: 'warning', recovering: 'warning', fault: 'error'
};

document.head.appendChild(E('link', { rel: 'stylesheet', type: 'text/css', href: L.resource('qmodem-voip/qmodem-voip.css') }));

return view.extend({
	load() {
		this.state = reducer.initialState();
		this.rpc = rpc;
		this.media = media.createMediaClient({
			issueToken: rpc.issueMediaToken,
			onDisconnect: () => {
				this.mediaAttached = false;
				this.mediaRetryAt = Date.now() + 1500;
				if (this.root)
					this.updateView();
			}
		});
		return Promise.all([
			L.resolveDefault(rpc.capabilities(), {}),
			L.resolveDefault(rpc.status(), {}),
			L.resolveDefault(rpc.callHistory(), { calls: [] })
		]).then((results) => {
			this.history = Array.isArray(results[2]?.calls) ? results[2].calls : [];
			this.historyFilter = 'all';
			this.capabilityPending = !results[0] || Object.keys(results[0]).length === 0;
			this.capabilityRetryAt = 0;
			this.dispatch({ type: 'CAPABILITIES', value: results[0] });
			this.dispatch({ type: 'SNAPSHOT', value: results[1] });
			return results;
		});
	},

	async createServiceMap() {
		await uci.load('network');
		const map = new form.Map('qmodem_voip');
		const section = map.section(form.NamedSection, 'main', 'main',
			_('Call service'),
			_('Changes are stored in UCI and take effect after Save & Apply.'));
		section.anonymous = true;
		section.addremove = false;
		const enabled = section.option(form.Flag, 'enabled',
			_('Enable experimental call service'));
		enabled.default = '0';
		enabled.rmempty = false;
		enabled.description = _('Starts modem voice preparation and browser media support.');
		const webEnabled = section.option(form.Flag, 'web_enabled',
			_('Enable HTTPS and WebSocket media access'));
		webEnabled.default = '0';
		webEnabled.rmempty = false;
		webEnabled.description = _('Enables the uhttpd HTTPS listener used by browser WSS media.');
		this.serviceMap = map;
		this.serviceOption = enabled;
		const sip = map.section(form.NamedSection, 'sip', 'sip',
			_('SIP connection'),
			_('The selected network interface supplies the SIP and RTP address.'));
		sip.anonymous = true;
		sip.addremove = false;
		const mode = sip.option(form.ListValue, 'mode', _('Connection mode'));
		mode.value('lan', _('Local SIP'));
		mode.value('outbound', _('Outbound Asterisk'));
		mode.default = 'lan';
		mode.rmempty = false;
		const listener = sip.option(form.ListValue, 'interface', _('Listening interface'));
		listener.default = 'wan';
		listener.rmempty = false;
		listener.description = _('Choose an OpenWrt network interface, such as wan for the lan4 port.');
		const interfaces = uci.sections('network', 'interface') || [];
		interfaces.forEach((network) => {
			if (network['.name'])
				listener.value(network['.name'], network['.name']);
		});
		if (!interfaces.some((network) => network['.name'] === 'wan'))
			listener.value('wan', 'wan');
		const server = sip.option(form.Value, 'outbound_server', _('Asterisk server'));
		server.datatype = 'host';
		server.rmempty = false;
		server.depends('mode', 'outbound');
		const port = sip.option(form.Value, 'outbound_port', _('Asterisk TLS port'));
		port.datatype = 'port';
		port.default = '5061';
		port.rmempty = false;
		port.depends('mode', 'outbound');
		const transport = sip.option(form.ListValue, 'outbound_transport', _('Transport'));
		transport.value('tls', _('TLS'));
		transport.default = 'tls';
		transport.rmempty = false;
		transport.depends('mode', 'outbound');
		const username = sip.option(form.Value, 'outbound_username', _('SIP account'));
		username.rmempty = false;
		username.depends('mode', 'outbound');
		const password = sip.option(form.Value, 'outbound_password', _('SIP password'));
		password.password = true;
		password.rmempty = false;
		password.depends('mode', 'outbound');
		const realm = sip.option(form.Value, 'outbound_realm', _('SIP realm'));
		realm.placeholder = 'asterisk';
		realm.depends('mode', 'outbound');
		const interval = sip.option(form.Value, 'outbound_register_interval', _('Registration interval'));
		interval.datatype = 'range(60,3600)';
		interval.default = '300';
		interval.rmempty = false;
		interval.depends('mode', 'outbound');
		return map;
	},

	dispatch(action) {
		this.state = reducer.reduce(this.state, action);
		if (this.root)
			this.updateView();
	},

	async refresh() {
		if (this.mediaConnecting)
			return null;
		if (this.capabilityPending && Date.now() >= this.capabilityRetryAt) {
			this.capabilityRetryAt = Date.now() + 2000;
			const capabilities = await L.resolveDefault(rpc.capabilities(), null);
			if (capabilities && Object.keys(capabilities).length !== 0) {
				this.capabilityPending = false;
				this.dispatch({ type: 'CAPABILITIES', value: capabilities });
			}
		}
		const request = L.resolveDefault(rpc.status(), null);
		this.statusRequest = request;
		const snapshot = await request;
		if (this.statusRequest === request)
			this.statusRequest = null;
		if (snapshot) {
			this.dispatch({ type: 'SNAPSHOT', value: snapshot });
			await this.syncMedia();
		}
		return this.state.snapshot;
	},

	async refreshHistory() {
		const response = await L.resolveDefault(rpc.callHistory(), null);
		if (!response) {
			this.showError({ error: 'history_unavailable' });
			return;
		}
		this.history = Array.isArray(response.calls) ? response.calls : [];
		this.updateHistory();
	},

	setHistoryFilter(filter) {
		this.historyFilter = filter === 'missed' ? 'missed' : 'all';
		this.updateHistory();
	},

	updateHistory() {
		if (!this.refs?.historyBody)
			return;
		const calls = (this.history || []).filter((call) => this.historyFilter !== 'missed' || call.missed);
		this.refs.historyBody.replaceChildren(...calls.map((call) => E('tr', {}, [
			E('td', {}, [ new Date(Number(call.started_at) * 1000).toLocaleString() ]),
			E('td', {}, [ call.direction === 'incoming' ? _('Incoming') : _('Outgoing') ]),
			E('td', {}, [ call.caller_id_withheld ? _('Withheld number') : (call.remote_number || _('Number unavailable')) ]),
			E('td', {}, [ call.result === 'missed' ? _('Missed') : (call.result === 'failed' ? _('Failed') : _('Completed')) ]),
			E('td', {}, [ `${Math.max(0, Number(call.duration_seconds) || 0)} s` ])
		])));
		this.refs.historyEmpty.hidden = calls.length !== 0;
		this.refs.historyAll.setAttribute('aria-pressed', this.historyFilter === 'all' ? 'true' : 'false');
		this.refs.historyMissed.setAttribute('aria-pressed', this.historyFilter === 'missed' ? 'true' : 'false');
	},

	showError(error) {
		this.dispatch({ type: 'ERROR', value: error });
		if (error)
			ui.addNotification(_('Call operation failed'), E('p', {}, [ reducer.errorMessage(error) ]), 'error');
	},

	async run(action, payload, after) {
		this.dispatch({ type: 'CLEAR_ERROR' });
		try {
			const response = await rpc[action](...(Array.isArray(payload) ? payload : []));
			if (response?.status === 'error') {
				if (action === 'generateSipCredentials')
					this.dispatch({ type: 'CREDENTIAL_RESULT', value: response });
				if (action === 'issueMediaToken')
					this.dispatch({ type: 'MEDIA_RESULT', value: response });
				this.showError(response);
				return null;
			}
			if (response && action === 'generateSipCredentials')
				this.dispatch({ type: 'CREDENTIAL_RESULT', value: response });
			else if (response && action === 'issueMediaToken')
				this.dispatch({ type: 'MEDIA_RESULT', value: response });
			else if (response)
				this.dispatch({ type: 'SNAPSHOT', value: response });
			if (after)
				after(response);
			return response;
		}
		catch (error) {
			this.showError({ error: error.code || 'unknown', message: error.message });
			return null;
		}
	},

	async generateCredentials(event) {
		event.preventDefault();
		if (!this.refs.sipUser.value.trim())
			return;
		await this.run('generateSipCredentials', [ this.refs.sipUser.value.trim() ], (response) => {
			this.refs.generatedUsername.textContent = response.username || '';
			this.refs.generatedPassword.textContent = response.password || '';
			this.refs.generatedCredentials.hidden = false;
		});
	},

	async originate(event) {
		event.preventDefault();
		const number = this.refs.dial.value.trim();
		if (!number)
			return;
		try {
			/* The audio action and form submit are separate DOM events.  A user can
			 * submit while getUserMedia() is still resolving; serialize them so the
			 * idle refresh in startMedia() cannot release the call's stream. */
			if (this.mediaStartRequest)
				await this.mediaStartRequest;
			await this.prepareBrowserAudio();
		}
		catch (error) {
			this.handleMediaError(error);
			return;
		}
		const response = await this.run('originate', [ 'browser', number ]);
		if (!response) {
			this.releasePendingMedia();
			return;
		}
		this.refs.dial.value = '';
		await this.syncMedia();
	},

	async answer() {
		try {
			await this.prepareBrowserAudio();
		}
		catch (error) {
			this.handleMediaError(error);
			return null;
		}
		const response = await this.run('answer', [ 'browser' ]);
		if (!response)
			this.releasePendingMedia();
		else
			await this.syncMedia();
		return response;
	},

	terminate(action) {
		this.disconnectMedia();
		return this.run(action, [ 'browser' ]);
	},

	async sendDtmf(digit) {
		if (this.dtmfPending)
			return null;
		this.dtmfPending = true;
		this.updateView();
		try {
			return await this.run('sendDtmf', [ 'browser', digit ]);
		}
		finally {
			this.dtmfPending = false;
			this.updateView();
		}
	},

	toggleMute() {
		this.muted = !this.muted;
		this.media.setMuted(this.muted);
		this.refs.mute.setAttribute('aria-pressed', this.muted ? 'true' : 'false');
		this.refs.mute.textContent = this.muted ? _('Unmute') : _('Mute');
	},

	async requestMediaStream() {
		if (this.media.hasAudio())
			return null;
		if (this.pendingMediaStream)
			return this.pendingMediaStream;
		if (this.mediaStreamRequest)
			return this.mediaStreamRequest;
		if (this.state.mediaStatus !== 'ready' || !navigator.mediaDevices?.getUserMedia)
			throw Object.assign(new Error('browser audio is unavailable'), { code: 'unsupported' });
		this.mediaStreamRequest = navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1, sampleRate: 48000 } });
		try {
			this.pendingMediaStream = await this.mediaStreamRequest;
			this.dispatch({ type: 'MEDIA_PERMISSION', value: 'granted' });
			return this.pendingMediaStream;
		}
		finally {
			this.mediaStreamRequest = null;
		}
	},

	async prepareBrowserAudio() {
		if (this.media.hasAudio()) {
			await this.media.resume();
			this.dispatch({ type: 'MEDIA_PERMISSION', value: 'granted' });
			return;
		}
		const stream = await this.requestMediaStream();
		await this.media.attachAudio(stream || this.pendingMediaStream, WORKLET_URL);
		this.pendingMediaStream = null;
		await this.media.resume();
		this.dispatch({ type: 'MEDIA_PERMISSION', value: 'granted' });
	},

	releasePendingMedia() {
		if (this.pendingMediaStream)
			this.pendingMediaStream.getTracks().forEach((track) => track.stop());
		this.pendingMediaStream = null;
	},

	closeMedia() {
		this.releasePendingMedia();
		this.media.close();
		this.mediaAttached = false;
	},

	disconnectMedia() {
		this.media.disconnect();
		this.mediaAttached = false;
	},

	handleMediaError(error, notify) {
		this.releasePendingMedia();
		this.media.close();
		this.mediaAttached = false;
		this.dispatch({ type: 'MEDIA_PERMISSION', value: error.name === 'NotAllowedError' ? 'denied' : 'error' });
		if (notify !== false)
			this.showError({ error: error.code || 'unsupported', message: error.message });
		else
			this.updateView();
	},

	async syncMedia() {
		/* Only reducer-accepted state may own media. A delayed poll can return
		 * an old idle snapshot after originate; acting on that raw response
		 * would cancel the new WebSocket while it is still CONNECTING. */
		let snapshot = this.state.snapshot;
		if (snapshot?.state === 'idle') {
			this.disconnectMedia();
			this.mediaRetryAt = 0;
			return;
		}
		if ([ 'disabled', 'fault' ].indexOf(snapshot?.state) !== -1) {
			this.closeMedia();
			return;
		}
		if ([ 'outgoing_setup', 'early_media', 'active' ].indexOf(snapshot?.state) === -1 ||
			(!this.pendingMediaStream && !this.media.hasAudio()) || this.media.isConnected() ||
			this.mediaConnecting || !this.state.mediaUrl || Date.now() < (this.mediaRetryAt || 0))
			return;
		this.mediaConnecting = true;
		try {
			/* Drain the one older request already in flight. Its response is reduced
			 * before use and therefore cannot supersede this call generation. */
			if (this.statusRequest)
				await this.statusRequest;
			snapshot = this.state.snapshot;
			if ([ 'outgoing_setup', 'early_media', 'active' ].indexOf(snapshot?.state) === -1)
				return;
			if (!this.media.hasAudio())
				await this.media.attachAudio(this.pendingMediaStream, WORKLET_URL);
			await this.media.resume();
			/* Build the playback node before opening the socket.  The modem can
			 * produce PCM immediately after the WebSocket upgrade; attaching first
			 * prevents those initial frames from being dropped before onmessage is
			 * installed. */
			await this.media.connect({ media: 'ready', url: this.state.mediaUrl, sessionId: luciRpc.getSessionID(), callRevision: snapshot.revision, httpsOrigin: location.origin });
			this.pendingMediaStream = null;
			this.mediaAttached = true;
			this.dispatch({ type: 'MEDIA_PERMISSION', value: 'granted' });
		}
		catch (error) {
			this.mediaAttached = false;
			this.media.disconnect();
			this.mediaRetryAt = Date.now() + 1500;
			if (!this.media.hasAudio())
				this.handleMediaError(error, false);
		}
		finally {
			this.mediaConnecting = false;
		}
	},

	startMedia() {
		if (this.state.mediaStatus !== 'ready')
			return;
		if (this.mediaStartRequest)
			return this.mediaStartRequest;
		this.mediaStartRequest = (async () => {
			try {
				await this.prepareBrowserAudio();
				/* Permission prompts can outlast a call-state transition.  Refresh
				 * before minting the token so the WebSocket is opened against the
				 * current call revision as soon as audio permission is granted. */
				await this.refresh();
			}
			catch (error) {
				this.handleMediaError(error);
			}
			finally {
				this.mediaStartRequest = null;
			}
		})();
		return this.mediaStartRequest;
	},

	maybePrepareMedia(model) {
		if (this.autoMediaAttempted || this.mediaStartRequest || this.media.hasAudio() ||
			!model.supported || !model.enabled || !model.stable ||
			this.state.mediaStatus !== 'ready' || this.state.mediaPermission === 'denied')
			return;
		this.autoMediaAttempted = true;
		this.mediaStartRequest = this.prepareBrowserAudio()
			.then(() => this.refresh())
			.catch((error) => this.handleMediaError(error, false))
			.finally(() => {
				this.mediaStartRequest = null;
				if (this.root)
					this.updateView();
			});
	},

	handleEvent(event) {
		if (event?.event) {
			this.dispatch({ type: 'EVENT', value: event });
			void this.syncMedia();
			if (event.state === 'idle' || event.state === 'disabled')
				void this.refreshHistory();
		}
	},

	updateTimer(seconds) {
		if (!this.refs.timer)
			return;
		seconds = Math.max(0, Math.floor(Number(seconds) || 0));
		this.refs.timer.textContent = `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
	},

	callDetail(model, snapshot) {
		if (model.disabledReason)
			return model.disabledReason;
		if (snapshot.caller_id_withheld)
			return _('Caller ID withheld.');
		if (!snapshot.number_present)
			return model.callTimerVisible || model.canAnswer ? _('Number unavailable.') : _('No active call.');
		if (model.state === 'incoming_ringing')
			return _('Incoming call from %s.').format(model.remoteNumber);
		if (model.state === 'outgoing_setup' || model.state === 'early_media')
			return _('Calling %s.').format(model.remoteNumber);
		return _('Connected with %s.').format(model.remoteNumber);
	},

	updateView() {
		const model = reducer.viewModel(this.state);
		const snapshot = this.state.snapshot || { state: 'disabled', enabled: false };
		const statusText = STATE_LABELS[model.state] || _('Unknown state');
		this.refs.support.textContent = model.capabilityPending ? _('Waiting') : (model.supported ? _('Supported') : _('Unsupported'));
		this.refs.support.className = `label ${model.capabilityPending ? '' : (model.supported ? 'success' : 'warning')}`.trim();
		this.refs.capability.textContent = model.capabilityPending ? _('Waiting for modem discovery and the read-only safety probe.') : (model.supported ? _('RM520N-GL passed the safety probe.') : _('The read-only hardware probe did not pass. Call controls stay unavailable.'));
		this.refs.serviceStatus.textContent = model.enabled ? _('Running') : _('Stopped');
		this.refs.serviceStatus.className = `label ${model.enabled ? 'success' : ''}`.trim();
		this.refs.serviceDetail.textContent = model.enabled ? _('The runtime is ready for calls.') : _('Use the UCI switch below, then Save & Apply.');
		if (this.refs.serviceSwitch)
			this.refs.serviceSwitch.disabled = model.capabilityPending || (!model.supported && !this.refs.serviceSwitch.checked);
		this.refs.sipStatus.textContent = this.state.credentialStatus === 'not_ready' ? _('SIP credential rotation is not ready in this backend.') : (model.registration === 'configured' ? _('Credentials configured. Registration is not reported by v1 status.') : _('SIP account is not configured. Registration is not reported by v1 status.'));
		if (!this.refs.sipUser.value && this.state.credentialUsername)
			this.refs.sipUser.value = this.state.credentialUsername;
		this.refs.callStatus.textContent = statusText;
		this.refs.callStatus.className = `label ${STATE_STYLES[model.state] || ''}`.trim();
		const callVisible = model.callTimerVisible || model.canAnswer;
		this.refs.remoteParty.textContent = !callVisible ? _('No active call') :
			(snapshot.caller_id_withheld ? _('Withheld number') : (model.remoteNumber || _('Number unavailable')));
		this.refs.callDetail.textContent = this.callDetail(model, snapshot);
		if (this.refs.incomingParty)
			this.refs.incomingParty.textContent = this.refs.remoteParty.textContent;
		this.refs.dial.disabled = !model.canOriginate;
		this.refs.dialButton.disabled = !model.canOriginate;
		this.refs.hangup.disabled = !model.canHangup;
		this.refs.mute.disabled = !model.canMute;
		this.refs.keypad.forEach((key) => { key.disabled = !model.canKeypad || this.dtmfPending; });
		this.refs.mediaStatus.textContent = this.mediaAttached ? _('Browser audio is connected.') : (this.state.mediaStatus === 'ready' ? _('Browser audio connects while the call is being established.') : _('Media backend is not ready. Browser audio remains inactive.'));
		this.refs.mediaAction.disabled = this.state.mediaStatus !== 'ready' || !model.enabled || Boolean(this.mediaStartRequest) || this.mediaAttached;
		this.refs.mediaAction.textContent = this.state.mediaPermission === 'denied' ? _('Retry browser audio') : (this.media.hasAudio() ? _('Resume browser audio') : _('Allow browser audio'));
		this.refs.permission.textContent = this.state.mediaPermission === 'denied' ? _('Microphone permission was denied. Allow it in the browser and try again.') : (this.state.mediaPermission === 'granted' ? _('Microphone permission is active.') : _('No microphone permission has been requested.'));
		this.refs.mediaBadge.textContent = this.mediaAttached ? _('Connected') : (this.media.hasAudio() ? _('Ready') : _('Not connected'));
		this.refs.mediaBadge.className = `label ${this.mediaAttached ? 'success' : (this.media.hasAudio() ? '' : 'warning')}`.trim();
		this.refs.mediaSummary.textContent = this.media.hasAudio() ? _('Microphone and playback are prepared.') : _('Permission will be requested automatically when possible.');
		this.refs.error.textContent = model.errorText;
		this.refs.error.hidden = !model.errorText;
		surface.setIncomingModal(this, model.canAnswer);
		this.updateTimer(model.callDurationSeconds);
		this.refs.timer.hidden = !model.callTimerVisible;
		this.refs.live.textContent = this.state.lastStatusKey !== this.lastRenderedStatus ? statusText : '';
		this.lastRenderedStatus = this.state.lastStatusKey;
		this.maybePrepareMedia(model);
	},

	async render() {
		const serviceMap = await this.createServiceMap();
		const serviceForm = await serviceMap.render();
		surface.build(this, serviceForm);
		this.updateView();
		this.updateHistory();
		this.pollFn = () => this.refresh();
		poll.add(this.pollFn, POLL_SECONDS);
		return this.root;
	},

	remove() {
		if (this.pollFn)
			poll.remove(this.pollFn);
		this.closeMedia();
		surface.setIncomingModal(this, false);
	}
});
