#!/usr/bin/env python3
"""Offline validation of OAuth handoff data and SSH cleanup; no network calls."""
import copy
import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest import mock
from urllib.parse import urlencode

SCRIPT = Path(__file__).resolve().parent.parent / 'scripts/cloud-auth-paper-pro.py'
SPEC = importlib.util.spec_from_file_location('quaderno_cloud_helper', SCRIPT)
helper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(helper)


def pending(provider='googleDrive'):
    request_id = 'A' * 43
    data = {'provider': provider, 'requestId': request_id, 'expiresAt': 1600, 'callbackPort': 48321}
    if provider == 'googleDrive':
        query = urlencode({'state': request_id, 'redirect_uri': 'http://127.0.0.1:48321/oauth/callback',
                           'code_challenge_method': 'S256', 'code_challenge': 'B' * 43})
        data['authorizationUrl'] = 'https://accounts.google.com/o/oauth2/v2/auth?' + query
    elif provider == 'dropbox':
        data['authorizationUrl'] = 'https://www.dropbox.com/oauth2/authorize?' + urlencode(
            {'state': request_id, 'code_challenge_method': 'S256'})
    else:
        data['authorizationUrl'] = 'https://microsoft.com/devicelogin'
        data['callbackPort'] = 0
    return data


class CloudHelperTests(unittest.TestCase):
    def test_valid_provider_requests(self):
        for provider in helper.PROVIDERS:
            with self.subTest(provider=provider):
                self.assertEqual(helper.validate_request(pending(provider), 1000)['provider'], provider)

    def test_expired_nonce_and_duplicate_callback_rejected(self):
        changes = [dict(expiresAt=999), dict(expiresAt=3000), dict(requestId=''),
                   dict(callbackPort=True), dict(callbackPort=22), dict(callbackPort=70000),
                   dict(provider='unknown')]
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                helper.validate_request(dict(pending(), **change), 1000)
        for addition in ['&state=wrong', '&redirect_uri=http%3A%2F%2F127.0.0.1%3A48321%2Fother']:
            data = pending(); data['authorizationUrl'] += addition
            with self.assertRaises(ValueError): helper.validate_request(data, 1000)

    def test_https_provider_and_loopback_port_must_match(self):
        for old, new in [('https://', 'http://'), ('accounts.google.com', 'accounts.google.com.attacker.invalid'),
                         ('accounts.google.com', 'secret@accounts.google.com'),
                         ('127.0.0.1%3A48321', '127.0.0.1%3A48322'), ('S256', 'plain'),
                         ('127.0.0.1%3A48321', '192.168.1.9%3A48321')]:
            data = pending(); data['authorizationUrl'] = data['authorizationUrl'].replace(old, new)
            with self.subTest(old=old, new=new), self.assertRaises(ValueError):
                helper.validate_request(data, 1000)
        for provider in ['dropbox', 'oneDrive']:
            data = pending(provider); data['authorizationUrl'] = 'https://attacker.invalid/login'
            with self.assertRaises(ValueError): helper.validate_request(data, 1000)

    def test_host_cannot_be_option_public_address_or_loopback(self):
        for host in ['--proxy-command=evil', '127.0.0.1', '0.0.0.0', '8.8.8.8', 'example.com',
                     'user@192.168.1.2', '192.168.1.2;echo', 'localhost', '::1']:
            with self.subTest(host=host), self.assertRaises(ValueError): helper.validate_host(host)
        for host in ['10.11.99.1', '192.168.1.20', '172.16.1.4', 'paper-pro.local']:
            self.assertEqual(helper.validate_host(host), host)

    def test_registration_is_separate_and_public_clients_have_no_secrets(self):
        valid = {'googleDrive': {'clientId': 'fixture.apps.googleusercontent.com'},
                 'dropbox': {'clientId': 'fixture-key'},
                 'oneDrive': {'clientId': '11111111-2222-3333-4444-555555555555', 'tenant': 'common'}}
        self.assertEqual(helper.validate_registration(valid), valid)
        for provider in ['dropbox', 'oneDrive']:
            invalid = copy.deepcopy(valid); invalid[provider]['clientSecret'] = 'fixture-secret'
            with self.assertRaises(ValueError): helper.validate_registration(invalid)
        for invalid in [{}, {'unknown': {}}, {'googleDrive': {'clientId': 'web-client'}},
                        {'oneDrive': {'clientId': 'fixture', 'tenant': '../other'}},
                        {'dropbox': {'clientId': 'fixture', 'accessToken': 'never-import-tokens'}}]:
            with self.assertRaises(ValueError): helper.validate_registration(invalid)

    @staticmethod
    def tablet(owned=True):
        tablet = object.__new__(helper.Tablet)
        tablet.owned = owned; tablet.control = '/tmp/fixture-unused'; tablet.target = 'root@10.11.99.1'
        tablet.base = ['ssh', '-S', tablet.control]; tablet.forward = None
        tablet.temporary = mock.Mock()
        return tablet

    def test_failed_identity_check_closes_owned_master(self):
        tablet = self.tablet()
        tablet.run = mock.Mock(side_effect=helper.CloudSetupError('fixture identity failure'))
        with mock.patch.object(helper.subprocess, 'run') as run:
            with self.assertRaises(ValueError): tablet.__enter__()
        self.assertTrue(any('-O' in call.args[0] and 'exit' in call.args[0] for call in run.call_args_list))
        tablet.temporary.cleanup.assert_called_once()

    def test_forward_cleanup_timeout_still_closes_master_and_tempdir(self):
        tablet = self.tablet(); tablet.forward = '127.0.0.1:48321:127.0.0.1:48321'
        def command(args, **unused):
            if 'cancel' in args: raise subprocess.TimeoutExpired(args, 10)
            return subprocess.CompletedProcess(args, 0)
        with mock.patch.object(helper.subprocess, 'run', side_effect=command) as run:
            tablet.__exit__(None, None, None)
        self.assertTrue(any('exit' in call.args[0] for call in run.call_args_list))
        tablet.temporary.cleanup.assert_called_once()

    def test_existing_ssh_master_is_never_closed(self):
        tablet = self.tablet(owned=False); tablet.forward = '127.0.0.1:48321:127.0.0.1:48321'
        with mock.patch.object(helper.subprocess, 'run') as run:
            tablet.__exit__(None, None, None)
        self.assertEqual(len(run.call_args_list), 1)
        self.assertIn('cancel', run.call_args_list[0].args[0])
        self.assertNotIn('exit', run.call_args_list[0].args[0])


if __name__ == '__main__':
    unittest.main()
