#!/usr/bin/env python3
"""Complete an explicitly requested Paper Pro OAuth login through SSH.

No access/refresh tokens leave the tablet. Google uses a localhost SSH forward;
Dropbox returns its one-use authorization code through the private SSH channel.
An optional maintainer registration file configures this installation separately.
"""
import argparse
import getpass
import ipaddress
import json
import os
from pathlib import Path
import re
import shlex
import stat
import subprocess
import tempfile
import time
from urllib.parse import parse_qs, urlparse
import webbrowser

REMOTE_DIRECTORY = '/home/root/.config/Quaderno/cloud'
PROVIDERS = {'googleDrive', 'dropbox', 'oneDrive'}


class CloudSetupError(ValueError):
    pass


def validate_host(value):
    try:
        address = ipaddress.ip_address(value)
        if address.version != 4 or not address.is_private or address.is_loopback or address.is_unspecified:
            raise CloudSetupError('Use the Paper Pro private IPv4 address.')
    except ValueError:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]{0,62}\.local', value):
            raise CloudSetupError('Use the Paper Pro private IPv4 address or its .local hostname.')
    return value


def validate_registration(data):
    if not isinstance(data, dict) or not data or set(data) - PROVIDERS:
        raise CloudSetupError('Registration must contain only googleDrive, dropbox or oneDrive.')
    for provider, config in data.items():
        if not isinstance(config, dict) or set(config) - {'clientId', 'clientSecret', 'tenant'}:
            raise CloudSetupError('Unexpected registration fields.')
        client = config.get('clientId', '')
        if not isinstance(client, str) or not re.fullmatch(r'[A-Za-z0-9._-]{1,256}', client):
            raise CloudSetupError('A valid maintainer-owned clientId is required.')
        if provider == 'googleDrive' and not client.endswith('.apps.googleusercontent.com'):
            raise CloudSetupError('Google requires a Desktop OAuth client registration.')
        secret = config.get('clientSecret', '')
        if not isinstance(secret, str) or len(secret) > 1024 or any(ch in secret for ch in '\r\n\0'):
            raise CloudSetupError('Invalid optional client secret.')
        if provider != 'googleDrive' and secret:
            raise CloudSetupError('Dropbox and Microsoft must use public clients, without a client secret.')
        if provider == 'oneDrive' and not re.fullmatch(r'common|consumers|organizations|[0-9a-fA-F-]{36}', config.get('tenant', 'common')):
            raise CloudSetupError('Invalid Microsoft tenant.')
    return data


def validate_request(data, current_time=None):
    if not isinstance(data, dict) or data.get('provider') not in PROVIDERS:
        raise CloudSetupError('No supported authorization request is pending.')
    current_time = time.time() if current_time is None else current_time
    expires = data.get('expiresAt', 0)
    if not isinstance(expires, (int, float)) or not current_time < expires <= current_time + 1800:
        raise CloudSetupError('The authorization request expired. Press Sign in again.')
    if not isinstance(data.get('requestId'), str) or not re.fullmatch(r'[A-Za-z0-9_-]{32,128}', data['requestId']):
        raise CloudSetupError('Invalid authorization request identifier.')
    url = data.get('authorizationUrl', '')
    if not isinstance(url, str) or len(url) > 16384:
        raise CloudSetupError('Invalid official authorization URL.')
    parsed = urlparse(url)
    if parsed.scheme != 'https' or parsed.username or parsed.password or parsed.fragment or parsed.port not in (None, 443):
        raise CloudSetupError('Authorization must use the provider HTTPS page.')
    provider = data['provider']
    if provider == 'googleDrive':
        if parsed.hostname != 'accounts.google.com' or parsed.path != '/o/oauth2/v2/auth':
            raise CloudSetupError('Unexpected Google authorization page.')
        port = data.get('callbackPort')
        if not isinstance(port, int) or isinstance(port, bool) or not 1024 <= port <= 65535:
            raise CloudSetupError('Invalid Google callback port.')
        query = parse_qs(parsed.query)
        if query.get('redirect_uri') != [f'http://127.0.0.1:{port}/oauth/callback'] or query.get('state') != [data['requestId']]:
            raise CloudSetupError('The Google callback does not match this tablet request.')
        if query.get('code_challenge_method') != ['S256']:
            raise CloudSetupError('Google authorization requires PKCE S256.')
    elif provider == 'dropbox':
        if parsed.hostname != 'www.dropbox.com' or parsed.path != '/oauth2/authorize':
            raise CloudSetupError('Unexpected Dropbox authorization page.')
        query = parse_qs(parsed.query)
        if query.get('state') != [data['requestId']] or query.get('code_challenge_method') != ['S256']:
            raise CloudSetupError('Dropbox authorization does not match this tablet request.')
    elif parsed.hostname not in {'microsoft.com', 'www.microsoft.com', 'login.microsoftonline.com'}:
        raise CloudSetupError('Unexpected Microsoft authorization page.')
    return data


