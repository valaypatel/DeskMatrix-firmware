#!/usr/bin/env python3
# tools/spotify_bootstrap.py
"""One-time Spotify OAuth bootstrap for the Office Desk Display.

Run this once (from any machine — the login itself can happen in any
browser, including your phone's) to mint a Spotify refresh token and push it
to the device's config. After this, the device polls Spotify's API on its
own; this script is never needed again unless you revoke access.
"""
import argparse
import json
import urllib.parse
import urllib.request


def build_authorize_url(client_id: str, redirect_uri: str) -> str:
    params = {
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": redirect_uri,
        "scope": "user-read-currently-playing",
    }
    return "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode(params)


def extract_code_from_redirect(pasted_url: str) -> str:
    """Pulls the `code` query param out of the URL the browser was sent to
    after login. Works even though that URL fails to load (it points at
    127.0.0.1, unreachable from a phone) — the browser still shows the full
    URL in its address bar for the user to copy."""
    parsed = urllib.parse.urlparse(pasted_url)
    query = urllib.parse.parse_qs(parsed.query)
    if "code" not in query:
        raise ValueError(f"no 'code' parameter found in: {pasted_url}")
    return query["code"][0]


def exchange_code_for_refresh_token(client_id: str, client_secret: str, code: str, redirect_uri: str) -> str:
    data = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": redirect_uri,
        "client_id": client_id,
        "client_secret": client_secret,
    }).encode()
    req = urllib.request.Request(
        "https://accounts.spotify.com/api/token",
        data=data,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    with urllib.request.urlopen(req) as resp:
        body = json.loads(resp.read())
    if "refresh_token" not in body:
        raise RuntimeError(f"token exchange failed: {body}")
    return body["refresh_token"]


def push_to_device(device_ip: str, client_id: str, client_secret: str, refresh_token: str, poll_sec: int) -> None:
    get_req = urllib.request.Request(f"http://{device_ip}/api/config")
    with urllib.request.urlopen(get_req) as resp:
        config = json.loads(resp.read())
    config["spotify"] = {
        "clientId": client_id,
        "clientSecret": client_secret,
        "refreshToken": refresh_token,
        "pollSec": poll_sec,
    }
    put_req = urllib.request.Request(
        f"http://{device_ip}/api/config",
        data=json.dumps(config).encode(),
        method="PUT",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(put_req) as resp:
        print(f"pushed to device: {resp.status} {resp.read().decode()}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-id", required=True)
    parser.add_argument("--client-secret", required=True)
    parser.add_argument("--redirect-uri", default="http://127.0.0.1:8888/callback")
    parser.add_argument("--poll-sec", type=int, default=5)
    parser.add_argument("--device-ip", help="If given, pushes the resulting config straight to the device")
    args = parser.parse_args()

    print("Open this URL in any browser (your phone is fine) and log in:\n")
    print(build_authorize_url(args.client_id, args.redirect_uri))
    print("\nAfter logging in, the browser will try to load a 127.0.0.1 page and fail — that's expected.")
    pasted = input("Copy the full URL from the browser's address bar and paste it here: ").strip()

    code = extract_code_from_redirect(pasted)
    refresh_token = exchange_code_for_refresh_token(args.client_id, args.client_secret, code, args.redirect_uri)
    print(f"\nRefresh token: {refresh_token}")

    if args.device_ip:
        push_to_device(args.device_ip, args.client_id, args.client_secret, refresh_token, args.poll_sec)


if __name__ == "__main__":
    main()
