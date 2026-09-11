'use strict';
'require baseclass';
'require ui';

function node(tag, attrs, children) {
	return E(tag, attrs || {}, children || []);
}

function button(label, style, handler, type) {
	return node('button', {
		type: type || 'button',
		class: `cbi-button cbi-button-${style || 'neutral'}`,
		click: handler || null
	}, [ label ]);
}

function field(label, control, help) {
	const content = [ control ];
	if (help)
		content.push(node('div', { class: 'cbi-value-description' }, [ help ]));

	return node('div', { class: 'cbi-value' }, [
		node('label', { class: 'cbi-value-title' }, [ label ]),
		node('div', { class: 'cbi-value-field' }, content)
	]);
}

function statusItem(label, value, detail) {
	return node('div', { class: 'qvoip-status-item' }, [
		node('div', { class: 'qvoip-status-label' }, [ label ]),
		value,
		detail
	]);
}

function textfield(context, name, options) {
	const widget = new ui.Textfield('', options || {});
	const rendered = widget.render();
	context.widgets[name] = widget;
	context.refs[name] = rendered.querySelector('input');
	return rendered;
}

function setIncomingModal(context, visible) {
	if (visible && !context.incomingModalOpen) {
		context.incomingModalOpen = true;
		context.refs.incomingParty = node('p', { class: 'qvoip-modal-party' }, [
			context.refs.remoteParty.textContent
		]);
		context.incomingModal = ui.showModal(_('Incoming call'), [
			context.refs.incomingParty,
			node('div', { class: 'right' }, [
				button(_('Reject'), 'negative', ui.createHandlerFn(context, () => context.terminate('reject'))),
				' ',
				button(_('Answer'), 'positive', ui.createHandlerFn(context, () => context.answer()))
			])
		], 'qvoip-incoming-modal');
	}
	else if (!visible && context.incomingModalOpen) {
		ui.hideModal();
		context.incomingModal = null;
		context.incomingModalOpen = false;
		context.refs.incomingParty = null;
	}
}