class Tablet:
    def __init__(self, host, control=None):
        self.target = 'root@' + validate_host(host)
        self.temporary = None
        self.owned = control is None
        if self.owned:
            self.temporary = tempfile.TemporaryDirectory(prefix='quaderno-cloud-', dir='/tmp')
            self.control = str(Path(self.temporary.name) / 'ssh')
        else:
            self.control = str(Path(control).expanduser())
            if not stat.S_ISSOCK(os.stat(self.control).st_mode):
                raise CloudSetupError('The SSH control path is not an existing socket.')
        self.base = ['ssh', '-S', self.control, '-o', 'ConnectTimeout=8']
        self.forward = None

    def __enter__(self):
        try:
            if self.owned:
                subprocess.run(self.base + ['-M', '-N', '-f', '-o', 'ControlPersist=no', self.target], check=True)
            identity = self.run('uname -m; dd if=/sys/devices/soc0/machine bs=128 count=1 2>/dev/null').decode().lower()
            if 'aarch64' not in identity or not any(name in identity for name in ['ferrari', 'paper pro']):
                raise CloudSetupError('The SSH destination is not a Paper Pro.')
        except BaseException:
            self.__exit__(None, None, None)
            raise
        return self

    def run(self, command, data=None, check=True):
        result = subprocess.run(self.base + ['-o', 'BatchMode=yes', self.target, command], input=data,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        if check and result.returncode:
            raise CloudSetupError('The private SSH operation failed; check the tablet connection and pending login.')
        return result.stdout

    def pending(self):
        path = shlex.quote(REMOTE_DIRECTORY + '/oauth-browser-request.json')
        raw = self.run(f'dd if={path} bs=16385 count=1 2>/dev/null', check=False)
        if not raw:
            return None
        if len(raw) > 16384:
            raise CloudSetupError('Authorization metadata is too large.')
        return validate_request(json.loads(raw))

    def write_private(self, filename, data):
        if filename not in {'oauth-registration.json', 'oauth-browser-response.json'}:
            raise CloudSetupError('Unsupported private configuration file.')
        command = '''set -eu
umask 077
directory=/home/root/.config/Quaderno/cloud
[ ! -L "$directory" ]
mkdir -p "$directory"
chmod 700 "$directory"
target="$directory/''' + filename + '''"
[ ! -L "$target" ]
temporary=$(mktemp "$directory/.oauth.XXXXXX")
trap 'rm -f "$temporary"' EXIT
cat > "$temporary"
chmod 600 "$temporary"
mv -f "$temporary" "$target"
'''
        self.run(command, json.dumps(data, separators=(',', ':')).encode())

    def forward_google(self, port):
        self.forward = f'127.0.0.1:{port}:127.0.0.1:{port}'
        result = subprocess.run(self.base + ['-O', 'forward', '-L', self.forward, '-o', 'ExitOnForwardFailure=yes', self.target],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        if result.returncode:
            self.forward = None
            raise CloudSetupError('The local callback port is busy. Cancel the tablet login and start a new one.')

    def __exit__(self, *unused):
        try:
            if self.forward:
                try:
                    subprocess.run(self.base + ['-O', 'cancel', '-L', self.forward, self.target],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10, check=False)
                except (OSError, subprocess.SubprocessError):
                    pass
        finally:
            try:
                if self.owned:
                    subprocess.run(self.base + ['-O', 'exit', self.target],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10, check=False)
            except (OSError, subprocess.SubprocessError):
                pass
            finally:
                if self.temporary:
                    self.temporary.cleanup()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='10.11.99.1', help='Paper Pro USB or private Wi-Fi address')
    parser.add_argument('--control', help='Reuse an already authenticated SSH control socket')
    parser.add_argument('--registration', type=Path, help='Install a maintainer OAuth registration JSON and exit')
    args = parser.parse_args()
    registration = None
    if args.registration:
        info = args.registration.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077 or info.st_size > 65536:
            raise CloudSetupError('Registration must be a regular private file (chmod 600), at most64KiB.')
        registration = validate_registration(json.loads(args.registration.read_text()))
    with Tablet(args.host, args.control) as tablet:
        if registration is not None:
            tablet.write_private('oauth-registration.json', registration)
            print('Registration installed privately. Reopen the cloud account dialog on Paper Pro, then choose Sign in.')
            return
        pending = tablet.pending()
        if pending is None:
            raise CloudSetupError('Open the cloud account in Quaderno on Paper Pro and press Sign in first.')
        if pending['provider'] == 'googleDrive':
            tablet.forward_google(pending['callbackPort'])
        if not webbrowser.open(pending['authorizationUrl'], new=1, autoraise=True):
            raise CloudSetupError('The browser could not be opened. No account credentials have been transferred.')
        print('Complete sign-in in the official browser page. Keep this assistant open.')
        if pending['provider'] == 'dropbox':
            code = getpass.getpass('Paste the authorization code supplied by Dropbox (hidden): ').strip()
            if not code or len(code) > 4096 or any(ch in code for ch in '\r\n\0 '):
                raise CloudSetupError('Invalid Dropbox authorization code.')
            tablet.write_private('oauth-browser-response.json', {'requestId': pending['requestId'], 'code': code})
            del code
        while time.time() < pending['expiresAt']:
            time.sleep(2)
            current = tablet.pending()
            if current is None or current['requestId'] != pending['requestId']:
                print('The login request ended. Check Quaderno for the connection result.')
                return
        raise CloudSetupError('Authorization expired. Start a new sign-in from Quaderno.')


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\nAssistant closed. Cancel the pending login in Quaderno if needed.')
        raise SystemExit(130)
    except (ValueError, OSError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        # Never print provider URLs, response bodies, client secrets or auth codes.
        print('Cloud sign-in could not complete: ' + str(error) if isinstance(error, CloudSetupError)
              else 'Cloud sign-in could not complete. Check the private configuration and SSH connection.')
        raise SystemExit(1)