function build(context, serviceForm) {
	context.refs = {};
	context.widgets = {};
	context.incomingModal = null;
	context.incomingModalOpen = false;

	context.refs.support = node('span', { class: 'label' });
	context.refs.capability = node('div', { class: 'qvoip-status-detail' });
	context.refs.serviceStatus = node('span', { class: 'label' });
	context.refs.serviceDetail = node('div', { class: 'qvoip-status-detail' });
	context.refs.mediaBadge = node('span', { class: 'label' });
	context.refs.mediaSummary = node('div', { class: 'qvoip-status-detail' });
	context.refs.serviceSwitch = serviceForm?.querySelector('input[type="checkbox"]') || null;

	context.refs.callStatus = node('span', { class: 'label' });
	context.refs.callDetail = node('div', {
		class: 'cbi-section-descr', id: 'qvoip-call-detail'
	});
	context.refs.remoteParty = node('strong', { class: 'qvoip-remote-party' }, [ _('No active call') ]);
	context.refs.timer = node('time', { class: 'qvoip-timer' }, [ '00:00' ]);
	const dial = textfield(context, 'dial', {
		id: 'qvoip-destination', name: 'destination', maxlength: 63, optional: false
	});
	context.refs.dial.setAttribute('type', 'tel');
	context.refs.dial.setAttribute('inputmode', 'tel');
	context.refs.dial.setAttribute('autocomplete', 'off');
	context.refs.dial.setAttribute('spellcheck', 'false');
	context.refs.dial.required = true;
	context.refs.dial.setAttribute('aria-describedby', 'qvoip-call-detail');
	context.refs.dialForm = node('form', { class: 'qvoip-dial-form' });
	context.refs.dialForm.addEventListener('submit', (event) => context.originate(event));
	context.refs.dialButton = button(_('Call'), 'action', null, 'submit');
	context.refs.dialButton.setAttribute('aria-describedby', 'qvoip-call-detail');
	context.refs.dialForm.append(
		field(_('Destination'), dial, _('Use digits and the modem-supported call control characters.')),
		context.refs.dialButton
	);

	context.refs.actions = node('div', { class: 'cbi-page-actions qvoip-call-actions' });
	context.refs.mute = button(_('Mute'), 'neutral', ui.createHandlerFn(context, () => context.toggleMute()));
	context.refs.hangup = button(_('Hang up'), 'negative', ui.createHandlerFn(context, () => context.terminate('hangup')));
	context.refs.actions.append(context.refs.mute, ' ', context.refs.hangup);
	context.refs.mute.setAttribute('aria-describedby', 'qvoip-call-detail');
	context.refs.hangup.setAttribute('aria-describedby', 'qvoip-call-detail');

	context.refs.keypad = [];
	const keypad = node('div', { class: 'qvoip-keypad', 'aria-describedby': 'qvoip-keypad-help' },
		['1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#'].map((value) => {
			const key = button(value, 'neutral', ui.createHandlerFn(context, () => context.sendDtmf(value)));
			key.classList.add('qvoip-key');
			key.disabled = true;
			key.dataset.digit = value;
			key.setAttribute('aria-label', _('Send DTMF %s').format(value));
			key.setAttribute('aria-describedby', 'qvoip-keypad-help');
			context.refs.keypad.push(key);
			return key;
		}));

	context.refs.mediaStatus = node('div', { class: 'cbi-section-descr' });
	context.refs.mediaAction = button(_('Allow browser audio'), 'action',
		ui.createHandlerFn(context, () => context.startMedia()));
	context.refs.permission = node('div', {
		class: 'cbi-value-description', id: 'qvoip-media-help'
	});
	context.refs.mediaAction.setAttribute('aria-describedby', 'qvoip-media-help');
	context.refs.live = node('div', { class: 'qvoip-live', 'aria-live': 'polite' });
	context.refs.error = node('div', { class: 'alert-message error', role: 'alert', hidden: true });

	const overview = node('div', { class: 'cbi-section qvoip-overview' }, [
		node('h3', {}, [ _('Service status') ]),
		node('div', { class: 'qvoip-status-grid' }, [
			statusItem(_('Hardware support'), context.refs.support, context.refs.capability),
			statusItem(_('Call service'), context.refs.serviceStatus, context.refs.serviceDetail),
			statusItem(_('Browser media'), context.refs.mediaBadge, context.refs.mediaSummary)
		])
	]);

	serviceForm.classList.add('qvoip-service-map');
	const callPanel = node('div', { class: 'cbi-section qvoip-call-panel' }, [
		node('div', { class: 'qvoip-section-heading' }, [
			node('h3', {}, [ _('Call') ]), context.refs.callStatus
		]),
		node('div', { class: 'qvoip-call-summary' }, [
			node('div', {}, [
				node('div', { class: 'qvoip-status-label' }, [ _('Remote party') ]),
				context.refs.remoteParty
			]),
			context.refs.timer
		]),
		context.refs.callDetail,
		context.refs.dialForm,
		context.refs.actions,
		node('div', { class: 'qvoip-dtmf' }, [
			node('h4', {}, [ _('DTMF keypad') ]),
			node('div', { id: 'qvoip-keypad-help', class: 'cbi-value-description' }, [
				_('Available while a browser-owned call is active.')
			]),
			keypad
		])
	]);

	const mediaPanel = node('div', { class: 'cbi-section qvoip-media-panel' }, [
		node('h3', {}, [ _('Audio and media') ]),
		context.refs.mediaStatus,
		context.refs.permission,
		node('div', { class: 'cbi-page-actions' }, [ context.refs.mediaAction ])
	]);

	context.refs.historyBody = node('tbody');
	context.refs.historyEmpty = node('div', { class: 'cbi-section-descr' }, [ _('No call records.') ]);
	context.refs.historyAll = button(_('All calls'), 'neutral', ui.createHandlerFn(context, () => context.setHistoryFilter('all')));
	context.refs.historyMissed = button(_('Missed'), 'neutral', ui.createHandlerFn(context, () => context.setHistoryFilter('missed')));
	context.refs.historyRefresh = button(_('Refresh'), 'neutral', ui.createHandlerFn(context, () => context.refreshHistory()));
	const historyPanel = node('div', { class: 'cbi-section qvoip-history-panel' }, [
		node('div', { class: 'qvoip-section-heading' }, [
			node('h3', {}, [ _('Call history') ]),
			node('div', { class: 'qvoip-history-tools' }, [
				context.refs.historyAll, ' ', context.refs.historyMissed, ' ', context.refs.historyRefresh
			])
		]),
		context.refs.historyEmpty,
		node('div', { class: 'table cbi-section-table qvoip-history-table' }, [
			node('table', {}, [
				node('thead', {}, [ node('tr', {}, [
					node('th', {}, [ _('Time') ]), node('th', {}, [ _('Direction') ]),
					node('th', {}, [ _('Remote party') ]), node('th', {}, [ _('Result') ]),
					node('th', {}, [ _('Duration') ])
				]) ]), context.refs.historyBody
			])
		])
	]);

	context.root = node('div', {
		class: 'qvoip-page', 'data-event-topic': context.rpc.eventTopic
	}, [
		node('h2', {}, [ _('Experimental QModem voice') ]),
		node('div', { class: 'cbi-map-descr' }, [
			_('Place and receive one modem call. Unsupported hardware remains disabled.')
		]),
		overview,
		serviceForm,
		node('div', { class: 'qvoip-workspace-grid' }, [ callPanel, mediaPanel ]),
		historyPanel,
		context.refs.error,
		context.refs.live
	]);
	return context.root;
}

const api = Object.freeze({ build, setIncomingModal });

if (typeof module !== 'undefined' && module.exports && typeof baseclass === 'undefined')
	module.exports = api;
else
	return baseclass.extend(api);
